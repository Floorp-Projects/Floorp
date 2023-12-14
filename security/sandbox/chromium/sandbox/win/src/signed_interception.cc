// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/signed_interception.h"

#include <stdint.h>

#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/policy_target.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sharedmem_ipc_client.h"
#include "sandbox/win/src/target_services.h"
#include "mozilla/Assertions.h"
#include "mozilla/sandboxing/sandboxLogging.h"

#if defined(DEBUG)
enum class BreakReason {
  None = 0,
  DesiredAccess = 1,
  ObjectAttributes = 2,
  MaximumSize = 3,
  SectionPageProtection = 3,
  AllocationAttributes = 4,
  Memory = 5,
  GetPath = 6,
  QueryBroker = 7,
  ValidParameter = 8,
  ResultCode = 9,
  NtStatus = 10,
  Exception = 11,
};
#endif  // DEBUG

namespace sandbox {

NTSTATUS WINAPI
TargetNtCreateSection(NtCreateSectionFunction orig_CreateSection,
                      PHANDLE section_handle,
                      ACCESS_MASK desired_access,
                      POBJECT_ATTRIBUTES object_attributes,
                      PLARGE_INTEGER maximum_size,
                      ULONG section_page_protection,
                      ULONG allocation_attributes,
                      HANDLE file_handle) {
#if defined(DEBUG)
  BreakReason break_reason = BreakReason::None;
  union {
    NTSTATUS nt_status;
    ResultCode result_code;
    DWORD exc_code;
  } faulty_value;
  wchar_t dll_path_local_copy[MAX_PATH]{};
  faulty_value.exc_code = 0;
#endif  // DEBUG
  do {
    // The section only needs to have SECTION_MAP_EXECUTE, but the permissions
    // vary depending on the OS. Windows 1903 and higher requests (SECTION_QUERY
    // | SECTION_MAP_READ | SECTION_MAP_EXECUTE) while previous OS versions also
    // request SECTION_MAP_WRITE. Just check for EXECUTE.
    if (!(desired_access & SECTION_MAP_EXECUTE)) {
#if defined(DEBUG)
      break_reason = BreakReason::DesiredAccess;
#endif  // DEBUG
      break;
    }
    if (object_attributes) {
#if defined(DEBUG)
      break_reason = BreakReason::ObjectAttributes;
#endif  // DEBUG
      break;
    }
    if (maximum_size) {
#if defined(DEBUG)
      break_reason = BreakReason::MaximumSize;
#endif  // DEBUG
      break;
    }
    if (section_page_protection != PAGE_EXECUTE) {
#if defined(DEBUG)
      break_reason = BreakReason::SectionPageProtection;
#endif  // DEBUG
      break;
    }
    if (allocation_attributes != SEC_IMAGE) {
#if defined(DEBUG)
      break_reason = BreakReason::AllocationAttributes;
#endif  // DEBUG
      break;
    }

    mozilla::sandboxing::LogBlocked("NtCreateSection");

    // IPC must be fully started.
    void* memory = GetGlobalIPCMemory();
    if (!memory) {
#if defined(DEBUG)
      break_reason = BreakReason::Memory;
#endif  // DEBUG
      break;
    }

    std::unique_ptr<wchar_t, NtAllocDeleter> path;

    if (!NtGetPathFromHandle(file_handle, &path)) {
#if defined(DEBUG)
      break_reason = BreakReason::GetPath;
#endif  // DEBUG
      break;
    }

    const wchar_t* const_name = path.get();

#if defined(DEBUG)
    // Do a suboptimal wide string copy. This function is too low-level to
    // depend on anything that lives in a DLL.
    for (int i = 0; i < MAX_PATH - 1; ++i) {
      wchar_t c = const_name[i];
      dll_path_local_copy[i] = c;
      if (!c) {
        break;
      }
    }
    dll_path_local_copy[MAX_PATH - 1] = 0;
#endif  // DEBUG

    CountedParameterSet<NameBased> params;
    params[NameBased::NAME] = ParamPickerMake(const_name);

    // Check if this will be sent to the broker.
    if (!QueryBroker(IpcTag::NTCREATESECTION, params.GetBase())) {
#if defined(DEBUG)
      break_reason = BreakReason::QueryBroker;
#endif  // DEBUG
      break;
    }

    if (!ValidParameter(section_handle, sizeof(HANDLE), WRITE)) {
#if defined(DEBUG)
      break_reason = BreakReason::ValidParameter;
#endif  // DEBUG
      break;
    }

    CrossCallReturn answer = {0};
    answer.nt_status = STATUS_INVALID_IMAGE_HASH;
    SharedMemIPCClient ipc(memory);
    ResultCode code =
        CrossCall(ipc, IpcTag::NTCREATESECTION, file_handle, &answer);

    if (code != SBOX_ALL_OK) {
#if defined(DEBUG)
      break_reason = BreakReason::ResultCode;
      faulty_value.result_code = code;
#endif  // DEBUG
      break;
    }

    if (!NT_SUCCESS(answer.nt_status)) {
#if defined(DEBUG)
      break_reason = BreakReason::NtStatus;
      faulty_value.nt_status = answer.nt_status;
#endif  // DEBUG
      break;
    }

    __try {
      *section_handle = answer.handle;
      mozilla::sandboxing::LogAllowed("NtCreateSection");
      return answer.nt_status;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
#if defined(DEBUG)
      break_reason = BreakReason::Exception;
      faulty_value.exc_code = GetExceptionCode();
#endif  // DEBUG
      break;
    }
  } while (false);

  // Fall back to the original API in all failure cases.
  NTSTATUS local_nt_status = orig_CreateSection(
      section_handle, desired_access, object_attributes, maximum_size,
      section_page_protection, allocation_attributes, file_handle);
#if defined(DEBUG)
  if (local_nt_status == STATUS_INVALID_IMAGE_HASH) {
    MOZ_CRASH_UNSAFE_PRINTF(
        "Prespawn CIG: STATUS_INVALID_IMAGE_HASH for %S (break_reason: %d, "
        "faulty_value: 0x%lx)\n",
        dll_path_local_copy, static_cast<int>(break_reason),
        faulty_value.exc_code);
  }
#endif  // DEBUG
  return local_nt_status;
}

}  // namespace sandbox
