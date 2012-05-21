/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSAXLocator_h__
#define nsSAXLocator_h__

#include "nsISAXLocator.h"
#include "nsString.h"

#define NS_SAXLOCATOR_CONTRACTID "@mozilla.org/saxparser/locator;1"
#define NS_SAXLOCATOR_CLASSNAME "SAX Locator"
#define NS_SAXLOCATOR_CID  \
{/* {c1cd4045-846b-43bb-a95e-745a3d7b40e0}*/ \
0xc1cd4045, 0x846b, 0x43bb, \
{ 0xa9, 0x5e, 0x74, 0x5a, 0x3d, 0x7b, 0x40, 0xe0} }

class nsSAXLocator : public nsISAXLocator
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISAXLOCATOR

  nsSAXLocator(nsString& aPublicId,
               nsString& aSystemId,
               PRInt32 aLineNumber,
               PRInt32 aColumnNumber);

private:
  nsString mPublicId;
  nsString mSystemId;
  PRInt32 mLineNumber;
  PRInt32 mColumnNumber;
};

#endif //nsSAXLocator_h__
