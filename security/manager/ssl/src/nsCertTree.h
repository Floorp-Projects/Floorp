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

#ifndef _NS_CERTTREE_H_
#define _NS_CERTTREE_H_

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsICertTree.h"
#include "nsITreeView.h"
#include "nsITreeBoxObject.h"
#include "nsITreeSelection.h"
#include "nsISupportsArray.h"
#include "pldhash.h"
#include "nsIX509CertDB.h"

typedef struct treeArrayElStr treeArrayEl;

struct CompareCacheHashEntry : PLDHashEntryHdr {
  CompareCacheHashEntry();

  enum { max_criterions = 3 };

  void *key; // no ownership
  PRPackedBool mCritInit[max_criterions];
  nsXPIDLString mCrit[max_criterions];
};

class nsCertTree : public nsICertTree
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICERTTREE
  NS_DECL_NSITREEVIEW

  nsCertTree();
  virtual ~nsCertTree();

  enum sortCriterion { sort_IssuerOrg, sort_Org, sort_Token, 
    sort_CommonName, sort_IssuedDateDescending, sort_Email, sort_None };

protected:
  nsresult InitCompareHash();
  void ClearCompareHash();
  void RemoveCacheEntry(void *key);

  typedef int (*nsCertCompareFunc)(void *, nsIX509Cert *a, nsIX509Cert *b);

  static CompareCacheHashEntry *getCacheEntry(void *cache, void *aCert);
  static void CmpInitCriterion(nsIX509Cert *cert, CompareCacheHashEntry *entry,
                               sortCriterion crit, PRInt32 level);
  static PRInt32 CmpByCrit(nsIX509Cert *a, CompareCacheHashEntry *ace, 
                           nsIX509Cert *b, CompareCacheHashEntry *bce, 
                           sortCriterion crit, PRInt32 level);
  static PRInt32 CmpBy(void *cache, nsIX509Cert *a, nsIX509Cert *b, 
                       sortCriterion c0, sortCriterion c1, sortCriterion c2);
  static PRInt32 CmpCACert(void *cache, nsIX509Cert *a, nsIX509Cert *b);
  static PRInt32 CmpWebSiteCert(void *cache, nsIX509Cert *a, nsIX509Cert *b);
  static PRInt32 CmpUserCert(void *cache, nsIX509Cert *a, nsIX509Cert *b);
  static PRInt32 CmpEmailCert(void *cache, nsIX509Cert *a, nsIX509Cert *b);
  nsCertCompareFunc GetCompareFuncFromCertType(PRUint32 aType);
  PRInt32 CountOrganizations();

  PRBool GetCertsByType(PRUint32 aType, nsCertCompareFunc aCertCmpFn,
                        void *aCertCmpFnArg, nsISupportsArray **_certs);

  PRBool GetCertsByTypeFromCache(nsINSSCertCache *aCache, PRUint32 aType,
                                 nsCertCompareFunc aCertCmpFn, void *aCertCmpFnArg,
                                 nsISupportsArray **_certs);
private:
  nsCOMPtr<nsISupportsArray>      mCertArray;
  nsCOMPtr<nsITreeBoxObject>  mTree;
  nsCOMPtr<nsITreeSelection>  mSelection;
  treeArrayEl                *mTreeArray;
  PRInt32                         mNumOrgs;
  PRInt32                         mNumRows;
  PLDHashTable mCompareCache;
  nsCOMPtr<nsINSSComponent> mNSSComponent;

  treeArrayEl *GetThreadDescAtIndex(PRInt32 _index);
  nsIX509Cert *GetCertAtIndex(PRInt32 _index);

  void FreeCertArray();
  nsresult UpdateUIContents();

  PRBool GetCertsByTypeFromCertList(CERTCertList *aCertList,
                                    PRUint32 aType,
                                    nsCertCompareFunc  aCertCmpFn,
                                    void              *aCertCmpFnArg,
                                    nsISupportsArray **_certs);

#ifdef DEBUG_CERT_TREE
  /* for debugging purposes */
  void dumpMap();
#endif
};

#endif /* _NS_CERTTREE_H_ */

