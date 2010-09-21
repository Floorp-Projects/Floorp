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

/* This class wraps an SVG document, for use by VectorImage objects. */

#ifndef mozilla_imagelib_SVGDocumentWrapper_h_
#define mozilla_imagelib_SVGDocumentWrapper_h_

#include "nsCOMPtr.h"
#include "nsIStreamListener.h"
#include "nsIObserver.h"
#include "nsIDocumentViewer.h"

class nsIAtom;
class nsIPresShell;
class nsIRequest;
class nsIDocumentViewer;
class nsILoadGroup;
class nsIFrame;
class nsIntSize;
class nsSVGSVGElement;

#define SVG_MIMETYPE     "image/svg+xml"
#define OBSERVER_SVC_CID "@mozilla.org/observer-service;1"


namespace mozilla {
namespace imagelib {

class SVGDocumentWrapper : public nsIStreamListener,
                           public nsIObserver
{
public:
  SVGDocumentWrapper();
  ~SVGDocumentWrapper();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSIOBSERVER

  enum Dimension {
    eWidth,
    eHeight
  };

  /**
   * Looks up the value of the wrapped SVG document's |width| or |height|
   * attribute in CSS pixels, and returns it by reference.  If the document has
   * a percent value for the queried attribute, then this method fails
   * (returns PR_FALSE).
   *
   * @param aDimension    Indicates whether the width or height is desired.
   * @param[out] aResult  If this method succeeds, then this outparam will be
                          populated with the width or height in CSS pixels.
   * @return PR_FALSE to indicate failure, if the queried attribute has a
   *         percent value.  Otherwise, PR_TRUE.
   *
   */
  PRBool    GetWidthOrHeight(Dimension aDimension, PRInt32& aResult);

  /**
   * Returns the root <svg> element for the wrapped document, or nsnull on
   * failure.
   */
  nsSVGSVGElement* GetRootSVGElem();

  /**
   * Returns the root nsIFrame* for the wrapped document, or nsnull on failure.
   *
   * @return the root nsIFrame* for the wrapped document, or nsnull on failure.
   */
  nsIFrame* GetRootLayoutFrame();

  /**
   * Returns (by reference) the nsIPresShell for the wrapped document.
   *
   * @param[out] aPresShell On success, this will be populated with a pointer
   *                        to the wrapped document's nsIPresShell.
   *
   * @return NS_OK on success, or an error code on failure.
   */
  inline nsresult  GetPresShell(nsIPresShell** aPresShell)
    { return mViewer->GetPresShell(aPresShell); }

  /**
   * Returns a PRBool indicating whether the wrapped document has been parsed
   * successfully.
   *
   * @return PR_TRUE if the document has been parsed successfully, 
   *         PR_FALSE otherwise (e.g. if there's a syntax error in the SVG).
   */
  inline PRBool    ParsedSuccessfully()  { return !!GetRootSVGElem(); }

  /**
   * Modifier to update the viewport dimensions of the wrapped document. This
   * method performs a synchronous "Flush_Layout" on the wrapped document,
   * since a viewport-change affects layout.
   *
   * @param aViewportSize The new viewport dimensions.
   */
  void UpdateViewportBounds(const nsIntSize& aViewportSize);


  /**
   * Returns a PRBool indicating whether the document has any SMIL animations.
   *
   * @return PR_TRUE if the document has any SMIL animations. Else, PR_FALSE.
   */
  PRBool    IsAnimated();

  /**
   * Indicates whether we should currently ignore rendering invalidations sent
   * from the wrapped SVG doc.
   *
   * @return PR_TRUE if we should ignore invalidations sent from this SVG doc.
   */
  PRBool ShouldIgnoreInvalidation() { return mIgnoreInvalidation; }

  /**
   * Methods to control animation.
   */
  void StartAnimation();
  void StopAnimation();
  void ResetAnimation();

private:
  nsresult SetupViewer(nsIRequest *aRequest,
                       nsIDocumentViewer** aViewer,
                       nsILoadGroup** aLoadGroup);
  void     DestroyViewer();
  void     RegisterForXPCOMShutdown();

  void     FlushLayout();

  nsCOMPtr<nsIDocumentViewer> mViewer;
  nsCOMPtr<nsILoadGroup>      mLoadGroup;
  nsCOMPtr<nsIStreamListener> mListener;
  PRPackedBool                mIgnoreInvalidation;

  // Lazily-initialized pointer to nsGkAtoms::svg, to make life easier in
  // non-libxul builds, which don't let us reference nsGkAtoms from imagelib.
  static nsIAtom* kSVGAtom;
};

} // namespace imagelib
} // namespace mozilla

#endif // mozilla_imagelib_SVGDocumentWrapper_h_
