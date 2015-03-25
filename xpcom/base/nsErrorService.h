/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsErrorService_h__
#define nsErrorService_h__

#include "mozilla/Attributes.h"

#include "nsIErrorService.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"

class nsErrorService final : public nsIErrorService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIERRORSERVICE

  nsErrorService()
  {
  }

  static nsresult
  Create(nsISupports* aOuter, const nsIID& aIID, void** aInstancePtr);

private:
  ~nsErrorService()
  {
  }

  nsClassHashtable<nsUint32HashKey, nsCString> mErrorStringBundleURLMap;
};

#endif // nsErrorService_h__
