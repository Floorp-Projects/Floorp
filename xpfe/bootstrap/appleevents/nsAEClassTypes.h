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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *  Simon Fraser <sfraser@netscape.com>
 */


#ifndef __AECLASSTYPES__
#define __AECLASSTYPES__


/* Verbs */
enum {

	kAEUnselect				= 'uslt'
};


/* Classes */
enum {
	kPrefsClassID				= 'PREF'
};

/* Prefs class properties */
enum {
	pPrefsUserName			= 'UNAM',
	pPrefsEmail				= 'EMIL',
	
	pWindowName				= 'NAME'			// not sure why this isn't in the headers
};


/* Classes */
enum {
	
	cMailServer				= 'SMTP',
	
	cMessageWindow			= 'MWIN',
	cNewGroupsWindow			= 'NWIN',
	cFullGroupsWindow			= 'FWIN'
	
};

/* Classes for coercion */
enum {
	cWrappedText				= 'cWtx'
};




#endif /* __AECLASSTYPES__ */
