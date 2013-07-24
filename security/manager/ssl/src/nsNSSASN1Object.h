/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _NSSASN_H_
#define _NSSASN_H_

#include "nscore.h"
#include "nsIX509Cert.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIASN1Sequence.h"
#include "nsIASN1PrintableItem.h"
#include "nsIMutableArray.h"

//
// Read comments in nsIX509Cert.idl for a description of the desired
// purpose for this ASN1 interface implementation.
//

class nsNSSASN1Sequence : public nsIASN1Sequence
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIASN1SEQUENCE
  NS_DECL_NSIASN1OBJECT

  nsNSSASN1Sequence();
  virtual ~nsNSSASN1Sequence();
  /* additional members */
private:
  nsCOMPtr<nsIMutableArray> mASN1Objects;
  nsString mDisplayName;
  nsString mDisplayValue;
  uint32_t mType;
  uint32_t mTag;
  bool     mIsValidContainer;
  bool     mIsExpanded;
};

class nsNSSASN1PrintableItem : public nsIASN1PrintableItem
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIASN1PRINTABLEITEM
  NS_DECL_NSIASN1OBJECT

  nsNSSASN1PrintableItem();
  virtual ~nsNSSASN1PrintableItem();
  /* additional members */
private:
  nsString mDisplayName;
  nsString mValue;
  uint32_t mType;
  uint32_t mTag;
  unsigned char *mData;
  uint32_t       mLen;
};

nsresult CreateFromDER(unsigned char *data,
                       unsigned int   len,
                       nsIASN1Object **retval);
#endif //_NSSASN_H_
