/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsILinkHandler_h___
#define nsILinkHandler_h___

#include "nsISupports.h"

class nsIInputStream;
class nsIDocShell;
class nsIRequest;
class nsIContent;
class nsString;
struct nsGUIEvent;

// Interface ID for nsILinkHandler
#define NS_ILINKHANDLER_IID \
 { 0xa6cf905b, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

enum nsLinkState {
  eLinkState_Unknown    = 0,
  eLinkState_Unvisited  = 1,
  eLinkState_Visited    = 2, 
  eLinkState_NotLink    = 3
};

// XXX Verb to use for link actuation. These are the verbs specified
// in the current XLink draft. We may actually want to support more
// (especially for extended links).
enum nsLinkVerb {
  eLinkVerb_Replace = 0,
  eLinkVerb_New     = 1,
  eLinkVerb_Embed   = 2,
  eLinkVerb_Undefined = 3
};

/**
 * Interface used for handling clicks on links
 */
class nsILinkHandler : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ILINKHANDLER_IID)

  /**
   * Process a click on a link.
   *
   * @param aContent the content for the frame that generated the trigger
   * @param aVerb the verb to use when the link is triggered
   * @param aURI a URI object that defines the destination for the link
   * @param aTargetSpec indicates where the link is targeted (may be an empty
   *        string)
   * @param aPostDataStream the POST data to send
   * @param aHeadersDataStream ???
   */
  NS_IMETHOD OnLinkClick(nsIContent* aContent, 
                         nsLinkVerb aVerb,
                         nsIURI* aURI,
                         const PRUnichar* aTargetSpec,
                         nsIInputStream* aPostDataStream = 0,
                         nsIInputStream* aHeadersDataStream = 0) = 0;

  /**
   * Process a click on a link.
   *
   * Works the same as OnLinkClick() except it happens immediately rather than
   * through an event.
   *
   * @param aContent the content for the frame that generated the trigger
   * @param aVerb the verb to use when the link is triggered
   * @param aURI a URI obect that defines the destination for the link
   * @param aTargetSpec indicates where the link is targeted (may be an empty
   *        string)
   * @param aPostDataStream the POST data to send
   * @param aHeadersDataStream ???
   * @param aDocShell (out-param) the DocShell that the request was opened on
   * @param aRequest the request that was opened
   */
  NS_IMETHOD OnLinkClickSync(nsIContent* aContent, 
                             nsLinkVerb aVerb,
                             nsIURI* aURI,
                             const PRUnichar* aTargetSpec,
                             nsIInputStream* aPostDataStream = 0,
                             nsIInputStream* aHeadersDataStream = 0,
                             nsIDocShell** aDocShell = 0,
                             nsIRequest** aRequest = 0) = 0;

  /**
   * Process a mouse-over a link.
   *
   * @param aContent the linked content.
   * @param aURI an URI object that defines the destination for the link
   * @param aTargetSpec indicates where the link is targeted (it may be an empty
   *        string)
   */
  NS_IMETHOD OnOverLink(nsIContent* aContent, 
                        nsIURI* aURLSpec,
                        const PRUnichar* aTargetSpec) = 0;

  /**
   * Process the mouse leaving a link.
   */
  NS_IMETHOD OnLeaveLink() = 0;
      

  /**
   * Get the state of a link to a given absolute URL
   */
  NS_IMETHOD GetLinkState(const nsACString& aLinkURI, nsLinkState& aState) = 0;
};

#endif /* nsILinkHandler_h___ */
