/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsNeckoUtil_h__
#define nsNeckoUtil_h__

#include "nsIURI.h"
#include "netCore.h"
#include "nsIInputStream.h"
#include "nsIStreamListener.h"
#include "nsIEventSinkGetter.h"

extern NS_NET nsresult
NS_NewURI(nsIURI* *result, const char* spec, nsIURI* baseURI = nsnull);

extern NS_NET nsresult
NS_OpenURI(nsIURI* uri, nsIInputStream* *result);

extern NS_NET nsresult
NS_OpenURI(nsIURI* uri, nsIStreamListener* aConsumer);

extern NS_NET nsresult
NS_MakeAbsoluteURI(const char* spec, nsIURI* base, char* *result);

////////////////////////////////////////////////////////////////////////////////
// Temporary stuff:

#include "nsIURL.h"

// XXX temporary, until we fix up the rest of the code to use URIs instead of URLs:
extern NS_NET nsresult
NS_NewURL(nsIURL* *result, const char* spec, nsIURL* baseURL = nsnull);

#endif // nsNeckoUtil_h__
