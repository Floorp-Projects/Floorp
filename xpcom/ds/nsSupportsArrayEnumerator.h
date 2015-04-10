/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSupportsArrayEnumerator_h___
#define nsSupportsArrayEnumerator_h___

#include "nsCOMPtr.h"
#include "nsIEnumerator.h"
#include "mozilla/Attributes.h"

class nsISupportsArray;

class nsSupportsArrayEnumerator final : public nsIBidirectionalEnumerator
{
public:
  NS_DECL_ISUPPORTS

  explicit nsSupportsArrayEnumerator(nsISupportsArray* aArray);

  // nsIEnumerator methods:
  NS_DECL_NSIENUMERATOR

  // nsIBidirectionalEnumerator methods:
  NS_DECL_NSIBIDIRECTIONALENUMERATOR

private:
  ~nsSupportsArrayEnumerator();

protected:
  nsCOMPtr<nsISupportsArray> mArray;
  int32_t               mCursor;

};

#endif // __nsSupportsArrayEnumerator_h

