// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/handle_closer_agent.h"

#include "base/logging.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/win_utils.h"

namespace {

// Returns type infomation for an NT object. This routine is expected to be
// called for invalid handles so it catches STATUS_INVALID_HANDLE exceptions
// that can be generated when handle tracing is enabled.
NTSTATUS QueryObjectTypeInformation(HANDLE handle,
                                    void* buffer,
                                    ULONG* size) {
  static NtQueryObject QueryObject = NULL;
  if (!QueryObject)
    ResolveNTFunctionPtr("NtQueryObject", &QueryObject);

  NTSTATUS status = STATUS_UNSUCCESSFUL;
  __try {
    status = QueryObject(handle, ObjectTypeInformation, buffer, *size, size);
  } __except(GetExceptionCode() == STATUS_INVALID_HANDLE ?
                 EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
    status = STATUS_INVALID_HANDLE;
  }
  return status;
}

}  // namespace

namespace sandbox {

// Memory buffer mapped from the parent, with the list of handles.
SANDBOX_INTERCEPT HandleCloserInfo* g_handles_to_close = NULL;

bool HandleCloserAgent::NeedsHandlesClosed() {
  return g_handles_to_close != NULL;
}

// Reads g_handles_to_close and creates the lookup map.
void HandleCloserAgent::InitializeHandlesToClose() {
  CHECK(g_handles_to_close != NULL);

  // Grab the header.
  HandleListEntry* entry = g_handles_to_close->handle_entries;
  for (size_t i = 0; i < g_handles_to_close->num_handle_types; ++i) {
    // Set the type name.
    char16* input = entry->handle_type;
    HandleMap::mapped_type& handle_names = handles_to_close_[input];
    input = reinterpret_cast<char16*>(reinterpret_cast<char*>(entry)
        + entry->offset_to_names);
    // Grab all the handle names.
    for (size_t j = 0; j < entry->name_count; ++j) {
      std::pair<HandleMap::mapped_type::iterator, bool> name
          = handle_names.insert(input);
      CHECK(name.second);
      input += name.first->size() + 1;
    }

    // Move on to the next entry.
    entry = reinterpret_cast<HandleListEntry*>(reinterpret_cast<char*>(entry)
        + entry->record_bytes);

    DCHECK(reinterpret_cast<char16*>(entry) >= input);
    DCHECK(reinterpret_cast<char16*>(entry) - input <
           sizeof(size_t) / sizeof(char16));
  }

  // Clean up the memory we copied over.
  ::VirtualFree(g_handles_to_close, 0, MEM_RELEASE);
  g_handles_to_close = NULL;
}

bool HandleCloserAgent::CloseHandles() {
  DWORD handle_count = UINT_MAX;
  const int kInvalidHandleThreshold = 100;
  const size_t kHandleOffset = sizeof(HANDLE);

  if (!::GetProcessHandleCount(::GetCurrentProcess(), &handle_count))
    return false;

  // Set up buffers for the type info and the name.
  std::vector<BYTE> type_info_buffer(sizeof(OBJECT_TYPE_INFORMATION) +
                                     32 * sizeof(wchar_t));
  OBJECT_TYPE_INFORMATION* type_info =
      reinterpret_cast<OBJECT_TYPE_INFORMATION*>(&(type_info_buffer[0]));
  string16 handle_name;
  HANDLE handle = NULL;
  int invalid_count = 0;

  // Keep incrementing until we hit the number of handles reported by
  // GetProcessHandleCount(). If we hit a very long sequence of invalid
  // handles we assume that we've run past the end of the table.
  while (handle_count && invalid_count < kInvalidHandleThreshold) {
    reinterpret_cast<size_t&>(handle) += kHandleOffset;
    NTSTATUS rc;

    // Get the type name, reusing the buffer.
    ULONG size = static_cast<ULONG>(type_info_buffer.size());
    rc = QueryObjectTypeInformation(handle, type_info, &size);
    while (rc == STATUS_INFO_LENGTH_MISMATCH ||
           rc == STATUS_BUFFER_OVERFLOW) {
      type_info_buffer.resize(size + sizeof(wchar_t));
      type_info = reinterpret_cast<OBJECT_TYPE_INFORMATION*>(
          &(type_info_buffer[0]));
      rc = QueryObjectTypeInformation(handle, type_info, &size);
      // Leave padding for the nul terminator.
      if (NT_SUCCESS(rc) && size == type_info_buffer.size())
        rc = STATUS_INFO_LENGTH_MISMATCH;
    }
    if (!NT_SUCCESS(rc) || !type_info->Name.Buffer) {
      ++invalid_count;
      continue;
    }

    --handle_count;
    type_info->Name.Buffer[type_info->Name.Length / sizeof(wchar_t)] = L'\0';

    // Check if we're looking for this type of handle.
    HandleMap::iterator result =
        handles_to_close_.find(type_info->Name.Buffer);
    if (result != handles_to_close_.end()) {
      HandleMap::mapped_type& names = result->second;
      // Empty set means close all handles of this type; otherwise check name.
      if (!names.empty()) {
        // Move on to the next handle if this name doesn't match.
        if (!GetHandleName(handle, &handle_name) || !names.count(handle_name))
          continue;
      }

      if (!::SetHandleInformation(handle, HANDLE_FLAG_PROTECT_FROM_CLOSE, 0))
        return false;
      if (!::CloseHandle(handle))
        return false;
    }
  }

  return true;
}

}  // namespace sandbox
