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
#include "nsILoadGroup.h"
#include "nsIEventSinkGetter.h"
#include "nsString.h"

#ifdef XP_MAC
	#define NECKO_EXPORT(returnType)	PR_PUBLIC_API(returnType)
#else
	#define NECKO_EXPORT(returnType)	returnType
#endif

// Warning: These functions should NOT be defined with NS_NET because
// the intention is that they'll be linked with the library/DLL that 
// uses them. NS_NET is for DLL imports.

extern nsresult
NS_NewURI(nsIURI* *result, const char* spec, nsIURI* baseURI = nsnull);

extern nsresult
NS_NewURI(nsIURI* *result, const nsString& spec, nsIURI* baseURI = nsnull);

extern nsresult
NS_OpenURI(nsIChannel* *result, nsIURI* uri, nsILoadGroup *aGroup,
           nsIEventSinkGetter *eventSinkGetter = nsnull);

extern nsresult
NS_OpenURI(nsIInputStream* *result, nsIURI* uri);

extern nsresult
NS_OpenURI(nsIStreamListener* aConsumer, nsISupports* context, nsIURI* uri, 
           nsILoadGroup *aGroup);

extern nsresult
NS_MakeAbsoluteURI(const char* spec, nsIURI* baseURI, char* *result);

extern nsresult
NS_MakeAbsoluteURI(const nsString& spec, nsIURI* baseURI, nsString& result);

extern nsresult
NS_NewLoadGroup(nsISupports* outer, nsIStreamObserver* observer,
                nsILoadGroup* parent, nsILoadGroup* *result);

extern nsresult
NS_NewPostDataStream(PRBool isFile, const char *data, PRUint32 encodeFlags,
                     nsIInputStream **result);

#endif // nsNeckoUtil_h__
