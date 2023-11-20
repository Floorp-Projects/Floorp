/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WinRegistry.h"
#include "nsThreadUtils.h"

namespace mozilla::widget::WinRegistry {

Key::Key(HKEY aParent, const nsString& aPath, KeyMode aMode, CreateFlag) {
  MOZ_ASSERT(aParent);
  DWORD disposition;
  ::RegCreateKeyExW(aParent, aPath.get(), 0, nullptr, REG_OPTION_NON_VOLATILE,
                    (REGSAM)aMode, nullptr, &mKey, &disposition);
}

Key::Key(HKEY aParent, const nsString& aPath, KeyMode aMode) {
  MOZ_ASSERT(aParent);
  ::RegOpenKeyExW(aParent, aPath.get(), 0, (REGSAM)aMode, &mKey);
}

uint32_t Key::GetChildCount() const {
  MOZ_ASSERT(mKey);
  DWORD result = 0;
  ::RegQueryInfoKeyW(mKey, nullptr, nullptr, nullptr, &result, nullptr, nullptr,
                     nullptr, nullptr, nullptr, nullptr, nullptr);
  return result;
}

bool Key::GetChildName(uint32_t aIndex, nsAString& aResult) const {
  MOZ_ASSERT(mKey);
  FILETIME lastWritten;

  wchar_t nameBuf[kMaxKeyNameLen + 1];
  DWORD nameLen = std::size(nameBuf);

  LONG rv = RegEnumKeyExW(mKey, aIndex, nameBuf, &nameLen, nullptr, nullptr,
                          nullptr, &lastWritten);
  if (rv != ERROR_SUCCESS) {
    return false;
  }
  aResult.Assign(nameBuf, nameLen);
  return true;
}

bool Key::RemoveChildKey(const nsString& aName) const {
  MOZ_ASSERT(mKey);
  return SUCCEEDED(RegDeleteKeyW(mKey, aName.get()));
}

uint32_t Key::GetValueCount() const {
  MOZ_ASSERT(mKey);
  DWORD result = 0;
  ::RegQueryInfoKeyW(mKey, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                     &result, nullptr, nullptr, nullptr, nullptr);
  return result;
}

bool Key::GetValueName(uint32_t aIndex, nsAString& aResult) const {
  MOZ_ASSERT(mKey);
  wchar_t nameBuf[kMaxValueNameLen + 1];
  DWORD nameLen = std::size(nameBuf);

  LONG rv = RegEnumValueW(mKey, aIndex, nameBuf, &nameLen, nullptr, nullptr,
                          nullptr, nullptr);
  if (rv != ERROR_SUCCESS) {
    return false;
  }
  aResult.Assign(nameBuf, nameLen);
  return true;
}

ValueType Key::GetValueType(const nsString& aName) const {
  MOZ_ASSERT(mKey);
  DWORD result;
  LONG rv =
      RegQueryValueExW(mKey, aName.get(), nullptr, &result, nullptr, nullptr);
  return SUCCEEDED(rv) ? ValueType(result) : ValueType::None;
}

bool Key::RemoveValue(const nsString& aName) const {
  MOZ_ASSERT(mKey);
  return SUCCEEDED(RegDeleteValueW(mKey, aName.get()));
}

Maybe<uint32_t> Key::GetValueAsDword(const nsString& aName) const {
  MOZ_ASSERT(mKey);
  DWORD type;
  DWORD value = 0;
  DWORD size = sizeof(DWORD);
  HRESULT rv = RegQueryValueExW(mKey, aName.get(), nullptr, &type,
                                (LPBYTE)&value, &size);
  if (FAILED(rv) || type != REG_DWORD) {
    return Nothing();
  }
  return Some(value);
}

bool Key::WriteValueAsDword(const nsString& aName, uint32_t aValue) {
  MOZ_ASSERT(mKey);
  return SUCCEEDED(RegSetValueExW(mKey, aName.get(), 0, REG_DWORD,
                                  (const BYTE*)&aValue, sizeof(aValue)));
}

Maybe<uint64_t> Key::GetValueAsQword(const nsString& aName) const {
  MOZ_ASSERT(mKey);
  DWORD type;
  uint64_t value = 0;
  DWORD size = sizeof(uint64_t);
  HRESULT rv = RegQueryValueExW(mKey, aName.get(), nullptr, &type,
                                (LPBYTE)&value, &size);
  if (FAILED(rv) || type != REG_QWORD) {
    return Nothing();
  }
  return Some(value);
}

bool Key::WriteValueAsQword(const nsString& aName, uint64_t aValue) {
  MOZ_ASSERT(mKey);
  return SUCCEEDED(RegSetValueExW(mKey, aName.get(), 0, REG_QWORD,
                                  (const BYTE*)&aValue, sizeof(aValue)));
}

bool Key::GetValueAsBinary(const nsString& aName,
                           nsTArray<uint8_t>& aResult) const {
  MOZ_ASSERT(mKey);
  DWORD type;
  DWORD size;
  LONG rv = RegQueryValueExW(mKey, aName.get(), nullptr, &type, nullptr, &size);
  if (FAILED(rv) || type != REG_BINARY) {
    return false;
  }
  if (!aResult.SetLength(size, fallible)) {
    return false;
  }
  rv = RegQueryValueExW(mKey, aName.get(), nullptr, nullptr, aResult.Elements(),
                        &size);
  return SUCCEEDED(rv);
}

Maybe<nsTArray<uint8_t>> Key::GetValueAsBinary(const nsString& aName) const {
  nsTArray<uint8_t> value;
  Maybe<nsTArray<uint8_t>> result;
  if (GetValueAsBinary(aName, value)) {
    result.emplace(std::move(value));
  }
  return result;
}

bool Key::WriteValueAsBinary(const nsString& aName,
                             Span<const uint8_t> aValue) {
  MOZ_ASSERT(mKey);
  return SUCCEEDED(RegSetValueExW(mKey, aName.get(), 0, REG_BINARY,
                                  (const BYTE*)aValue.data(), aValue.size()));
}

static bool IsStringType(DWORD aType, StringFlags aFlags) {
  switch (aType) {
    case REG_SZ:
      return bool(aFlags & StringFlags::Sz);
    case REG_EXPAND_SZ:
      return bool(aFlags & StringFlags::ExpandSz);
    case REG_MULTI_SZ:
      return bool(aFlags & StringFlags::LegacyMultiSz);
    default:
      return false;
  }
}

bool Key::GetValueAsString(const nsString& aName, nsString& aResult,
                           StringFlags aFlags) const {
  MOZ_ASSERT(mKey);
  DWORD type;
  DWORD size;
  LONG rv = RegQueryValueExW(mKey, aName.get(), nullptr, &type, nullptr, &size);
  if (FAILED(rv) || !IsStringType(type, aFlags)) {
    return false;
  }
  if (!size) {
    aResult.Truncate();
    return true;
  }
  // The buffer size must be a multiple of 2.
  if (NS_WARN_IF(size % 2 != 0)) {
    return false;
  }
  size_t resultLen = size / 2;
  {
    auto handleOrError =
        aResult.BulkWrite(resultLen, 0, /* aAllowShrinking = */ false);
    if (NS_WARN_IF(handleOrError.isErr())) {
      return false;
    }
    auto handle = handleOrError.unwrap();
    auto len = GetValueAsString(aName, {handle.Elements(), handle.Length() + 1},
                                aFlags & ~StringFlags::ExpandEnvironment);
    if (NS_WARN_IF(!len)) {
      return false;
    }
    handle.Finish(*len, /* aAllowShrinking = */ false);
    if (*len && !aResult.CharAt(*len - 1)) {
      // The string passed to us had a null terminator in the final
      // position.
      aResult.Truncate(*len - 1);
    }
  }
  if (type == REG_EXPAND_SZ && (aFlags & StringFlags::ExpandEnvironment)) {
    resultLen = ExpandEnvironmentStringsW(aResult.get(), nullptr, 0);
    if (resultLen > 1) {
      nsString expandedResult;
      // |resultLen| includes the terminating null character
      resultLen--;
      if (!expandedResult.SetLength(resultLen, fallible)) {
        return false;
      }
      resultLen = ExpandEnvironmentStringsW(aResult.get(), expandedResult.get(),
                                            resultLen + 1);
      if (resultLen <= 0) {
        return false;
      }
      aResult = std::move(expandedResult);
    } else if (resultLen == 1) {
      // It apparently expands to nothing (just a null terminator).
      resultLen = 0;
      aResult.Truncate();
    }
  }
  return true;
}

Maybe<nsString> Key::GetValueAsString(const nsString& aName,
                                      StringFlags aFlags) const {
  nsString value;
  Maybe<nsString> result;
  if (GetValueAsString(aName, value, aFlags)) {
    result.emplace(std::move(value));
  }
  return result;
}

Maybe<uint32_t> Key::GetValueAsString(const nsString& aName,
                                      Span<char16_t> aBuffer,
                                      StringFlags aFlags) const {
  MOZ_ASSERT(mKey);
  MOZ_ASSERT(aBuffer.Length(), "Empty buffer?");
  MOZ_ASSERT(!(aFlags & StringFlags::ExpandEnvironment),
             "Environment expansion not performed on a single buffer");

  DWORD size = aBuffer.LengthBytes();
  DWORD type;
  HRESULT rv = RegQueryValueExW(mKey, aName.get(), nullptr, &type,
                                (LPBYTE)aBuffer.data(), &size);
  if (FAILED(rv)) {
    return Nothing();
  }
  if (!IsStringType(type, aFlags)) {
    return Nothing();
  }
  uint32_t len = size ? size / sizeof(char16_t) - 1 : 0;
  aBuffer[len] = 0;
  return Some(len);
}

KeyWatcher::KeyWatcher(Key&& aKey,
                       nsISerialEventTarget* aTargetSerialEventTarget,
                       Callback&& aCallback)
    : mKey(std::move(aKey)),
      mEventTarget(aTargetSerialEventTarget),
      mCallback(std::move(aCallback)) {
  MOZ_ASSERT(mKey);
  MOZ_ASSERT(mEventTarget);
  MOZ_ASSERT(mCallback);
  mEvent = CreateEvent(nullptr, /* bManualReset = */ FALSE,
                       /* bInitialState = */ FALSE, nullptr);
  if (NS_WARN_IF(!mEvent)) {
    return;
  }

  if (NS_WARN_IF(!Register())) {
    return;
  }

  // The callback only dispatches to the relevant event target, so we can use
  // WT_EXECUTEINWAITTHREAD.
  RegisterWaitForSingleObject(&mWaitObject, mEvent, WatchCallback, this,
                              INFINITE, WT_EXECUTEINWAITTHREAD);
}

void KeyWatcher::WatchCallback(void* aContext, BOOLEAN) {
  auto* watcher = static_cast<KeyWatcher*>(aContext);
  watcher->Register();
  watcher->mEventTarget->Dispatch(
      NS_NewRunnableFunction("KeyWatcher callback", watcher->mCallback));
}

// As per the documentation in:
// https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regnotifychangekeyvalue
//
// This function detects a single change. After the caller receives a
// notification event, it should call the function again to receive the next
// notification.
bool KeyWatcher::Register() {
  MOZ_ASSERT(mEvent);
  DWORD flags = REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_ATTRIBUTES |
                REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_CHANGE_SECURITY |
                REG_NOTIFY_THREAD_AGNOSTIC;
  HRESULT rv =
      RegNotifyChangeKeyValue(mKey.RawKey(), /* bWatchSubtree = */ TRUE, flags,
                              mEvent, /* fAsynchronous = */ TRUE);
  return !NS_WARN_IF(FAILED(rv));
}

KeyWatcher::~KeyWatcher() {
  if (mWaitObject) {
    UnregisterWait(mWaitObject);
    CloseHandle(mWaitObject);
  }
  if (mEvent) {
    CloseHandle(mEvent);
  }
}

}  // namespace mozilla::widget::WinRegistry
