/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsIXPFCContentSink_h___
#define nsIXPFCContentSink_h___

#include "nsISupports.h"
#include "nsIWebViewerContainer.h"
//#include "nsIXPFCContentSinkContainer.h"

// c0718b70-7515-11d2-943f-006008268c31
#define NS_IXPFC_CONTENT_SINK_IID \
{ 0xc0c10130, 0x7515, 0x11d2, \
{ 0x94, 0x3f, 0x00, 0x60, 0x08, 0x26, 0x8c, 0x31 } }

class nsIXPFCContentSink : public nsISupports
{
public:
  NS_IMETHOD      Init() = 0;
  NS_IMETHOD      SetViewerContainer(nsIWebViewerContainer * aViewerContainer) = 0;
  //NS_IMETHOD      SetContentSinkContainer(nsIXPFCContentSinkContainer * aContentSinkContainer) = 0;
  NS_IMETHOD      SetContentSinkContainer(nsISupports * aContentSinkContainer) = 0;
};

#endif


