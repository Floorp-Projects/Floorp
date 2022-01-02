/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStoreRestoreData_h
#define mozilla_dom_SessionStoreRestoreData_h

#include "mozilla/Tuple.h"
#include "mozilla/dom/sessionstore/SessionStoreTypes.h"
#include "mozilla/dom/SessionStoreData.h"
#include "nsISessionStoreRestoreData.h"

namespace mozilla {
namespace dom {

#define NS_SESSIONSTORERESTOREDATA_IID               \
  {                                                  \
    0x88800c5b, 0xe142, 0x4ac6, {                    \
      0x91, 0x52, 0xca, 0xbd, 0x3b, 0x74, 0xb9, 0xf8 \
    }                                                \
  }

class SessionStoreRestoreData final : public nsISessionStoreRestoreData {
 public:
  SessionStoreRestoreData() = default;
  bool IsEmpty();
  SessionStoreRestoreData* FindDataForChild(BrowsingContext* aContext);
  bool CanRestoreInto(nsIURI* aDocumentURI);
  bool RestoreInto(RefPtr<BrowsingContext> aContext);

  NS_DECL_ISUPPORTS
  NS_DECL_NSISESSIONSTORERESTOREDATA
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_SESSIONSTORERESTOREDATA_IID)

  struct Entry {
    sessionstore::FormEntry mData;
    bool mIsXPath;
  };

 private:
  virtual ~SessionStoreRestoreData() = default;
  void AddFormEntry(bool aIsXPath, const nsAString& aIdOrXPath,
                    sessionstore::FormEntryValue aValue);

  nsCString mScroll;
  nsCOMPtr<nsIURI> mURI;
  nsString mInnerHTML;
  nsTArray<Entry> mEntries;
  nsTArray<RefPtr<SessionStoreRestoreData>> mChildren;

  friend struct mozilla::ipc::IPDLParamTraits<SessionStoreRestoreData*>;
};

NS_DEFINE_STATIC_IID_ACCESSOR(SessionStoreRestoreData,
                              NS_SESSIONSTORERESTOREDATA_IID)

}  // namespace dom
}  // namespace mozilla

#endif
