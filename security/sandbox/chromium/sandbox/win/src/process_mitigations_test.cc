// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_mitigations.h"

#include <d3d9.h>
#include <initguid.h>
#include <opmapi.h>
#include <psapi.h>
#include <windows.h>

#include <map>
#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/scoped_native_library.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/startup_information.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/process_mitigations_win32k_policy.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/target_services.h"
#include "sandbox/win/tests/common/controller.h"
#include "sandbox/win/tests/integration_tests/integration_tests_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Timeouts for synchronization.
#define event_timeout \
  static_cast<DWORD>((TestTimeouts::action_timeout()).InMillisecondsRoundedUp())

// API defined in winbase.h.
typedef decltype(GetProcessDEPPolicy)* GetProcessDEPPolicyFunction;

// API defined in processthreadsapi.h.
typedef decltype(
    GetProcessMitigationPolicy)* GetProcessMitigationPolicyFunction;
GetProcessMitigationPolicyFunction get_process_mitigation_policy;

// APIs defined in wingdi.h.
typedef decltype(AddFontMemResourceEx)* AddFontMemResourceExFunction;
typedef decltype(RemoveFontMemResourceEx)* RemoveFontMemResourceExFunction;

// APIs defined in integration_tests_common.h
typedef decltype(WasHookCalled)* WasHookCalledFunction;
typedef decltype(SetHook)* SetHookFunction;

#if !defined(_WIN64)
bool CheckWin8DepPolicy() {
  PROCESS_MITIGATION_DEP_POLICY policy = {};
  if (!get_process_mitigation_policy(::GetCurrentProcess(), ProcessDEPPolicy,
                                     &policy, sizeof(policy))) {
    return false;
  }
  return policy.Enable && policy.Permanent;
}
#endif  // !defined(_WIN64)

bool CheckWin8AslrPolicy() {
  PROCESS_MITIGATION_ASLR_POLICY policy = {};
  if (!get_process_mitigation_policy(::GetCurrentProcess(), ProcessASLRPolicy,
                                     &policy, sizeof(policy))) {
    return false;
  }
  return policy.EnableForceRelocateImages && policy.DisallowStrippedImages;
}

bool CheckWin8StrictHandlePolicy() {
  PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY policy = {};
  if (!get_process_mitigation_policy(::GetCurrentProcess(),
                                     ProcessStrictHandleCheckPolicy, &policy,
                                     sizeof(policy))) {
    return false;
  }
  return policy.RaiseExceptionOnInvalidHandleReference &&
         policy.HandleExceptionsPermanentlyEnabled;
}

bool CheckWin8Win32CallPolicy() {
  PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY policy = {};
  if (!get_process_mitigation_policy(::GetCurrentProcess(),
                                     ProcessSystemCallDisablePolicy, &policy,
                                     sizeof(policy))) {
    return false;
  }
  return policy.DisallowWin32kSystemCalls;
}

bool CheckWin8ExtensionPointPolicy() {
  PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY policy = {};
  if (!get_process_mitigation_policy(::GetCurrentProcess(),
                                     ProcessExtensionPointDisablePolicy,
                                     &policy, sizeof(policy))) {
    return false;
  }
  return policy.DisableExtensionPoints;
}

bool CheckWin10FontPolicy() {
  PROCESS_MITIGATION_FONT_DISABLE_POLICY policy = {};
  if (!get_process_mitigation_policy(::GetCurrentProcess(),
                                     ProcessFontDisablePolicy, &policy,
                                     sizeof(policy))) {
    return false;
  }
  return policy.DisableNonSystemFonts;
}

bool CheckWin10ImageLoadNoRemotePolicy() {
  PROCESS_MITIGATION_IMAGE_LOAD_POLICY policy = {};
  if (!get_process_mitigation_policy(::GetCurrentProcess(),
                                     ProcessImageLoadPolicy, &policy,
                                     sizeof(policy))) {
    return false;
  }
  return policy.NoRemoteImages;
}

// Spawn Windows process (with or without mitigation enabled).
bool SpawnWinProc(PROCESS_INFORMATION* pi, bool success_test, HANDLE* event) {
  base::win::StartupInformation startup_info;
  DWORD creation_flags = 0;

  if (!success_test) {
    DWORD64 flags =
        PROCESS_CREATION_MITIGATION_POLICY_EXTENSION_POINT_DISABLE_ALWAYS_ON;
    // This test only runs on >= Win8, so don't have to handle
    // illegal 64-bit flags on 32-bit <= Win7.
    size_t flags_size = sizeof(flags);

    if (!startup_info.InitializeProcThreadAttributeList(1) ||
        !startup_info.UpdateProcThreadAttribute(
            PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY, &flags, flags_size)) {
      ADD_FAILURE();
      return false;
    }
    creation_flags = EXTENDED_STARTUPINFO_PRESENT;
  }

  // Command line must be writable.
  base::string16 cmd_writeable(g_winproc_file);

  if (!::CreateProcessW(NULL, &cmd_writeable[0], NULL, NULL, FALSE,
                        creation_flags, NULL, NULL, startup_info.startup_info(),
                        pi)) {
    ADD_FAILURE();
    return false;
  }
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(*event, event_timeout));

  return true;
}

//------------------------------------------------------------------------------
// 1. Spawn a Windows process (with or without mitigation enabled).
// 2. Load the hook Dll locally.
// 3. Create a global named event for the hook to trigger.
// 4. Start the hook (for the specific WinProc or globally).
// 5. Send a keystroke event.
// 6. Ask the hook Dll if it received a hook callback.
// 7. Cleanup the hooking.
// 8. Signal the Windows process to shutdown.
//
// Do NOT use any ASSERTs in this function.  Cleanup required.
//------------------------------------------------------------------------------
void TestWin8ExtensionPointHookWrapper(bool is_success_test, bool global_hook) {
  // Set up a couple global events that this test will use.
  HANDLE winproc_event = ::CreateEventW(NULL, FALSE, FALSE, g_winproc_event);
  if (winproc_event == NULL || winproc_event == INVALID_HANDLE_VALUE) {
    ADD_FAILURE();
    return;
  }
  base::win::ScopedHandle scoped_winproc_event(winproc_event);

  HANDLE hook_event = ::CreateEventW(NULL, FALSE, FALSE, g_hook_event);
  if (hook_event == NULL || hook_event == INVALID_HANDLE_VALUE) {
    ADD_FAILURE();
    return;
  }
  base::win::ScopedHandle scoped_hook_event(hook_event);

  // 1. Spawn WinProc.
  PROCESS_INFORMATION proc_info = {};
  if (!SpawnWinProc(&proc_info, is_success_test, &winproc_event))
    return;

  // From this point on, no return on failure.  Cleanup required.
  bool all_good = true;

  // 2. Load the hook DLL.
  base::FilePath hook_dll_path(g_hook_dll_file);
  base::ScopedNativeLibrary dll(hook_dll_path);
  EXPECT_TRUE(dll.is_valid());

  HOOKPROC hook_proc =
      reinterpret_cast<HOOKPROC>(dll.GetFunctionPointer(g_hook_handler_func));
  WasHookCalledFunction was_hook_called =
      reinterpret_cast<WasHookCalledFunction>(
          dll.GetFunctionPointer(g_was_hook_called_func));
  SetHookFunction set_hook = reinterpret_cast<SetHookFunction>(
      dll.GetFunctionPointer(g_set_hook_func));
  if (!hook_proc || !was_hook_called || !set_hook) {
    ADD_FAILURE();
    all_good = false;
  }

  // 3. Try installing the hook (either on a remote target thread,
  //    or globally).
  HHOOK hook = nullptr;
  if (all_good) {
    DWORD target = 0;
    if (!global_hook)
      target = proc_info.dwThreadId;
    hook = ::SetWindowsHookExW(WH_KEYBOARD, hook_proc, dll.get(), target);
    if (!hook) {
      ADD_FAILURE();
      all_good = false;
    } else
      // Pass the hook DLL the hook handle.
      set_hook(hook);
  }

  // 4. Inject a keyboard event.
  if (all_good) {
    // Note: that PostThreadMessage and SendMessage APIs will not deliver
    // a keystroke in such a way that triggers a "legitimate" hook.
    // Have to use targetless SendInput or keybd_event.  The latter is
    // less code and easier to work with.
    keybd_event(VkKeyScan(L'A'), 0, 0, 0);
    keybd_event(VkKeyScan(L'A'), 0, KEYEVENTF_KEYUP, 0);
    // Give it a chance to hit the hook handler...
    ::WaitForSingleObject(hook_event, event_timeout);

    // 5. Did the hook get hit?  Was it expected to?
    if (global_hook)
      EXPECT_EQ((is_success_test ? true : false), was_hook_called());
    else
      // ***IMPORTANT: when targeting a specific thread id, the
      // PROCESS_CREATION_MITIGATION_POLICY_EXTENSION_POINT_DISABLE
      // mitigation does NOT disable the hook API.  It ONLY
      // stops global hooks from running in a process.  Hence,
      // the hook will hit (TRUE) even in the "failure"
      // case for a non-global/targeted hook.
      EXPECT_EQ((is_success_test ? true : true), was_hook_called());
  }

  // 6. Disable hook.
  if (hook)
    EXPECT_TRUE(::UnhookWindowsHookEx(hook));

  // 7. Trigger shutdown of WinProc.
  if (proc_info.hProcess) {
    if (::PostThreadMessageW(proc_info.dwThreadId, WM_QUIT, 0, 0)) {
      // Note: The combination/perfect-storm of a Global Hook, in a
      // WinProc that has the EXTENSION_POINT_DISABLE mitigation ON, and the
      // use of the SendInput or keybd_event API to inject a keystroke,
      // results in the target becoming unresponsive.  If any one of these
      // states are changed, the problem does not occur.  This means the WM_QUIT
      // message is not handled and the call to WaitForSingleObject times out.
      // Therefore not checking the return val.
      ::WaitForSingleObject(winproc_event, event_timeout);
    } else {
      // Ensure no strays.
      ::TerminateProcess(proc_info.hProcess, 0);
      ADD_FAILURE();
    }
    EXPECT_TRUE(::CloseHandle(proc_info.hThread));
    EXPECT_TRUE(::CloseHandle(proc_info.hProcess));
  }
}

//------------------------------------------------------------------------------
// 1. Set up the AppInit Dll in registry settings. (Enable)
// 2. Spawn a Windows process (with or without mitigation enabled).
// 3. Check if the AppInit Dll got loaded in the Windows process or not.
// 4. Signal the Windows process to shutdown.
// 5. Restore original reg settings.
//
// Do NOT use any ASSERTs in this function.  Cleanup required.
//------------------------------------------------------------------------------
void TestWin8ExtensionPointAppInitWrapper(bool is_success_test) {
  // 0.5 Get path of current module.  The appropriate build of the
  //     AppInit DLL will be in the same directory (and the
  //     full path is needed for reg).
  wchar_t path[MAX_PATH];
  if (!::GetModuleFileNameW(NULL, path, MAX_PATH)) {
    ADD_FAILURE();
    return;
  }
  // Only want the directory.  Switch file name for the AppInit DLL.
  base::FilePath full_dll_path(path);
  full_dll_path = full_dll_path.DirName();
  full_dll_path = full_dll_path.Append(g_hook_dll_file);
  wchar_t* non_const = const_cast<wchar_t*>(full_dll_path.value().c_str());
  // Now make sure the path is in "short-name" form for registry.
  DWORD length = ::GetShortPathNameW(non_const, NULL, 0);
  std::vector<wchar_t> short_name(length);
  if (!::GetShortPathNameW(non_const, &short_name[0], length)) {
    ADD_FAILURE();
    return;
  }

  // 1. Reg setup.
  const wchar_t* app_init_reg_path =
      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows";
  const wchar_t* dlls_value_name = L"AppInit_DLLs";
  const wchar_t* enabled_value_name = L"LoadAppInit_DLLs";
  const wchar_t* signing_value_name = L"RequireSignedAppInit_DLLs";
  std::wstring orig_dlls;
  std::wstring new_dlls;
  DWORD orig_enabled_value = 0;
  DWORD orig_signing_value = 0;
  base::win::RegKey app_init_key(HKEY_LOCAL_MACHINE, app_init_reg_path,
                                 KEY_QUERY_VALUE | KEY_SET_VALUE);
  // Backup the existing settings.
  if (!app_init_key.Valid() || !app_init_key.HasValue(dlls_value_name) ||
      !app_init_key.HasValue(enabled_value_name) ||
      ERROR_SUCCESS != app_init_key.ReadValue(dlls_value_name, &orig_dlls) ||
      ERROR_SUCCESS !=
          app_init_key.ReadValueDW(enabled_value_name, &orig_enabled_value)) {
    ADD_FAILURE();
    return;
  }
  if (app_init_key.HasValue(signing_value_name)) {
    if (ERROR_SUCCESS !=
        app_init_key.ReadValueDW(signing_value_name, &orig_signing_value)) {
      ADD_FAILURE();
      return;
    }
  }

  // Set the new settings (obviously requires local admin privileges).
  new_dlls = orig_dlls;
  if (!orig_dlls.empty())
    new_dlls.append(L",");
  new_dlls.append(short_name.data());

  // From this point on, no return on failure.  Cleanup required.
  bool all_good = true;

  if (app_init_key.HasValue(signing_value_name)) {
    if (ERROR_SUCCESS !=
        app_init_key.WriteValue(signing_value_name, static_cast<DWORD>(0))) {
      ADD_FAILURE();
      all_good = false;
    }
  }
  if (ERROR_SUCCESS !=
          app_init_key.WriteValue(dlls_value_name, new_dlls.c_str()) ||
      ERROR_SUCCESS !=
          app_init_key.WriteValue(enabled_value_name, static_cast<DWORD>(1))) {
    ADD_FAILURE();
    all_good = false;
  }

  // 2. Spawn WinProc.
  HANDLE winproc_event = INVALID_HANDLE_VALUE;
  base::win::ScopedHandle scoped_event;
  PROCESS_INFORMATION proc_info = {};
  if (all_good) {
    winproc_event = ::CreateEventW(NULL, FALSE, FALSE, g_winproc_event);
    if (winproc_event == NULL || winproc_event == INVALID_HANDLE_VALUE) {
      ADD_FAILURE();
      all_good = false;
    } else {
      scoped_event.Set(winproc_event);
      if (!SpawnWinProc(&proc_info, is_success_test, &winproc_event))
        all_good = false;
    }
  }

  // 3. Check loaded modules in WinProc to see if the AppInit dll is loaded.
  bool dll_loaded = false;
  if (all_good) {
    std::vector<HMODULE>(modules);
    if (!base::win::GetLoadedModulesSnapshot(proc_info.hProcess, &modules)) {
      ADD_FAILURE();
      all_good = false;
    } else {
      for (HMODULE module : modules) {
        wchar_t name[MAX_PATH] = {};
        if (::GetModuleFileNameExW(proc_info.hProcess, module, name,
                                   MAX_PATH) &&
            ::wcsstr(name, g_hook_dll_file)) {
          // Found it.
          dll_loaded = true;
          break;
        }
      }
    }
  }

  // Was the test result as expected?
  if (all_good)
    EXPECT_EQ((is_success_test ? true : false), dll_loaded);

  // 4. Trigger shutdown of WinProc.
  if (proc_info.hProcess) {
    if (::PostThreadMessageW(proc_info.dwThreadId, WM_QUIT, 0, 0)) {
      ::WaitForSingleObject(winproc_event, event_timeout);
    } else {
      // Ensure no strays.
      ::TerminateProcess(proc_info.hProcess, 0);
      ADD_FAILURE();
    }
    EXPECT_TRUE(::CloseHandle(proc_info.hThread));
    EXPECT_TRUE(::CloseHandle(proc_info.hProcess));
  }

  // 5. Reg Restore
  EXPECT_EQ(ERROR_SUCCESS,
            app_init_key.WriteValue(enabled_value_name, orig_enabled_value));
  if (app_init_key.HasValue(signing_value_name))
    EXPECT_EQ(ERROR_SUCCESS,
              app_init_key.WriteValue(signing_value_name, orig_signing_value));
  EXPECT_EQ(ERROR_SUCCESS,
            app_init_key.WriteValue(dlls_value_name, orig_dlls.c_str()));
}

void TestWin10ImageLoadRemote(bool is_success_test) {
  // ***Insert a manual testing share UNC path here!
  // E.g.: \\\\hostname\\sharename\\calc.exe
  std::wstring unc = L"\"\\\\hostname\\sharename\\calc.exe\"";

  sandbox::TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  // Set a policy that would normally allow for process creation.
  policy->SetJobLevel(sandbox::JOB_NONE, 0);
  policy->SetTokenLevel(sandbox::USER_UNPROTECTED, sandbox::USER_UNPROTECTED);
  runner.SetDisableCsrss(false);

  if (!is_success_test) {
    // Enable the NoRemote mitigation.
    EXPECT_EQ(policy->SetDelayedProcessMitigations(
                  sandbox::MITIGATION_IMAGE_LOAD_NO_REMOTE),
              sandbox::SBOX_ALL_OK);
  }

  std::wstring test = L"TestChildProcess ";
  test += unc.c_str();
  EXPECT_EQ((is_success_test ? sandbox::SBOX_TEST_SUCCEEDED
                             : sandbox::SBOX_TEST_FAILED),
            runner.RunTest(test.c_str()));
}

bool CheckWin10ImageLoadNoLowLabelPolicy() {
  PROCESS_MITIGATION_IMAGE_LOAD_POLICY policy = {};
  if (!get_process_mitigation_policy(::GetCurrentProcess(),
                                     ProcessImageLoadPolicy, &policy,
                                     sizeof(policy))) {
    return false;
  }
  return policy.NoLowMandatoryLabelImages;
}

void TestWin10ImageLoadLowLabel(bool is_success_test) {
  // Setup a mandatory low executable for this test (calc.exe).
  // If anything fails during setup, ASSERT to end test.
  base::FilePath orig_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &orig_path));
  orig_path = orig_path.Append(L"calc.exe");

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath new_path = temp_dir.GetPath();
  new_path = new_path.Append(L"lowIL_calc.exe");

  // Test file will be cleaned up by the ScopedTempDir.
  ASSERT_TRUE(base::CopyFileW(orig_path, new_path));

  std::wstring cmd_line = L"icacls \"";
  cmd_line += new_path.value().c_str();
  cmd_line += L"\" /setintegritylevel Low";

  base::LaunchOptions options = base::LaunchOptionsForTest();
  base::Process setup_proc = base::LaunchProcess(cmd_line.c_str(), options);
  ASSERT_TRUE(setup_proc.IsValid());

  int exit_code = 1;
  if (!setup_proc.WaitForExitWithTimeout(base::TimeDelta::FromSeconds(10),
                                         &exit_code)) {
    // Might have timed out, or might have failed.
    // Terminate to make sure we clean up any mess.
    setup_proc.Terminate(0, false);
    ASSERT_TRUE(false);
  }
  // Make sure icacls was successful.
  ASSERT_EQ(0, exit_code);

  sandbox::TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  // Set a policy that would normally allow for process creation.
  policy->SetJobLevel(sandbox::JOB_NONE, 0);
  policy->SetTokenLevel(sandbox::USER_UNPROTECTED, sandbox::USER_UNPROTECTED);
  runner.SetDisableCsrss(false);

  if (!is_success_test) {
    // Enable the NoLowLabel mitigation.
    EXPECT_EQ(policy->SetDelayedProcessMitigations(
                  sandbox::MITIGATION_IMAGE_LOAD_NO_LOW_LABEL),
              sandbox::SBOX_ALL_OK);
  }

  std::wstring test = L"TestChildProcess ";
  test += new_path.value().c_str();

  EXPECT_EQ((is_success_test ? sandbox::SBOX_TEST_SUCCEEDED
                             : sandbox::SBOX_TEST_FAILED),
            runner.RunTest(test.c_str()));
}

BOOL CALLBACK MonitorEnumCallback(HMONITOR monitor,
                                  HDC hdc_monitor,
                                  LPRECT rect_monitor,
                                  LPARAM data) {
  std::map<HMONITOR, base::string16>& monitors =
      *reinterpret_cast<std::map<HMONITOR, base::string16>*>(data);
  MONITORINFOEXW monitor_info = {};
  monitor_info.cbSize = sizeof(monitor_info);

  if (!::GetMonitorInfoW(monitor,
                         reinterpret_cast<MONITORINFO*>(&monitor_info)))
    return FALSE;
  monitors[monitor] = monitor_info.szDevice;
  return TRUE;
}

std::map<HMONITOR, std::wstring> EnumerateMonitors() {
  std::map<HMONITOR, std::wstring> result;
  ::EnumDisplayMonitors(nullptr, nullptr, MonitorEnumCallback,
                        reinterpret_cast<LPARAM>(&result));
  return result;
}

#define HMONITOR_ENTRY(monitor)                 \
  result[reinterpret_cast<HMONITOR>(monitor)] = \
      base::StringPrintf(L"\\\\.\\DISPLAY%X", monitor)

std::map<HMONITOR, std::wstring> GetTestMonitors() {
  std::map<HMONITOR, std::wstring> result;

  HMONITOR_ENTRY(0x11111111);
  HMONITOR_ENTRY(0x22222222);
  HMONITOR_ENTRY(0x44444444);
  HMONITOR_ENTRY(0x88888888);
  return result;
}

std::wstring UnicodeStringToString(PUNICODE_STRING name) {
  return std::wstring(name->Buffer,
                      name->Buffer + (name->Length / sizeof(name->Buffer[0])));
}

// Returns an index 1, 2, 4 or 8 depening on the device. 0 on error.
DWORD GetTestDeviceMonitorIndex(PUNICODE_STRING device_name) {
  std::wstring name = UnicodeStringToString(device_name);
  std::map<HMONITOR, std::wstring> monitors = GetTestMonitors();
  for (const auto& monitor : monitors) {
    if (name == monitor.second)
      return static_cast<DWORD>(reinterpret_cast<uintptr_t>(monitor.first)) &
             0xF;
  }
  return 0;
}

NTSTATUS WINAPI GetSuggestedOPMProtectedOutputArraySizeTest(
    PUNICODE_STRING device_name,
    DWORD* suggested_output_array_size) {
  DWORD monitor = GetTestDeviceMonitorIndex(device_name);
  if (!monitor)
    return STATUS_OBJECT_NAME_NOT_FOUND;
  *suggested_output_array_size = monitor;
  return STATUS_SUCCESS;
}

NTSTATUS WINAPI
CreateOPMProtectedOutputsTest(PUNICODE_STRING device_name,
                              DXGKMDT_OPM_VIDEO_OUTPUT_SEMANTICS vos,
                              DWORD output_array_size,
                              DWORD* num_in_output_array,
                              OPM_PROTECTED_OUTPUT_HANDLE* output_array) {
  DWORD monitor = GetTestDeviceMonitorIndex(device_name);
  if (!monitor)
    return STATUS_OBJECT_NAME_NOT_FOUND;
  if (vos != DXGKMDT_OPM_VOS_OPM_SEMANTICS)
    return STATUS_INVALID_PARAMETER;
  if (output_array_size != monitor)
    return STATUS_INVALID_PARAMETER;
  *num_in_output_array = monitor - 1;
  for (DWORD index = 0; index < monitor - 1; ++index) {
    output_array[index] =
        reinterpret_cast<OPM_PROTECTED_OUTPUT_HANDLE>((monitor << 4) + index);
  }
  return STATUS_SUCCESS;
}

ULONG CalculateCertLength(ULONG monitor) {
  return (monitor * 0x800) + 0xabc;
}

NTSTATUS WINAPI GetCertificateTest(PUNICODE_STRING device_name,
                                   DXGKMDT_CERTIFICATE_TYPE certificate_type,
                                   BYTE* certificate,
                                   ULONG certificate_length) {
  DWORD monitor = GetTestDeviceMonitorIndex(device_name);
  if (!monitor)
    return STATUS_OBJECT_NAME_NOT_FOUND;
  if (certificate_type != DXGKMDT_OPM_CERTIFICATE)
    return STATUS_INVALID_PARAMETER;
  if (certificate_length != CalculateCertLength(monitor))
    return STATUS_INVALID_PARAMETER;
  memset(certificate, 'A' + monitor, certificate_length);
  return STATUS_SUCCESS;
}

NTSTATUS WINAPI
GetCertificateSizeTest(PUNICODE_STRING device_name,
                       DXGKMDT_CERTIFICATE_TYPE certificate_type,
                       ULONG* certificate_length) {
  DWORD monitor = GetTestDeviceMonitorIndex(device_name);
  if (!monitor)
    return STATUS_OBJECT_NAME_NOT_FOUND;
  if (certificate_type != DXGKMDT_OPM_CERTIFICATE)
    return STATUS_INVALID_PARAMETER;
  *certificate_length = CalculateCertLength(monitor);
  return STATUS_SUCCESS;
}

// Check for valid output handle and return the monitor index.
DWORD IsValidProtectedOutput(OPM_PROTECTED_OUTPUT_HANDLE protected_output) {
  uintptr_t handle = reinterpret_cast<uintptr_t>(protected_output);
  uintptr_t monitor = handle >> 4;
  uintptr_t index = handle & 0xF;
  switch (monitor) {
    case 1:
    case 2:
    case 4:
    case 8:
      break;
    default:
      return 0;
  }
  if (index >= (monitor - 1))
    return 0;
  return static_cast<DWORD>(monitor);
}

NTSTATUS WINAPI
GetCertificateByHandleTest(OPM_PROTECTED_OUTPUT_HANDLE protected_output,
                           DXGKMDT_CERTIFICATE_TYPE certificate_type,
                           BYTE* certificate,
                           ULONG certificate_length) {
  DWORD monitor = IsValidProtectedOutput(protected_output);
  if (!monitor)
    return STATUS_INVALID_HANDLE;
  if (certificate_type != DXGKMDT_OPM_CERTIFICATE)
    return STATUS_INVALID_PARAMETER;
  if (certificate_length != CalculateCertLength(monitor))
    return STATUS_INVALID_PARAMETER;
  memset(certificate, 'A' + monitor, certificate_length);
  return STATUS_SUCCESS;
}

NTSTATUS WINAPI
GetCertificateSizeByHandleTest(OPM_PROTECTED_OUTPUT_HANDLE protected_output,
                               DXGKMDT_CERTIFICATE_TYPE certificate_type,
                               ULONG* certificate_length) {
  DWORD monitor = IsValidProtectedOutput(protected_output);
  if (!monitor)
    return STATUS_INVALID_HANDLE;
  if (certificate_type != DXGKMDT_OPM_CERTIFICATE)
    return STATUS_INVALID_PARAMETER;
  *certificate_length = CalculateCertLength(monitor);
  return STATUS_SUCCESS;
}

NTSTATUS WINAPI
DestroyOPMProtectedOutputTest(OPM_PROTECTED_OUTPUT_HANDLE protected_output) {
  if (!IsValidProtectedOutput(protected_output))
    return STATUS_INVALID_HANDLE;
  return STATUS_SUCCESS;
}

NTSTATUS WINAPI ConfigureOPMProtectedOutputTest(
    OPM_PROTECTED_OUTPUT_HANDLE protected_output,
    const DXGKMDT_OPM_CONFIGURE_PARAMETERS* parameters,
    ULONG additional_parameters_size,
    const BYTE* additional_parameters) {
  if (!IsValidProtectedOutput(protected_output))
    return STATUS_INVALID_HANDLE;
  if (additional_parameters && additional_parameters_size)
    return STATUS_INVALID_PARAMETER;
  return STATUS_SUCCESS;
}

NTSTATUS WINAPI GetOPMInformationTest(
    OPM_PROTECTED_OUTPUT_HANDLE protected_output,
    const DXGKMDT_OPM_GET_INFO_PARAMETERS* parameters,
    DXGKMDT_OPM_REQUESTED_INFORMATION* requested_information) {
  DWORD monitor = IsValidProtectedOutput(protected_output);
  if (!monitor)
    return STATUS_INVALID_HANDLE;
  memset(requested_information, '0' + monitor,
         sizeof(DXGKMDT_OPM_REQUESTED_INFORMATION));
  return STATUS_SUCCESS;
}

NTSTATUS WINAPI
GetOPMRandomNumberTest(OPM_PROTECTED_OUTPUT_HANDLE protected_output,
                       DXGKMDT_OPM_RANDOM_NUMBER* random_number) {
  DWORD monitor = IsValidProtectedOutput(protected_output);
  if (!monitor)
    return STATUS_INVALID_HANDLE;
  memset(random_number->abRandomNumber, '!' + monitor,
         sizeof(random_number->abRandomNumber));
  return STATUS_SUCCESS;
}

NTSTATUS WINAPI SetOPMSigningKeyAndSequenceNumbersTest(
    OPM_PROTECTED_OUTPUT_HANDLE protected_output,
    const DXGKMDT_OPM_ENCRYPTED_PARAMETERS* parameters) {
  DWORD monitor = IsValidProtectedOutput(protected_output);
  if (!monitor)
    return STATUS_INVALID_HANDLE;
  DXGKMDT_OPM_ENCRYPTED_PARAMETERS test_params = {};
  memset(test_params.abEncryptedParameters, 'a' + monitor,
         sizeof(test_params.abEncryptedParameters));
  if (memcmp(test_params.abEncryptedParameters,
             parameters->abEncryptedParameters,
             sizeof(test_params.abEncryptedParameters)) != 0)
    return STATUS_INVALID_PARAMETER;
  return STATUS_SUCCESS;
}

BOOL WINAPI EnumDisplayMonitorsTest(HDC hdc,
                                    LPCRECT clip_rect,
                                    MONITORENUMPROC enum_function,
                                    LPARAM data) {
  RECT rc = {};
  for (const auto& monitor : GetTestMonitors()) {
    if (!enum_function(monitor.first, hdc, &rc, data))
      return FALSE;
  }
  return TRUE;
}

BOOL WINAPI GetMonitorInfoWTest(HMONITOR monitor, LPMONITORINFO monitor_info) {
  std::map<HMONITOR, std::wstring> monitors = GetTestMonitors();
  if (monitor_info->cbSize != sizeof(MONITORINFO) &&
      monitor_info->cbSize != sizeof(MONITORINFOEXW))
    return FALSE;
  auto it = monitors.find(monitor);
  if (it == monitors.end())
    return FALSE;
  if (monitor_info->cbSize == sizeof(MONITORINFOEXW)) {
    MONITORINFOEXW* monitor_info_ex =
        reinterpret_cast<MONITORINFOEXW*>(monitor_info);
    size_t copy_size = (it->second.size() + 1) * sizeof(WCHAR);
    if (copy_size > sizeof(monitor_info_ex->szDevice) - sizeof(WCHAR))
      copy_size = sizeof(monitor_info_ex->szDevice) - sizeof(WCHAR);
    memset(monitor_info_ex->szDevice, 0, sizeof(monitor_info_ex->szDevice));
    memcpy(monitor_info_ex->szDevice, it->second.c_str(), copy_size);
  }
  return TRUE;
}

#define RETURN_TEST_FUNC(n)    \
  if (strcmp(name, #n) == 0) { \
    return n##Test;            \
  }

void* FunctionOverrideForTest(const char* name) {
  RETURN_TEST_FUNC(GetSuggestedOPMProtectedOutputArraySize);
  RETURN_TEST_FUNC(CreateOPMProtectedOutputs);
  RETURN_TEST_FUNC(GetCertificate);
  RETURN_TEST_FUNC(GetCertificateSize);
  RETURN_TEST_FUNC(DestroyOPMProtectedOutput);
  RETURN_TEST_FUNC(ConfigureOPMProtectedOutput);
  RETURN_TEST_FUNC(GetOPMInformation);
  RETURN_TEST_FUNC(GetOPMRandomNumber);
  RETURN_TEST_FUNC(SetOPMSigningKeyAndSequenceNumbers);
  RETURN_TEST_FUNC(EnumDisplayMonitors);
  RETURN_TEST_FUNC(GetMonitorInfoW);
  RETURN_TEST_FUNC(GetCertificateByHandle);
  RETURN_TEST_FUNC(GetCertificateSizeByHandle);
  NOTREACHED();
  return nullptr;
}

}  // namespace

namespace sandbox {

// A shared helper test command that will attempt to CreateProcess with a given
// command line. The second optional parameter will cause the child process to
// return that as an exit code on termination.
//
// ***Make sure you've enabled basic process creation in the
// test sandbox settings via:
// sandbox::TargetPolicy::SetJobLevel(),
// sandbox::TargetPolicy::SetTokenLevel(),
// and TestRunner::SetDisableCsrss().
SBOX_TESTS_COMMAND int TestChildProcess(int argc, wchar_t** argv) {
  if (argc < 1)
    return SBOX_TEST_INVALID_PARAMETER;

  int desired_exit_code = 0;

  if (argc == 2) {
    desired_exit_code = wcstoul(argv[1], nullptr, 0);
  }

  std::wstring cmd = argv[0];
  base::LaunchOptions options = base::LaunchOptionsForTest();
  base::Process setup_proc = base::LaunchProcess(cmd.c_str(), options);

  if (setup_proc.IsValid()) {
    setup_proc.Terminate(desired_exit_code, false);
    return SBOX_TEST_SUCCEEDED;
  }
  // Note: GetLastError from CreateProcess returns 5, "ERROR_ACCESS_DENIED".
  return SBOX_TEST_FAILED;
}

//------------------------------------------------------------------------------
// Win8 Checks:
// MITIGATION_DEP(_NO_ATL_THUNK)
// MITIGATION_RELOCATE_IMAGE(_REQUIRED) - ASLR
// MITIGATION_STRICT_HANDLE_CHECKS
// >= Win8
//------------------------------------------------------------------------------

SBOX_TESTS_COMMAND int CheckWin8(int argc, wchar_t** argv) {
  get_process_mitigation_policy =
      reinterpret_cast<GetProcessMitigationPolicyFunction>(::GetProcAddress(
          ::GetModuleHandleW(L"kernel32.dll"), "GetProcessMitigationPolicy"));
  if (!get_process_mitigation_policy)
    return SBOX_TEST_NOT_FOUND;

#if !defined(_WIN64)  // DEP is always enabled on 64-bit.
  if (!CheckWin8DepPolicy())
    return SBOX_TEST_FIRST_ERROR;
#endif

  if (!CheckWin8AslrPolicy())
    return SBOX_TEST_SECOND_ERROR;

  if (!CheckWin8StrictHandlePolicy())
    return SBOX_TEST_THIRD_ERROR;

  return SBOX_TEST_SUCCEEDED;
}

TEST(ProcessMitigationsTest, CheckWin8) {
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  // ASLR cannot be forced on start in debug builds.
  constexpr sandbox::MitigationFlags kDebugDelayedMitigations =
      MITIGATION_RELOCATE_IMAGE | MITIGATION_RELOCATE_IMAGE_REQUIRED;

  sandbox::MitigationFlags mitigations =
      MITIGATION_DEP | MITIGATION_DEP_NO_ATL_THUNK;
#if defined(NDEBUG)
  mitigations |= kDebugDelayedMitigations;
#endif

  EXPECT_EQ(policy->SetProcessMitigations(mitigations), SBOX_ALL_OK);

  mitigations |= MITIGATION_STRICT_HANDLE_CHECKS;

#if !defined(NDEBUG)
  mitigations |= kDebugDelayedMitigations;
#endif

  EXPECT_EQ(policy->SetDelayedProcessMitigations(mitigations), SBOX_ALL_OK);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckWin8"));
}

//------------------------------------------------------------------------------
// DEP (MITIGATION_DEP)
// < Win8 x86
//------------------------------------------------------------------------------

SBOX_TESTS_COMMAND int CheckDep(int argc, wchar_t** argv) {
  GetProcessDEPPolicyFunction get_process_dep_policy =
      reinterpret_cast<GetProcessDEPPolicyFunction>(::GetProcAddress(
          ::GetModuleHandleW(L"kernel32.dll"), "GetProcessDEPPolicy"));
  if (get_process_dep_policy) {
    BOOL is_permanent = FALSE;
    DWORD dep_flags = 0;

    if (!get_process_dep_policy(::GetCurrentProcess(), &dep_flags,
                                &is_permanent)) {
      return SBOX_TEST_FIRST_ERROR;
    }

    if (!(dep_flags & PROCESS_DEP_ENABLE) || !is_permanent)
      return SBOX_TEST_SECOND_ERROR;

  } else {
    NtQueryInformationProcessFunction query_information_process = NULL;
    ResolveNTFunctionPtr("NtQueryInformationProcess",
                         &query_information_process);
    if (!query_information_process)
      return SBOX_TEST_NOT_FOUND;

    ULONG size = 0;
    ULONG dep_flags = 0;
    if (!SUCCEEDED(query_information_process(::GetCurrentProcess(),
                                             ProcessExecuteFlags, &dep_flags,
                                             sizeof(dep_flags), &size))) {
      return SBOX_TEST_THIRD_ERROR;
    }

    static const int MEM_EXECUTE_OPTION_DISABLE = 2;
    static const int MEM_EXECUTE_OPTION_PERMANENT = 8;
    dep_flags &= 0xff;

    if (dep_flags !=
        (MEM_EXECUTE_OPTION_DISABLE | MEM_EXECUTE_OPTION_PERMANENT)) {
      return SBOX_TEST_FOURTH_ERROR;
    }
  }

  return SBOX_TEST_SUCCEEDED;
}

#if !defined(_WIN64)  // DEP is always enabled on 64-bit.
TEST(ProcessMitigationsTest, CheckDep) {
  if (base::win::GetVersion() > base::win::VERSION_WIN7)
    return;

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  EXPECT_EQ(policy->SetProcessMitigations(MITIGATION_DEP |
                                          MITIGATION_DEP_NO_ATL_THUNK |
                                          MITIGATION_SEHOP),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckDep"));
}
#endif

//------------------------------------------------------------------------------
// Win32k Lockdown (MITIGATION_WIN32K_DISABLE)
// >= Win8
//------------------------------------------------------------------------------

SBOX_TESTS_COMMAND int CheckWin8Lockdown(int argc, wchar_t** argv) {
  get_process_mitigation_policy =
      reinterpret_cast<GetProcessMitigationPolicyFunction>(::GetProcAddress(
          ::GetModuleHandleW(L"kernel32.dll"), "GetProcessMitigationPolicy"));
  if (!get_process_mitigation_policy)
    return SBOX_TEST_NOT_FOUND;

  if (!CheckWin8Win32CallPolicy())
    return SBOX_TEST_FIRST_ERROR;
  return SBOX_TEST_SUCCEEDED;
}

SBOX_TESTS_COMMAND int CheckWin8MonitorsRedirection(int argc, wchar_t** argv) {
  std::map<HMONITOR, base::string16> monitors = EnumerateMonitors();
  std::map<HMONITOR, base::string16> monitors_to_test = GetTestMonitors();
  if (monitors.size() != monitors_to_test.size())
    return SBOX_TEST_FIRST_ERROR;

  for (const auto& monitor : monitors) {
    auto result = monitors_to_test.find(monitor.first);
    if (result == monitors_to_test.end())
      return SBOX_TEST_SECOND_ERROR;
    if (result->second != monitor.second)
      return SBOX_TEST_THIRD_ERROR;
  }
  return SBOX_TEST_SUCCEEDED;
}

SBOX_TESTS_COMMAND int CheckWin8MonitorInfo(int argc, wchar_t** argv) {
  std::map<HMONITOR, base::string16> monitors_to_test = GetTestMonitors();
  MONITORINFO monitor_info = {};
  MONITORINFOEXW monitor_info_exw = {};
  MONITORINFOEXA monitor_info_exa = {};
  HMONITOR valid_monitor = monitors_to_test.begin()->first;
  std::wstring valid_device = monitors_to_test.begin()->second;
  monitor_info.cbSize = sizeof(MONITORINFO);
  if (!::GetMonitorInfoW(valid_monitor, &monitor_info))
    return SBOX_TEST_FIRST_ERROR;
  monitor_info.cbSize = sizeof(MONITORINFO);
  if (!::GetMonitorInfoA(valid_monitor, &monitor_info))
    return SBOX_TEST_SECOND_ERROR;
  monitor_info_exw.cbSize = sizeof(MONITORINFOEXW);
  if (!::GetMonitorInfoW(valid_monitor,
                         reinterpret_cast<MONITORINFO*>(&monitor_info_exw)) ||
      valid_device != monitor_info_exw.szDevice) {
    return SBOX_TEST_THIRD_ERROR;
  }
  monitor_info_exa.cbSize = sizeof(MONITORINFOEXA);
  if (!::GetMonitorInfoA(valid_monitor,
                         reinterpret_cast<MONITORINFO*>(&monitor_info_exa)) ||
      valid_device != base::ASCIIToUTF16(monitor_info_exa.szDevice)) {
    return SBOX_TEST_FOURTH_ERROR;
  }

  // Invalid size checks.
  monitor_info.cbSize = 0;
  if (::GetMonitorInfoW(valid_monitor, &monitor_info))
    return SBOX_TEST_FIFTH_ERROR;
  monitor_info.cbSize = 0x10000;
  if (::GetMonitorInfoW(valid_monitor, &monitor_info))
    return SBOX_TEST_SIXTH_ERROR;

  // Check that an invalid handle isn't accepted.
  HMONITOR invalid_monitor = reinterpret_cast<HMONITOR>(-1);
  monitor_info.cbSize = sizeof(MONITORINFO);
  if (::GetMonitorInfoW(invalid_monitor, &monitor_info))
    return SBOX_TEST_SEVENTH_ERROR;

  return SBOX_TEST_SUCCEEDED;
}

bool RunTestsOnVideoOutputConfigure(uintptr_t monitor_index,
                                    IOPMVideoOutput* video_output) {
  OPM_CONFIGURE_PARAMETERS config_params = {};
  OPM_SET_PROTECTION_LEVEL_PARAMETERS* protection_level =
      reinterpret_cast<OPM_SET_PROTECTION_LEVEL_PARAMETERS*>(
          config_params.abParameters);
  protection_level->ulProtectionType = OPM_PROTECTION_TYPE_HDCP;
  protection_level->ulProtectionLevel = OPM_HDCP_ON;
  config_params.guidSetting = OPM_SET_PROTECTION_LEVEL;
  config_params.cbParametersSize = sizeof(OPM_SET_PROTECTION_LEVEL_PARAMETERS);
  HRESULT hr = video_output->Configure(&config_params, 0, nullptr);
  if (FAILED(hr))
    return false;
  protection_level->ulProtectionType = OPM_PROTECTION_TYPE_DPCP;
  hr = video_output->Configure(&config_params, 0, nullptr);
  if (FAILED(hr))
    return false;
  protection_level->ulProtectionLevel = OPM_HDCP_OFF;
  hr = video_output->Configure(&config_params, 0, nullptr);
  if (FAILED(hr))
    return false;
  BYTE dummy_byte = 0;
  hr = video_output->Configure(&config_params, 1, &dummy_byte);
  if (SUCCEEDED(hr))
    return false;
  protection_level->ulProtectionType = 0xFFFFFFFF;
  hr = video_output->Configure(&config_params, 0, nullptr);
  if (SUCCEEDED(hr))
    return false;
  // Invalid protection level test.
  protection_level->ulProtectionType = OPM_PROTECTION_TYPE_HDCP;
  protection_level->ulProtectionLevel = OPM_HDCP_ON + 1;
  hr = video_output->Configure(&config_params, 0, nullptr);
  if (SUCCEEDED(hr))
    return false;
  hr = video_output->Configure(&config_params, 0, nullptr);
  if (SUCCEEDED(hr))
    return false;
  config_params.guidSetting = OPM_SET_HDCP_SRM;
  OPM_SET_HDCP_SRM_PARAMETERS* srm_parameters =
      reinterpret_cast<OPM_SET_HDCP_SRM_PARAMETERS*>(
          config_params.abParameters);
  srm_parameters->ulSRMVersion = 1;
  config_params.cbParametersSize = sizeof(OPM_SET_HDCP_SRM_PARAMETERS);
  hr = video_output->Configure(&config_params, 0, nullptr);
  if (SUCCEEDED(hr))
    return false;
  return true;
}

bool RunTestsOnVideoOutputFinishInitialization(uintptr_t monitor_index,
                                               IOPMVideoOutput* video_output) {
  OPM_ENCRYPTED_INITIALIZATION_PARAMETERS init_params = {};
  memset(init_params.abEncryptedInitializationParameters, 'a' + monitor_index,
         sizeof(init_params.abEncryptedInitializationParameters));
  HRESULT hr = video_output->FinishInitialization(&init_params);
  if (FAILED(hr))
    return false;
  memset(init_params.abEncryptedInitializationParameters, 'Z' + monitor_index,
         sizeof(init_params.abEncryptedInitializationParameters));
  hr = video_output->FinishInitialization(&init_params);
  if (SUCCEEDED(hr))
    return false;
  return true;
}

bool RunTestsOnVideoOutputStartInitialization(uintptr_t monitor_index,
                                              IOPMVideoOutput* video_output) {
  OPM_RANDOM_NUMBER random_number = {};
  BYTE* certificate = nullptr;
  ULONG certificate_length = 0;

  HRESULT hr = video_output->StartInitialization(&random_number, &certificate,
                                                 &certificate_length);
  if (FAILED(hr))
    return false;

  if (certificate_length != CalculateCertLength(monitor_index))
    return false;

  for (ULONG i = 0; i < certificate_length; ++i) {
    if (certificate[i] != 'A' + monitor_index)
      return false;
  }

  for (ULONG i = 0; i < sizeof(random_number.abRandomNumber); ++i) {
    if (random_number.abRandomNumber[i] != '!' + monitor_index)
      return false;
  }

  return true;
}

static bool SendSingleGetInfoRequest(uintptr_t monitor_index,
                                     IOPMVideoOutput* video_output,
                                     const GUID& request,
                                     ULONG data_length,
                                     void* data) {
  OPM_GET_INFO_PARAMETERS params = {};
  OPM_REQUESTED_INFORMATION requested_information = {};
  BYTE* requested_information_ptr =
      reinterpret_cast<BYTE*>(&requested_information);
  params.guidInformation = request;
  params.cbParametersSize = data_length;
  memcpy(params.abParameters, data, data_length);
  HRESULT hr = video_output->GetInformation(&params, &requested_information);
  if (FAILED(hr))
    return false;
  for (size_t i = 0; i < sizeof(OPM_REQUESTED_INFORMATION); ++i) {
    if (requested_information_ptr[i] != '0' + monitor_index)
      return false;
  }
  return true;
}

bool RunTestsOnVideoOutputGetInformation(uintptr_t monitor_index,
                                         IOPMVideoOutput* video_output) {
  ULONG dummy = 0;
  if (!SendSingleGetInfoRequest(monitor_index, video_output,
                                OPM_GET_CONNECTOR_TYPE, 0, nullptr)) {
    return false;
  }
  if (!SendSingleGetInfoRequest(monitor_index, video_output,
                                OPM_GET_SUPPORTED_PROTECTION_TYPES, 0,
                                nullptr)) {
    return false;
  }
  // These should fail due to invalid parameter sizes.
  if (SendSingleGetInfoRequest(monitor_index, video_output,
                               OPM_GET_CONNECTOR_TYPE, sizeof(dummy), &dummy)) {
    return false;
  }
  if (SendSingleGetInfoRequest(monitor_index, video_output,
                               OPM_GET_SUPPORTED_PROTECTION_TYPES,
                               sizeof(dummy), &dummy)) {
    return false;
  }
  ULONG protection_type = OPM_PROTECTION_TYPE_HDCP;
  if (!SendSingleGetInfoRequest(monitor_index, video_output,
                                OPM_GET_ACTUAL_PROTECTION_LEVEL,
                                sizeof(protection_type), &protection_type)) {
    return false;
  }
  protection_type = OPM_PROTECTION_TYPE_DPCP;
  if (!SendSingleGetInfoRequest(monitor_index, video_output,
                                OPM_GET_ACTUAL_PROTECTION_LEVEL,
                                sizeof(protection_type), &protection_type)) {
    return false;
  }
  // These should fail as unsupported or invalid parameters.
  protection_type = OPM_PROTECTION_TYPE_ACP;
  if (SendSingleGetInfoRequest(monitor_index, video_output,
                               OPM_GET_ACTUAL_PROTECTION_LEVEL,
                               sizeof(protection_type), &protection_type)) {
    return false;
  }
  if (SendSingleGetInfoRequest(monitor_index, video_output,
                               OPM_GET_ACTUAL_PROTECTION_LEVEL, 0, nullptr)) {
    return false;
  }
  protection_type = OPM_PROTECTION_TYPE_HDCP;
  if (!SendSingleGetInfoRequest(monitor_index, video_output,
                                OPM_GET_VIRTUAL_PROTECTION_LEVEL,
                                sizeof(protection_type), &protection_type)) {
    return false;
  }
  protection_type = OPM_PROTECTION_TYPE_DPCP;
  if (!SendSingleGetInfoRequest(monitor_index, video_output,
                                OPM_GET_VIRTUAL_PROTECTION_LEVEL,
                                sizeof(protection_type), &protection_type)) {
    return false;
  }
  // These should fail as unsupported or invalid parameters.
  protection_type = OPM_PROTECTION_TYPE_ACP;
  if (SendSingleGetInfoRequest(monitor_index, video_output,
                               OPM_GET_VIRTUAL_PROTECTION_LEVEL,
                               sizeof(protection_type), &protection_type)) {
    return false;
  }
  if (SendSingleGetInfoRequest(monitor_index, video_output,
                               OPM_GET_VIRTUAL_PROTECTION_LEVEL, 0, nullptr)) {
    return false;
  }
  // This should fail with unsupported request.
  if (SendSingleGetInfoRequest(monitor_index, video_output, OPM_GET_CODEC_INFO,
                               0, nullptr)) {
    return false;
  }
  return true;
}

int RunTestsOnVideoOutput(uintptr_t monitor_index,
                          IOPMVideoOutput* video_output) {
  if (!RunTestsOnVideoOutputStartInitialization(monitor_index, video_output))
    return SBOX_TEST_FIRST_ERROR;

  if (!RunTestsOnVideoOutputFinishInitialization(monitor_index, video_output))
    return SBOX_TEST_SECOND_ERROR;

  if (!RunTestsOnVideoOutputConfigure(monitor_index, video_output))
    return SBOX_TEST_THIRD_ERROR;

  if (!RunTestsOnVideoOutputGetInformation(monitor_index, video_output))
    return SBOX_TEST_FOURTH_ERROR;

  return SBOX_TEST_SUCCEEDED;
}

SBOX_TESTS_COMMAND int CheckWin8OPMApis(int argc, wchar_t** argv) {
  std::map<HMONITOR, base::string16> monitors = GetTestMonitors();
  for (const auto& monitor : monitors) {
    ULONG output_count = 0;
    IOPMVideoOutput** outputs = nullptr;
    uintptr_t monitor_index = reinterpret_cast<uintptr_t>(monitor.first) & 0xF;
    HRESULT hr = OPMGetVideoOutputsFromHMONITOR(
        monitor.first, OPM_VOS_OPM_SEMANTICS, &output_count, &outputs);
    if (monitor_index > 4) {
      // These should fail because the certificate is too large.
      if (SUCCEEDED(hr))
        return SBOX_TEST_FIRST_ERROR;
      continue;
    }
    if (FAILED(hr))
      return SBOX_TEST_SECOND_ERROR;
    if (output_count != monitor_index - 1)
      return SBOX_TEST_THIRD_ERROR;
    for (ULONG output_index = 0; output_index < output_count; ++output_index) {
      int result = RunTestsOnVideoOutput(monitor_index, outputs[output_index]);
      outputs[output_index]->Release();
      if (result != SBOX_TEST_SUCCEEDED)
        return result;
    }
    ::CoTaskMemFree(outputs);
  }
  return SBOX_TEST_SUCCEEDED;
}

// This test validates that setting the MITIGATION_WIN32K_DISABLE mitigation on
// the target process causes the launch to fail in process initialization.
// The test process itself links against user32/gdi32.
TEST(ProcessMitigationsTest, CheckWin8Win32KLockDownFailure) {
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  EXPECT_EQ(policy->SetProcessMitigations(MITIGATION_WIN32K_DISABLE),
            SBOX_ALL_OK);
  EXPECT_NE(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckWin8Lockdown"));
}

// This test validates that setting the MITIGATION_WIN32K_DISABLE mitigation
// along with the policy to fake user32 and gdi32 initialization successfully
// launches the target process.
// The test process itself links against user32/gdi32.
TEST(ProcessMitigationsTest, CheckWin8Win32KLockDownSuccess) {
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();
  ProcessMitigationsWin32KLockdownPolicy::SetOverrideForTestCallback(
      FunctionOverrideForTest);

  EXPECT_EQ(policy->SetProcessMitigations(MITIGATION_WIN32K_DISABLE),
            SBOX_ALL_OK);
  EXPECT_EQ(policy->AddRule(sandbox::TargetPolicy::SUBSYS_WIN32K_LOCKDOWN,
                            sandbox::TargetPolicy::FAKE_USER_GDI_INIT, NULL),
            sandbox::SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckWin8Lockdown"));
  EXPECT_NE(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"CheckWin8MonitorsRedirection"));
  EXPECT_NE(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckWin8MonitorInfo"));
  EXPECT_NE(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckWin8OPMApis"));
}

// This test validates the even though we're running under win32k lockdown
// we can use the IPC redirection to enumerate the list of monitors.
TEST(ProcessMitigationsTest, CheckWin8Win32KRedirection) {
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();
  ProcessMitigationsWin32KLockdownPolicy::SetOverrideForTestCallback(
      FunctionOverrideForTest);

  EXPECT_EQ(policy->SetProcessMitigations(MITIGATION_WIN32K_DISABLE),
            SBOX_ALL_OK);
  EXPECT_EQ(policy->AddRule(sandbox::TargetPolicy::SUBSYS_WIN32K_LOCKDOWN,
                            sandbox::TargetPolicy::IMPLEMENT_OPM_APIS, NULL),
            sandbox::SBOX_ALL_OK);
  policy->SetEnableOPMRedirection();
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckWin8Lockdown"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"CheckWin8MonitorsRedirection"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckWin8MonitorInfo"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckWin8OPMApis"));
}

//------------------------------------------------------------------------------
// Disable extension points (MITIGATION_EXTENSION_POINT_DISABLE).
// >= Win8
//------------------------------------------------------------------------------
SBOX_TESTS_COMMAND int CheckWin8ExtensionPointSetting(int argc,
                                                      wchar_t** argv) {
  get_process_mitigation_policy =
      reinterpret_cast<GetProcessMitigationPolicyFunction>(::GetProcAddress(
          ::GetModuleHandleW(L"kernel32.dll"), "GetProcessMitigationPolicy"));
  if (!get_process_mitigation_policy)
    return SBOX_TEST_NOT_FOUND;

  if (!CheckWin8ExtensionPointPolicy())
    return SBOX_TEST_FIRST_ERROR;
  return SBOX_TEST_SUCCEEDED;
}

// This test validates that setting the MITIGATION_EXTENSION_POINT_DISABLE
// mitigation enables the setting on a process.
TEST(ProcessMitigationsTest, CheckWin8ExtensionPointPolicySuccess) {
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  EXPECT_EQ(policy->SetProcessMitigations(MITIGATION_EXTENSION_POINT_DISABLE),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"CheckWin8ExtensionPointSetting"));
}

// This test validates that a "legitimate" global hook CAN be set on the
// sandboxed proc/thread if the MITIGATION_EXTENSION_POINT_DISABLE
// mitigation is not set.
//
// MANUAL testing only.
TEST(ProcessMitigationsTest,
     DISABLED_CheckWin8ExtensionPoint_GlobalHook_Success) {
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;

  HANDLE mutex = ::CreateMutexW(NULL, FALSE, g_extension_point_test_mutex);
  EXPECT_TRUE(mutex != NULL && mutex != INVALID_HANDLE_VALUE);
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(mutex, event_timeout));

  // (is_success_test, global_hook)
  TestWin8ExtensionPointHookWrapper(true, true);

  EXPECT_TRUE(::ReleaseMutex(mutex));
  EXPECT_TRUE(::CloseHandle(mutex));
}

// This test validates that setting the MITIGATION_EXTENSION_POINT_DISABLE
// mitigation prevents a global hook on WinProc.
//
// MANUAL testing only.
TEST(ProcessMitigationsTest,
     DISABLED_CheckWin8ExtensionPoint_GlobalHook_Failure) {
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;

  HANDLE mutex = ::CreateMutexW(NULL, FALSE, g_extension_point_test_mutex);
  EXPECT_TRUE(mutex != NULL && mutex != INVALID_HANDLE_VALUE);
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(mutex, event_timeout));

  // (is_success_test, global_hook)
  TestWin8ExtensionPointHookWrapper(false, true);

  EXPECT_TRUE(::ReleaseMutex(mutex));
  EXPECT_TRUE(::CloseHandle(mutex));
}

// This test validates that a "legitimate" hook CAN be set on the sandboxed
// proc/thread if the MITIGATION_EXTENSION_POINT_DISABLE mitigation is not set.
//
// MANUAL testing only.
TEST(ProcessMitigationsTest, DISABLED_CheckWin8ExtensionPoint_Hook_Success) {
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;

  HANDLE mutex = ::CreateMutexW(NULL, FALSE, g_extension_point_test_mutex);
  EXPECT_TRUE(mutex != NULL && mutex != INVALID_HANDLE_VALUE);
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(mutex, event_timeout));

  // (is_success_test, global_hook)
  TestWin8ExtensionPointHookWrapper(true, false);

  EXPECT_TRUE(::ReleaseMutex(mutex));
  EXPECT_TRUE(::CloseHandle(mutex));
}

// *** Important: MITIGATION_EXTENSION_POINT_DISABLE does NOT prevent
// hooks targetted at a specific thread id.  It only prevents
// global hooks.  So this test does NOT actually expect the hook
// to fail (see TestWin8ExtensionPointHookWrapper function) even
// with the mitigation on.
//
// MANUAL testing only.
TEST(ProcessMitigationsTest, DISABLED_CheckWin8ExtensionPoint_Hook_Failure) {
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;

  HANDLE mutex = ::CreateMutexW(NULL, FALSE, g_extension_point_test_mutex);
  EXPECT_TRUE(mutex != NULL && mutex != INVALID_HANDLE_VALUE);
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(mutex, event_timeout));

  // (is_success_test, global_hook)
  TestWin8ExtensionPointHookWrapper(false, false);

  EXPECT_TRUE(::ReleaseMutex(mutex));
  EXPECT_TRUE(::CloseHandle(mutex));
}

// This test validates that an AppInit Dll CAN be added to a target
// WinProc if the MITIGATION_EXTENSION_POINT_DISABLE mitigation is not set.
//
// MANUAL testing only.
// Must run this test as admin/elevated.
TEST(ProcessMitigationsTest, DISABLED_CheckWin8ExtensionPoint_AppInit_Success) {
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;

  HANDLE mutex = ::CreateMutexW(NULL, FALSE, g_extension_point_test_mutex);
  EXPECT_TRUE(mutex != NULL && mutex != INVALID_HANDLE_VALUE);
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(mutex, event_timeout));

  TestWin8ExtensionPointAppInitWrapper(true);

  EXPECT_TRUE(::ReleaseMutex(mutex));
  EXPECT_TRUE(::CloseHandle(mutex));
}

// This test validates that setting the MITIGATION_EXTENSION_POINT_DISABLE
// mitigation prevents the loading of any AppInit Dll into WinProc.
//
// MANUAL testing only.
// Must run this test as admin/elevated.
TEST(ProcessMitigationsTest, DISABLED_CheckWin8ExtensionPoint_AppInit_Failure) {
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;

  HANDLE mutex = ::CreateMutexW(NULL, FALSE, g_extension_point_test_mutex);
  EXPECT_TRUE(mutex != NULL && mutex != INVALID_HANDLE_VALUE);
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(mutex, event_timeout));

  TestWin8ExtensionPointAppInitWrapper(false);

  EXPECT_TRUE(::ReleaseMutex(mutex));
  EXPECT_TRUE(::CloseHandle(mutex));
}

//------------------------------------------------------------------------------
// Disable non-system font loads (MITIGATION_NONSYSTEM_FONT_DISABLE)
// >= Win10
//------------------------------------------------------------------------------

SBOX_TESTS_COMMAND int CheckWin10FontLockDown(int argc, wchar_t** argv) {
  get_process_mitigation_policy =
      reinterpret_cast<GetProcessMitigationPolicyFunction>(::GetProcAddress(
          ::GetModuleHandleW(L"kernel32.dll"), "GetProcessMitigationPolicy"));
  if (!get_process_mitigation_policy)
    return SBOX_TEST_NOT_FOUND;

  if (!CheckWin10FontPolicy())
    return SBOX_TEST_FIRST_ERROR;
  return SBOX_TEST_SUCCEEDED;
}

SBOX_TESTS_COMMAND int CheckWin10FontLoad(int argc, wchar_t** argv) {
  if (argc < 1)
    return SBOX_TEST_INVALID_PARAMETER;

  HMODULE gdi_module = ::LoadLibraryW(L"gdi32.dll");
  if (!gdi_module)
    return SBOX_TEST_NOT_FOUND;

  AddFontMemResourceExFunction add_font_mem_resource =
      reinterpret_cast<AddFontMemResourceExFunction>(
          ::GetProcAddress(gdi_module, "AddFontMemResourceEx"));

  RemoveFontMemResourceExFunction rem_font_mem_resource =
      reinterpret_cast<RemoveFontMemResourceExFunction>(
          ::GetProcAddress(gdi_module, "RemoveFontMemResourceEx"));

  if (!add_font_mem_resource || !rem_font_mem_resource)
    return SBOX_TEST_NOT_FOUND;

  // Open font file passed in as an argument.
  base::File file(base::FilePath(argv[0]),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    // Failed to open the font file passed in.
    return SBOX_TEST_NOT_FOUND;

  std::vector<char> font_data;
  int64_t len = file.GetLength();
  font_data.resize(len);

  int read = file.Read(0, &font_data[0], len);
  file.Close();

  if (read != len)
    return SBOX_TEST_NOT_FOUND;

  DWORD font_count = 0;
  HANDLE font_handle = add_font_mem_resource(
      &font_data[0], static_cast<DWORD>(font_data.size()), NULL, &font_count);

  if (font_handle) {
    rem_font_mem_resource(font_handle);
    return SBOX_TEST_SUCCEEDED;
  }

  return SBOX_TEST_FAILED;
}

// This test validates that setting the MITIGATION_NON_SYSTEM_FONTS_DISABLE
// mitigation enables the setting on a process.
TEST(ProcessMitigationsTest, CheckWin10NonSystemFontLockDownPolicySuccess) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10)
    return;

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  EXPECT_EQ(policy->SetProcessMitigations(MITIGATION_NONSYSTEM_FONT_DISABLE),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckWin10FontLockDown"));
}

// This test validates that we can load a non-system font
// if the MITIGATION_NON_SYSTEM_FONTS_DISABLE
// mitigation is NOT set.
TEST(ProcessMitigationsTest, CheckWin10NonSystemFontLockDownLoadSuccess) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10)
    return;

  base::FilePath font_path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_WINDOWS_FONTS, &font_path));
  // Arial font should always be available
  font_path = font_path.Append(L"arial.ttf");

  TestRunner runner;
  EXPECT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_READONLY,
                               font_path.value().c_str()));

  std::wstring test_command = L"CheckWin10FontLoad \"";
  test_command += font_path.value().c_str();
  test_command += L"\"";
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));
}

// This test validates that setting the MITIGATION_NON_SYSTEM_FONTS_DISABLE
// mitigation prevents the loading of a non-system font.
TEST(ProcessMitigationsTest, CheckWin10NonSystemFontLockDownLoadFailure) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10)
    return;

  base::FilePath font_path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_WINDOWS_FONTS, &font_path));
  // Arial font should always be available
  font_path = font_path.Append(L"arial.ttf");

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();
  EXPECT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_READONLY,
                               font_path.value().c_str()));

  // Turn on the non-system font disable mitigation.
  EXPECT_EQ(policy->SetProcessMitigations(MITIGATION_NONSYSTEM_FONT_DISABLE),
            SBOX_ALL_OK);

  std::wstring test_command = L"CheckWin10FontLoad \"";
  test_command += font_path.value().c_str();
  test_command += L"\"";

  EXPECT_EQ(SBOX_TEST_FAILED, runner.RunTest(test_command.c_str()));
}

//------------------------------------------------------------------------------
// Disable image load from remote devices (MITIGATION_IMAGE_LOAD_NO_REMOTE).
// >= Win10_TH2
//------------------------------------------------------------------------------

SBOX_TESTS_COMMAND int CheckWin10ImageLoadNoRemote(int argc, wchar_t** argv) {
  get_process_mitigation_policy =
      reinterpret_cast<GetProcessMitigationPolicyFunction>(::GetProcAddress(
          ::GetModuleHandleW(L"kernel32.dll"), "GetProcessMitigationPolicy"));
  if (!get_process_mitigation_policy)
    return SBOX_TEST_NOT_FOUND;

  if (!CheckWin10ImageLoadNoRemotePolicy())
    return SBOX_TEST_FIRST_ERROR;
  return SBOX_TEST_SUCCEEDED;
}

// This test validates that setting the MITIGATION_IMAGE_LOAD_NO_REMOTE
// mitigation enables the setting on a process.
TEST(ProcessMitigationsTest, CheckWin10ImageLoadNoRemotePolicySuccess) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10_TH2)
    return;

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  EXPECT_EQ(
      policy->SetDelayedProcessMitigations(MITIGATION_IMAGE_LOAD_NO_REMOTE),
      SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"CheckWin10ImageLoadNoRemote"));
}

// This test validates that we CAN create a new process from
// a remote UNC device, if the MITIGATION_IMAGE_LOAD_NO_REMOTE
// mitigation is NOT set.
//
// MANUAL testing only.
TEST(ProcessMitigationsTest, DISABLED_CheckWin10ImageLoadNoRemoteSuccess) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10_TH2)
    return;

  TestWin10ImageLoadRemote(true);
}

// This test validates that setting the MITIGATION_IMAGE_LOAD_NO_REMOTE
// mitigation prevents creating a new process from a remote
// UNC device.
//
// MANUAL testing only.
TEST(ProcessMitigationsTest, DISABLED_CheckWin10ImageLoadNoRemoteFailure) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10_TH2)
    return;

  TestWin10ImageLoadRemote(false);
}

//------------------------------------------------------------------------------
// Disable image load when "mandatory low label" (integrity level).
// (MITIGATION_IMAGE_LOAD_NO_LOW_LABEL)
// >= Win10_TH2
//------------------------------------------------------------------------------

SBOX_TESTS_COMMAND int CheckWin10ImageLoadNoLowLabel(int argc, wchar_t** argv) {
  get_process_mitigation_policy =
      reinterpret_cast<GetProcessMitigationPolicyFunction>(::GetProcAddress(
          ::GetModuleHandleW(L"kernel32.dll"), "GetProcessMitigationPolicy"));
  if (!get_process_mitigation_policy)
    return SBOX_TEST_NOT_FOUND;

  if (!CheckWin10ImageLoadNoLowLabelPolicy())
    return SBOX_TEST_FIRST_ERROR;
  return SBOX_TEST_SUCCEEDED;
}

// This test validates that setting the MITIGATION_IMAGE_LOAD_NO_LOW_LABEL
// mitigation enables the setting on a process.
TEST(ProcessMitigationsTest, CheckWin10ImageLoadNoLowLabelPolicySuccess) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10_TH2)
    return;

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  EXPECT_EQ(
      policy->SetDelayedProcessMitigations(MITIGATION_IMAGE_LOAD_NO_LOW_LABEL),
      SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"CheckWin10ImageLoadNoLowLabel"));
}

// This test validates that we CAN create a new process with
// low mandatory label (IL), if the MITIGATION_IMAGE_LOAD_NO_LOW_LABEL
// mitigation is NOT set.
TEST(ProcessMitigationsTest, CheckWin10ImageLoadNoLowLabelSuccess) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10_TH2)
    return;

  TestWin10ImageLoadLowLabel(true);
}

// This test validates that setting the MITIGATION_IMAGE_LOAD_NO_LOW_LABEL
// mitigation prevents creating a new process with low mandatory label (IL).
TEST(ProcessMitigationsTest, CheckWin10ImageLoadNoLowLabelFailure) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10_TH2)
    return;

  TestWin10ImageLoadLowLabel(false);
}

//------------------------------------------------------------------------------
// Disable child process creation.
// - JobLevel <= JOB_LIMITED_USER (on < WIN10_TH2).
// - JobLevel <= JOB_LIMITED_USER which also triggers setting
//   PROC_THREAD_ATTRIBUTE_CHILD_PROCESS_POLICY to
//   PROCESS_CREATION_CHILD_PROCESS_RESTRICTED in
//   BrokerServicesBase::SpawnTarget (on >= WIN10_TH2).
//------------------------------------------------------------------------------

// This test validates that we can spawn a child process if
// MITIGATION_CHILD_PROCESS_CREATION_RESTRICTED mitigation is
// not set.
TEST(ProcessMitigationsTest, CheckChildProcessSuccess) {
  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  // Set a policy that would normally allow for process creation.
  policy->SetJobLevel(JOB_INTERACTIVE, 0);
  policy->SetTokenLevel(USER_UNPROTECTED, USER_UNPROTECTED);
  runner.SetDisableCsrss(false);

  base::FilePath cmd;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &cmd));
  cmd = cmd.Append(L"calc.exe");

  std::wstring test_command = L"TestChildProcess ";
  test_command += cmd.value().c_str();

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));
}

// This test validates that setting the
// MITIGATION_CHILD_PROCESS_CREATION_RESTRICTED mitigation prevents
// the spawning of child processes.
TEST(ProcessMitigationsTest, CheckChildProcessFailure) {
  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  // Now set the job level to be <= JOB_LIMITED_USER
  // and ensure we can no longer create a child process.
  policy->SetJobLevel(JOB_LIMITED_USER, 0);
  policy->SetTokenLevel(USER_UNPROTECTED, USER_UNPROTECTED);
  runner.SetDisableCsrss(false);

  base::FilePath cmd;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &cmd));
  cmd = cmd.Append(L"calc.exe");

  std::wstring test_command = L"TestChildProcess ";
  test_command += cmd.value().c_str();

  EXPECT_EQ(SBOX_TEST_FAILED, runner.RunTest(test_command.c_str()));
}

// This test validates that when the sandboxed target within a job spawns a
// child process and the target process exits abnormally, the broker correctly
// handles the JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS message.
// Because this involves spawning a child process from the target process and is
// very similar to the above CheckChildProcess* tests, this test is here rather
// than elsewhere closer to the other Job tests.
TEST(ProcessMitigationsTest, CheckChildProcessAbnormalExit) {
  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  // Set a policy that would normally allow for process creation.
  policy->SetJobLevel(JOB_INTERACTIVE, 0);
  policy->SetTokenLevel(USER_UNPROTECTED, USER_UNPROTECTED);
  runner.SetDisableCsrss(false);

  base::FilePath cmd;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &cmd));
  cmd = cmd.Append(L"calc.exe");

  std::wstring test_command(base::StringPrintf(L"TestChildProcess %ls 0x%08X",
                                               cmd.value().c_str(),
                                               STATUS_ACCESS_VIOLATION));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));
}

}  // namespace sandbox
