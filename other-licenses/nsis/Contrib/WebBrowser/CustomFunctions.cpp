// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0.If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "WebBrowser.h"
#include "exdll.h"

extern WebBrowser* gBrowser;
void Init(HWND hWndParent, int string_size, TCHAR* variables,
          stack_t** stacktop, extra_parameters* extra);

static void CustomFunctionWrapper(void* NSISFunctionAddr, VARIANT jsArg,
                                  VARIANT* retVal) {
  // Marshal the argument passed to the JavaScript function onto the NSIS stack.
  switch (jsArg.vt) {
    case VT_BSTR:
      pushstring(jsArg.bstrVal);
      break;
    case VT_I4: {
      TCHAR intArgStr[32] = _T("");
      _itot_s(jsArg.intVal, intArgStr, 10);
      pushstring(intArgStr);
      break;
    }
    case VT_BOOL:
      pushstring(jsArg.boolVal == VARIANT_TRUE ? _T("1") : _T("0"));
      break;
    default:
      // No other argument types supported.
      pushstring(_T(""));
      break;
  }

  // Call the NSIS function.
  int rv = g_executeCodeSegment((int)NSISFunctionAddr, nullptr);

  // Retrieve the return value from the NSIS stack.
  TCHAR* nsisRetval =
      (TCHAR*)HeapAlloc(GetProcessHeap(), 0, g_stringsize * sizeof(TCHAR));
  popstring(nsisRetval);

  // Pass the return value back to JavaScript, if it asked for one.
  if (retVal) {
    VariantInit(retVal);
    retVal->vt = VT_BSTR;
    retVal->bstrVal = SysAllocString(nsisRetval);
  }

  HeapFree(GetProcessHeap(), 0, nsisRetval);
}

PLUGINFUNCTION(RegisterCustomFunction) {
  if (!gBrowser) {
    Init(hWndParent, string_size, variables, stacktop, extra);
  }

  TCHAR* funcAddrStr =
      (TCHAR*)HeapAlloc(GetProcessHeap(), 0, g_stringsize * sizeof(TCHAR));
  popstring(funcAddrStr);

  TCHAR* funcName =
      (TCHAR*)HeapAlloc(GetProcessHeap(), 0, g_stringsize * sizeof(TCHAR));
  popstring(funcName);

  if (gBrowser && funcAddrStr && funcName) {
    // Apparently GetFunctionAddress returnes a 1-indexed offset, but
    // ExecuteCodeSegment expects a 0-indexed one. Or something.
    uintptr_t funcAddr = static_cast<uintptr_t>(_ttoi64(funcAddrStr)) - 1;
    gBrowser->AddCustomFunction(funcName, CustomFunctionWrapper,
                                (void*)funcAddr);
  }

  HeapFree(GetProcessHeap(), 0, funcName);
  HeapFree(GetProcessHeap(), 0, funcAddrStr);
}
