// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_HANDLE_TABLE_H_
#define SANDBOX_SRC_HANDLE_TABLE_H_

#include <windows.h>
#include <vector>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "sandbox/win/src/nt_internals.h"

namespace sandbox {

// HandleTable retrieves the global handle table and provides helper classes
// for iterating through the table and retrieving handle info.
class HandleTable {
 public:
  static const base::char16* HandleTable::kTypeProcess;
  static const base::char16* HandleTable::kTypeThread;
  static const base::char16* HandleTable::kTypeFile;
  static const base::char16* HandleTable::kTypeDirectory;
  static const base::char16* HandleTable::kTypeKey;
  static const base::char16* HandleTable::kTypeWindowStation;
  static const base::char16* HandleTable::kTypeDesktop;
  static const base::char16* HandleTable::kTypeService;
  static const base::char16* HandleTable::kTypeMutex;
  static const base::char16* HandleTable::kTypeSemaphore;
  static const base::char16* HandleTable::kTypeEvent;
  static const base::char16* HandleTable::kTypeTimer;
  static const base::char16* HandleTable::kTypeNamedPipe;
  static const base::char16* HandleTable::kTypeJobObject;
  static const base::char16* HandleTable::kTypeFileMap;
  static const base::char16* HandleTable::kTypeAlpcPort;

  class Iterator;

  // Used by the iterator to provide simple caching accessors to handle data.
  class HandleEntry {
   public:
    bool operator==(const HandleEntry& rhs) const {
      return handle_entry_ == rhs.handle_entry_;
    }

    bool operator!=(const HandleEntry& rhs) const {
      return handle_entry_ != rhs.handle_entry_;
    }

    const SYSTEM_HANDLE_INFORMATION* handle_entry() const {
      return handle_entry_;
    }

    const OBJECT_TYPE_INFORMATION* TypeInfo();

    const base::string16& Name();

    const base::string16& Type();

    bool IsType(const base::string16& type_string);

   private:
    friend class Iterator;
    friend class HandleTable;

    enum UpdateType {
      UPDATE_INFO_ONLY,
      UPDATE_INFO_AND_NAME,
      UPDATE_INFO_AND_TYPE_NAME,
    };

    explicit HandleEntry(const SYSTEM_HANDLE_INFORMATION* handle_info_entry);

    bool needs_info_update() { return handle_entry_ != last_entry_; }

    void UpdateInfo(UpdateType flag);

    OBJECT_TYPE_INFORMATION* type_info_internal() {
      return reinterpret_cast<OBJECT_TYPE_INFORMATION*>(
          &(type_info_buffer_[0]));
    }

    const SYSTEM_HANDLE_INFORMATION* handle_entry_;
    const SYSTEM_HANDLE_INFORMATION* last_entry_;
    std::vector<BYTE> type_info_buffer_;
    base::string16 handle_name_;
    base::string16 type_name_;

    DISALLOW_COPY_AND_ASSIGN(HandleEntry);
  };

  class Iterator {
   public:
    Iterator(const HandleTable& table, const SYSTEM_HANDLE_INFORMATION* start,
             const SYSTEM_HANDLE_INFORMATION* stop);

    Iterator(const Iterator& it);

    Iterator& operator++() {
      if (++(current_.handle_entry_) == end_)
        current_.handle_entry_ = table_.end();
      return *this;
    }

    bool operator==(const Iterator& rhs) const {
      return current_ == rhs.current_;
    }

    bool operator!=(const Iterator& rhs) const {
      return current_ != rhs.current_;
    }

    HandleEntry& operator*() { return current_; }

    operator const SYSTEM_HANDLE_INFORMATION*() {
      return current_.handle_entry_;
    }

    HandleEntry* operator->() { return &current_; }

   private:
    const HandleTable& table_;
    HandleEntry current_;
    const SYSTEM_HANDLE_INFORMATION* end_;
  };

  HandleTable();

  Iterator begin() const {
    return Iterator(*this, handle_info()->Information,
        &handle_info()->Information[handle_info()->NumberOfHandles]);
  }

  const SYSTEM_HANDLE_INFORMATION_EX* handle_info() const {
    return reinterpret_cast<const SYSTEM_HANDLE_INFORMATION_EX*>(
        &(handle_info_buffer_[0]));
  }

  // Returns an iterator to the handles for only the supplied process ID.
  Iterator HandlesForProcess(ULONG process_id) const;
  const SYSTEM_HANDLE_INFORMATION* end() const {
    return &handle_info()->Information[handle_info()->NumberOfHandles];
  }

 private:
  SYSTEM_HANDLE_INFORMATION_EX* handle_info_internal() {
    return reinterpret_cast<SYSTEM_HANDLE_INFORMATION_EX*>(
        &(handle_info_buffer_[0]));
  }

  std::vector<BYTE> handle_info_buffer_;

  DISALLOW_COPY_AND_ASSIGN(HandleTable);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_HANDLE_TABLE_H_
