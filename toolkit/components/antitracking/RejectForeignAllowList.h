/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RejectForeignAllowList_h
#define mozilla_RejectForeignAllowList_h

#include "nsIUrlClassifierExceptionListService.h"

class nsIHttpChannel;
class nsIURI;

namespace mozilla {

namespace dom {
class Document;
}

class RejectForeignAllowList final
    : public nsIUrlClassifierExceptionListObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIEREXCEPTIONLISTOBSERVER

  static bool Check(dom::Document* aDocument);
  static bool Check(nsIHttpChannel* aChannel);

 private:
  static RejectForeignAllowList* GetOrCreate();

  RejectForeignAllowList();
  ~RejectForeignAllowList();

  bool CheckInternal(nsIURI* aURI);

  nsCString mList;
};

}  // namespace mozilla

#endif  // mozilla_RejectForeignAllowList_h
