/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef	nsIRDFFTP_h__
#define	nsIRDFFTP_h__

#include "nscore.h"
#include "nsISupports.h"
#include "nsIRDFNode.h"



#define NS_IRDFFTPDATAOURCE_IID \
{ 0x1222e6f0, 0xa5e3, 0x11d2, { 0x8b, 0x7c, 0x00, 0x80, 0x5f, 0x8a, 0x7d, 0xb7 } }

class nsIRDFFTPDataSource : public nsIRDFDataSource
{
public:
};


#define NS_IRDFFTPDATASOURCECALLBACK_IID \
{ 0x204a1a00, 0xa5e4, 0x11d2, { 0x8b, 0x7c, 0x00, 0x80, 0x5f, 0x8a, 0x7d, 0xb8 } }

class nsIRDFFTPDataSourceCallback : public nsIStreamListener
{
public:
};


#endif // nsIRDFFTP_h__
