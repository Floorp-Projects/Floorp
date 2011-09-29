/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Honza Bambas <honzab@firemni.cz>
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

#ifndef nsOfflineCacheUpdateGlue_h
#define nsOfflineCacheUpdateGlue_h

#include "nsIOfflineCacheUpdate.h"

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsWeakReference.h"

class nsOfflineCacheUpdate;

namespace mozilla {
namespace docshell {

// Like FORWARD_SAFE except methods:
//    Schedule
//    Init
#define NS_ADJUSTED_FORWARD_NSIOFFLINECACHEUPDATE(_to) \
  NS_SCRIPTABLE NS_IMETHOD GetStatus(PRUint16 *aStatus) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetStatus(aStatus); } \
  NS_SCRIPTABLE NS_IMETHOD GetPartial(bool *aPartial) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetPartial(aPartial); } \
  NS_SCRIPTABLE NS_IMETHOD GetIsUpgrade(bool *aIsUpgrade) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetIsUpgrade(aIsUpgrade); } \
  NS_SCRIPTABLE NS_IMETHOD GetUpdateDomain(nsACString & aUpdateDomain) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetUpdateDomain(aUpdateDomain); } \
  NS_SCRIPTABLE NS_IMETHOD GetManifestURI(nsIURI **aManifestURI) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetManifestURI(aManifestURI); } \
  NS_SCRIPTABLE NS_IMETHOD GetSucceeded(bool *aSucceeded) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetSucceeded(aSucceeded); } \
  NS_SCRIPTABLE NS_IMETHOD InitPartial(nsIURI *aManifestURI, const nsACString & aClientID, nsIURI *aDocumentURI) { return !_to ? NS_ERROR_NULL_POINTER : _to->InitPartial(aManifestURI, aClientID, aDocumentURI); } \
  NS_SCRIPTABLE NS_IMETHOD AddDynamicURI(nsIURI *aURI) { return !_to ? NS_ERROR_NULL_POINTER : _to->AddDynamicURI(aURI); } \
  NS_SCRIPTABLE NS_IMETHOD AddObserver(nsIOfflineCacheUpdateObserver *aObserver, bool aHoldWeak) { return !_to ? NS_ERROR_NULL_POINTER : _to->AddObserver(aObserver, aHoldWeak); } \
  NS_SCRIPTABLE NS_IMETHOD RemoveObserver(nsIOfflineCacheUpdateObserver *aObserver) { return !_to ? NS_ERROR_NULL_POINTER : _to->RemoveObserver(aObserver); } 

class OfflineCacheUpdateGlue : public nsSupportsWeakReference
                               , public nsIOfflineCacheUpdate
                               , public nsIOfflineCacheUpdateObserver
{
public:
    NS_DECL_ISUPPORTS

private:
    nsIOfflineCacheUpdate* EnsureUpdate();

public:
    NS_ADJUSTED_FORWARD_NSIOFFLINECACHEUPDATE(EnsureUpdate())
    NS_SCRIPTABLE NS_IMETHOD Schedule(void);
    NS_SCRIPTABLE NS_IMETHOD Init(nsIURI *aManifestURI, 
                                  nsIURI *aDocumentURI, 
                                  nsIDOMDocument *aDocument);

    NS_DECL_NSIOFFLINECACHEUPDATEOBSERVER

    OfflineCacheUpdateGlue();
    ~OfflineCacheUpdateGlue();

    void SetDocument(nsIDOMDocument *aDocument);

private:
    nsRefPtr<nsOfflineCacheUpdate> mUpdate;

    /* Document that requested this update */
    nsCOMPtr<nsIDOMDocument> mDocument;
    nsCOMPtr<nsIURI> mDocumentURI;
};

}
}

#endif
