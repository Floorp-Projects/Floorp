/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_WinRegistry_h__
#define mozilla_widget_WinRegistry_h__

#include <windows.h>
#include <functional>
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/Maybe.h"
#include "mozilla/Span.h"

class nsISerialEventTarget;

namespace mozilla::widget::WinRegistry {

// According to MSDN, the following limits apply (in characters excluding room
// for terminating null character):
static constexpr size_t kMaxKeyNameLen = 255;
static constexpr size_t kMaxValueNameLen = 16383;

/// https://learn.microsoft.com/en-us/windows/win32/shell/regsam
enum class KeyMode : uint32_t {
  AllAccess = KEY_ALL_ACCESS,
  QueryValue = KEY_QUERY_VALUE,
  CreateLink = KEY_CREATE_LINK,
  CreateSubKey = KEY_CREATE_SUB_KEY,
  EnumerateSubkeys = KEY_ENUMERATE_SUB_KEYS,
  Execute = KEY_EXECUTE,
  Notify = KEY_NOTIFY,
  Read = KEY_READ,
  SetValue = KEY_SET_VALUE,
  Write = KEY_WRITE,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(KeyMode);

enum class StringFlags : uint32_t {
  // Whether to allow REG_SZ strings.
  Sz = 1 << 0,
  // Whether to read EXPAND_SZ strings.
  ExpandSz = 1 << 1,
  // Whether to treat MULTI_SZ values as strings. This is a historical
  // idiosyncrasy of the nsIWindowsRegKey, but most likely not what you want.
  LegacyMultiSz = 1 << 2,
  // Whether to expand environment variables in EXPAND_SZ values.
  // Only makes sense along with the ExpandSz variable.
  ExpandEnvironment = 1 << 3,
  // By default, only allow regular Sz, and don't perform environment expansion.
  Default = Sz,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(StringFlags);

// Convenience alias for legacy WinUtils callers, to preserve behavior.
// Chances are users of these flags could just look at Sz strings, tho.
static constexpr auto kLegacyWinUtilsStringFlags =
    StringFlags::Sz | StringFlags::ExpandSz;

// https://learn.microsoft.com/en-us/windows/win32/sysinfo/registry-value-types
enum class ValueType : uint32_t {
  Binary = REG_BINARY,
  Dword = REG_DWORD,
  ExpandSz = REG_EXPAND_SZ,
  Link = REG_LINK,
  MultiSz = REG_MULTI_SZ,
  None = REG_NONE,
  Qword = REG_QWORD,
  Sz = REG_SZ,
};

class Key {
 public:
  enum CreateFlag {
    Create,
  };

  Key() = default;
  Key(const Key&) = delete;
  Key(Key&& aOther) { std::swap(mKey, aOther.mKey); }

  Key& operator=(const Key&) = delete;
  Key& operator=(Key&& aOther) {
    std::swap(mKey, aOther.mKey);
    return *this;
  }

  Key(HKEY aParent, const nsString& aPath, KeyMode aMode, CreateFlag);
  Key(HKEY aParent, const nsString& aPath, KeyMode aMode);

  Key(const Key& aParent, const nsString& aPath, KeyMode aMode, CreateFlag)
      : Key(aParent.mKey, aPath, aMode, Create) {}
  Key(const Key& aParent, const nsString& aPath, KeyMode aMode)
      : Key(aParent.mKey, aPath, aMode) {}

  ~Key() {
    if (mKey) {
      ::RegCloseKey(mKey);
    }
  }

  explicit operator bool() const { return !!mKey; }

  uint32_t GetChildCount() const;
  [[nodiscard]] bool GetChildName(uint32_t aIndex, nsAString& aResult) const;
  [[nodiscard]] bool RemoveChildKey(const nsString& aName) const;

  uint32_t GetValueCount() const;
  [[nodiscard]] bool GetValueName(uint32_t aIndex, nsAString& aResult) const;
  ValueType GetValueType(const nsString& aName) const;
  bool RemoveValue(const nsString& aName) const;

  Maybe<uint32_t> GetValueAsDword(const nsString& aName) const;
  bool WriteValueAsDword(const nsString& aName, uint32_t aValue);

  Maybe<uint64_t> GetValueAsQword(const nsString& aName) const;
  [[nodiscard]] bool WriteValueAsQword(const nsString& aName, uint64_t aValue);

  [[nodiscard]] bool GetValueAsBinary(const nsString& aName,
                                      nsTArray<uint8_t>&) const;
  Maybe<nsTArray<uint8_t>> GetValueAsBinary(const nsString& aName) const;
  [[nodiscard]] bool WriteValueAsBinary(const nsString& aName,
                                        Span<const uint8_t> aValue);

  [[nodiscard]] bool GetValueAsString(const nsString& aName, nsString& aResult,
                                      StringFlags = StringFlags::Default) const;
  Maybe<nsString> GetValueAsString(const nsString& aName,
                                   StringFlags = StringFlags::Default) const;
  // Reads a string value into a buffer. Returns Some(length) if the string is
  // read fully, in which case the passed memory region contains a
  // null-terminated string.
  // Doesn't perform environment expansion (and asserts if you pass the
  // ExpandEnvironment flag).
  [[nodiscard]] Maybe<uint32_t> GetValueAsString(
      const nsString& aName, Span<char16_t>,
      StringFlags = StringFlags::Default) const;
  [[nodiscard]] Maybe<uint32_t> GetValueAsString(
      const nsString& aName, Span<wchar_t> aBuffer,
      StringFlags aFlags = StringFlags::Default) const {
    return GetValueAsString(
        aName, Span<char16_t>((char16_t*)aBuffer.data(), aBuffer.Length()),
        aFlags);
  }

  [[nodiscard]] bool WriteValueAsString(const nsString& aName,
                                        const nsString& aValue) {
    MOZ_ASSERT(mKey);
    return SUCCEEDED(RegSetValueExW(mKey, aName.get(), 0, REG_SZ,
                                    (const BYTE*)aValue.get(),
                                    (aValue.Length() + 1) * sizeof(char16_t)));
  }

  HKEY RawKey() const { return mKey; }

 private:
  HKEY mKey = nullptr;
};

inline bool HasKey(HKEY aRootKey, const nsString& aKeyName) {
  return !!Key(aRootKey, aKeyName, KeyMode::Read);
}

// Returns a single string value from the registry into a buffer.
[[nodiscard]] inline bool GetString(HKEY aRootKey, const nsString& aKeyName,
                                    const nsString& aValueName,
                                    Span<char16_t> aBuffer,
                                    StringFlags aFlags = StringFlags::Default) {
  Key k(aRootKey, aKeyName, KeyMode::QueryValue);
  return k && k.GetValueAsString(aValueName, aBuffer, aFlags);
}
[[nodiscard]] inline bool GetString(HKEY aRootKey, const nsString& aKeyName,
                                    const nsString& aValueName,
                                    Span<wchar_t> aBuffer,
                                    StringFlags aFlags = StringFlags::Default) {
  return GetString(aRootKey, aKeyName, aValueName,
                   Span<char16_t>((char16_t*)aBuffer.data(), aBuffer.Length()),
                   aFlags);
}
[[nodiscard]] inline bool GetString(HKEY aRootKey, const nsString& aKeyName,
                                    const nsString& aValueName,
                                    nsString& aBuffer,
                                    StringFlags aFlags = StringFlags::Default) {
  Key k(aRootKey, aKeyName, KeyMode::QueryValue);
  return k && k.GetValueAsString(aValueName, aBuffer, aFlags);
}
inline Maybe<nsString> GetString(HKEY aRootKey, const nsString& aKeyName,
                                 const nsString& aValueName,
                                 StringFlags aFlags = StringFlags::Default) {
  Key k(aRootKey, aKeyName, KeyMode::QueryValue);
  if (!k) {
    return Nothing();
  }
  return k.GetValueAsString(aValueName, aFlags);
}

class KeyWatcher final {
 public:
  using Callback = std::function<void()>;

  KeyWatcher(const KeyWatcher&) = delete;

  const Key& GetKey() const { return mKey; }

  // Start watching a key. The watching is recursive (the whole key subtree is
  // watched), and the callback is executed every time the key or any of its
  // descendants change until the watcher is destroyed.
  //
  // @param aKey the key to watch. Must have been opened with the
  //             KeyMode::Notify flag.
  // @param aTargetSerialEventTarget the target event target to dispatch the
  //        callback to.
  // @param aCallback the closure to run every time that registry key changes.
  KeyWatcher(Key&& aKey, nsISerialEventTarget* aTargetSerialEventTarget,
             Callback&& aCallback);

  ~KeyWatcher();

 private:
  static void CALLBACK WatchCallback(void* aContext, BOOLEAN);
  bool Register();

  Key mKey;
  nsCOMPtr<nsISerialEventTarget> mEventTarget;
  Callback mCallback;
  HANDLE mEvent = nullptr;
  HANDLE mWaitObject = nullptr;
};

}  // namespace mozilla::widget::WinRegistry

#endif
