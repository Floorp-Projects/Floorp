/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 *  Javier Delgadillo <javi@netscape.com>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
#ifndef _NSSASNOUTLINER_H_
#define _NSSASNOUTLINER_H_

#include "nscore.h"
#include "nsIX509Cert.h"
#include "nsIASN1Outliner.h"
#include "nsIOutlinerView.h"
#include "nsIOutlinerBoxObject.h"
#include "nsIOutlinerSelection.h"
#include "nsCOMPtr.h"

//4bfaa9f0-1dd2-11b2-afae-a82cbaa0b606
#define NS_NSSASN1OUTINER_CID  {             \
   0x4bfaa9f0,                               \
   0x1dd2,                                   \
   0x11b2,                                   \
   {0xaf,0xae,0xa8,0x2c,0xba,0xa0,0xb6,0x06} \
  }
  

class nsNSSASN1Outliner : public nsIASN1Outliner
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIASN1OUTLINER
  NS_DECL_NSIOUTLINERVIEW
  
  nsNSSASN1Outliner();
  virtual ~nsNSSASN1Outliner();
protected:
  PRUint32 CountNumberOfVisibleRows(nsIASN1Object *asn1Object);
  nsresult GetASN1ObjectAtIndex(PRUint32 index, nsIASN1Object *sourceObject,
                                nsIASN1Object **retval);
  PRInt32 GetParentOfObjectAtIndex(PRUint32 index, nsIASN1Object *sourceObject);
  PRInt32 GetLevelsTilIndex(PRUint32 index, nsIASN1Object *sourceObject);
  nsCOMPtr<nsIASN1Object> mASN1Object;
  nsCOMPtr<nsIOutlinerSelection> mSelection;
  nsCOMPtr<nsIOutlinerBoxObject> mOutliner;
};
#endif //_NSSASNOUTLINER_H_
