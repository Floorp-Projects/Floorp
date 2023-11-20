/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include "nsCOMPtr.h"
#include "nsWindowsRegKey.h"
#include "mozilla/RefPtr.h"
#include "mozilla/widget/WinRegistry.h"

//-----------------------------------------------------------------------------

using namespace mozilla::widget;

class nsWindowsRegKey final : public nsIWindowsRegKey {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWINDOWSREGKEY

  nsWindowsRegKey() = default;

 private:
  ~nsWindowsRegKey() = default;

  WinRegistry::Key mKey;
};

NS_IMPL_ISUPPORTS(nsWindowsRegKey, nsIWindowsRegKey)

NS_IMETHODIMP
nsWindowsRegKey::Close() {
  mKey = WinRegistry::Key();
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::Open(uint32_t aRootKey, const nsAString& aPath,
                      uint32_t aMode) {
  mKey = WinRegistry::Key((HKEY)(uintptr_t)(aRootKey), PromiseFlatString(aPath),
                          WinRegistry::KeyMode(aMode));
  return mKey ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::Create(uint32_t aRootKey, const nsAString& aPath,
                        uint32_t aMode) {
  mKey =
      WinRegistry::Key((HKEY)(uintptr_t)(aRootKey), PromiseFlatString(aPath),
                       WinRegistry::KeyMode(aMode), WinRegistry::Key::Create);
  return mKey ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::OpenChild(const nsAString& aPath, uint32_t aMode,
                           nsIWindowsRegKey** aResult) {
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<nsWindowsRegKey> child = new nsWindowsRegKey();
  child->mKey = WinRegistry::Key(mKey, PromiseFlatString(aPath),
                                 WinRegistry::KeyMode(aMode));
  if (!child->mKey) {
    return NS_ERROR_FAILURE;
  }
  child.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::CreateChild(const nsAString& aPath, uint32_t aMode,
                             nsIWindowsRegKey** aResult) {
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<nsWindowsRegKey> child = new nsWindowsRegKey();
  child->mKey =
      WinRegistry::Key(mKey, PromiseFlatString(aPath),
                       WinRegistry::KeyMode(aMode), WinRegistry::Key::Create);
  if (!child->mKey) {
    return NS_ERROR_FAILURE;
  }
  child.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::GetChildCount(uint32_t* aResult) {
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  *aResult = mKey.GetChildCount();
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::GetChildName(uint32_t aIndex, nsAString& aResult) {
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return mKey.GetChildName(aIndex, aResult) ? NS_OK : NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsWindowsRegKey::HasChild(const nsAString& aName, bool* aResult) {
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  // Check for the existence of a child key by opening the key with minimal
  // rights.  Perhaps there is a more efficient way to do this?
  *aResult = !!WinRegistry::Key(mKey, PromiseFlatString(aName),
                                WinRegistry::KeyMode::Read);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::GetValueCount(uint32_t* aResult) {
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  *aResult = mKey.GetValueCount();
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::GetValueName(uint32_t aIndex, nsAString& aResult) {
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return mKey.GetValueName(aIndex, aResult) ? NS_OK : NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsWindowsRegKey::HasValue(const nsAString& aName, bool* aResult) {
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  *aResult = mKey.GetValueType(PromiseFlatString(aName)) !=
             WinRegistry::ValueType::None;
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::RemoveChild(const nsAString& aName) {
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return mKey.RemoveChildKey(PromiseFlatString(aName)) ? NS_OK
                                                       : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::RemoveValue(const nsAString& aName) {
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return mKey.RemoveValue(PromiseFlatString(aName)) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::GetValueType(const nsAString& aName, uint32_t* aResult) {
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  *aResult = uint32_t(mKey.GetValueType(PromiseFlatString(aName)));
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::ReadStringValue(const nsAString& aName, nsAString& aResult) {
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  constexpr auto kFlags = WinRegistry::StringFlags::Sz |
                          WinRegistry::StringFlags::ExpandSz |
                          WinRegistry::StringFlags::LegacyMultiSz |
                          WinRegistry::StringFlags::ExpandEnvironment;
  auto value = mKey.GetValueAsString(PromiseFlatString(aName), kFlags);
  if (!value) {
    return NS_ERROR_FAILURE;
  }
  aResult = value.extract();
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::ReadIntValue(const nsAString& aName, uint32_t* aResult) {
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  auto value = mKey.GetValueAsDword(PromiseFlatString(aName));
  if (!value) {
    return NS_ERROR_FAILURE;
  }
  *aResult = value.extract();
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::ReadInt64Value(const nsAString& aName, uint64_t* aResult) {
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  auto value = mKey.GetValueAsQword(PromiseFlatString(aName));
  if (!value) {
    return NS_ERROR_FAILURE;
  }
  *aResult = value.extract();
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::ReadBinaryValue(const nsAString& aName, nsACString& aResult) {
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  auto value = mKey.GetValueAsBinary(PromiseFlatString(aName));
  if (!value) {
    return NS_ERROR_FAILURE;
  }
  nsTArray<uint8_t> data = value.extract();
  if (!aResult.Assign((const char*)data.Elements(), data.Length(),
                      mozilla::fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsRegKey::WriteStringValue(const nsAString& aName,
                                  const nsAString& aValue) {
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return mKey.WriteValueAsString(PromiseFlatString(aName),
                                 PromiseFlatString(aValue))
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::WriteIntValue(const nsAString& aName, uint32_t aValue) {
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return mKey.WriteValueAsDword(PromiseFlatString(aName), aValue)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::WriteInt64Value(const nsAString& aName, uint64_t aValue) {
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return mKey.WriteValueAsQword(PromiseFlatString(aName), aValue)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsRegKey::WriteBinaryValue(const nsAString& aName,
                                  const nsACString& aValue) {
  if (NS_WARN_IF(!mKey)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return mKey.WriteValueAsBinary(PromiseFlatString(aName), aValue)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

//-----------------------------------------------------------------------------

void NS_NewWindowsRegKey(nsIWindowsRegKey** aResult) {
  RefPtr<nsWindowsRegKey> key = new nsWindowsRegKey();
  key.forget(aResult);
}

//-----------------------------------------------------------------------------

nsresult nsWindowsRegKeyConstructor(const nsIID& aIID, void** aResult) {
  nsCOMPtr<nsIWindowsRegKey> key;
  NS_NewWindowsRegKey(getter_AddRefs(key));
  return key->QueryInterface(aIID, aResult);
}
