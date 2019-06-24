/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_Registration_h
#define mozilla_mscom_Registration_h

#include <memory>

struct IUnknown;

namespace mozilla {
namespace mscom {

/**
 * Assumptions:
 * (1) The DLL is located in the same dir as the module this code resides in;
 * (2) The DLL exports GetProxyDllInfo. This is not exported by default; it must
 *     be specified in the EXPORTS section of the DLL's module definition file.
 */
class RegisteredProxy
{
public:
  RegisteredProxy(uintptr_t aModule, IUnknown* aClassObject, uint32_t aRegCookie);
  RegisteredProxy(RegisteredProxy&& aOther);
  RegisteredProxy& operator=(RegisteredProxy&& aOther);

  ~RegisteredProxy();

private:
  RegisteredProxy() = delete;
  RegisteredProxy(RegisteredProxy&) = delete;
  RegisteredProxy& operator=(RegisteredProxy&) = delete;

private:
  // Not using Windows types here because we shouldn't #include windows.h here
  // since it might pull in COM code which we want to do very carefully in
  // Registrationc.cpp.
  uintptr_t mModule;
  IUnknown* mClassObject;
  uint32_t  mRegCookie;
};

std::unique_ptr<RegisteredProxy> RegisterProxyDll(const wchar_t* aLeafName);

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_Registration_h

