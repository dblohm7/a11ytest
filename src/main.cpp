#define INITGUID

#include "mscom.h"
#include "winselect.h"
#include "ArrayLength.h"
#include "Accessible2.h"
#include "Registration.h"

#include <oleacc.h>
#include <comdef.h>

#include <deque>
#include <string>
#include <stdio.h>
using namespace std;

#define PRINT_UNIQUE_ID

DEFINE_GUID(IID_IAccessible2, 0xE89F726E,0xC4F4,0x4c19,0xBB,0x19,0xB6,0x47,0xD7,0xFA,0x84,0x78);
_COM_SMARTPTR_TYPEDEF(IAccessible2, IID_IAccessible2);

#define HRCHECK(msg) \
  if (FAILED(hr)) { \
    printf("%s, HRESULT == 0x%08X", msg, hr); \
    return 1; \
  }

IAccessiblePtr
GetParent(IAccessiblePtr& aAcc)
{
  IAccessiblePtr result;

  IDispatchPtr disp;
  HRESULT hr = aAcc->get_accParent(&disp);
  if (FAILED(hr)) {
    return result;
  }

  disp->QueryInterface(IID_IAccessible, (void**)&result);
  printf("GetParent aAcc: 0x%p, parent: 0x%p\n", aAcc.GetInterfacePtr(),
         result.GetInterfacePtr());
  return result;
}

IAccessiblePtr
GetAcc(IAccessible2Ptr& aAcc2)
{
  IAccessiblePtr acc;
  HRESULT hr = aAcc2->QueryInterface(IID_IAccessible, (void**)&acc);
  if (FAILED(hr)) {
    return nullptr;
  }
  return acc;
}

IServiceProviderPtr
GetServiceProvider(IAccessiblePtr& aAcc)
{
  IServiceProviderPtr svcProv;
  HRESULT hr = aAcc->QueryInterface(IID_IServiceProvider, (void**)&svcProv);
  if (FAILED(hr)) {
    return nullptr;
  }
  // printf("IAccessible: 0x%p\n", aAcc.GetInterfacePtr());
  // printf("IServiceProvider: 0x%p\n", svcProv.GetInterfacePtr());
  return svcProv;
}

IAccessible2Ptr
GetIA2(IServiceProviderPtr& aSvcProv)
{
  IAccessible2Ptr acc2;
  HRESULT hr = aSvcProv->QueryService(IID_IAccessible2,
                                      IID_IAccessible2, (void**)&acc2);
  if (FAILED(hr)) {
    printf("QueryService(IID_IAccessible2) failed with hr 0x%08X\n", hr);
    return nullptr;
  }
  // printf("IAccessible2: 0x%p\n", acc2.GetInterfacePtr());
  return acc2;
}

IAccessible2Ptr
GetIA2(IAccessiblePtr& aAcc)
{
  if (!aAcc) {
    return nullptr;
  }
  IServiceProviderPtr svcProv(GetServiceProvider(aAcc));
  if (!svcProv) {
    return nullptr;
  }

  return GetIA2(svcProv);
}

HRESULT
GetUniqueId(IAccessiblePtr& aAcc, long& aOutUniqueId)
{
  IAccessible2Ptr acc2(GetIA2(aAcc));
  if (!acc2) {
    return E_FAIL;
  }
  HRESULT hr = acc2->get_uniqueID(&aOutUniqueId);
  if (SUCCEEDED(hr)) {
    printf("GetUniqueId aAcc: 0x%p, UniqueID: %d\n", aAcc.GetInterfacePtr(),
           aOutUniqueId);
  }
  return hr;
}

HRESULT
GetWindowHandle(IAccessiblePtr& aAcc, HWND& aOutHwnd)
{
  IAccessible2Ptr acc2(GetIA2(aAcc));
  if (!acc2) {
    return E_FAIL;
  }
  HRESULT hr = acc2->get_windowHandle(&aOutHwnd);
  if (SUCCEEDED(hr)) {
    printf("GetWindowHandle aAcc: 0x%p, HWND: 0x%p\n", aAcc.GetInterfacePtr(),
           aOutHwnd);
  }
  return hr;
}

HRESULT
GetParentUniqueId(IAccessiblePtr& aAcc, long& aOutUniqueId)
{
  if (!aAcc) {
    return E_INVALIDARG;
  }
  IAccessiblePtr parent = GetParent(aAcc);
  if (!parent) {
    return E_FAIL;
  }
  return GetUniqueId(parent, aOutUniqueId);
}

#if 0
void
DumpAccInfo(const long aIndex, IAccessiblePtr& aAcc)
{
  long parentUniqueId;
  HRESULT hr = GetParentUniqueId(aAcc, parentUniqueId);
  if (FAILED(hr)) {
    return;
  }

  VARIANT varChildSelf;
  VariantInit(&varChildSelf);
  varChildSelf.vt = VT_I4;
  varChildSelf.lVal = CHILDID_SELF;

  BSTR bstr;
  hr = aAcc->get_accName(varChildSelf, &bstr);
  if (FAILED(hr)) {
    return;
  }

  VARIANT varRole;
  hr = aAcc->get_accRole(varChildSelf, &varRole);
  if (FAILED(hr)) {
    return;
  }
  if (varRole.vt != VT_I4) {
    return;
  }

  printf("Child %d: 0x%p, \"%S\", parent uniqueid is %d, role is 0x%X\n", aIndex,
          aAcc.GetInterfacePtr(), bstr, parentUniqueId, varRole.lVal);
}
#endif

#if 0
void
DumpAccInfo(IAccessiblePtr& aAcc)
{
  printf("BEGIN DumpAccInfo for 0x%p\n", aAcc.GetInterfacePtr());
  HRESULT hr;

  printf("BEGIN GetParentUniqueId for 0x%p\n", aAcc.GetInterfacePtr());
  long parentUniqueId;
  hr = GetParentUniqueId(aAcc, parentUniqueId);
  bool parentUidValid = SUCCEEDED(hr);
  printf("END GetParentUniqueId for 0x%p\n", aAcc.GetInterfacePtr());

  printf("BEGIN GetUniqueId for 0x%p\n", aAcc.GetInterfacePtr());
  long uniqueId;
  hr = GetUniqueId(aAcc, uniqueId);
  bool uidValid = SUCCEEDED(hr);
  printf("END GetUniqueId for 0x%p\n", aAcc.GetInterfacePtr());

  VARIANT varChildSelf;
  VariantInit(&varChildSelf);
  varChildSelf.vt = VT_I4;
  varChildSelf.lVal = CHILDID_SELF;

  BSTR bstr;
  hr = aAcc->get_accName(varChildSelf, &bstr);
  if (FAILED(hr)) {
    printf("get_accName\n");
    return;
  }

  VARIANT varRole;
  hr = aAcc->get_accRole(varChildSelf, &varRole);
  if (FAILED(hr)) {
    printf("get_accRole\n");
    return;
  }
  if (varRole.vt != VT_I4) {
    printf("varRole.vt == VT_I4\n");
    return;
  }

  IAccessiblePtr parent = GetParent(aAcc);

  printf("0x%p, parent is 0x%p, \"%S\", role is 0x%X",
         aAcc.GetInterfacePtr(), parent.GetInterfacePtr(), bstr, varRole.lVal);
  if (uidValid) {
    printf(", uniqueId is %d", uniqueId);
  }
  if (parentUidValid) {
    printf(", parentUniqueId is %d", parentUniqueId);
  }
  printf("\n");
  printf("END DumpAccInfo for 0x%p\n", aAcc.GetInterfacePtr());
}
#endif

#define log(fmt, ...)

#if 0

void
DumpAccInfo(IAccessiblePtr& aAcc)
{
  IAccessiblePtr parent = GetParent(aAcc);
  IAccessible2Ptr parent2 = GetIA2(parent);
  IAccessible2Ptr acc2 = GetIA2(aAcc);

  log("BEGIN DumpAccInfo for 0x%p\n", aAcc.GetInterfacePtr());
  HRESULT hr = E_FAIL;

  log("BEGIN GetParentUniqueId for 0x%p\n", aAcc.GetInterfacePtr());
  long parentUniqueId;
  if (parent2) {
    hr = parent2->get_uniqueID(&parentUniqueId);
  }
  bool parentUidValid = SUCCEEDED(hr);
  log("END GetParentUniqueId for 0x%p\n", aAcc.GetInterfacePtr());

  log("BEGIN GetUniqueId for 0x%p\n", aAcc.GetInterfacePtr());
  hr = E_FAIL;
  long uniqueId;
  if (acc2) {
    hr = acc2->get_uniqueID(&uniqueId);
  }
  bool uidValid = SUCCEEDED(hr);
  if (acc2 && !uidValid) {
    printf("ERROR GetUniqueId for 0x%p failed with code 0x%08X\n",
           acc2.GetInterfacePtr(), hr);
  }
  log("END GetUniqueId for 0x%p\n", aAcc.GetInterfacePtr());
  HWND hwnd;
  if (acc2) {
    hr = acc2->get_windowHandle(&hwnd);
    if (SUCCEEDED(hr)) {
      printf("HWND for 0x%p is 0x%p\n", acc2.GetInterfacePtr(), hwnd);
    } else {
      printf("ERROR get_windowHandle for 0x%p failed with code 0x%08X\n",
             acc2.GetInterfacePtr(), hr);
    }
  }

  VARIANT varChildSelf;
  VariantInit(&varChildSelf);
  varChildSelf.vt = VT_I4;
  varChildSelf.lVal = CHILDID_SELF;

  BSTR bstr;
  hr = aAcc->get_accName(varChildSelf, &bstr);
  if (FAILED(hr)) {
    printf("get_accName\n");
    return;
  }

  VARIANT varRole;
  hr = aAcc->get_accRole(varChildSelf, &varRole);
  if (FAILED(hr)) {
    printf("get_accRole\n");
    return;
  }
  if (varRole.vt != VT_I4) {
    printf("varRole.vt == VT_I4\n");
    return;
  }


  printf("0x%p, parent is 0x%p, \"%S\", role is 0x%X",
         aAcc.GetInterfacePtr(), parent.GetInterfacePtr(), bstr, varRole.lVal);
  if (uidValid) {
    printf(", uniqueId is %d", uniqueId);
  }
  if (parentUidValid) {
    printf(", parentUniqueId is %d", parentUniqueId);
  }
  printf("\n");
  log("END DumpAccInfo for 0x%p\n", aAcc.GetInterfacePtr());
}

#endif

void
DumpAccInfo(IAccessiblePtr& aAcc)
{
  IAccessible2Ptr acc2 = GetIA2(aAcc);
  IAccessiblePtr accBack;
  if (acc2) {
    // accBack = GetAcc(acc2);
  }
  printf("BEGIN\n");
  printf("IAccessible: 0x%p\n", aAcc.GetInterfacePtr());
  BSTR bstr;
  const VARIANT kVarChildSelf = {VT_I4};
  if (SUCCEEDED(aAcc->get_accName(kVarChildSelf, &bstr))) {
    printf("Name: \"%S\"\n", bstr);
    SysFreeString(bstr);
  }
  VARIANT varValue;
  if (SUCCEEDED(aAcc->get_accRole(kVarChildSelf, &varValue))) {
    printf("Role: 0x%x\n", varValue.lVal);
  }
  if (SUCCEEDED(aAcc->get_accState(kVarChildSelf, &varValue))) {
    printf("State: 0x%x\n", varValue.lVal);
  }
  if (acc2) {
    printf("IAccessible2: 0x%p\n", acc2.GetInterfacePtr());
    long childId;
    HRESULT hr = acc2->get_uniqueID(&childId);
    if (SUCCEEDED(hr)) {
      printf("Child ID: 0x%08X\n", childId);
    }
  }
  if (accBack) {
    printf("Back to IAccessible: 0x%p\n", accBack.GetInterfacePtr());
  }
  printf("END\n");
}

IAccessiblePtr
Navigate(IAccessiblePtr& aAcc, long aNavDir)
{
  VARIANT varStart, varOut;
  VariantInit(&varStart);
  varStart.vt = VT_I4;
  varStart.lVal = CHILDID_SELF;

  IAccessiblePtr result;

  HRESULT hr = aAcc->accNavigate(aNavDir, varStart, &varOut);
  if (FAILED(hr)) {
    return result;
  }

  if (varOut.vt != VT_DISPATCH) {
    return result;
  }

  varOut.pdispVal->QueryInterface(IID_IAccessible, (void**)&result);
  return result;
}

IAccessiblePtr
GetFirstChild(IAccessiblePtr& aAcc)
{
  return Navigate(aAcc, NAVDIR_FIRSTCHILD);
}

IAccessiblePtr
GetNextSibling(IAccessiblePtr& aAcc)
{
  return Navigate(aAcc, NAVDIR_NEXT);
}

void
DoDfs(IAccessiblePtr& aAcc)
{
  const unsigned int kMaxLevel = 0xFFFFFFFF;
  unsigned int curLevel = 0;

  std::deque<IAccessiblePtr> q;
  q.push_front(aAcc);

  while (!q.empty()) {
    IAccessiblePtr acc = q.front();
    q.pop_front();
    DumpAccInfo(acc);
    if (curLevel < kMaxLevel) {
      IAccessiblePtr nextAcc = GetFirstChild(acc);
      while (nextAcc) {
        q.push_front(nextAcc);
        nextAcc = GetNextSibling(nextAcc);
        // nextAcc = nullptr;
      }
      ++curLevel;
    }
  }
}

static bool
IsVisibleState(const long aState)
{
  return (aState & (STATE_SYSTEM_INVISIBLE | STATE_SYSTEM_OFFSCREEN)) == 0;
}

static bool
IsVisible(IAccessiblePtr aAcc)
{
  const VARIANT kChildIdSelf = {VT_I4};
  VARIANT varState;
  HRESULT hr = aAcc->get_accState(kChildIdSelf, &varState);
  if (SUCCEEDED(hr) && varState.vt == VT_I4 && IsVisibleState(varState.lVal)) {
    return true;
  }
  return false;
}

IAccessiblePtr
DoDfsFindRole(IAccessiblePtr& aAcc, const long aRole)
{
  const unsigned int kMaxLevel = 0xFFFFFFFF;
  unsigned int curLevel = 0;
  const VARIANT kChildIdSelf = {VT_I4};

  std::deque<IAccessiblePtr> q;
  q.push_front(aAcc);

  while (!q.empty()) {
    IAccessiblePtr acc = q.front();
    q.pop_front();
    VARIANT varRole;
    HRESULT hr = acc->get_accRole(kChildIdSelf, &varRole);
    if (SUCCEEDED(hr) && varRole.vt == VT_I4 && varRole.lVal == aRole) {
      // Check that we're visible too
      if (IsVisible(acc)) {
        return acc;
      }
    }
    if (curLevel < kMaxLevel) {
      IAccessiblePtr nextAcc = GetFirstChild(acc);
      while (nextAcc) {
        q.push_front(nextAcc);
        nextAcc = GetNextSibling(nextAcc);
      }
      ++curLevel;
    }
  }

  return nullptr;
}

char*
GetSource(long uniqueId)
{
  if (uniqueId >= 0) {
    return "other";
  }
  uint32_t contentId = (~uint32_t(uniqueId) & 0x7F000000UL) >> 24;
  if (contentId) {
    return "content";
  }
  return "chrome";
}

int
QueryAccInfo(HWND aHwnd, IAccessiblePtr aAcc)
{
  const VARIANT kChildIdSelf = {VT_I4};
  VARIANT varVal;
  BSTR bstr;
  long childCount, uniqueId;
  AccessibleStates ia2States;
  IA2Locale ia2Locale;
  HWND hwnd;

  IAccessible2Ptr acc2 = GetIA2(aAcc);

  // queries: role, state, ia2 state, keyboard shortcut, ia2 attrs, name, desc, locale, child count, value
  // let's also add uniqueid and hwnd
  HRESULT hr = acc2->get_accRole(kChildIdSelf, &varVal);
  HRCHECK("get_accRole");
  hr = acc2->get_accState(kChildIdSelf, &varVal);
  HRCHECK("get_accState");
  hr = acc2->get_accKeyboardShortcut(kChildIdSelf, &bstr);
  HRCHECK("get_accKeyboardShortcut");
  if (hr == S_OK) {
    SysFreeString(bstr);
  }
  hr = acc2->get_accName(kChildIdSelf, &bstr);
  HRCHECK("get_accName");
  if (hr == S_OK) {
    SysFreeString(bstr);
  }
  hr = acc2->get_accDescription(kChildIdSelf, &bstr);
  HRCHECK("get_accDescription");
  if (hr == S_OK) {
    SysFreeString(bstr);
  }
  hr = acc2->get_accChildCount(&childCount);
  HRCHECK("get_accChildCount");
  hr = acc2->get_accValue(kChildIdSelf, &bstr);
  HRCHECK("get_accValue");
  if (hr == S_OK) {
    SysFreeString(bstr);
  }
  hr = acc2->get_states(&ia2States);
  HRCHECK("get_states");
  hr = acc2->get_locale(&ia2Locale);
  HRCHECK("get_locale");
  hr = acc2->get_attributes(&bstr);
  HRCHECK("get_attributes");
  if (hr == S_OK) {
    SysFreeString(bstr);
  }
  hr = acc2->get_uniqueID(&uniqueId);
  HRCHECK("get_uniqueID");

#if defined(PRINT_UNIQUE_ID)
  printf("ID: 0x%08X (%s)\n", uniqueId, GetSource(uniqueId));
#endif

  hr = acc2->get_windowHandle(&hwnd);
  HRCHECK("get_windowHandle");
  if (hwnd != aHwnd) {
    printf("WTF! hwnd mismatch! You suck!\n");
    return 1;
  }

#if 0
  // Test code
  IAccessibleRelation* relations[64] = {};
  long count = 0;
  hr = acc2->get_relations(64, &relations[0], &count);
#endif
  return 0;
}

int
FindDocumentAndDump(HWND aHwnd)
{
  const VARIANT kChildIdSelf = {VT_I4};
  VARIANT varVal;
  LARGE_INTEGER start, end, freq;
  BSTR bstr;
  long childCount, uniqueId;
  AccessibleStates ia2States;
  IA2Locale ia2Locale;
  HWND hwnd;

  QueryPerformanceCounter(&start);

  IAccessiblePtr root;
  HRESULT hr = AccessibleObjectFromWindow(aHwnd, OBJID_CLIENT, IID_IAccessible,
                                          (void**)&root);
  if (FAILED(hr)) {
    printf("AccessibleObjectFromWindow failed! You suck!\n");
    return 1;
  }
  // printf("OBJID_CLIENT IAccessible: 0x%p\n", acc.GetInterfacePtr());

  IAccessiblePtr doc = DoDfsFindRole(root, ROLE_SYSTEM_DOCUMENT);
  if (!doc) {
    printf("Couldn't find document! You suck!\n");
    return 1;
  }
  // printf("Document: 0x%p\n", doc.GetInterfacePtr());

  IAccessible2Ptr doc2 = GetIA2(doc);

  // queries: role, state, ia2 state, keyboard shortcut, ia2 attrs, name, desc, locale, child count, value
  // let's also add uniqueid and hwnd
  hr = doc2->get_accRole(kChildIdSelf, &varVal);
  HRCHECK("get_accRole");
  hr = doc2->get_accState(kChildIdSelf, &varVal);
  HRCHECK("get_accState");
  hr = doc2->get_accKeyboardShortcut(kChildIdSelf, &bstr);
  HRCHECK("get_accKeyboardShortcut");
  if (hr == S_OK) {
    SysFreeString(bstr);
  }
  hr = doc2->get_accName(kChildIdSelf, &bstr);
  HRCHECK("get_accName");
  if (hr == S_OK) {
    SysFreeString(bstr);
  }
  hr = doc2->get_accDescription(kChildIdSelf, &bstr);
  HRCHECK("get_accDescription");
  if (hr == S_OK) {
    SysFreeString(bstr);
  }
  hr = doc2->get_accChildCount(&childCount);
  HRCHECK("get_accChildCount");
  hr = doc2->get_accValue(kChildIdSelf, &bstr);
  HRCHECK("get_accValue");
  if (hr == S_OK) {
    SysFreeString(bstr);
  }
  hr = doc2->get_states(&ia2States);
  HRCHECK("get_states");
  hr = doc2->get_locale(&ia2Locale);
  HRCHECK("get_locale");
  hr = doc2->get_attributes(&bstr);
  HRCHECK("get_attributes");
  if (hr == S_OK) {
    SysFreeString(bstr);
  }
  hr = doc2->get_uniqueID(&uniqueId);
  HRCHECK("get_uniqueID");
  hr = doc2->get_windowHandle(&hwnd);
  HRCHECK("get_windowHandle");
  if (hwnd != aHwnd) {
    printf("WTF! hwnd mismatch! You suck!\n");
    return 1;
  }

  QueryPerformanceCounter(&end);
  QueryPerformanceFrequency(&freq);

  double startMs = static_cast<double>(start.QuadPart * 1000) / static_cast<double>(freq.QuadPart);
  double endMs = static_cast<double>(end.QuadPart * 1000) / static_cast<double>(freq.QuadPart);
  double diff = endMs - startMs;
  printf("Total execution time: %g ms\n", diff);
  return 0;
}

void
DoDfsVisible(HWND aHwnd, IAccessiblePtr& aAcc)
{
  LARGE_INTEGER start, end, freq;
  QueryPerformanceCounter(&start);

  const unsigned int kMaxLevel = 0xFFFFFFFF;
  unsigned int curLevel = 0;

  std::deque<IAccessiblePtr> q;
  q.push_front(aAcc);

  while (!q.empty()) {
    IAccessiblePtr acc = q.front();
    q.pop_front();
    QueryAccInfo(aHwnd, acc);
    if (curLevel < kMaxLevel) {
      IAccessiblePtr nextAcc = GetFirstChild(acc);
      while (nextAcc) {
        if (IsVisible(nextAcc)) {
          q.push_front(nextAcc);
        }
        nextAcc = GetNextSibling(nextAcc);
      }
      ++curLevel;
    }
  }

  QueryPerformanceCounter(&end);
  QueryPerformanceFrequency(&freq);

  double startMs = static_cast<double>(start.QuadPart * 1000) / static_cast<double>(freq.QuadPart);
  double endMs = static_cast<double>(end.QuadPart * 1000) / static_cast<double>(freq.QuadPart);
  double diff = endMs - startMs;
  printf("Total execution time: %g ms\n", diff);
}

int main(int argc, char* argv[])
{
  mozilla::STARegion sta;

  auto proxyDll(mozilla::mscom::RegisterProxyDll(L"ia2marshal.dll"));
  if (!proxyDll) {
    printf("NULL proxyDll!\n");
    return 1;
  }

  WCHAR caption[256] = {0};
  WCHAR className[256] = {0};
  HWND hwnd = aspk::SelectWindow();
  GetWindowText(hwnd, caption, ArrayLength(caption));
  GetClassName(hwnd, className, ArrayLength(className));
  printf("HWND: %p \"%S\" \"%S\"\n", hwnd, className, caption);
  if (!hwnd) {
    printf("You suck!\n");
    return 1;
  }

  IAccessiblePtr acc;
  HRESULT hr = AccessibleObjectFromWindow(hwnd, OBJID_CLIENT, IID_IAccessible, (void**)&acc);
  if (FAILED(hr)) {
    printf("You suck!\n");
    return 1;
  }
  DoDfsVisible(hwnd, acc);

#if 0
  // This is the speed test
  int result = FindDocumentAndDump(hwnd);
  if (result) {
    return result;
  }
#endif

#if 0
  // Alright, let's try to get an interface...
  IAccessiblePtr acc;
  HRESULT hr = AccessibleObjectFromWindow(hwnd, OBJID_CLIENT, IID_IAccessible, (void**)&acc);
  if (FAILED(hr)) {
    printf("You suck!\n");
    return 1;
  }
  printf("OBJID_CLIENT IAccessible: 0x%p\n", acc.GetInterfacePtr());

  IAccessiblePtr doc = DoDfsFindRole(acc, ROLE_SYSTEM_DOCUMENT);
  if (!doc) {
    printf("Couldn't find document! You suck!\n");
    return 1;
  }

  printf("Document: 0x%p\n", doc.GetInterfacePtr());
#endif

#if 0
  DumpAccInfo(acc);
#endif

#if 0
  IServiceProviderPtr svcProv;
  hr = acc->QueryInterface(IID_IServiceProvider, (void**)&svcProv);
  if (FAILED(hr)) {
    printf("QI failed IID_IServiceProvider\n");
    return 1;
  }
  printf("IServiceProvider: 0x%p\n", svcProv.GetInterfacePtr());
#endif

#if 0
  IEnumVARIANTPtr enumChildren;
  hr = acc->QueryInterface(IID_IEnumVARIANT, (void**)&enumChildren);
  HRCHECK("QueryInterface IID_IEnumVARIANT");

  hr = enumChildren->Reset();
  HRCHECK("IEnumVARIANT::Reset");

  ULONG count = 1;
  VARIANT vChildren;
  hr = enumChildren->Next(count, &vChildren, &count);
  HRCHECK("IEnumVARIANT::Next");

  if (vChildren.vt != VT_DISPATCH) {
    printf("vChildren: Bad VARIANT type, got 0x%04hx instead!\n", vChildren.vt);
    return 1;
  }

  IAccessiblePtr child;
  hr = vChildren.pdispVal->QueryInterface(IID_IAccessible, (void**)&child);
  if (hr != S_OK) {
    printf("vChildren->QueryInterface(IID_IAccessible)\n");
    return 1;
  }
#endif

#if 0
  IAccessiblePtr child(GetFirstChild(acc));
  if (!child) {
    printf("GetFirstChild(acc)\n");
    return 1;
  }
#endif

#if 0
  IAccessiblePtr root(GetParent(child));
  if (!root) {
    printf("GetParent(child)\n");
    return 1;
  }
  printf("Root IAccessible: 0x%p\n", acc.GetInterfacePtr());

  IServiceProviderPtr svcProv2(GetServiceProvider(root));
  if (!svcProv2) {
    printf("Get svcProv2\n");
    return 1;
  }
  printf("IServiceProvider 2: 0x%p\n", svcProv2.GetInterfacePtr());

  long uid;
  GetUniqueId(acc, uid);
  long uid2;
  GetUniqueId(root, uid2);

  long uid2a;
  hr = GetParentUniqueId(child, uid2a);
  HRCHECK("GetParentUniqueId(child)");
#endif

#if 0
  long rootUniqueId;
  hr = GetUniqueId(acc, rootUniqueId);
  HRCHECK("GetUniqueId(acc)");
  printf("Root accessible's IA2 unique ID is %d\n", rootUniqueId);
#endif

#if 0
  IAccessible2Ptr acc2;
  hr = svcProv->QueryService(IID_IAccessible2, IID_IAccessible2, (void**)&acc2);
  if (FAILED(hr)) {
    printf("QueryService failed IID_IAccessible2! HRESULT: 0x%08X\n", hr);
    return 1;
  }

  long rootUniqueId;
  hr = acc2->get_uniqueID(&rootUniqueId);
  HRCHECK("acc2->get_uniqueID");
  printf("Root accessible's IA2 unique ID is %d\n", rootUniqueId);

  // Let's try to get a document
  long childCount = 0;
  hr = acc->get_accChildCount(&childCount);
  if (FAILED(hr)) {
    printf("get_accChildCount on root failed! HRESULT: 0x%08X\n", hr);
    return 1;
  }

  printf("Root accessible has %d children:\n\n", childCount);
#endif

  // DoDfs(acc);

  // DumpAccInfo(acc);
#if 0
  IAccessiblePtr firstChild = GetFirstChild(acc);
  DumpAccInfo(firstChild);
#endif

#if 0
  VARIANT varStart, varOut;
  VariantInit(&varStart);
  varStart.vt = VT_I4;
  varStart.lVal = CHILDID_SELF;

  hr = acc->accNavigate(NAVDIR_FIRSTCHILD, varStart, &varOut);
  HRCHECK("acc->accNavigate");

  IAccessiblePtr loopAcc;
  hr = varOut.pdispVal->QueryInterface(IID_IAccessible, (void**)&loopAcc);
  HRCHECK("varOut.pdispVal->QI on first child failed");

  long i = 0;
  while (loopAcc) {
    DumpAccInfo(i++, loopAcc);
    hr = loopAcc->accNavigate(NAVDIR_NEXT, varStart, &varOut);
    if (FAILED(hr)) {
      break;
    }
    if (!(varOut.vt & VT_DISPATCH)) {
      printf("accNavigate did not give us an IDispatch*\n");
      return 1;
    }
    IAccessiblePtr qiAcc;
    hr = varOut.pdispVal->QueryInterface(IID_IAccessible, (void**)&qiAcc);
    HRCHECK("varOut.pdispVal->QI failed");
    loopAcc = qiAcc;
  }
#endif
  fflush(stdout);
  return 0;
}

