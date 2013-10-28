// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/registry.h"

#include <shlwapi.h>
#include <algorithm>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"

#pragma comment(lib, "shlwapi.lib")  // for SHDeleteKey

namespace base {
namespace win {

namespace {

// RegEnumValue() reports the number of characters from the name that were
// written to the buffer, not how many there are. This constant is the maximum
// name size, such that a buffer with this size should read any name.
const DWORD MAX_REGISTRY_NAME_SIZE = 16384;

// Registry values are read as BYTE* but can have wchar_t* data whose last
// wchar_t is truncated. This function converts the reported |byte_size| to
// a size in wchar_t that can store a truncated wchar_t if necessary.
inline DWORD to_wchar_size(DWORD byte_size) {
  return (byte_size + sizeof(wchar_t) - 1) / sizeof(wchar_t);
}

}  // namespace

// RegKey ----------------------------------------------------------------------

RegKey::RegKey()
    : key_(NULL),
      watch_event_(0) {
}

RegKey::RegKey(HKEY key)
    : key_(key),
      watch_event_(0) {
}

RegKey::RegKey(HKEY rootkey, const wchar_t* subkey, REGSAM access)
    : key_(NULL),
      watch_event_(0) {
  if (rootkey) {
    if (access & (KEY_SET_VALUE | KEY_CREATE_SUB_KEY | KEY_CREATE_LINK))
      Create(rootkey, subkey, access);
    else
      Open(rootkey, subkey, access);
  } else {
    DCHECK(!subkey);
  }
}

RegKey::~RegKey() {
  Close();
}

LONG RegKey::Create(HKEY rootkey, const wchar_t* subkey, REGSAM access) {
  DWORD disposition_value;
  return CreateWithDisposition(rootkey, subkey, &disposition_value, access);
}

LONG RegKey::CreateWithDisposition(HKEY rootkey, const wchar_t* subkey,
                                   DWORD* disposition, REGSAM access) {
  DCHECK(rootkey && subkey && access && disposition);
  Close();

  LONG result = RegCreateKeyEx(rootkey, subkey, 0, NULL,
                               REG_OPTION_NON_VOLATILE, access, NULL, &key_,
                               disposition);
  return result;
}

LONG RegKey::CreateKey(const wchar_t* name, REGSAM access) {
  DCHECK(name && access);
  HKEY subkey = NULL;
  LONG result = RegCreateKeyEx(key_, name, 0, NULL, REG_OPTION_NON_VOLATILE,
                               access, NULL, &subkey, NULL);
  Close();

  key_ = subkey;
  return result;
}

LONG RegKey::Open(HKEY rootkey, const wchar_t* subkey, REGSAM access) {
  DCHECK(rootkey && subkey && access);
  Close();

  LONG result = RegOpenKeyEx(rootkey, subkey, 0, access, &key_);
  return result;
}

LONG RegKey::OpenKey(const wchar_t* relative_key_name, REGSAM access) {
  DCHECK(relative_key_name && access);
  HKEY subkey = NULL;
  LONG result = RegOpenKeyEx(key_, relative_key_name, 0, access, &subkey);

  // We have to close the current opened key before replacing it with the new
  // one.
  Close();

  key_ = subkey;
  return result;
}

void RegKey::Close() {
  StopWatching();
  if (key_) {
    ::RegCloseKey(key_);
    key_ = NULL;
  }
}

void RegKey::Set(HKEY key) {
  if (key_ != key) {
    Close();
    key_ = key;
  }
}

HKEY RegKey::Take() {
  StopWatching();
  HKEY key = key_;
  key_ = NULL;
  return key;
}

bool RegKey::HasValue(const wchar_t* name) const {
  return RegQueryValueEx(key_, name, 0, NULL, NULL, NULL) == ERROR_SUCCESS;
}

DWORD RegKey::GetValueCount() const {
  DWORD count = 0;
  LONG result = RegQueryInfoKey(key_, NULL, 0, NULL, NULL, NULL, NULL, &count,
                                NULL, NULL, NULL, NULL);
  return (result == ERROR_SUCCESS) ? count : 0;
}

LONG RegKey::GetValueNameAt(int index, std::wstring* name) const {
  wchar_t buf[256];
  DWORD bufsize = arraysize(buf);
  LONG r = ::RegEnumValue(key_, index, buf, &bufsize, NULL, NULL, NULL, NULL);
  if (r == ERROR_SUCCESS)
    *name = buf;

  return r;
}

LONG RegKey::DeleteKey(const wchar_t* name) {
  DCHECK(key_);
  DCHECK(name);
  LONG result = SHDeleteKey(key_, name);
  return result;
}

LONG RegKey::DeleteValue(const wchar_t* value_name) {
  DCHECK(key_);
  LONG result = RegDeleteValue(key_, value_name);
  return result;
}

LONG RegKey::ReadValueDW(const wchar_t* name, DWORD* out_value) const {
  DCHECK(out_value);
  DWORD type = REG_DWORD;
  DWORD size = sizeof(DWORD);
  DWORD local_value = 0;
  LONG result = ReadValue(name, &local_value, &size, &type);
  if (result == ERROR_SUCCESS) {
    if ((type == REG_DWORD || type == REG_BINARY) && size == sizeof(DWORD))
      *out_value = local_value;
    else
      result = ERROR_CANTREAD;
  }

  return result;
}

LONG RegKey::ReadInt64(const wchar_t* name, int64* out_value) const {
  DCHECK(out_value);
  DWORD type = REG_QWORD;
  int64 local_value = 0;
  DWORD size = sizeof(local_value);
  LONG result = ReadValue(name, &local_value, &size, &type);
  if (result == ERROR_SUCCESS) {
    if ((type == REG_QWORD || type == REG_BINARY) &&
        size == sizeof(local_value))
      *out_value = local_value;
    else
      result = ERROR_CANTREAD;
  }

  return result;
}

LONG RegKey::ReadValue(const wchar_t* name, std::wstring* out_value) const {
  DCHECK(out_value);
  const size_t kMaxStringLength = 1024;  // This is after expansion.
  // Use the one of the other forms of ReadValue if 1024 is too small for you.
  wchar_t raw_value[kMaxStringLength];
  DWORD type = REG_SZ, size = sizeof(raw_value);
  LONG result = ReadValue(name, raw_value, &size, &type);
  if (result == ERROR_SUCCESS) {
    if (type == REG_SZ) {
      *out_value = raw_value;
    } else if (type == REG_EXPAND_SZ) {
      wchar_t expanded[kMaxStringLength];
      size = ExpandEnvironmentStrings(raw_value, expanded, kMaxStringLength);
      // Success: returns the number of wchar_t's copied
      // Fail: buffer too small, returns the size required
      // Fail: other, returns 0
      if (size == 0 || size > kMaxStringLength) {
        result = ERROR_MORE_DATA;
      } else {
        *out_value = expanded;
      }
    } else {
      // Not a string. Oops.
      result = ERROR_CANTREAD;
    }
  }

  return result;
}

LONG RegKey::ReadValue(const wchar_t* name,
                       void* data,
                       DWORD* dsize,
                       DWORD* dtype) const {
  LONG result = RegQueryValueEx(key_, name, 0, dtype,
                                reinterpret_cast<LPBYTE>(data), dsize);
  return result;
}

LONG RegKey::ReadValues(const wchar_t* name,
                        std::vector<std::wstring>* values) {
  values->clear();

  DWORD type = REG_MULTI_SZ;
  DWORD size = 0;
  LONG result = ReadValue(name, NULL, &size, &type);
  if (FAILED(result) || size == 0)
    return result;

  if (type != REG_MULTI_SZ)
    return ERROR_CANTREAD;

  std::vector<wchar_t> buffer(size / sizeof(wchar_t));
  result = ReadValue(name, &buffer[0], &size, NULL);
  if (FAILED(result) || size == 0)
    return result;

  // Parse the double-null-terminated list of strings.
  // Note: This code is paranoid to not read outside of |buf|, in the case where
  // it may not be properly terminated.
  const wchar_t* entry = &buffer[0];
  const wchar_t* buffer_end = entry + (size / sizeof(wchar_t));
  while (entry < buffer_end && entry[0] != '\0') {
    const wchar_t* entry_end = std::find(entry, buffer_end, L'\0');
    values->push_back(std::wstring(entry, entry_end));
    entry = entry_end + 1;
  }
  return 0;
}

LONG RegKey::WriteValue(const wchar_t* name, DWORD in_value) {
  return WriteValue(
      name, &in_value, static_cast<DWORD>(sizeof(in_value)), REG_DWORD);
}

LONG RegKey::WriteValue(const wchar_t * name, const wchar_t* in_value) {
  return WriteValue(name, in_value,
      static_cast<DWORD>(sizeof(*in_value) * (wcslen(in_value) + 1)), REG_SZ);
}

LONG RegKey::WriteValue(const wchar_t* name,
                        const void* data,
                        DWORD dsize,
                        DWORD dtype) {
  DCHECK(data || !dsize);

  LONG result = RegSetValueEx(key_, name, 0, dtype,
      reinterpret_cast<LPBYTE>(const_cast<void*>(data)), dsize);
  return result;
}

LONG RegKey::StartWatching() {
  DCHECK(key_);
  if (!watch_event_)
    watch_event_ = CreateEvent(NULL, TRUE, FALSE, NULL);

  DWORD filter = REG_NOTIFY_CHANGE_NAME |
                 REG_NOTIFY_CHANGE_ATTRIBUTES |
                 REG_NOTIFY_CHANGE_LAST_SET |
                 REG_NOTIFY_CHANGE_SECURITY;

  // Watch the registry key for a change of value.
  LONG result = RegNotifyChangeKeyValue(key_, TRUE, filter, watch_event_, TRUE);
  if (result != ERROR_SUCCESS) {
    CloseHandle(watch_event_);
    watch_event_ = 0;
  }

  return result;
}

bool RegKey::HasChanged() {
  if (watch_event_) {
    if (WaitForSingleObject(watch_event_, 0) == WAIT_OBJECT_0) {
      StartWatching();
      return true;
    }
  }
  return false;
}

LONG RegKey::StopWatching() {
  LONG result = ERROR_INVALID_HANDLE;
  if (watch_event_) {
    CloseHandle(watch_event_);
    watch_event_ = 0;
    result = ERROR_SUCCESS;
  }
  return result;
}

// RegistryValueIterator ------------------------------------------------------

RegistryValueIterator::RegistryValueIterator(HKEY root_key,
                                             const wchar_t* folder_key)
    : name_(MAX_PATH, L'\0'),
      value_(MAX_PATH, L'\0') {
  LONG result = RegOpenKeyEx(root_key, folder_key, 0, KEY_READ, &key_);
  if (result != ERROR_SUCCESS) {
    key_ = NULL;
  } else {
    DWORD count = 0;
    result = ::RegQueryInfoKey(key_, NULL, 0, NULL, NULL, NULL, NULL, &count,
                               NULL, NULL, NULL, NULL);

    if (result != ERROR_SUCCESS) {
      ::RegCloseKey(key_);
      key_ = NULL;
    } else {
      index_ = count - 1;
    }
  }

  Read();
}

RegistryValueIterator::~RegistryValueIterator() {
  if (key_)
    ::RegCloseKey(key_);
}

DWORD RegistryValueIterator::ValueCount() const {
  DWORD count = 0;
  LONG result = ::RegQueryInfoKey(key_, NULL, 0, NULL, NULL, NULL, NULL,
                                  &count, NULL, NULL, NULL, NULL);
  if (result != ERROR_SUCCESS)
    return 0;

  return count;
}

bool RegistryValueIterator::Valid() const {
  return key_ != NULL && index_ >= 0;
}

void RegistryValueIterator::operator++() {
  --index_;
  Read();
}

bool RegistryValueIterator::Read() {
  if (Valid()) {
    DWORD capacity = static_cast<DWORD>(name_.capacity());
    DWORD name_size = capacity;
    // |value_size_| is in bytes. Reserve the last character for a NUL.
    value_size_ = static_cast<DWORD>((value_.size() - 1) * sizeof(wchar_t));
    LONG result = ::RegEnumValue(
        key_, index_, WriteInto(&name_, name_size), &name_size, NULL, &type_,
        reinterpret_cast<BYTE*>(vector_as_array(&value_)), &value_size_);

    if (result == ERROR_MORE_DATA) {
      // Registry key names are limited to 255 characters and fit within
      // MAX_PATH (which is 260) but registry value names can use up to 16,383
      // characters and the value itself is not limited
      // (from http://msdn.microsoft.com/en-us/library/windows/desktop/
      // ms724872(v=vs.85).aspx).
      // Resize the buffers and retry if their size caused the failure.
      DWORD value_size_in_wchars = to_wchar_size(value_size_);
      if (value_size_in_wchars + 1 > value_.size())
        value_.resize(value_size_in_wchars + 1, L'\0');
      value_size_ = static_cast<DWORD>((value_.size() - 1) * sizeof(wchar_t));
      name_size = name_size == capacity ? MAX_REGISTRY_NAME_SIZE : capacity;
      result = ::RegEnumValue(
          key_, index_, WriteInto(&name_, name_size), &name_size, NULL, &type_,
          reinterpret_cast<BYTE*>(vector_as_array(&value_)), &value_size_);
    }

    if (result == ERROR_SUCCESS) {
      DCHECK_LT(to_wchar_size(value_size_), value_.size());
      value_[to_wchar_size(value_size_)] = L'\0';
      return true;
    }
  }

  name_[0] = L'\0';
  value_[0] = L'\0';
  value_size_ = 0;
  return false;
}

// RegistryKeyIterator --------------------------------------------------------

RegistryKeyIterator::RegistryKeyIterator(HKEY root_key,
                                         const wchar_t* folder_key) {
  LONG result = RegOpenKeyEx(root_key, folder_key, 0, KEY_READ, &key_);
  if (result != ERROR_SUCCESS) {
    key_ = NULL;
  } else {
    DWORD count = 0;
    LONG result = ::RegQueryInfoKey(key_, NULL, 0, NULL, &count, NULL, NULL,
                                    NULL, NULL, NULL, NULL, NULL);

    if (result != ERROR_SUCCESS) {
      ::RegCloseKey(key_);
      key_ = NULL;
    } else {
      index_ = count - 1;
    }
  }

  Read();
}

RegistryKeyIterator::~RegistryKeyIterator() {
  if (key_)
    ::RegCloseKey(key_);
}

DWORD RegistryKeyIterator::SubkeyCount() const {
  DWORD count = 0;
  LONG result = ::RegQueryInfoKey(key_, NULL, 0, NULL, &count, NULL, NULL,
                                  NULL, NULL, NULL, NULL, NULL);
  if (result != ERROR_SUCCESS)
    return 0;

  return count;
}

bool RegistryKeyIterator::Valid() const {
  return key_ != NULL && index_ >= 0;
}

void RegistryKeyIterator::operator++() {
  --index_;
  Read();
}

bool RegistryKeyIterator::Read() {
  if (Valid()) {
    DWORD ncount = arraysize(name_);
    FILETIME written;
    LONG r = ::RegEnumKeyEx(key_, index_, name_, &ncount, NULL, NULL,
                            NULL, &written);
    if (ERROR_SUCCESS == r)
      return true;
  }

  name_[0] = '\0';
  return false;
}

}  // namespace win
}  // namespace base
