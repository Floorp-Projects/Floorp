/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef nsIRDFObserver_h__
#define nsIRDFObserver_h__

/*
  This file defines the interface for RDF observers.
*/

#include "nsISupports.h"
#include "rdf.h"


// 3cc75360-484a-11d2-bc16-00805f912fe7
#define NS_IRDFOBSERVER_IID \
{ \
    0x3cc75360, \
    0x484a, \
    0x11d2, \
  { 0xbc, 0x16, 0x00, 0x80, 0x5f, 0x91, 0x2f, 0xe7 } \
}

class nsIRDFDataSource;      

class nsIRDFObserver : public nsISupports
{
public:

  NS_IMETHOD HandleEvent(nsIRDFDataSource *source,
                         RDF_Event event) = 0;

};


#endif /* nsIRDFObserver_h__ */
