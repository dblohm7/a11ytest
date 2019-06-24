/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Registration.h"

// COM registration data structures are built with C code, so we need to
// simulate that in our C++ code by defining CINTERFACE
#define CINTERFACE

#include "ArrayLength.h"

#include <objidl.h>
#include <rpcproxy.h>
#include <shlwapi.h>

#include <utility>

using std::forward;
using std::make_unique;
using std::unique_ptr;

/* This code MUST NOT use any non-inlined internal Mozilla APIs, as it will be
   compiled into DLLs that COM may load into non-Mozilla processes! */

namespace {

// This function is defined in generated code for proxy DLLs but is not declared
// in rpcproxy.h, so we need this typedef.
typedef void (RPC_ENTRY *GetProxyDllInfoFnPtr)(const ProxyFileInfo*** aInfo,
                                               const CLSID** aId);

} // anonymous namespace

namespace mozilla {
namespace mscom {

unique_ptr<RegisteredProxy>
RegisterProxyDll(const wchar_t* aLeafName)
{
  HMODULE thisModule = nullptr;
  if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCTSTR>(&RegisterProxyDll),
                         &thisModule)) {
    return nullptr;
  }
  wchar_t modulePathBuf[MAX_PATH + 1] = {0};
  DWORD fileNameResult = GetModuleFileName(thisModule, modulePathBuf,
                                   ArrayLength(modulePathBuf));
  if (!fileNameResult || (fileNameResult == ArrayLength(modulePathBuf) &&
        ::GetLastError() == ERROR_INSUFFICIENT_BUFFER)) {
    return nullptr;
  }
  if (!PathRemoveFileSpec(modulePathBuf)) {
    return nullptr;
  }
  if (!PathAppend(modulePathBuf, aLeafName)) {
    return nullptr;
  }

  HMODULE proxyDll = LoadLibrary(modulePathBuf);
  if (!proxyDll) {
    return nullptr;
  }

  auto DllGetClassObjectFn = reinterpret_cast<LPFNGETCLASSOBJECT>(
      GetProcAddress(proxyDll, "DllGetClassObject"));
  if (!DllGetClassObjectFn) {
    FreeLibrary(proxyDll);
    return nullptr;
  }

  auto GetProxyDllInfoFn = reinterpret_cast<GetProxyDllInfoFnPtr>(
      GetProcAddress(proxyDll, "GetProxyDllInfo"));
  if (!GetProxyDllInfoFn) {
    FreeLibrary(proxyDll);
    return nullptr;
  }

  const ProxyFileInfo** proxyInfo = nullptr;
  const CLSID* proxyClsid = nullptr;
  GetProxyDllInfoFn(&proxyInfo, &proxyClsid);
  if (!proxyInfo || !proxyClsid) {
    FreeLibrary(proxyDll);
    return nullptr;
  }

  IUnknown* classObject = nullptr;
  HRESULT hr = DllGetClassObjectFn(*proxyClsid, IID_IUnknown,
                                   (void**) &classObject);
  if (FAILED(hr)) {
    FreeLibrary(proxyDll);
    return nullptr;
  }

  DWORD regCookie;
  hr = CoRegisterClassObject(*proxyClsid, classObject, CLSCTX_INPROC_SERVER,
                             REGCLS_MULTIPLEUSE, &regCookie);
  if (FAILED(hr)) {
    classObject->lpVtbl->Release(classObject);
    FreeLibrary(proxyDll);
    return nullptr;
  }

  // RegisteredProxy takes ownership of proxyDll and classObject references
  auto result(make_unique<RegisteredProxy>(reinterpret_cast<uintptr_t>(proxyDll),
                                           classObject, regCookie));

  while (*proxyInfo) {
    const ProxyFileInfo& curInfo = **proxyInfo;
    for (unsigned short i = 0, e = curInfo.TableSize; i < e; ++i) {
      hr = CoRegisterPSClsid(*(curInfo.pStubVtblList[i]->header.piid),
                             *proxyClsid);
      if (FAILED(hr)) {
        return nullptr;
      }
    }
    ++proxyInfo;
  }

  return result;
}

RegisteredProxy::RegisteredProxy(uintptr_t aModule, IUnknown* aClassObject,
                                 uint32_t aRegCookie)
  : mModule(aModule)
  , mClassObject(aClassObject)
  , mRegCookie(aRegCookie)
{
}

RegisteredProxy::~RegisteredProxy()
{
  /*
  if (mClassObject) {
    ::CoRevokeClassObject(mRegCookie);
    mClassObject->lpVtbl->Release(mClassObject);
  }
  if (mModule) {
    ::FreeLibrary(reinterpret_cast<HMODULE>(mModule));
  }
  */
}

RegisteredProxy::RegisteredProxy(RegisteredProxy&& aOther)
{
  *this = forward<RegisteredProxy>(aOther);
}

RegisteredProxy&
RegisteredProxy::operator=(RegisteredProxy&& aOther)
{
  mModule = aOther.mModule;
  aOther.mModule = 0;
  mClassObject = aOther.mClassObject;
  aOther.mClassObject = nullptr;
  mRegCookie = aOther.mRegCookie;
  aOther.mRegCookie = 0;
  return *this;
}

} // namespace mscom
} // namespace mozilla
