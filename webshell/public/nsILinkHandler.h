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
#ifndef nsILinkHandler_h___
#define nsILinkHandler_h___

#include "nsweb.h"
#include "nsIWebWidget.h"
class nsIFrame;
class nsString;
class nsIPostData;
struct nsGUIEvent;

/* 52bd1e30-ce3f-11d1-9328-00805f8add32 */
#define NS_ILINKHANDLER_IID   \
{ 0x52bd1e30, 0xce3f, 0x11d1, \
  {0x93, 0x28, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

enum nsLinkState {
  eLinkState_Unvisited  = 0,
  eLinkState_Visited    = 1, 
  eLinkState_OutOfDate  = 2,  // visited, but the cache is stale
  eLinkState_Active     = 3,  // mouse is down on link
  eLinkState_Hover      = 4   // mouse is hovering over link
};

/**
 * Interface used for handling clicks on links
 */
class nsILinkHandler : public nsISupports {
public:
  NS_IMETHOD Init(nsIWebWidget* aWidget) = 0;

  NS_IMETHOD GetWebWidget(nsIWebWidget** aResult) = 0;

  /**
   * Process a click on a link. aFrame is the frame that contains the
   * linked content. aURLSpec is an absolute url spec that defines the
   * destination for the link. aTargetSpec indicates where the link is
   * targeted (it may be an empty string).
   */
  NS_IMETHOD OnLinkClick(nsIFrame* aFrame, 
                         const nsString& aURLSpec,
                         const nsString& aTargetSpec,
                         nsIPostData* aPostData = 0) = 0;

  /**
   * Process a mouse-over a link. aFrame is the frame that contains the
   * linked content. aURLSpec is an absolute url spec that defines the
   * destination for the link. aTargetSpec indicates where the link is
   * targeted (it may be an empty string).
   */
  NS_IMETHOD OnOverLink(nsIFrame* aFrame, 
                        const nsString& aURLSpec,
                        const nsString& aTargetSpec,
                        nsIPostData* aPostData = 0) = 0;

  /**
   * Get the state of a link to a given absolute URL
   */
  NS_IMETHOD GetLinkState(const nsString& aURLSpec, nsLinkState& aState) = 0;
};

// Standard link handler that does what you would expect (XXX doc...)
extern NS_WEB nsresult NS_NewLinkHandler(nsILinkHandler** aInstancePtrResult);

#endif /* nsILinkHandler_h___ */
