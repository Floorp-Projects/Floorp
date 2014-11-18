// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/handle_closer.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/interceptors.h"
#include "sandbox/win/src/internal_types.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/process_thread_interception.h"
#include "sandbox/win/src/win_utils.h"

namespace {

template<typename T> T RoundUpToWordSize(T v) {
  if (size_t mod = v % sizeof(size_t))
    v += sizeof(size_t) - mod;
  return v;
}

template<typename T> T* RoundUpToWordSize(T* v) {
  return reinterpret_cast<T*>(RoundUpToWordSize(reinterpret_cast<size_t>(v)));
}

}  // namespace

namespace sandbox {

// Memory buffer mapped from the parent, with the list of handles.
SANDBOX_INTERCEPT HandleCloserInfo* g_handles_to_close;

HandleCloser::HandleCloser() {}

ResultCode HandleCloser::AddHandle(const base::char16* handle_type,
                                   const base::char16* handle_name) {
  if (!handle_type)
    return SBOX_ERROR_BAD_PARAMS;

  HandleMap::iterator names = handles_to_close_.find(handle_type);
  if (names == handles_to_close_.end()) {  // We have no entries for this type.
    std::pair<HandleMap::iterator, bool> result = handles_to_close_.insert(
        HandleMap::value_type(handle_type, HandleMap::mapped_type()));
    names = result.first;
    if (handle_name)
      names->second.insert(handle_name);
  } else if (!handle_name) {  // Now we need to close all handles of this type.
    names->second.clear();
  } else if (!names->second.empty()) {  // Add another name for this type.
    names->second.insert(handle_name);
  }  // If we're already closing all handles of type then we're done.

  return SBOX_ALL_OK;
}

size_t HandleCloser::GetBufferSize() {
  size_t bytes_total = offsetof(HandleCloserInfo, handle_entries);

  for (HandleMap::iterator i = handles_to_close_.begin();
       i != handles_to_close_.end(); ++i) {
    size_t bytes_entry = offsetof(HandleListEntry, handle_type) +
        (i->first.size() + 1) * sizeof(base::char16);
    for (HandleMap::mapped_type::iterator j = i->second.begin();
         j != i->second.end(); ++j) {
      bytes_entry += ((*j).size() + 1) * sizeof(base::char16);
    }

    // Round up to the nearest multiple of word size.
    bytes_entry = RoundUpToWordSize(bytes_entry);
    bytes_total += bytes_entry;
  }

  return bytes_total;
}

bool HandleCloser::InitializeTargetHandles(TargetProcess* target) {
  // Do nothing on an empty list (global pointer already initialized to NULL).
  if (handles_to_close_.empty())
    return true;

  size_t bytes_needed = GetBufferSize();
  scoped_ptr<size_t[]> local_buffer(
      new size_t[bytes_needed / sizeof(size_t)]);

  if (!SetupHandleList(local_buffer.get(), bytes_needed))
    return false;

  HANDLE child = target->Process();

  // Allocate memory in the target process without specifying the address
  void* remote_data = ::VirtualAllocEx(child, NULL, bytes_needed,
                                       MEM_COMMIT, PAGE_READWRITE);
  if (NULL == remote_data)
    return false;

  // Copy the handle buffer over.
  SIZE_T bytes_written;
  BOOL result = ::WriteProcessMemory(child, remote_data, local_buffer.get(),
                                     bytes_needed, &bytes_written);
  if (!result || bytes_written != bytes_needed) {
    ::VirtualFreeEx(child, remote_data, 0, MEM_RELEASE);
    return false;
  }

  g_handles_to_close = reinterpret_cast<HandleCloserInfo*>(remote_data);

  ResultCode rc = target->TransferVariable("g_handles_to_close",
                                           &g_handles_to_close,
                                           sizeof(g_handles_to_close));

  return (SBOX_ALL_OK == rc);
}

bool HandleCloser::SetupHandleList(void* buffer, size_t buffer_bytes) {
  ::ZeroMemory(buffer, buffer_bytes);
  HandleCloserInfo* handle_info = reinterpret_cast<HandleCloserInfo*>(buffer);
  handle_info->record_bytes = buffer_bytes;
  handle_info->num_handle_types = handles_to_close_.size();

  base::char16* output = reinterpret_cast<base::char16*>(
      &handle_info->handle_entries[0]);
  base::char16* end = reinterpret_cast<base::char16*>(
      reinterpret_cast<char*>(buffer) + buffer_bytes);
  for (HandleMap::iterator i = handles_to_close_.begin();
       i != handles_to_close_.end(); ++i) {
    if (output >= end)
      return false;
    HandleListEntry* list_entry = reinterpret_cast<HandleListEntry*>(output);
    output = &list_entry->handle_type[0];

    // Copy the typename and set the offset and count.
    i->first._Copy_s(output, i->first.size(), i->first.size());
    *(output += i->first.size()) = L'\0';
    output++;
    list_entry->offset_to_names = reinterpret_cast<char*>(output) -
        reinterpret_cast<char*>(list_entry);
    list_entry->name_count = i->second.size();

    // Copy the handle names.
    for (HandleMap::mapped_type::iterator j = i->second.begin();
         j != i->second.end(); ++j) {
      output = std::copy((*j).begin(), (*j).end(), output) + 1;
    }

    // Round up to the nearest multiple of sizeof(size_t).
    output = RoundUpToWordSize(output);
    list_entry->record_bytes = reinterpret_cast<char*>(output) -
        reinterpret_cast<char*>(list_entry);
  }

  DCHECK_EQ(reinterpret_cast<size_t>(output), reinterpret_cast<size_t>(end));
  return output <= end;
}

bool GetHandleName(HANDLE handle, base::string16* handle_name) {
  static NtQueryObject QueryObject = NULL;
  if (!QueryObject)
    ResolveNTFunctionPtr("NtQueryObject", &QueryObject);

  ULONG size = MAX_PATH;
  scoped_ptr<UNICODE_STRING, base::FreeDeleter> name;
  NTSTATUS result;

  do {
    name.reset(static_cast<UNICODE_STRING*>(malloc(size)));
    DCHECK(name.get());
    result = QueryObject(handle, ObjectNameInformation, name.get(),
                         size, &size);
  } while (result == STATUS_INFO_LENGTH_MISMATCH ||
           result == STATUS_BUFFER_OVERFLOW);

  if (NT_SUCCESS(result) && name->Buffer && name->Length)
    handle_name->assign(name->Buffer, name->Length / sizeof(wchar_t));
  else
    handle_name->clear();

  return NT_SUCCESS(result);
}

}  // namespace sandbox
