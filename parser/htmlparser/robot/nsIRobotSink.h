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
#ifndef nsIRobotSink_h___
#define nsIRobotSink_h___

#include "nsIHTMLContentSink.h"
class nsIURL;
class nsIRobotSinkObserver;

/* 61256800-cfd8-11d1-9328-00805f8add32 */
#define NS_IROBOTSINK_IID     \
{ 0x61256800, 0xcfd8, 0x11d1, \
  {0x93, 0x28, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

class nsIRobotSink : public nsIHTMLContentSink {
public:
  NS_IMETHOD Init(nsIURL* aDocumentURL) = 0;
  NS_IMETHOD AddObserver(nsIRobotSinkObserver* aObserver) = 0;
  NS_IMETHOD RemoveObserver(nsIRobotSinkObserver* aObserver) = 0;
};

extern nsresult NS_NewRobotSink(nsIRobotSink** aInstancePtrResult);

#endif /* nsIRobotSink_h___ */
