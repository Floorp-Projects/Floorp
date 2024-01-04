/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <wrl.h>

using namespace Microsoft::WRL;

BOOL WINAPI DllMain(_In_opt_ HINSTANCE aInstance, _In_ DWORD aReason,
                    _In_opt_ LPVOID aReserved) {
  if (DLL_PROCESS_ATTACH == aReason) {
    DisableThreadLibraryCalls(aInstance);
    Module<InProc>::GetModule().Create();
  } else if (DLL_PROCESS_DETACH == aReason) {
    Module<InProc>::GetModule().Terminate();
  }
  return TRUE;
}

HRESULT WINAPI
DllGetActivationFactory(_In_ HSTRING aActivatibleClassId,
                        _COM_Outptr_ IActivationFactory** aFactory) {
  auto& module = Module<InProc>::GetModule();
  return module.GetActivationFactory(aActivatibleClassId, aFactory);
}

HRESULT WINAPI DllCanUnloadNow() {
  auto& module = Module<InProc>::GetModule();
  return (module.Terminate()) ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(_In_ REFCLSID aRclsid, _In_ REFIID aRiid,
                         _COM_Outptr_ LPVOID FAR* aPpv) {
  auto& module = Module<InProc>::GetModule();
  return module.GetClassObject(aRclsid, aRiid, aPpv);
}
