/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#ifndef nsIXPFCXMLContentSink_h___
#define nsIXPFCXMLContentSink_h___

#include "nsISupports.h"
#include "nsIWebViewerContainer.h"
#include "nsIXPFCCanvas.h"

//c47f83e0-4910-11d2-924a-00805f8a7ab6
#define NS_IXPFC_XML_CONTENT_SINK_IID   \
{ 0xc47f83e0, 0x4910, 0x11d2,    \
{ 0x92, 0x4a, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

class nsIXPFCXMLContentSink : public nsISupports
{

public:

  NS_IMETHOD                 Init() = 0 ;
  NS_IMETHOD                 SetViewerContainer(nsIWebViewerContainer * aViewerContainer) = 0;
  NS_IMETHOD                 SetRootCanvas(nsIXPFCCanvas * aCanvas) = 0;

};

#endif /* nsIXPFCXMLContentSink_h___ */
