/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/*
 * This file declares the XULStore API for C++ via the mozilla::XULStore
 * namespace and the mozilla::XULStoreIterator class.  It also declares
 * the mozilla::XULStore::GetService() function that the component manager
 * uses to instantiate and retrieve the nsIXULStore singleton.
 */

#ifndef mozilla_XULStore_h
#define mozilla_XULStore_h

#include "nsIXULStore.h"

namespace mozilla {
class XULStoreIterator final {
 public:
  bool HasMore() const;
  nsresult GetNext(nsAString* item);

 private:
  XULStoreIterator() = delete;
  XULStoreIterator(const XULStoreIterator&) = delete;
  XULStoreIterator& operator=(const XULStoreIterator&) = delete;
  ~XULStoreIterator() = delete;
};

template <>
class DefaultDelete<XULStoreIterator> {
 public:
  void operator()(XULStoreIterator* ptr) const;
};

namespace XULStore {
// Instantiates and retrieves the nsIXULStore singleton that JS uses to access
// the store.  C++ code should use the mozilla::XULStore::* functions instead.
already_AddRefed<nsIXULStore> GetService();

nsresult SetValue(const nsAString& doc, const nsAString& id,
                  const nsAString& attr, const nsAString& value);
nsresult HasValue(const nsAString& doc, const nsAString& id,
                  const nsAString& attr, bool& has_value);
nsresult GetValue(const nsAString& doc, const nsAString& id,
                  const nsAString& attr, nsAString& value);
nsresult RemoveValue(const nsAString& doc, const nsAString& id,
                     const nsAString& attr);
nsresult GetIDs(const nsAString& doc, UniquePtr<XULStoreIterator>& iter);
nsresult GetAttrs(const nsAString& doc, const nsAString& id,
                  UniquePtr<XULStoreIterator>& iter);
};  // namespace XULStore
};  // namespace mozilla

#endif  // mozilla_XULStore_h
