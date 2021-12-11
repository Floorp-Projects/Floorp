// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_PROCESS_MITIGATIONS_WIN32K_INTERCEPTION_H_
#define SANDBOX_SRC_PROCESS_MITIGATIONS_WIN32K_INTERCEPTION_H_

#include <windows.h>

#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_types.h"

namespace sandbox {

const DWORD kProtectedVideoOutputSectionSize = 16 * 1024;
const DWORD kMaxEnumMonitors = 32;

struct EnumMonitorsResult {
  DWORD monitor_count;
  HMONITOR monitors[kMaxEnumMonitors];
};

typedef BOOL(WINAPI* GdiDllInitializeFunction)(HANDLE dll,
                                               DWORD reason,
                                               LPVOID reserved);

using GetStockObjectFunction = decltype(&::GetStockObject);

using RegisterClassWFunction = decltype(&::RegisterClassW);

using EnumDisplayMonitorsFunction = decltype(&::EnumDisplayMonitors);

using EnumDisplayDevicesAFunction = decltype(&::EnumDisplayDevicesA);

using GetMonitorInfoWFunction = decltype(&::GetMonitorInfoW);
using GetMonitorInfoAFunction = decltype(&::GetMonitorInfoA);

extern "C" {

// Interceptor for the  GdiDllInitialize function.
SANDBOX_INTERCEPT BOOL WINAPI
TargetGdiDllInitialize(GdiDllInitializeFunction orig_gdi_dll_initialize,
                       HANDLE dll,
                       DWORD reason);

// Interceptor for the GetStockObject function.
SANDBOX_INTERCEPT HGDIOBJ WINAPI
TargetGetStockObject(GetStockObjectFunction orig_get_stock_object, int object);

// Interceptor for the RegisterClassW function.
SANDBOX_INTERCEPT ATOM WINAPI
TargetRegisterClassW(RegisterClassWFunction orig_register_class_function,
                     const WNDCLASS* wnd_class);

SANDBOX_INTERCEPT BOOL WINAPI TargetEnumDisplayMonitors(
    EnumDisplayMonitorsFunction orig_enum_display_monitors_function,
    HDC hdc,
    LPCRECT clip_rect,
    MONITORENUMPROC enum_function,
    LPARAM data);

SANDBOX_INTERCEPT BOOL WINAPI TargetEnumDisplayDevicesA(
    EnumDisplayDevicesAFunction orig_enum_display_devices_function,
    LPCSTR device,
    DWORD device_number,
    PDISPLAY_DEVICEA display_device,
    DWORD flags);

SANDBOX_INTERCEPT BOOL WINAPI
TargetGetMonitorInfoA(GetMonitorInfoAFunction orig_get_monitor_info_function,
                      HMONITOR monitor,
                      LPMONITORINFO monitor_info);

SANDBOX_INTERCEPT BOOL WINAPI
TargetGetMonitorInfoW(GetMonitorInfoWFunction orig_get_monitor_info_function,
                      HMONITOR monitor,
                      LPMONITORINFO monitor_info);

SANDBOX_INTERCEPT NTSTATUS WINAPI TargetCreateOPMProtectedOutputs(
    CreateOPMProtectedOutputsFunction orig_create_proceted_outputs_function,
    PUNICODE_STRING device_name,
    DXGKMDT_OPM_VIDEO_OUTPUT_SEMANTICS vos,
    DWORD protected_output_array_size,
    DWORD* output_array_size,
    OPM_PROTECTED_OUTPUT_HANDLE* protected_outputs);

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetGetCertificate(GetCertificateFunction orig_get_certificate_function,
                     PUNICODE_STRING device_name,
                     DXGKMDT_CERTIFICATE_TYPE certificate_type,
                     BYTE* certificate,
                     ULONG certificate_length);

SANDBOX_INTERCEPT NTSTATUS WINAPI TargetGetCertificateSize(
    GetCertificateSizeFunction orig_get_certificate_size_function,
    PUNICODE_STRING device_name,
    DXGKMDT_CERTIFICATE_TYPE certificate_type,
    ULONG* certificate_length);

SANDBOX_INTERCEPT NTSTATUS WINAPI TargetGetCertificateByHandle(
    GetCertificateByHandleFunction orig_get_certificate_function,
    OPM_PROTECTED_OUTPUT_HANDLE protected_output,
    DXGKMDT_CERTIFICATE_TYPE certificate_type,
    BYTE* certificate,
    ULONG certificate_length);

SANDBOX_INTERCEPT NTSTATUS WINAPI TargetGetCertificateSizeByHandle(
    GetCertificateSizeByHandleFunction orig_get_certificate_size_function,
    OPM_PROTECTED_OUTPUT_HANDLE protected_output,
    DXGKMDT_CERTIFICATE_TYPE certificate_type,
    ULONG* certificate_length);

SANDBOX_INTERCEPT NTSTATUS WINAPI TargetDestroyOPMProtectedOutput(
    DestroyOPMProtectedOutputFunction orig_destroy_protected_output_function,
    OPM_PROTECTED_OUTPUT_HANDLE protected_output);

SANDBOX_INTERCEPT NTSTATUS WINAPI TargetConfigureOPMProtectedOutput(
    ConfigureOPMProtectedOutputFunction
        origin_configure_protected_output_function,
    OPM_PROTECTED_OUTPUT_HANDLE protected_output,
    const DXGKMDT_OPM_CONFIGURE_PARAMETERS* parameters,
    ULONG additional_parameters_size,
    const BYTE* additional_parameters);

SANDBOX_INTERCEPT NTSTATUS WINAPI TargetGetOPMInformation(
    GetOPMInformationFunction origin_get_information_function,
    OPM_PROTECTED_OUTPUT_HANDLE protected_output,
    const DXGKMDT_OPM_GET_INFO_PARAMETERS* parameters,
    DXGKMDT_OPM_REQUESTED_INFORMATION* requested_information);

SANDBOX_INTERCEPT NTSTATUS WINAPI TargetGetOPMRandomNumber(
    GetOPMRandomNumberFunction orig_get_random_number_function,
    OPM_PROTECTED_OUTPUT_HANDLE protected_output,
    DXGKMDT_OPM_RANDOM_NUMBER* random_number);

SANDBOX_INTERCEPT NTSTATUS WINAPI TargetGetSuggestedOPMProtectedOutputArraySize(
    GetSuggestedOPMProtectedOutputArraySizeFunction
        orig_get_suggested_size_function,
    PUNICODE_STRING device_name,
    DWORD* suggested_output_array_size);

SANDBOX_INTERCEPT NTSTATUS WINAPI TargetSetOPMSigningKeyAndSequenceNumbers(
    SetOPMSigningKeyAndSequenceNumbersFunction orig_set_signing_keys_function,
    OPM_PROTECTED_OUTPUT_HANDLE protected_output,
    const DXGKMDT_OPM_ENCRYPTED_PARAMETERS* parameters);

}  // extern "C"

}  // namespace sandbox

#endif  // SANDBOX_SRC_PROCESS_MITIGATIONS_WIN32K_INTERCEPTION_H_
