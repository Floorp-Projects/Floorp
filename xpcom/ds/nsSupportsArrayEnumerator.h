/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSupportsArrayEnumerator_h___
#define nsSupportsArrayEnumerator_h___

#include "nsIEnumerator.h"
#include "mozilla/Attributes.h"

class nsISupportsArray;

class nsSupportsArrayEnumerator MOZ_FINAL : public nsIBidirectionalEnumerator
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
  nsISupportsArray*     mArray;
  int32_t               mCursor;

};

#endif // __nsSupportsArrayEnumerator_h

