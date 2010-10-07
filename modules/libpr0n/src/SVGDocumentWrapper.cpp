/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Holbert <dholbert@mozilla.com>
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

#include "SVGDocumentWrapper.h"
#include "mozilla/dom/Element.h"
#include "nsIAtom.h"
#include "nsICategoryManager.h"
#include "nsIChannel.h"
#include "nsIDocument.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIDocumentViewer.h"
#include "nsIDOMSVGAnimatedLength.h"
#include "nsIDOMSVGLength.h"
#include "nsIHttpChannel.h"
#include "nsIObserverService.h"
#include "nsIParser.h"
#include "nsIPresShell.h"
#include "nsIRequest.h"
#include "nsIStreamListener.h"
#include "nsIXMLContentSink.h"
#include "nsNetCID.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsSize.h"
#include "gfxRect.h"
#include "nsSVGSVGElement.h"
#include "nsSVGLength2.h"

using namespace mozilla::dom;

namespace mozilla {
namespace imagelib {

nsIAtom* SVGDocumentWrapper::kSVGAtom = nsnull; // lazily initialized

NS_IMPL_ISUPPORTS3(SVGDocumentWrapper,
                   nsIStreamListener,
                   nsIRequestObserver,
                   nsIObserver)

SVGDocumentWrapper::SVGDocumentWrapper()
 : mIgnoreInvalidation(PR_FALSE)
{
  // Lazy-initialize our "svg" atom.  (It'd be nicer to just use nsGkAtoms::svg
  // directly, but we can't access it from here in non-libxul builds.)
  if (!SVGDocumentWrapper::kSVGAtom) {
    SVGDocumentWrapper::kSVGAtom =
      NS_NewPermanentAtom(NS_LITERAL_STRING("svg"));
  }
}

SVGDocumentWrapper::~SVGDocumentWrapper()
{
  DestroyViewer();
}

void
SVGDocumentWrapper::DestroyViewer()
{
  if (mViewer) {
    mViewer->GetDocument()->OnPageHide(PR_FALSE, nsnull);
    mViewer->Close(nsnull);
    mViewer->Destroy();
    mViewer = nsnull;
  }
}

PRBool
SVGDocumentWrapper::GetWidthOrHeight(Dimension aDimension,
                                     PRInt32& aResult)
{
  nsSVGSVGElement* rootElem = GetRootSVGElem();
  NS_ABORT_IF_FALSE(rootElem, "root elem missing or of wrong type");
  nsresult rv;

  // Get the width or height SVG object
  nsRefPtr<nsIDOMSVGAnimatedLength> domAnimLength;
  if (aDimension == eWidth) {
    rv = rootElem->GetWidth(getter_AddRefs(domAnimLength));
  } else {
    NS_ABORT_IF_FALSE(aDimension == eHeight, "invalid dimension");
    rv = rootElem->GetHeight(getter_AddRefs(domAnimLength));
  }
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  NS_ENSURE_TRUE(domAnimLength, PR_FALSE);

  // Get the animated value from the object
  nsRefPtr<nsIDOMSVGLength> domLength;
  rv = domAnimLength->GetAnimVal(getter_AddRefs(domLength));
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  NS_ENSURE_TRUE(domLength, PR_FALSE);

  // Check if it's a percent value (and fail if so)
  PRUint16 unitType;
  rv = domLength->GetUnitType(&unitType);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  if (unitType == nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE) {
    return PR_FALSE;
  }

  // Non-percent value - woot! Grab it & return it.
  float floatLength;
  rv = domLength->GetValue(&floatLength);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  aResult = nsSVGUtils::ClampToInt(floatLength);

  return PR_TRUE;
}

nsIFrame*
SVGDocumentWrapper::GetRootLayoutFrame()
{
  Element* rootElem = GetRootSVGElem();
  return rootElem ? rootElem->GetPrimaryFrame() : nsnull;
}

void
SVGDocumentWrapper::UpdateViewportBounds(const nsIntSize& aViewportSize)
{
  NS_ABORT_IF_FALSE(!mIgnoreInvalidation, "shouldn't be reentrant");
  mIgnoreInvalidation = PR_TRUE;
  mViewer->SetBounds(nsIntRect(nsIntPoint(0, 0), aViewportSize));
  FlushLayout();
  mIgnoreInvalidation = PR_FALSE;
}

PRBool
SVGDocumentWrapper::IsAnimated()
{
#ifdef MOZ_SMIL
  nsIDocument* doc = mViewer->GetDocument();
  return doc && doc->HasAnimationController() &&
    doc->GetAnimationController()->HasRegisteredAnimations();
#else
  return PR_FALSE;
#endif // MOZ_SMIL
}

void
SVGDocumentWrapper::StartAnimation()
{
  nsSVGSVGElement* svgElem = GetRootSVGElem();
  if (!svgElem)
    return;

#ifdef DEBUG
  nsresult rv = 
#endif
    svgElem->UnpauseAnimations();
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "UnpauseAnimations failed");
}

void
SVGDocumentWrapper::StopAnimation()
{
  // This method gets called for animated images during shutdown, after we've
  // already Observe()'d XPCOM shutdown and cleared out our mViewer pointer.
  // When that happens, we need to bail out early, or else GetRootSVGElem will
  // try to deref mViewer (= null) and crash as a result.
  if (!mViewer)
    return;

  nsSVGSVGElement* svgElem = GetRootSVGElem();
  if (!svgElem)
    return;

#ifdef DEBUG
  nsresult rv = 
#endif
    svgElem->PauseAnimations();
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "PauseAnimations failed");
}

void
SVGDocumentWrapper::ResetAnimation()
{
  nsSVGSVGElement* svgElem = GetRootSVGElem();
  if (!svgElem)
    return;

#ifdef DEBUG
  nsresult rv = 
#endif
    svgElem->SetCurrentTime(0.0f);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "SetCurrentTime failed");
}


/** nsIStreamListener methods **/

/* void onDataAvailable (in nsIRequest request, in nsISupports ctxt,
                         in nsIInputStream inStr, in unsigned long sourceOffset,
                         in unsigned long count); */
NS_IMETHODIMP
SVGDocumentWrapper::OnDataAvailable(nsIRequest* aRequest, nsISupports* ctxt,
                                    nsIInputStream* inStr,
                                    PRUint32 sourceOffset,
                                    PRUint32 count)
{
  return mListener->OnDataAvailable(aRequest, ctxt, inStr,
                                    sourceOffset, count);
}

/** nsIRequestObserver methods **/

/* void onStartRequest (in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP
SVGDocumentWrapper::OnStartRequest(nsIRequest* aRequest, nsISupports* ctxt)
{
  nsresult rv = SetupViewer(aRequest,
                            getter_AddRefs(mViewer),
                            getter_AddRefs(mLoadGroup));

  if (NS_SUCCEEDED(rv) &&
      NS_SUCCEEDED(mListener->OnStartRequest(aRequest, nsnull))) {
    mViewer->GetDocument()->SetIsBeingUsedAsImage();
    rv = mViewer->Init(nsnull, nsIntRect(0, 0, 0, 0));
    if (NS_SUCCEEDED(rv)) {
      rv = mViewer->Open(nsnull, nsnull);
    }
  }
  return rv;
}


/* void onStopRequest (in nsIRequest request, in nsISupports ctxt,
                       in nsresult status); */
NS_IMETHODIMP
SVGDocumentWrapper::OnStopRequest(nsIRequest* aRequest, nsISupports* ctxt,
                                  nsresult status)
{
  if (mListener) {
    mListener->OnStopRequest(aRequest, ctxt, status);
    // A few levels up the stack, imgRequest::OnStopRequest is about to tell
    // all of its observers that we know our size and are ready to paint.  That
    // might not be true at this point, though -- so here, we synchronously
    // finish parsing & layout in our helper-document to make sure we can hold
    // up to this promise.
    nsCOMPtr<nsIParser> parser = do_QueryInterface(mListener);
    if (!parser->IsComplete()) {
      parser->ContinueInterruptedParsing();
    }
    FlushLayout();
    mListener = nsnull;

    // In a normal document, this would be called by nsDocShell - but we don't
    // have a nsDocShell. So we do it ourselves. (If we don't, painting will
    // stay suppressed for a little while longer, for no good reason).
    mViewer->LoadComplete(NS_OK);
  }

  return NS_OK;
}

/** nsIObserver Methods **/
NS_IMETHODIMP
SVGDocumentWrapper::Observe(nsISupports* aSubject,
                            const char* aTopic,
                            const PRUnichar *aData)
{
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    // Clean up at XPCOM shutdown time.
    DestroyViewer();
    if (mListener)
      mListener = nsnull;
    if (mLoadGroup)
      mLoadGroup = nsnull;
  } else {
    NS_ERROR("Unexpected observer topic.");
  }
  return NS_OK;
}

/** Private helper methods **/

// This method is largely cribbed from
// nsExternalResourceMap::PendingLoad::SetupViewer.
nsresult
SVGDocumentWrapper::SetupViewer(nsIRequest* aRequest,
                                nsIDocumentViewer** aViewer,
                                nsILoadGroup** aLoadGroup)
{
  nsCOMPtr<nsIChannel> chan(do_QueryInterface(aRequest));
  NS_ENSURE_TRUE(chan, NS_ERROR_UNEXPECTED);

  // Check for HTTP error page
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aRequest));
  if (httpChannel) {
    PRBool requestSucceeded;
    if (NS_FAILED(httpChannel->GetRequestSucceeded(&requestSucceeded)) ||
        !requestSucceeded) {
      return NS_ERROR_FAILURE;
    }
  }

  // Give this document its own loadgroup
  nsCOMPtr<nsILoadGroup> loadGroup;
  chan->GetLoadGroup(getter_AddRefs(loadGroup));

  nsCOMPtr<nsILoadGroup> newLoadGroup =
        do_CreateInstance(NS_LOADGROUP_CONTRACTID);
  NS_ENSURE_TRUE(newLoadGroup, NS_ERROR_OUT_OF_MEMORY);
  newLoadGroup->SetLoadGroup(loadGroup);

  nsCOMPtr<nsICategoryManager> catMan =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(catMan, NS_ERROR_NOT_AVAILABLE);
  nsXPIDLCString contractId;
  nsresult rv = catMan->GetCategoryEntry("Gecko-Content-Viewers", SVG_MIMETYPE,
                                         getter_Copies(contractId));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory =
    do_GetService(contractId);
  NS_ENSURE_TRUE(docLoaderFactory, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIContentViewer> viewer;
  nsCOMPtr<nsIStreamListener> listener;
  rv = docLoaderFactory->CreateInstance("external-resource", chan,
                                        newLoadGroup,
                                        SVG_MIMETYPE, nsnull, nsnull,
                                        getter_AddRefs(listener),
                                        getter_AddRefs(viewer));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocumentViewer> docViewer = do_QueryInterface(viewer);
  NS_ENSURE_TRUE(docViewer, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIParser> parser = do_QueryInterface(listener);
  NS_ENSURE_TRUE(parser, NS_ERROR_UNEXPECTED);

  // XML-only, because this is for SVG content
  nsIContentSink* sink = parser->GetContentSink();
  nsCOMPtr<nsIXMLContentSink> xmlSink = do_QueryInterface(sink);
  NS_ENSURE_TRUE(sink, NS_ERROR_UNEXPECTED);

  listener.swap(mListener);
  docViewer.swap(*aViewer);
  newLoadGroup.swap(*aLoadGroup);

  RegisterForXPCOMShutdown();
  return NS_OK;
}

void
SVGDocumentWrapper::RegisterForXPCOMShutdown()
{
  // Listen for xpcom-shutdown so that we can drop references to our
  // helper-document at that point. (Otherwise, we won't get cleaned up
  // until imgLoader::Shutdown, which can happen after the JAR service
  // and RDF service have been unregistered.)
  nsresult rv;
  nsCOMPtr<nsIObserverService> obsSvc = do_GetService(OBSERVER_SVC_CID, &rv);
  if (NS_FAILED(rv) ||
      NS_FAILED(obsSvc->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                    PR_FALSE))) {
    NS_WARNING("Failed to register as observer of XPCOM shutdown");
  }
}

void
SVGDocumentWrapper::FlushLayout()
{
  nsCOMPtr<nsIPresShell> presShell;
  mViewer->GetPresShell(getter_AddRefs(presShell));
  if (presShell) {
    presShell->FlushPendingNotifications(Flush_Layout);
  }
}

nsSVGSVGElement*
SVGDocumentWrapper::GetRootSVGElem()
{
  if (!mViewer)
    return nsnull; // Can happen during destruction

  nsIDocument* doc = mViewer->GetDocument();
  if (!doc)
    return nsnull; // Can happen during destruction

  Element* rootElem = mViewer->GetDocument()->GetRootElement();
  if (!rootElem ||
      rootElem->GetNameSpaceID() != kNameSpaceID_SVG ||
      rootElem->Tag() != SVGDocumentWrapper::kSVGAtom) {
    return nsnull;
  }

  return static_cast<nsSVGSVGElement*>(rootElem);
}

} // namespace imagelib
} // namespace mozilla
