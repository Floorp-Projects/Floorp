/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#ifndef nsIWindowMediator_h__
#define nsIWindowMediator_h__

#include "nscore.h"
#include "nsIRDFDataSource.h"

#define NS_WINDOWMEDIATOR_CID \
{ 0x0659cb83, 0xfaad, 0x11d2, { 0x8e, 0x19, 0xb2, 0x06, 0x62, 0x0a, 0x65, 0x7c } }

#define NS_IWINDOWMEDIATOR_IID \
{ 0x0659cb81, 0xfaad, 0x11d2, { 0x8e, 0x19, 0xb2, 0x06, 0x62, 0x0a, 0x65, 0x7c } }




class nsIDOMWindow;
class nsIWebShellWindow;
class nsString;
class nsIWindowMediator: public nsIRDFDataSource
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IWINDOWMEDIATOR_IID; return iid; }

	/*
		iterates over the windows of type inType in the order that they were created. if inType  is null
		iterates over all windows
	*/
	NS_IMETHOD GetEnumerator( nsString* inType, nsISimpleEnumerator** outEnumerator ) =0;

	
	/*
		Returns the window of type inType ( if null return any window type ) which has the most recent
		time stamp
	*/
	NS_IMETHOD GetMostRecentWindow( nsString* inType, nsIDOMWindow** outWindow ) =  0;
	
	/*
		Get window given an RDF resource.
	*/
	NS_IMETHOD GetWindowForResource( nsIRDFResource* inResource, nsIDOMWindow** outWindow ) = 0;
	
	/* Add the webshellwindow to the list */
	NS_IMETHOD RegisterWindow( nsIWebShellWindow* inWindow) = 0;
	
	/* remove the window from the list */
	NS_IMETHOD UnregisterWindow( nsIWebShellWindow* inWindow ) =  0;
	
	/* Call when the window gains focus. Used to determine the most recent window */
	NS_IMETHOD UpdateWindowTimeStamp( nsIWebShellWindow* inWindow ) =  0;
	
	/* */
	NS_IMETHOD UpdateWindowTitle( nsIWebShellWindow* inWindow, const nsString& inTitle ) = 0;
	
};

nsresult NS_NewWindowMediatorFactory(nsIFactory** aResult);
#endif 
