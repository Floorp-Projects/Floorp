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

#define NS_WindowMediator_CID \
{ 0x0659cb83, 0xfaad, 0x11d2, { 0x8e, 0x19, 0xb2, 0x06, 0x62, 0x0a, 0x65, 0x7c } }

#define NS_IWindowMediator_IID \
{ 0x0659cb81, 0xfaad, 0x11d2, { 0x8e, 0x19, 0xb2, 0x06, 0x62, 0x0a, 0x65, 0x7c } }




class nsIDOMWindow;
class nsString;
class nsIWindowMediator: public nsIRDFDataSource
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IWindowMediator_IID; return iid; }

/*
		iterate from inStartWindow to the end of the list looking for the next window of type inWindoType
		if inStartWindow is null, start from the top of the list
		if inWindowType is null, find all window types
		outWindow will be null if no more windows can be found
	*/
	NS_IMETHOD GetNextWindow( nsIDOMWindow* inStartWindow, nsString* inWindowType,
							  nsIDOMWindow** outWindow ) = 0;
	
	/*
		Returns the window of type inType ( if null return any window type ) which has the most recent
		time stamp
	*/
	NS_IMETHOD GetMostRecentWindow( nsString* inType, nsIDOMWindow** outWindow ) =  0;
	
	/*
		Get window given an RDF resource.
	*/
		NS_IMETHOD GetWindowForResource( nsIRDFResource* inResource, nsIDOMWindow** outWindow ) = 0;
	// Eventually I would like to get the type out of the XUL
	NS_IMETHOD RegisterWindow( nsIDOMWindow* inWindow, const nsString* inType) = 0;
	
	/* remove the window from the list */
	NS_IMETHOD UnregisterWindow( nsIDOMWindow* inWindow ) =  0;
	
	/* Call when the window gains focus. Used to determine the most recent window */
	NS_IMETHOD UpdateWindowTimeStamp( nsIDOMWindow* inWindow ) =  0;
	
	/* 
		Fetching this from the DOM currently( 4/25/99) doesn't doesn't do what I want since we end up with the XUL
		title rather than the content webshell  
	*/
	NS_IMETHOD UpdateWindowTitle( nsIDOMWindow* inWindow, const nsString& inTitle ) = 0;
	
};

nsresult NS_NewWindowMediator(nsIWindowMediator** result);
#endif 
