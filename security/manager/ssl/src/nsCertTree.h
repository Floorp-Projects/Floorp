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
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 *  Ian McGreer <mcgreer@netscape.com>
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
 *
 */

#ifndef _NS_CERTOUTLINER_H_
#define _NS_CERTOUTLINER_H_

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsICertOutliner.h"
#include "nsIOutlinerView.h"
#include "nsIOutlinerBoxObject.h"
#include "nsIOutlinerSelection.h"
#include "nsISupportsArray.h"

typedef struct outlinerArrayElStr outlinerArrayEl;

class nsCertOutliner : public nsICertOutliner
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICERTOUTLINER
  NS_DECL_NSIOUTLINERVIEW

  nsCertOutliner();
  virtual ~nsCertOutliner();

protected:
  static PRInt32 CmpByToken(nsIX509Cert *a, nsIX509Cert *b);
  static PRInt32 CmpByOrg(nsIX509Cert *a, nsIX509Cert *b);
  static PRInt32 CmpByName(nsIX509Cert *a, nsIX509Cert *b);
  static PRInt32 CmpByTok_Org_Name(nsIX509Cert *a, nsIX509Cert *b);
  PRInt32 CountOrganizations();

private:
  nsCOMPtr<nsISupportsArray>      mCertArray;
  nsCOMPtr<nsIOutlinerBoxObject>  mOutliner;
  nsCOMPtr<nsIOutlinerSelection>  mSelection;
  outlinerArrayEl                *mOutlinerArray;
  PRInt32                         mNumOrgs;
  PRInt32                         mNumRows;

  outlinerArrayEl *GetThreadDescAtIndex(PRInt32 _index);
  nsIX509Cert *GetCertAtIndex(PRInt32 _index);

  void FreeCertArray();

#ifdef DEBUG_CERT_OUTLINER
  /* for debugging purposes */
  void dumpMap();
#endif
};

#endif /* _NS_CERTOUTLINER_H_ */

