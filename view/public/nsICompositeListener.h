/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is Mozilla Communicator.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corp.  Portions created by Netscape are Copyright (C) 1999 Netscape 
 * Communications Corp.  All Rights Reserved.
 * 
 * Contributor(s): 
 *   Kin Blas
 */

#ifndef nsICompositeListener_h___
#define nsICompositeListener_h___

#include "nsISupports.h"

// forward declarations
class nsIView;
class nsIViewManager;
class nsIRenderingContext;
class nsIRegion;

// IID for the nsICompositeListener interface
// {5661ce55-7c42-11d3-009d-1d060b0f8baff}
#define NS_ICOMPOSITELISTENER_IID \
{ 0x5661ce55, 0x7c42, 0x11d3, { 0x9d, 0x1d, 0x0, 0x60, 0xb0, 0xf8, 0xba, 0xff } }

/**
 * Provides a way for a client of an nsIViewManager to learn about composite
 * events.
 */
class nsICompositeListener : public nsISupports {
public:

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICOMPOSITELISTENER_IID)

  /**
   * Notification before a view is refreshed.
   * @param aViewManager view manager that "owns" the view. The view does NOT
   *        hold a reference to the view manager
   * @param aView view to be refreshed.
   * @param aContext rendering context to be used during the refresh.
   * @param aRegion region of the view to be refreshed, IN DEVICE COORDINATES
   * @param aUpdateFlags see bottom of nsIViewManager.h for description
   * @result The result of the notification, NS_OK if no errors
   */
  NS_IMETHOD WillRefreshRegion(nsIViewManager *aViewManager,
                               nsIView *aView,
                               nsIRenderingContext *aContext,
                               nsIRegion *aRegion,
                               PRUint32 aUpdateFlags) = 0;

  /**
   * Notification after a view was refreshed.
   * @param aViewManager view manager that "owns" the view. The view does NOT
   *        hold a reference to the view manager
   * @param aView view that was refreshed.
   * @param aContext rendering context that was used during the refresh.
   * @param aRegion region of the view that was refreshed, IN DEVICE COORDINATES
   * @param aUpdateFlags see bottom of nsIViewManager.h for description
   * @result The result of the notification, NS_OK if no errors
   */
  NS_IMETHOD DidRefreshRegion(nsIViewManager *aViewManager,
                              nsIView *aView,
                              nsIRenderingContext *aContext,
                              nsIRegion *aRegion,
                              PRUint32 aUpdateFlags) = 0;

  /**
   * Notification before a view is refreshed.
   * @param aViewManager view manager that "owns" the view. The view does NOT
   *        hold a reference to the view manager
   * @param aView view to be refreshed.
   * @param aContext rendering context to be used during the refresh.
   * @param aRect rect region of the view to be refreshed.
   * @param aUpdateFlags see bottom of nsIViewManager.h for description
   * @result The result of the notification, NS_OK if no errors
   */
  NS_IMETHOD WillRefreshRect(nsIViewManager *aViewManager,
                             nsIView *aView,
                             nsIRenderingContext *aContext,
                             const nsRect *aRect,
                             PRUint32 aUpdateFlags) = 0;

  /**
   * Notification after a view was refreshed.
   * @param aViewManager view manager that "owns" the view. The view does NOT
   *        hold a reference to the view manager
   * @param aView view that was refreshed.
   * @param aContext rendering context that was used during the refresh.
   * @param aRect rect region of the view that was refreshed.
   * @param aUpdateFlags see bottom of nsIViewManager.h for description
   * @result The result of the notification, NS_OK if no errors
   */
  NS_IMETHOD DidRefreshRect(nsIViewManager *aViewManager,
                            nsIView *aView,
                            nsIRenderingContext *aContext,
                            const nsRect *aRect,
                            PRUint32 aUpdateFlags) = 0;
};

#endif /* nsICompositeListener_h___ */

