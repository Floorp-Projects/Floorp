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

#include "nsNSSComponent.h" // for PIPNSS string bundle calls.
#include "nsCertOutliner.h"
#include "nsIX509Cert.h"
#include "nsIX509CertDB.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"

#include "prlog.h"
#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

// outlinerArrayElStr
//
// structure used to hold map of outliner.  Each thread (an organization
// field from a cert) has an element in the array.  The numChildren field
// stores the number of certs corresponding to that thread.
struct outlinerArrayElStr {
  PRUnichar *orgName;     /* heading for thread                   */
  PRBool     open;        /* toggle open state for thread         */
  PRInt32    certIndex;   /* index into cert array for 1st cert   */
  PRInt32    numChildren; /* number of chidren (certs) for thread */
};

NS_IMPL_ISUPPORTS2(nsCertOutliner, nsICertOutliner, nsIOutlinerView)

nsCertOutliner::nsCertOutliner() : mOutlinerArray(NULL)
{
  NS_INIT_ISUPPORTS();
}

nsCertOutliner::~nsCertOutliner()
{
  if (mOutlinerArray)
    nsMemory::Free(mOutlinerArray);
}

void
nsCertOutliner::FreeCertArray()
{
  if (mCertArray) {
    PRUint32 count;
    nsresult rv = mCertArray->Count(&count);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
    PRInt32 i;
    for (i = count - 1; i >= 0; i--)
      mCertArray->RemoveElementAt(i);
  }
  mCertArray = nsnull;
}

// CmpByToken
//
// Compare two certificate by their token name.  Returns -1, 0, 1 as
// in strcmp.  No token name (null) is treated as <.
PRInt32
nsCertOutliner::CmpByToken(nsIX509Cert *a, nsIX509Cert *b)
{
  PRInt32 cmp1;
  nsXPIDLString aTok, bTok;
  a->GetTokenName(getter_Copies(aTok));
  b->GetTokenName(getter_Copies(bTok));
  if (aTok != nsnull && bTok != nsnull) {
    nsAutoString aStr(aTok);
    cmp1 = aStr.CompareWithConversion(bTok);
  } else {
    cmp1 = (aTok == nsnull) ? -1 : 1;
  }
  return cmp1;
}

// CmpByIssuerOrg
//
// Compare two certificates by their O= field.  Returns -1, 0, 1 as
// in strcmp.  No organization (null) is treated as <.
PRInt32
nsCertOutliner::CmpByIssuerOrg(nsIX509Cert *a, nsIX509Cert *b)
{
  PRInt32 cmp1;
  nsXPIDLString aOrg, bOrg;
  a->GetIssuerOrganization(getter_Copies(aOrg));
  b->GetIssuerOrganization(getter_Copies(bOrg));
  if (aOrg != nsnull && bOrg != nsnull) {
    nsAutoString aStr(aOrg);
    cmp1 = aStr.CompareWithConversion(bOrg);
  } else {
    cmp1 = (aOrg == nsnull) ? -1 : 1;
  }
  return cmp1;
}

// CmpByName
//
// Compare two certificates by their CN= field.  Returns -1, 0, 1 as
// in strcmp.  No common name (null) is treated as <.
PRInt32
nsCertOutliner::CmpByName(nsIX509Cert *a, nsIX509Cert *b)
{
  PRInt32 cmp1;
  nsXPIDLString aName, bName;
  a->GetOrganization(getter_Copies(aName));
  b->GetOrganization(getter_Copies(bName));
  if (aName != nsnull && bName != nsnull) {
    nsAutoString aStr(aName);
    cmp1 = aStr.CompareWithConversion(bName);
  } else {
    cmp1 = (aName == nsnull) ? -1 : 1;
  }
  return cmp1;
}

// CmpByTok_IssuerOrg_Name
//
// Compare two certificates by token name, issuer organization, 
// and common name, in that order.  Used to sort cert list.
PRInt32
nsCertOutliner::CmpByTok_IssuerOrg_Name(nsIX509Cert *a, nsIX509Cert *b)
{
  PRInt32 cmp;
  cmp = CmpByToken(a, b);
  if (cmp != 0) return cmp;
  cmp = CmpByIssuerOrg(a, b);
  if (cmp != 0) return cmp;
  return CmpByName(a, b);
}

// CountOrganizations
//
// Count the number of different organizations encountered in the cert
// list.  Note that the same organization of a different token is counted
// seperately.
PRInt32
nsCertOutliner::CountOrganizations()
{
  PRUint32 i, certCount;
  nsresult rv = mCertArray->Count(&certCount);
  if (NS_FAILED(rv)) return -1;
  if (certCount == 0) return 0;
  nsCOMPtr<nsISupports> isupport = dont_AddRef(mCertArray->ElementAt(0));
  nsCOMPtr<nsIX509Cert> orgCert = do_QueryInterface(isupport);
  nsCOMPtr<nsIX509Cert> nextCert = nsnull;
  PRInt32 orgCount = 1;
  for (i=1; i<certCount; i++) {
    isupport = dont_AddRef(mCertArray->ElementAt(i));
    nextCert = do_QueryInterface(isupport);
    if (!(CmpByToken(orgCert, nextCert) == 0 &&
          CmpByIssuerOrg(orgCert, nextCert) == 0)) {
      orgCert = nextCert;
      orgCount++;
    }
  }
  return orgCount;
}

// GetThreadDescAtIndex
//
// If the row at index is an organization thread, return the collection
// associated with that thread.  Otherwise, return null.
outlinerArrayEl *
nsCertOutliner::GetThreadDescAtIndex(PRInt32 index)
{
  int i, idx=0;
  if (index < 0) return nsnull;
  for (i=0; i<mNumOrgs; i++) {
    if (index == idx) {
      return &mOutlinerArray[i];
    }
    if (mOutlinerArray[i].open == PR_FALSE) {
      idx++;
    } else {
      idx += mOutlinerArray[i].numChildren + 1;
    }
    if (idx > index) break;
  }
  return nsnull;
}

//  GetCertAtIndex
//
//  If the row at index is a cert, return that cert.  Otherwise, return null.
nsIX509Cert *
nsCertOutliner::GetCertAtIndex(PRInt32 index)
{
  int i, idx = 0, cIndex = 0, nc;
  nsIX509Cert *rawPtr = nsnull;
  if (index < 0) return nsnull;
  // Loop over the threads
  for (i=0; i<mNumOrgs; i++) {
    if (index == idx) return nsnull; // index is for thread
    idx++; // get past the thread
    nc = (mOutlinerArray[i].open) ? mOutlinerArray[i].numChildren : 0;
    if (index < idx + nc) { // cert is within range of this thread
      PRInt32 certIndex = cIndex + index - idx;
      nsCOMPtr<nsISupports> isupport = 
                             dont_AddRef(mCertArray->ElementAt(certIndex));
      nsCOMPtr<nsIX509Cert> cert = do_QueryInterface(isupport);
      rawPtr = cert;
      NS_IF_ADDREF(rawPtr);
      break;
    }
    if (mOutlinerArray[i].open)
      idx += mOutlinerArray[i].numChildren;
    cIndex += mOutlinerArray[i].numChildren;
    if (idx > index) break;
  }
  return rawPtr;
}

// LoadCerts
//
// Load all of the certificates in the DB for this type.  Sort them
// by token, organization, then common name.
NS_IMETHODIMP 
nsCertOutliner::LoadCerts(PRUint32 aType)
{
  nsresult rv;
  PRBool rowsChanged = PR_FALSE;
  PRInt32 numChanged = 0;
  if (mOutlinerArray) {
    FreeCertArray();
    nsMemory::Free(mOutlinerArray);
    mOutlinerArray = NULL;
    rowsChanged = PR_TRUE;
    numChanged = mNumRows;
    mNumRows = 0;
  }
  nsCOMPtr<nsIX509CertDB> certdb = do_GetService(NS_X509CERTDB_CONTRACTID);
  if (certdb == nsnull) return NS_ERROR_FAILURE;
  rv = certdb->GetCertsByType(aType, 
                              CmpByTok_IssuerOrg_Name,
                              getter_AddRefs(mCertArray));
  if (NS_FAILED(rv)) return rv;
  PRUint32 count;
  rv = mCertArray->Count(&count);
  if (NS_FAILED(rv)) return rv;
  mNumOrgs = CountOrganizations();
  mOutlinerArray = (outlinerArrayEl *)nsMemory::Alloc(
                                           sizeof(outlinerArrayEl) * mNumOrgs);
  PRUint32 j = 0;
  nsCOMPtr<nsISupports> isupport = dont_AddRef(mCertArray->ElementAt(j));
  nsCOMPtr<nsIX509Cert> orgCert = do_QueryInterface(isupport);
  for (PRInt32 i=0; i<mNumOrgs; i++) {
    orgCert->GetIssuerOrganization(&mOutlinerArray[i].orgName);
    mOutlinerArray[i].open = PR_TRUE;
    mOutlinerArray[i].certIndex = j;
    mOutlinerArray[i].numChildren = 1;
    if (++j >= count) break;
    isupport = dont_AddRef(mCertArray->ElementAt(j));
    nsCOMPtr<nsIX509Cert> nextCert = do_QueryInterface(isupport);
    while (CmpByIssuerOrg(orgCert, nextCert) == 0) {
      mOutlinerArray[i].numChildren++;
      if (++j >= count) break;
      isupport = dont_AddRef(mCertArray->ElementAt(j));
      nextCert = do_QueryInterface(isupport);
    }
    orgCert = nextCert;
  }
  mNumRows = count + mNumOrgs;
  if (rowsChanged) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("[%d,%d]", mNumRows, numChanged));
    numChanged = mNumRows - numChanged;
    if (mOutliner) mOutliner->RowCountChanged(0, numChanged);
  }
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////////
//
//  Begin nsIOutlinerView methods
//
/////////////////////////////////////////////////////////////////////////////

/* nsIX509Cert getCert(in unsigned long index); */
NS_IMETHODIMP
nsCertOutliner::GetCert(PRUint32 aIndex, nsIX509Cert **_cert)
{
  NS_ENSURE_ARG(_cert);
  *_cert = GetCertAtIndex(aIndex);
  //nsCOMPtr<nsIX509Cert> cert = GetCertAtIndex(aIndex);
  //if (cert) {
    //*_cert = cert;
    //NS_ADDREF(*_cert);
  //}
  return NS_OK;
}

/* readonly attribute long rowCount; */
NS_IMETHODIMP 
nsCertOutliner::GetRowCount(PRInt32 *aRowCount)
{
  PRUint32 count = 0;
  for (PRInt32 i=0; i<mNumOrgs; i++) {
    if (mOutlinerArray[i].open == PR_TRUE) {
      count += mOutlinerArray[i].numChildren + 1;
    } else {
      count++;
    }
  }
  *aRowCount = count;
  return NS_OK;
}

/* attribute nsIOutlinerSelection selection; */
NS_IMETHODIMP 
nsCertOutliner::GetSelection(nsIOutlinerSelection * *aSelection)
{
  *aSelection = mSelection;
  NS_IF_ADDREF(*aSelection);
  return NS_OK;
}

NS_IMETHODIMP 
nsCertOutliner::SetSelection(nsIOutlinerSelection * aSelection)
{
  mSelection = aSelection;
  return NS_OK;
}

/* void getRowProperties (in long index, in nsISupportsArray properties); */
NS_IMETHODIMP 
nsCertOutliner::GetRowProperties(PRInt32 index, nsISupportsArray *properties)
{
  return NS_OK;
}

/* void getCellProperties (in long row, in wstring colID, 
 *                           in nsISupportsArray properties); 
 */
NS_IMETHODIMP 
nsCertOutliner::GetCellProperties(PRInt32 row, const PRUnichar *colID, 
                                  nsISupportsArray *properties)
{
  return NS_OK;
}

/* void getColumnProperties (in wstring colID, 
 *                           in nsIDOMElement colElt, 
 *                           in nsISupportsArray properties); 
 */
NS_IMETHODIMP 
nsCertOutliner::GetColumnProperties(const PRUnichar *colID, 
                                    nsIDOMElement *colElt, 
                                    nsISupportsArray *properties)
{
  return NS_OK;
}

/* boolean isContainer (in long index); */
NS_IMETHODIMP 
nsCertOutliner::IsContainer(PRInt32 index, PRBool *_retval)
{
  outlinerArrayEl *el = GetThreadDescAtIndex(index);
  if (el) {
    *_retval = PR_TRUE;
  } else {
    *_retval = PR_FALSE;
  }
  return NS_OK;
}

/* boolean isContainerOpen (in long index); */
NS_IMETHODIMP 
nsCertOutliner::IsContainerOpen(PRInt32 index, PRBool *_retval)
{
  outlinerArrayEl *el = GetThreadDescAtIndex(index);
  if (el && el->open == PR_TRUE) {
    *_retval = PR_TRUE;
  } else {
    *_retval = PR_FALSE;
  }
  return NS_OK;
}

/* boolean isContainerEmpty (in long index); */
NS_IMETHODIMP 
nsCertOutliner::IsContainerEmpty(PRInt32 index, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

/* boolean isSeparator (in long index); */
NS_IMETHODIMP 
nsCertOutliner::IsSeparator(PRInt32 index, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

/* long getParentIndex (in long rowIndex); */
NS_IMETHODIMP 
nsCertOutliner::GetParentIndex(PRInt32 rowIndex, PRInt32 *_retval)
{
  int i, idx = 0;
  for (i=0; i<mNumOrgs; i++) {
    if (rowIndex == idx) break; // index is for thread
    if (rowIndex < idx + mOutlinerArray[i].numChildren + 1) {
      *_retval = idx;
      return NS_OK;
    }
    idx += mOutlinerArray[i].numChildren + 1;
    if (idx > rowIndex) break;
  }
  *_retval = -1;
  return NS_OK;
}

/* boolean hasNextSibling (in long rowIndex, in long afterIndex); */
NS_IMETHODIMP 
nsCertOutliner::HasNextSibling(PRInt32 rowIndex, PRInt32 afterIndex, 
                               PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

/* long getLevel (in long index); */
NS_IMETHODIMP 
nsCertOutliner::GetLevel(PRInt32 index, PRInt32 *_retval)
{
  outlinerArrayEl *el = GetThreadDescAtIndex(index);
  if (el) {
    *_retval = 0;
  } else {
    *_retval = 1;
  }
  return NS_OK;
}

/* wstring getCellText (in long row, in wstring colID); */
NS_IMETHODIMP 
nsCertOutliner::GetCellText(PRInt32 row, const PRUnichar *colID, 
                            PRUnichar **_retval)
{
  nsresult rv;
  char *col = NS_CONST_CAST(char *, NS_ConvertUCS2toUTF8(colID).get());
  outlinerArrayEl *el = GetThreadDescAtIndex(row);
  if (el != nsnull) {
    if (strcmp(col, "certcol") == 0) {
      nsAutoString oName(el->orgName);
      *_retval = ToNewUnicode(oName);
    } else {
      *_retval = nsnull;
    }
    return NS_OK;
  }
  nsCOMPtr<nsIX509Cert> cert = GetCertAtIndex(row);
  if (cert == nsnull) return NS_ERROR_FAILURE;
  char *str = NULL;
  PRUnichar *wstr = NULL;
  if (strcmp(col, "certcol") == 0) {
    rv = cert->GetCommonName(&wstr);
    if (NS_FAILED(rv) || !wstr) {
      // can this be fixed to not do copying?
      PRUnichar *tmp = nsnull;
      rv = cert->GetNickname(&tmp);
      nsAutoString nick(tmp);
      char *tmps = ToNewCString(nick);
      char *mark = strchr(tmps, ':');
      if (mark) {
        str = PL_strdup(mark + 1);
      } else {
        wstr = ToNewUnicode(nick);
      }
      nsMemory::Free(tmp);
      nsMemory::Free(tmps);
    }
  } else if (strcmp(col, "tokencol") == 0) {
    rv = cert->GetTokenName(&wstr);
  } else if (strcmp(col, "emailcol") == 0) {
    rv = cert->GetEmailAddress(&wstr);
  } else if (strcmp(col, "verifiedcol") == 0) {
    PRUint32 verified;
    nsCOMPtr<nsINSSComponent> nssComponent(
                                      do_GetService(kNSSComponentCID, &rv));
    if (NS_FAILED(rv)) return rv;
    PRBool ocspEnabled;
    cert->GetUsesOCSP(&ocspEnabled);
    if (ocspEnabled) {
      nssComponent->DisableOCSP();
    }
    rv = cert->GetPurposes(&verified, NULL);
    if (verified == nsIX509Cert::VERIFIED_OK) {
      nsAutoString vfy;
      rv = nssComponent->GetPIPNSSBundleString(
                                NS_LITERAL_STRING("VerifiedTrue").get(), vfy);
      if (!NS_FAILED(rv))
        wstr = ToNewUnicode(vfy);
    } else {
      nsAutoString vfy;
      rv = nssComponent->GetPIPNSSBundleString(
                                NS_LITERAL_STRING("VerifiedFalse").get(), vfy);
      if (!NS_FAILED(rv))
        wstr = ToNewUnicode(vfy);
    }
    if (ocspEnabled) {
      nssComponent->EnableOCSP();
    }
  } else if (strcmp(col, "purposecol") == 0) {
    PRUint32 verified;
    PRBool ocspEnabled;
    nsCOMPtr<nsINSSComponent> nssComponent(
                                      do_GetService(kNSSComponentCID, &rv));
    if (NS_FAILED(rv)) return rv;
  
    cert->GetUsesOCSP(&ocspEnabled);
    if (ocspEnabled) {
      nssComponent->DisableOCSP();
    }
    rv = cert->GetPurposes(&verified, &wstr);
    if (ocspEnabled) {
      nssComponent->EnableOCSP();
    }
  } else if (strcmp(col, "issuedcol") == 0) {
    rv = cert->GetIssuedDate(&wstr);
  } else if (strcmp(col, "expiredcol") == 0) {
    rv = cert->GetExpiresDate(&wstr);
  } else if (strcmp(col, "serialnumcol") == 0) {
    rv = cert->GetSerialNumber(&wstr);
/*
  } else if (strcmp(col, "certdbkeycol") == 0) {
    rv = cert->GetDbKey(&str);
*/
  } else {
    return NS_ERROR_FAILURE;
  }
  if (str) {
    nsAutoString astr = NS_ConvertASCIItoUCS2(str);
    wstr = ToNewUnicode(astr);
  }
  *_retval = wstr;
  return rv;
}

/* void setOutliner (in nsIOutlinerBoxObject outliner); */
NS_IMETHODIMP 
nsCertOutliner::SetOutliner(nsIOutlinerBoxObject *outliner)
{
  mOutliner = outliner;
  return NS_OK;
}

/* void toggleOpenState (in long index); */
NS_IMETHODIMP 
nsCertOutliner::ToggleOpenState(PRInt32 index)
{
  outlinerArrayEl *el = GetThreadDescAtIndex(index);
  if (el) el->open = !el->open;
  PRInt32 fac = (el->open) ? 1 : -1;
  if (mOutliner) mOutliner->RowCountChanged(index, fac * el->numChildren);
  mSelection->Select(index);
  return NS_OK;
}

/* void cycleHeader (in wstring colID, in nsIDOMElement elt); */
NS_IMETHODIMP 
nsCertOutliner::CycleHeader(const PRUnichar *colID, nsIDOMElement *elt)
{
  return NS_OK;
}

/* void selectionChanged (); */
NS_IMETHODIMP 
nsCertOutliner::SelectionChanged()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void cycleCell (in long row, in wstring colID); */
NS_IMETHODIMP 
nsCertOutliner::CycleCell(PRInt32 row, const PRUnichar *colID)
{
  return NS_OK;
}

/* boolean isEditable (in long row, in wstring colID); */
NS_IMETHODIMP 
nsCertOutliner::IsEditable(PRInt32 row, const PRUnichar *colID, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

/* void setCellText (in long row, in wstring colID, in wstring value); */
NS_IMETHODIMP 
nsCertOutliner::SetCellText(PRInt32 row, const PRUnichar *colID, 
                            const PRUnichar *value)
{
  return NS_OK;
}

/* void performAction (in wstring action); */
NS_IMETHODIMP 
nsCertOutliner::PerformAction(const PRUnichar *action)
{
  return NS_OK;
}

/* void performActionOnRow (in wstring action, in long row); */
NS_IMETHODIMP 
nsCertOutliner::PerformActionOnRow(const PRUnichar *action, PRInt32 row)
{
  return NS_OK;
}

/* void performActionOnCell (in wstring action, in long row, 
 *                           in wstring colID); 
 */
NS_IMETHODIMP 
nsCertOutliner::PerformActionOnCell(const PRUnichar *action, PRInt32 row, 
                                    const PRUnichar *colID)
{
  return NS_OK;
}

#ifdef DEBUG_CERT_OUTLINER
void
nsCertOutliner::dumpMap()
{
  for (int i=0; i<mNumOrgs; i++) {
    nsAutoString org(mOutlinerArray[i].orgName);
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("ORG[%s]", NS_LossyConvertUCS2toASCII(org).get()));
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("OPEN[%d]", mOutlinerArray[i].open));
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("INDEX[%d]", mOutlinerArray[i].certIndex));
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("NCHILD[%d]", mOutlinerArray[i].numChildren));
  }
  for (int i=0; i<mNumRows; i++) {
    outlinerArrayEl *el = GetThreadDescAtIndex(i);
    if (el != nsnull) {
      nsAutoString td(el->orgName);
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("thread desc[%d]: %s", i, NS_LossyConvertUCS2toASCII(td).get()));
    }
    nsCOMPtr<nsIX509Cert> ct = GetCertAtIndex(i);
    if (ct != nsnull) {
      PRUnichar *goo;
      ct->GetCommonName(&goo);
      nsAutoString doo(goo);
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("cert [%d]: %s", i, NS_LossyConvertUCS2toASCII(doo).get()));
    }
  }
}
#endif

//
// CanDropOn
//
// Can't drop on the thread pane.
//
NS_IMETHODIMP nsCertOutliner::CanDropOn(PRInt32 index, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;
  
  return NS_OK;
}

//
// CanDropBeforeAfter
//
// Can't drop on the thread pane.
//
NS_IMETHODIMP nsCertOutliner::CanDropBeforeAfter(PRInt32 index, PRBool before, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;
  
  return NS_OK;
}


//
// Drop
//
// Can't drop on the thread pane.
//
NS_IMETHODIMP nsCertOutliner::Drop(PRInt32 row, PRInt32 orient)
{
  return NS_OK;
}


//
// IsSorted
//
// ...
//
NS_IMETHODIMP nsCertOutliner::IsSorted(PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}
