/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/XULStore.h"
#include "nsCOMPtr.h"
#include "nsIXULStore.h"

namespace mozilla {

// The XULStore API is implemented in Rust and exposed to C++ via a set of
// C functions with the "xulstore_" prefix.  We declare them in this anonymous
// namespace to prevent C++ code outside this file from accessing them,
// as they are an internal implementation detail, and C++ code should use
// the mozilla::XULStore::* functions and mozilla::XULStoreIterator class
// declared in XULStore.h.
namespace {
extern "C" {
void xulstore_new_service(nsIXULStore** result);
nsresult xulstore_set_value(const nsAString* doc, const nsAString* id,
                            const nsAString* attr, const nsAString* value);
nsresult xulstore_has_value(const nsAString* doc, const nsAString* id,
                            const nsAString* attr, bool* has_value);
nsresult xulstore_get_value(const nsAString* doc, const nsAString* id,
                            const nsAString* attr, nsAString* value);
nsresult xulstore_remove_value(const nsAString* doc, const nsAString* id,
                               const nsAString* attr);
XULStoreIterator* xulstore_get_ids(const nsAString* doc, nsresult* result);
XULStoreIterator* xulstore_get_attrs(const nsAString* doc, const nsAString* id,
                                     nsresult* result);
bool xulstore_iter_has_more(const XULStoreIterator*);
nsresult xulstore_iter_get_next(XULStoreIterator*, nsAString* value);
void xulstore_iter_free(XULStoreIterator* iterator);
}

// A static reference to the nsIXULStore singleton that JS uses to access
// the store.  Retrieved via mozilla::XULStore::GetService().
static StaticRefPtr<nsIXULStore> sXULStore;
}  // namespace

bool XULStoreIterator::HasMore() const { return xulstore_iter_has_more(this); }

nsresult XULStoreIterator::GetNext(nsAString* item) {
  return xulstore_iter_get_next(this, item);
}

void DefaultDelete<XULStoreIterator>::operator()(XULStoreIterator* ptr) const {
  xulstore_iter_free(ptr);
}

namespace XULStore {
already_AddRefed<nsIXULStore> GetService() {
  nsCOMPtr<nsIXULStore> xulStore;

  if (sXULStore) {
    xulStore = sXULStore;
  } else {
    xulstore_new_service(getter_AddRefs(xulStore));
    sXULStore = xulStore;
    mozilla::ClearOnShutdown(&sXULStore);
  }

  return xulStore.forget();
}

nsresult SetValue(const nsAString& doc, const nsAString& id,
                  const nsAString& attr, const nsAString& value) {
  return xulstore_set_value(&doc, &id, &attr, &value);
}
nsresult HasValue(const nsAString& doc, const nsAString& id,
                  const nsAString& attr, bool& has_value) {
  return xulstore_has_value(&doc, &id, &attr, &has_value);
}
nsresult GetValue(const nsAString& doc, const nsAString& id,
                  const nsAString& attr, nsAString& value) {
  return xulstore_get_value(&doc, &id, &attr, &value);
}
nsresult RemoveValue(const nsAString& doc, const nsAString& id,
                     const nsAString& attr) {
  return xulstore_remove_value(&doc, &id, &attr);
}
nsresult GetIDs(const nsAString& doc, UniquePtr<XULStoreIterator>& iter) {
  // We assign the value of the iter here in C++ via a return value
  // rather than in the Rust function via an out parameter in order
  // to ensure that any old value is deleted, since the UniquePtr's
  // assignment operator won't delete the old value if the assignment
  // happens in Rust.
  nsresult result;
  iter.reset(xulstore_get_ids(&doc, &result));
  return result;
}
nsresult GetAttrs(const nsAString& doc, const nsAString& id,
                  UniquePtr<XULStoreIterator>& iter) {
  // We assign the value of the iter here in C++ via a return value
  // rather than in the Rust function via an out parameter in order
  // to ensure that any old value is deleted, since the UniquePtr's
  // assignment operator won't delete the old value if the assignment
  // happens in Rust.
  nsresult result;
  iter.reset(xulstore_get_attrs(&doc, &id, &result));
  return result;
}

};  // namespace XULStore
};  // namespace mozilla
