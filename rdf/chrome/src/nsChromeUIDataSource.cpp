/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsCOMPtr.h"
#include "nsChromeUIDataSource.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFObserver.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsCRTGlue.h"
#include "rdf.h"
#include "nsIServiceManager.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIRDFResource.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFContainer.h"
#include "nsHashtable.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsISimpleEnumerator.h"
#include "nsNetUtil.h"
#include "nsISupportsArray.h"
#include "nsIIOService.h"

////////////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

nsChromeUIDataSource::nsChromeUIDataSource(nsIRDFDataSource* aComposite)
{
  mComposite = aComposite;
  mComposite->AddObserver(this);

  nsresult rv;
  rv = CallGetService(kRDFServiceCID, &mRDFService);
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");

  mRDFService->RegisterDataSource(this, PR_TRUE);
}

nsChromeUIDataSource::~nsChromeUIDataSource()
{
  mRDFService->UnregisterDataSource(this);

  NS_IF_RELEASE(mRDFService);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsChromeUIDataSource)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsChromeUIDataSource)
  if (tmp->mComposite) {
    tmp->mComposite->RemoveObserver(tmp);
    tmp->mComposite = nsnull;
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mObservers);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsChromeUIDataSource)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mComposite)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mObservers)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsChromeUIDataSource,
                                          nsIRDFDataSource)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsChromeUIDataSource,
                                           nsIRDFDataSource)

NS_INTERFACE_MAP_BEGIN(nsChromeUIDataSource)
  NS_INTERFACE_MAP_ENTRY(nsIRDFDataSource)
  NS_INTERFACE_MAP_ENTRY(nsIRDFObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIRDFDataSource)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsChromeUIDataSource)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
//
// nsIRDFDataSource interface
//

NS_IMETHODIMP
nsChromeUIDataSource::GetURI(char** aURI)
{
  *aURI = NS_strdup("rdf:chrome");
	if (! *aURI)
		return NS_ERROR_OUT_OF_MEMORY;

	return NS_OK;
}

NS_IMETHODIMP
nsChromeUIDataSource::GetSource(nsIRDFResource* property,
                                nsIRDFNode* target,
                                PRBool tv,
                                nsIRDFResource** source)
{
  return mComposite->GetSource(property, target, tv, source);
}

NS_IMETHODIMP
nsChromeUIDataSource::GetSources(nsIRDFResource* aProperty,
                                    nsIRDFNode* aTarget,
                                    PRBool aTruthValue,
                                    nsISimpleEnumerator** aResult)
{
  return mComposite->GetSources(aProperty, aTarget, aTruthValue, aResult);
}

NS_IMETHODIMP
nsChromeUIDataSource::GetTarget(nsIRDFResource* aSource,
                                   nsIRDFResource* aProperty,
                                   PRBool aTruthValue,
                                   nsIRDFNode** aResult)
{
  return mComposite->GetTarget(aSource, aProperty, aTruthValue, aResult);
}

NS_IMETHODIMP
nsChromeUIDataSource::GetTargets(nsIRDFResource* aSource,
                                 nsIRDFResource* aProperty,
                                 PRBool aTruthValue,
                                 nsISimpleEnumerator** aResult)
{
  return mComposite->GetTargets(aSource, aProperty, aTruthValue, aResult);
}

NS_IMETHODIMP
nsChromeUIDataSource::Assert(nsIRDFResource* aSource, 
                             nsIRDFResource* aProperty, 
                             nsIRDFNode* aTarget,
                             PRBool aTruthValue)
{
  return mComposite->Assert(aSource, aProperty, aTarget, aTruthValue);
}

NS_IMETHODIMP
nsChromeUIDataSource::Unassert(nsIRDFResource* aSource,
                                  nsIRDFResource* aProperty,
                                  nsIRDFNode* aTarget)
{
  return mComposite->Unassert(aSource, aProperty, aTarget);
}

NS_IMETHODIMP
nsChromeUIDataSource::Change(nsIRDFResource* aSource,
                                nsIRDFResource* aProperty,
                                nsIRDFNode* aOldTarget,
                                nsIRDFNode* aNewTarget)
{
  return mComposite->Change(aSource, aProperty, aOldTarget, aNewTarget);
}

NS_IMETHODIMP
nsChromeUIDataSource::Move(nsIRDFResource* aOldSource,
                              nsIRDFResource* aNewSource,
                              nsIRDFResource* aProperty,
                              nsIRDFNode* aTarget)
{
  return mComposite->Move(aOldSource, aNewSource, aProperty, aTarget);
}


NS_IMETHODIMP
nsChromeUIDataSource::HasAssertion(nsIRDFResource* aSource,
                                      nsIRDFResource* aProperty,
                                      nsIRDFNode* aTarget,
                                      PRBool aTruthValue,
                                      PRBool* aResult)
{
  return mComposite->HasAssertion(aSource, aProperty, aTarget, aTruthValue, aResult);
}

NS_IMETHODIMP
nsChromeUIDataSource::AddObserver(nsIRDFObserver* aObserver)
{
  NS_PRECONDITION(aObserver != nsnull, "null ptr");
  if (! aObserver)
      return NS_ERROR_NULL_POINTER;

  // XXX ensure uniqueness?

  mObservers.AppendObject(aObserver);
  return NS_OK;
}

NS_IMETHODIMP
nsChromeUIDataSource::RemoveObserver(nsIRDFObserver* aObserver)
{
  NS_PRECONDITION(aObserver != nsnull, "null ptr");
  if (! aObserver)
      return NS_ERROR_NULL_POINTER;

  mObservers.RemoveObject(aObserver);
  return NS_OK;
}

NS_IMETHODIMP 
nsChromeUIDataSource::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *result)
{
  return mComposite->HasArcIn(aNode, aArc, result);
}

NS_IMETHODIMP 
nsChromeUIDataSource::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result)
{
  return mComposite->HasArcOut(aSource, aArc, result);
}

NS_IMETHODIMP
nsChromeUIDataSource::ArcLabelsIn(nsIRDFNode* aTarget, nsISimpleEnumerator** aResult)
{
  return mComposite->ArcLabelsIn(aTarget, aResult);
}

NS_IMETHODIMP
nsChromeUIDataSource::ArcLabelsOut(nsIRDFResource* aSource,
                                      nsISimpleEnumerator** aResult)
{
  return mComposite->ArcLabelsOut(aSource, aResult);
}

NS_IMETHODIMP
nsChromeUIDataSource::GetAllResources(nsISimpleEnumerator** aResult)
{
  return mComposite->GetAllResources(aResult);
}

NS_IMETHODIMP
nsChromeUIDataSource::GetAllCmds(nsIRDFResource* source,
                                        nsISimpleEnumerator/*<nsIRDFResource>*/** result)
{
	return mComposite->GetAllCmds(source, result);
}

NS_IMETHODIMP
nsChromeUIDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                          nsIRDFResource*   aCommand,
                                          nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                          PRBool* aResult)
{
  return mComposite->IsCommandEnabled(aSources, aCommand, aArguments, aResult);
}

NS_IMETHODIMP
nsChromeUIDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                   nsIRDFResource*   aCommand,
                                   nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
  return mComposite->DoCommand(aSources, aCommand, aArguments);
}

NS_IMETHODIMP
nsChromeUIDataSource::BeginUpdateBatch() {
  return mComposite->BeginUpdateBatch();
}

NS_IMETHODIMP
nsChromeUIDataSource::EndUpdateBatch() {
  return mComposite->EndUpdateBatch();
}
                                                                               
//////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsChromeUIDataSource::OnAssert(nsIRDFDataSource* aDataSource,
                               nsIRDFResource* aSource,
                               nsIRDFResource* aProperty,
                               nsIRDFNode* aTarget)
{
  PRInt32 count = mObservers.Count();

  for (PRInt32 i = count - 1; i >= 0; --i) {
    mObservers[i]->OnAssert(this, aSource, aProperty, aTarget);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsChromeUIDataSource::OnUnassert(nsIRDFDataSource* aDataSource,
                                 nsIRDFResource* aSource,
                                 nsIRDFResource* aProperty,
                                 nsIRDFNode* aTarget)
{
  PRInt32 count = mObservers.Count();
  for (PRInt32 i = count - 1; i >= 0; --i) {
    mObservers[i]->OnUnassert(aDataSource, aSource, aProperty, aTarget);
  }
  return NS_OK;
}


NS_IMETHODIMP
nsChromeUIDataSource::OnChange(nsIRDFDataSource* aDataSource,
                               nsIRDFResource* aSource,
                               nsIRDFResource* aProperty,
                               nsIRDFNode* aOldTarget,
                               nsIRDFNode* aNewTarget)
{
  PRInt32 count = mObservers.Count();

  for (PRInt32 i = count - 1; i >= 0; --i) {
    mObservers[i]->OnChange(aDataSource, aSource, aProperty, aOldTarget, aNewTarget);
  }
  return NS_OK;
}


NS_IMETHODIMP
nsChromeUIDataSource::OnMove(nsIRDFDataSource* aDataSource,
                             nsIRDFResource* aOldSource,
                             nsIRDFResource* aNewSource,
                             nsIRDFResource* aProperty,
                             nsIRDFNode* aTarget)
{
  PRInt32 count = mObservers.Count();

  for (PRInt32 i = count - 1; i >= 0; --i) {
    mObservers[i]->OnMove(aDataSource, aOldSource, aNewSource, aProperty, aTarget);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsChromeUIDataSource::OnBeginUpdateBatch(nsIRDFDataSource* aDataSource)
{
  PRInt32 count = mObservers.Count();

  for (PRInt32 i = count - 1; i >= 0; --i) {
    mObservers[i]->OnBeginUpdateBatch(aDataSource);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsChromeUIDataSource::OnEndUpdateBatch(nsIRDFDataSource* aDataSource)
{
  PRInt32 count = mObservers.Count();

  for (PRInt32 i = count - 1; i >= 0; --i) {
    mObservers[i]->OnEndUpdateBatch(aDataSource);
  }
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////
nsresult
NS_NewChromeUIDataSource(nsIRDFDataSource* aComposite, nsIRDFDataSource** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
      return NS_ERROR_NULL_POINTER;

  nsChromeUIDataSource* ChromeUIDataSource = new nsChromeUIDataSource(aComposite);
  if (ChromeUIDataSource == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult = ChromeUIDataSource);
  return NS_OK;
}
