/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIXPFCCanvasManager_h___
#define nsIXPFCCanvasManager_h___

#include "nsISupports.h"

class nsIXPFCCanvas;
class nsIView;
class nsIWidget;
class nsIWebViewerContainer;

// IID for the nsIXPFCCanvasManager interface
#define NS_IXPFC_CANVAS_MANAGER_IID   \
{ 0xa4853b10, 0x28a4, 0x11d2,    \
{ 0x92, 0x46, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

/**
 * XPFC CanvasManager interface. This is the interface for managing 
 * canvas's within the scoope of an embeddable widget
 */
class nsIXPFCCanvasManager : public nsISupports
{

public:

  /**
   * Initialize the nsIXPFCCanvasManager
   * @result The result of the initialization, NS_Ok if no errors
   */
  NS_IMETHOD  Init() = 0;

  /**
   * Find the canvas from the given widget
   * @param aWidget the widget aggregated by the canvas
   * @result nsIXPFCCanvas pointer, The resultant canvas, nsnull if none found
   */
  NS_IMETHOD_(nsIXPFCCanvas *)  CanvasFromView(nsIView * aView) = 0;
  NS_IMETHOD_(nsIXPFCCanvas *)  CanvasFromWidget(nsIWidget * aWidget) = 0;

  /**
   * Get a reference to the root canvas
   * @param aCanvas out paramater, the root canvas to be filled in
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD  GetRootCanvas(nsIXPFCCanvas ** aCanvas) = 0;

  /**
   * Set the Root Canvas
   * @param aCanvas the root canvas
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD  SetRootCanvas(nsIXPFCCanvas * aCanvas) = 0;

  /**
   * Register an association between a canvas and a widget
   * @param aCanvas the canvas
   * @param aWidget the widget
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD RegisterView(nsIXPFCCanvas * aCanvas, 
                          nsIView * aView) = 0;

  NS_IMETHOD RegisterWidget(nsIXPFCCanvas * aCanvas, 
                            nsIWidget * aWidget) = 0;

  /**
   * UnRegister an association between a canvas and a widget
   * @param aCanvas the canvas
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD  Unregister(nsIXPFCCanvas * aCanvas) = 0;

  /**
   * Get the canvas with Keyboard Focus
   * @result nsIXPFCCanvas pointer, the canvas with focus
   */
  NS_IMETHOD_(nsIXPFCCanvas *) GetFocusedCanvas() = 0;

  /**
   * Set the canvas with Keyboard Focus
   * @param nsIXPFCCanvas pointer, the canvas to set focus to
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD SetFocusedCanvas(nsIXPFCCanvas * aCanvas) = 0;

  /**
   * Get the canvas with Keyboard Focus
   * @result nsIXPFCCanvas pointer, the canvas with focus
   */
  NS_IMETHOD_(nsIXPFCCanvas *) GetPressedCanvas() = 0;

  /**
   * Set the canvas with Keyboard Focus
   * @param nsIXPFCCanvas pointer, the canvas to set focus to
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD SetPressedCanvas(nsIXPFCCanvas * aCanvas) = 0;

  /**
   * Get the canvas with Keyboard Focus
   * @result nsIXPFCCanvas pointer, the canvas with focus
   */
  NS_IMETHOD_(nsIXPFCCanvas *) GetMouseOverCanvas() = 0;

  /**
   * Set the canvas with Keyboard Focus
   * @param nsIXPFCCanvas pointer, the canvas to set focus to
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD SetMouseOverCanvas(nsIXPFCCanvas * aCanvas) = 0;


  NS_IMETHOD_(nsIWebViewerContainer *) GetWebViewerContainer() = 0;
  NS_IMETHOD SetWebViewerContainer(nsIWebViewerContainer * aWebViewerContainer) = 0;


};

#endif /* nsIXPFCCanvasManager_h___ */
