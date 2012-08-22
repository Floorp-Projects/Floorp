/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//------------------------------------------------------------------------

#ifndef nsRwsService_h__
#define nsRwsService_h__

#include "nsIRwsService.h"
#include "rws.h"

// e314efd1-f4ef-49e0-bd98-12d4e87a63a7
#define NS_RWSSERVICE_CID \
{ 0xe314efd1, 0xf4ef,0x49e0, { 0xbd, 0x98, 0x12, 0xd4, 0xe8, 0x7a, 0x63, 0xa7 } }

#define NS_RWSSERVICE_CONTRACTID "@mozilla.org/rwsos2;1"

//------------------------------------------------------------------------

NS_IMETHODIMP nsRwsServiceConstructor(nsISupports *aOuter, REFNSIID aIID,
                                      void **aResult);

//------------------------------------------------------------------------

class ExtCache;

class nsRwsService : public nsIRwsService, public nsIObserver
{
public:
  NS_DECL_NSIRWSSERVICE
  NS_DECL_NSIOBSERVER
  NS_DECL_ISUPPORTS

  nsRwsService();

private:
  ~nsRwsService();

protected:
  static nsresult RwsConvert(uint32_t type, uint32_t value, uint32_t *result);
  static nsresult RwsConvert(uint32_t type, uint32_t value, nsAString& result);

  ExtCache *mExtCache;
};

//------------------------------------------------------------------------

#endif // nsRwsService_h__
