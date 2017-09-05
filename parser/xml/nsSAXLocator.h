/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSAXLocator_h__
#define nsSAXLocator_h__

#include "nsISAXLocator.h"
#include "nsString.h"
#include "mozilla/Attributes.h"

class nsSAXLocator final : public nsISAXLocator
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISAXLOCATOR

  nsSAXLocator(nsString& aPublicId,
               nsString& aSystemId,
               int32_t aLineNumber,
               int32_t aColumnNumber);

private:
  ~nsSAXLocator() {}

  nsString mPublicId;
  nsString mSystemId;
  int32_t mLineNumber;
  int32_t mColumnNumber;
};

#endif //nsSAXLocator_h__
