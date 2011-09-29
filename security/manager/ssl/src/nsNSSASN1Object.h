/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Javier Delgadillo <javi@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
  NS_DECL_ISUPPORTS
  NS_DECL_NSIASN1SEQUENCE
  NS_DECL_NSIASN1OBJECT

  nsNSSASN1Sequence();
  virtual ~nsNSSASN1Sequence();
  /* additional members */
private:
  nsCOMPtr<nsIMutableArray> mASN1Objects;
  nsString mDisplayName;
  nsString mDisplayValue;
  PRUint32 mType;
  PRUint32 mTag;
  bool     mIsValidContainer;
  bool     mIsExpanded;
};

class nsNSSASN1PrintableItem : public nsIASN1PrintableItem
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIASN1PRINTABLEITEM
  NS_DECL_NSIASN1OBJECT

  nsNSSASN1PrintableItem();
  virtual ~nsNSSASN1PrintableItem();
  /* additional members */
private:
  nsString mDisplayName;
  nsString mValue;
  PRUint32 mType;
  PRUint32 mTag;
  unsigned char *mData;
  PRUint32       mLen;
};

nsresult CreateFromDER(unsigned char *data,
                       unsigned int   len,
                       nsIASN1Object **retval);
#endif //_NSSASN_H_
