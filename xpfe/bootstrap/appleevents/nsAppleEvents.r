/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


/*----------------------------------------------------------------------------
	The Apple Events Template resource 'aedt' (found in Types.r)	
	is used to associate an Event Class and Event ID with a
	unique integer value.  These integer values are private to the
	application processing the events.
	
	restriction:  PowerPlant uses integer valuse below 4000	
	All data for aetds is now centrally located in resae.h								
----------------------------------------------------------------------------*/


#include	"types.r"
// #include	"AEUserTermTypes.r"
#include	"nsAppleEvents.h"	// Provides AE Types, IDs, and AE_RefNums
			// Found in mozilla/xpfe/appshell/public

/*--------------------------------------------------------------------------*/
/*			-----  World Wide Web - Spyglass Suite   -----					*/
/*--------------------------------------------------------------------------*/	

resource 'aedt' (kSpyGlass_aedtResID, "World-Wide-Web Suite (Spyglass spec)") {
	{
		AE_spy_receive_suite, AE_spy_openURL, 				AE_OpenURL,					// WWW! OURL
#ifndef MOZILLA_FIVE
		AE_spy_receive_suite, AE_spy_registerViewer, 		AE_RegisterViewer,			// WWW! RGVW
		AE_spy_receive_suite, AE_spy_unregisterViewer, 		AE_UnregisterViewer,		// WWW! UNRV
		AE_spy_receive_suite, AE_spy_showFile, 				AE_ShowFile,				// WWW! SHWF
		AE_spy_receive_suite, AE_spy_parse, 				AE_ParseAnchor,				// WWW! PRSA
		AE_spy_receive_suite, AE_spy_registerURLecho, 		AE_RegisterURLEcho,			// WWW! RGUE
		AE_spy_receive_suite, AE_spy_unregisterURLecho,		AE_UnregisterURLEcho,		// WWW! UNRU
		AE_spy_receive_suite, AE_spy_activate, 				AE_SpyActivate,				// WWW! ACTV
		AE_spy_receive_suite, AE_spy_listwindows, 			AE_SpyListWindows,			// WWW! LSTW
		AE_spy_receive_suite, AE_spy_getwindowinfo, 		AE_GetWindowInfo,			// WWW! WNFO
		AE_spy_receive_suite, AE_spy_registerWinClose,		AE_RegisterWinClose,		// WWW! RGWC
		AE_spy_receive_suite, AE_spy_unregisterWinClose, 	AE_UnregisterWinClose,		// WWW! UNRC
		AE_spy_receive_suite, AE_spy_register_protocol, 	AE_RegisterProtocol,		// WWW! RGPR
		AE_spy_receive_suite, AE_spy_unregister_protocol, 	AE_UnregisterProtocol,		// WWW! UNRP
		AE_spy_receive_suite, AE_spy_CancelProgress, 		AE_CancelProgress,			// WWW! CNCL
		AE_spy_receive_suite, AE_spy_findURL, 				AE_FindURL					// WWW! FURL
#endif // MOZILLA_FIVE
	}
};

/*--------------------------------------------------------------------------*/
/*			-----  Netscape and Macintosh Std URL Suites   -----			*/
/*--------------------------------------------------------------------------*/	

resource 'aedt' (kURLSuite_aedtResID, "Netscape & URL suite") {
	{
#ifdef MOZILLA_FIVE
		AE_url_suite, AE_url_getURL, 			AE_GetURL								// GURL GURL
	,	AE_www_suite, AE_www_doJavaScript,		AE_DoJavascript							// MOSS jscr
#else
		AE_www_suite, AE_www_workingURL, 		AE_GetWD								// MOSS wurl
	,	AE_www_suite, AE_www_openBookmark,		AE_OpenBookmark							// MOSS book
	,	AE_www_suite, AE_www_ReadHelpFile, 		AE_ReadHelpFile							// MOSS help
	,	AE_www_suite, AE_www_go, 				AE_Go									// MOSS gogo
	,	AE_url_suite, AE_url_getURL, 			AE_GetURL								// GURL GURL
	,	AE_www_suite, AE_www_ProfileManager,	AE_OpenProfileManager					// MOSS prfl
	,	AE_www_suite, AE_www_openAddressBook,	AE_OpenAddressBook						// MOSS addr
	,	AE_www_suite, AE_www_openComponent,		AE_OpenComponent						// MOSS cpnt
	,	AE_www_suite, AE_www_getActiveProfile,	AE_GetActiveProfile						// MOSS upro
	,	AE_www_suite, AE_www_handleCommand,		AE_HandleCommand						// MOSS hcmd
	,	AE_www_suite, AE_www_getImportData,		AE_GetProfileImportData					// MOSS Impt
	,	AE_www_suite, AE_www_GuestMode,			AE_OpenGuestMode						// MOSS gues
#endif // MOZILLA_FIVE
	}
};

/*--------------------------------------------------------------------------*/
/*			Required Suite, we will leave this hard wired for now			*/
/*--------------------------------------------------------------------------*/	

resource 'aedt' (kRequired_aedtResID, "Required Suite") {
	{
		'aevt', 'oapp', 1001,
		'aevt', 'odoc', 1002,
		'aevt', 'pdoc', 1003,
		'aevt', 'quit', 1004
	}
};

/*--------------------------------------------------------------------------*/
/*			Core Suite, we will leave this hard wired for now				*/
/*--------------------------------------------------------------------------*/	

resource 'aedt' (kCore_aedtResID, "Core Suite") {
	{

		'core', 'clon', 2001,
		'core', 'clos', 2002,
		'core', 'cnte', 2003,
		'core', 'crel', 2004,
		'core', 'delo', 2005,
		'core', 'doex', 2006,
		'core', 'qobj', 2007,
		'core', 'getd', 2008,
		'core', 'dsiz', 2009,
		'core', 'gtei', 2010,
		'core', 'move', 2011,
		'core', 'save', 2012,
		'core', 'setd', 2013
	}
};

/*--------------------------------------------------------------------------*/
/*			Misc Standards, we will leave this hard wired for now			*/
/*--------------------------------------------------------------------------*/	

resource 'aedt' (kMisc_aedtResID, "Misc Standards") {
	{
		'aevt', 'obit', 3001,
		'misc', 'begi', 3002,
		'misc', 'copy', 3003,
		'misc', 'cpub', 3004,
		'misc', 'cut ', 3005,
		'misc', 'dosc', 3006,
		'misc', 'edit', 3007,
		'misc', 'endt', 3008,
		'misc', 'imgr', 3009,
		'misc', 'isun', 3010,
		'misc', 'mvis', 3011,
		'misc', 'past', 3012,
		'misc', 'redo', 3013,
		'misc', 'rvrt', 3014,
		'misc', 'ttrm', 3015,
		'misc', 'undo', 3016
	}
};





