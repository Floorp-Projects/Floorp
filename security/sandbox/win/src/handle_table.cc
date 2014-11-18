// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/handle_table.h"

#include <algorithm>
#include <cstdlib>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "sandbox/win/src/win_utils.h"

namespace {

bool CompareHandleEntries(const SYSTEM_HANDLE_INFORMATION& a,
                          const SYSTEM_HANDLE_INFORMATION& b) {
  return a.ProcessId < b.ProcessId;
}

}  // namespace

namespace sandbox {

const base::char16* HandleTable::kTypeProcess = L"Process";
const base::char16* HandleTable::kTypeThread = L"Thread";
const base::char16* HandleTable::kTypeFile = L"File";
const base::char16* HandleTable::kTypeDirectory = L"Directory";
const base::char16* HandleTable::kTypeKey = L"Key";
const base::char16* HandleTable::kTypeWindowStation = L"WindowStation";
const base::char16* HandleTable::kTypeDesktop = L"Desktop";
const base::char16* HandleTable::kTypeService = L"Service";
const base::char16* HandleTable::kTypeMutex = L"Mutex";
const base::char16* HandleTable::kTypeSemaphore = L"Semaphore";
const base::char16* HandleTable::kTypeEvent = L"Event";
const base::char16* HandleTable::kTypeTimer = L"Timer";
const base::char16* HandleTable::kTypeNamedPipe = L"NamedPipe";
const base::char16* HandleTable::kTypeJobObject = L"JobObject";
const base::char16* HandleTable::kTypeFileMap = L"FileMap";
const base::char16* HandleTable::kTypeAlpcPort = L"ALPC Port";

HandleTable::HandleTable() {
  static NtQuerySystemInformation QuerySystemInformation = NULL;
  if (!QuerySystemInformation)
    ResolveNTFunctionPtr("NtQuerySystemInformation", &QuerySystemInformation);

  ULONG size = 0x15000;
  NTSTATUS result;
  do {
    handle_info_buffer_.resize(size);
    result = QuerySystemInformation(SystemHandleInformation,
        handle_info_internal(), size, &size);
  } while (result == STATUS_INFO_LENGTH_MISMATCH);

  // We failed, so make an empty table.
  if (!NT_SUCCESS(result)) {
    handle_info_buffer_.resize(0);
    return;
  }

  // Sort it to make process lookups faster.
  std::sort(handle_info_internal()->Information,
      handle_info_internal()->Information +
      handle_info_internal()->NumberOfHandles, CompareHandleEntries);
}

HandleTable::Iterator HandleTable::HandlesForProcess(ULONG process_id) const {
  SYSTEM_HANDLE_INFORMATION key;
  key.ProcessId = process_id;

  const SYSTEM_HANDLE_INFORMATION* start = handle_info()->Information;
  const SYSTEM_HANDLE_INFORMATION* finish =
      &handle_info()->Information[handle_info()->NumberOfHandles];

  start = std::lower_bound(start, finish, key, CompareHandleEntries);
  if (start->ProcessId != process_id)
    return Iterator(*this, finish, finish);
  finish = std::upper_bound(start, finish, key, CompareHandleEntries);
  return Iterator(*this, start, finish);
}

HandleTable::HandleEntry::HandleEntry(
    const SYSTEM_HANDLE_INFORMATION* handle_info_entry)
    : handle_entry_(handle_info_entry), last_entry_(0) {
}

void HandleTable::HandleEntry::UpdateInfo(UpdateType flag) {
  static NtQueryObject QueryObject = NULL;
  if (!QueryObject)
    ResolveNTFunctionPtr("NtQueryObject", &QueryObject);

  NTSTATUS result;

  // Always update the basic type info, but grab the names as needed.
  if (needs_info_update()) {
    handle_name_.clear();
    type_name_.clear();
    last_entry_ = handle_entry_;

    // Most handle names are very short, so start small and reuse this buffer.
    if (type_info_buffer_.empty())
      type_info_buffer_.resize(sizeof(OBJECT_TYPE_INFORMATION)
          + (32 * sizeof(wchar_t)));
    ULONG size = static_cast<ULONG>(type_info_buffer_.size());
    result = QueryObject(reinterpret_cast<HANDLE>(handle_entry_->Handle),
        ObjectTypeInformation, type_info_internal(), size, &size);
    while (result == STATUS_INFO_LENGTH_MISMATCH) {
      type_info_buffer_.resize(size);
      result = QueryObject(reinterpret_cast<HANDLE>(handle_entry_->Handle),
          ObjectTypeInformation, type_info_internal(), size, &size);
    }

    if (!NT_SUCCESS(result)) {
      type_info_buffer_.clear();
      return;
    }
  }

  // Don't bother copying out names until we ask for them, and then cache them.
  switch (flag) {
    case UPDATE_INFO_AND_NAME:
      if (type_info_buffer_.size() && handle_name_.empty()) {
        ULONG size = MAX_PATH;
        scoped_ptr<UNICODE_STRING, base::FreeDeleter> name;
        do {
          name.reset(static_cast<UNICODE_STRING*>(malloc(size)));
          DCHECK(name.get());
          result = QueryObject(reinterpret_cast<HANDLE>(
              handle_entry_->Handle), ObjectNameInformation, name.get(),
              size, &size);
        } while (result == STATUS_INFO_LENGTH_MISMATCH);

        if (NT_SUCCESS(result)) {
          handle_name_.assign(name->Buffer, name->Length / sizeof(wchar_t));
        }
      }
      break;

    case UPDATE_INFO_AND_TYPE_NAME:
      if (!type_info_buffer_.empty() && type_info_internal()->Name.Buffer &&
          type_name_.empty()) {
        type_name_.assign(type_info_internal()->Name.Buffer,
            type_info_internal()->Name.Length / sizeof(wchar_t));
      }
      break;
  }
}

const OBJECT_TYPE_INFORMATION* HandleTable::HandleEntry::TypeInfo() {
  UpdateInfo(UPDATE_INFO_ONLY);
  return type_info_buffer_.empty() ? NULL : type_info_internal();
}

const base::string16& HandleTable::HandleEntry::Name() {
  UpdateInfo(UPDATE_INFO_AND_NAME);
  return handle_name_;
}

const base::string16& HandleTable::HandleEntry::Type() {
  UpdateInfo(UPDATE_INFO_AND_TYPE_NAME);
  return type_name_;
}

bool HandleTable::HandleEntry::IsType(const base::string16& type_string) {
  UpdateInfo(UPDATE_INFO_ONLY);
  if (type_info_buffer_.empty())
    return false;
  return type_string.compare(0,
      type_info_internal()->Name.Length / sizeof(wchar_t),
      type_info_internal()->Name.Buffer) == 0;
}

HandleTable::Iterator::Iterator(const HandleTable& table,
                                const SYSTEM_HANDLE_INFORMATION* start,
                                const SYSTEM_HANDLE_INFORMATION* end)
    : table_(table), current_(start), end_(end) {
}

HandleTable::Iterator::Iterator(const Iterator& it)
    : table_(it.table_), current_(it.current_.handle_entry_), end_(it.end_) {
}

}  // namespace sandbox
