/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#define MOZILLA_FIVE

/*--------------------------------------------------------------------------*/
/*			----  Resource and Command Code Base IDs  ----					*/
/*		In Communicator 4.x, this was cmd/macfe/include/resae.h				*/
/*		See also xpfe/bootstrap/nsAppleEvents.r for the res definitions		*/
/*--------------------------------------------------------------------------*/	


//	----  Base Values for Apple Event Command sets
//----------------------------------------------------------------------------------------

#define		kSpyGlass_CmdBase			5000		// Base value for SpyGlass commands
#define		kURLSuite_CmdBase			5100		// Base value for URL commands

//	----  aedt Resource IDs used to associate Commands with Event Suite and ID pairs
//----------------------------------------------------------------------------------------

enum	{
	kRequired_aedtResID	= 128,			// Resource ID for Required suite's aedt resource
	kCore_aedtResID,					// Resource ID for Core suite's aedt resource
	kMisc_aedtResID,					// Resource ID for Misc suite's aedt resource
	kPowerPlant_aedtResID,				// Resource ID for PowerPlant suite's aedt resource
	kURLSuite_aedtResID,				// Resource ID for URL suite's aedt resource
	kSpyGlass_aedtResID					// Resource ID for SpyGlass suite's aedt resource
};

/*--------------------------------------------------------------------------*/
/*					----  Event Command Codes  ----							*/
/*--------------------------------------------------------------------------*/	

/*----------------------------------------------------------------------------
	The Apple Events Template resource 'aedt' (found in Types.r)	
	is used to associate an Event Class and Event ID with a
	unique integer value.  These integer values are private to the
	application processing the events.
	
	restriction:  PowerPlant uses integer valuse below 4000									
----------------------------------------------------------------------------*/


//	----  World Wide Web / Spyglass Suite
//----------------------------------------------------------------------------------------
enum	{
	AE_OpenURL = kSpyGlass_CmdBase // OpenURL								WWW! OURL

#ifndef MOZILLA_FIVE
,	AE_RegisterViewer				// RegisterViewer						WWW! RGVW
,	AE_UnregisterViewer				// UnregisterViewer						WWW! UNRV
,	AE_ShowFile						// ShowFile								WWW! SHWF
,	AE_ParseAnchor					// ParseAnchor							WWW! PRSA
,	AE_RegisterURLEcho				// Register URL echo					WWW! RGUE
,	AE_UnregisterURLEcho			// Unregister URL echo					WWW! UNRU
,	AE_SpyActivate					// Activate								WWW! ACTV
,	AE_SpyListWindows				// ListWindows							WWW! LSTW
,	AE_GetWindowInfo				// GetWindowInfo						WWW! WNFO
,	AE_RegisterWinClose				// RegisterWindowClose					WWW! RGWC
,	AE_UnregisterWinClose			// UnregisterWindowClose				WWW! UNRC
,	AE_RegisterProtocol				// RegisterProtocol						WWW! RGPR
,	AE_UnregisterProtocol			// UnregisterProtocol					WWW! UNRP
,	AE_CancelProgress				// Cancel download						WWW! CNCL
,	AE_FindURL						// Find the URL for the file			WWW! FURL
#endif // MOZILLA_FIVE
};

//	----  Netscape Experimental Suite and Macintosh URL suite
//----------------------------------------------------------------------------------------
enum	{

#ifdef MOZILLA_FIVE
	AE_GetURL = kURLSuite_CmdBase	// GetURL								GURL GURL
,	AE_DoJavascript					// Do Javascript						MOSS jscr
#else
	AE_GetWD = kURLSuite_CmdBase	// Get working directory of app  		MOSS WURL
,	AE_OpenBookmark					// Open bookmarks						MOSS book
,	AE_ReadHelpFile					// Read help file						MOSS help
,	AE_Go							// Go									MOSS gogo
,	AE_OpenProfileManager			// Launch app with user profile mgr		MOSS prfl
,	AE_GetURL						// GetURL								GURL GURL
,	AE_OpenAddressBook				// Open Address Book					MOSS addr
,	AE_OpenComponent				// Open a component						MOSS cpnt
,	AE_GetActiveProfile				// Get the name of the active profile	MOSS upro
,	AE_HandleCommand				// Handle a command ae					MOSS ncmd
,	AE_GetProfileImportData			// Handle a request from import module	MOSS Impt
,	AE_OpenGuestMode				// Open in guest (roaming) kiosk mode	MOSS gues
#endif // MOZILLA_FIVE
};


//	----  The so called Option Suite.  Never implemented, left here for historical purposes
//----------------------------------------------------------------------------------------

#define AE_GetOption			4016	// GetOption
#define AE_SetOption			4017	// SetOption
#define AE_ListOptions			4018	// ListOptions

//	----  These are supposed to be "Events We Send" but I don't see these constants used
//----------------------------------------------------------------------------------------

#define AE_ViewDocFile			4019	// ViewDocFile
#define AE_BeginProgress		4020	// Begin progress
#define AE_SetProgressRange		4021	// Set progress range
#define AE_MakingProgress		4022	// Making progress
#define AE_EndProgress			4023	// End progress
#define AE_QueryViewer			4024	// Query viewer
#define AE_URLEcho				4026	// URL echo
#define AE_WindowClose			4027	// WindowClose


/*----------------------------------------------------------------------------
	List of Suites, Event IDs, and (I guess) parameters for events.
	This information is public and needed by any application wanting
	to send Apple Events to our application.						
----------------------------------------------------------------------------*/

/*********************************************************************************
 * Netscape suite
 * Event: Go
 * Arguments: keyDirectObject (objectSpecifier for a window
 *            direction       (stil in flux, currently back, forward, home, and again)
 *								should really be a history object
 *********************************************************************************/
// Event codes
#define AE_www_suite 					'MOSS'

#define AE_www_doJavaScript				'jscr'  // Execute a javascript string

#ifndef MOZILLA_FIVE
#define AE_www_workingURL				'wurl'	// Get working URL

#define AE_www_go	 					'gogo'	// keyDirectObject HWIN, direction 'dire'
// direction can be kAENext, kAEPrevious, AE_www_go_again, AE_www_go_home
#define AE_www_go_direction 			'dire'	// directions
#define AE_www_go_again 				'agai'	// keyDirectObject HWIN 
#define AE_www_go_home					'home'	// keyDirectObject HWIN 
#define AE_www_super_reload				'srld'	// keyDirectObject HWIN 

#define AE_www_openBookmark				'book'	// Open Bookmark file
#define AE_www_openAddressBook			'addr'	// Open Address Book

#define AE_www_ReadHelpFile  			'help'	// keyDirectObject is the file
#define AE_www_ReadHelpFileID 			'idid'	// Help file id. If none, use "DEFAULT"
#define AE_www_ReadHelpFileSearchText 	'sear' // Search text, no default

#define AE_www_ProfileManager			'prfl'	//obsolete
#define AE_www_GuestMode				'gues'  // Open in guest (roaming only) mode for kiosks

//Component stuff
#define AE_www_openComponent			'cpnt'

#define AE_www_comp_navigator			'navg'
#define AE_www_comp_inbox				'inbx'
#define AE_www_comp_collabra			'colb'
#define AE_www_comp_composer			'cpsr'
#define AE_www_comp_conference			'conf'
#define AE_www_comp_calendar			'cald'
#define AE_www_comp_ibmHostOnDemand		'ibmh'
#define AE_www_comp_netcaster			'netc'

//Handle a command
#define AE_www_handleCommand			'hcmd'

//Get active profile
#define AE_www_getActiveProfile			'upro'

// Handle request from an external import module for relevant data
#define AE_www_getImportData			'Impt'
#endif // MOZILLA_FIVE

// Objects
#define AE_www_typeWindow				'HWIN'
// window properties
#define AE_www_typeWindowURL			'curl'	// Property: current URL
#define AE_www_typeWindowID				'wiid'	// unique ID
#define AE_www_typeWindowBusy 			'busy'	// Are we busy
// application properties
#define AE_www_typeApplicationAlert 	'ALAP'
#define AE_www_typeKioskMode			'KOSK'	// Kiosk mode


/*********************************************************************************
 * URL suite
 * Standard Mac "GetURL suite, as defined by John Norstad and others
 * Look around ftp://ftp.acns.nwu.edu/pub/newswatcher/ for official spec
 * Event: GetURL
 * Arguments: 	keyDirectObject (typeText, the url
 *            	to (destination) (typeFSS optional file to save to)
 *				from (refererer) (typeText, the referer)
 *				with (window) (typeObjectSpec, the window)
 *********************************************************************************/
#define AE_url_suite				'GURL'

// Event codes
#define AE_url_getURL 				'GURL'	// keyDirectObject typeChar URL, 
// AE_www_typeWindow window window	to load the url in
#define AE_url_getURLdestination 	'dest'	
#define AE_url_getURLrefererer 		'refe'
#define AE_url_getURLname			'name'	// window name









/*********************************************************************************
 * "Spyglass" suite
 * http://www.spyglass.com:4040/newtechnology/integration/iapi.htm
 *
 * Accepted events:
 * Event:	OpenURL
 * Arguments: 
 *			S: keyDirectObject typeChar the url
 *			S: typeFSS into
 *			S: typeLongInteger windowID (unique window ID for applescript)
 *			typeLongInteger flags -- unused
 *			S: typeWildCard post data -- you can post a form
 *			S: typeChar MIME type	-- for post. Defaults to application/x-www-form-urlencoded
 *			S: typePSN Progress app
 *	Reply:	windowID
 *
 * Event:	RegisterViewer
 * Arguments:
 *			keyDirectObject typeApplSignature
 *			typeChar		MIME type
 *	Reply:	bool success
 *
 * Event:	UnRegisterViewer
 *			keyDirectObject typeApplSignature
 *			typeChar		MIME type
 *	Reply:	none
 *
 * Event: RegisterURLEcho
 * 			keyDirectObject typeApplSignature (optional). Otherwise, sender is used
 *	Reply:	typeBoolean on success, errAECoercionFail if already registered
 *
 * Event: UnregisterURLEcho
 *			keyDirectObject typeApplSignature (optional). Otherwise, sender is used
 *
 *  SENDING:
 * Event:	ViewDocFile
 * Arguments:
 *			keyDirectObject	typeAlias file spec
 *			typeChar	url
 *			typeChar	mime type
 *			typeLongInteger window id
 *	Reply:	none
 *
 * Event:	BeginProgress
 * Arguments:
 *			keyDirectObject typeLongInteger windowID
 *			typeChar message
 *  Reply:	typeLongInteger transactionID
 *
 * Event:	SetProgressRange
 * Arguments:
 *			keyDirectObject typeLongInteger transactionID
 *			typeLongInteger	max value. -1 if the value is unknown
 *  Reply:	none
 *
 * Event:	MakingProgress
 * Arguments:
 *			keyDirectObject typeLongInteger transactionID
 *			typeText message
 *			typeLongInteger current value of the transaction
 *	Reply: typeBoolean cancel
 *
 * Event:	EndProgress
 * Arguments:
 *			keyDirectObject typeLongInteger transactionID
 *	Reply: none
 *
 * Event:	QueryViewer
 * Arguments:
 *			keyDirectObject typeChar url
 *			typeChar MIME type
 *  Reply:	typeFSS fileSpec
 *
 * Event:	ShowFile
 * Arguments:
 *			keyDirectObject typeAlias -- the file
 *
 *********************************************************************************/
#define AE_spy_receive_suite		'WWW!'
#define AE_spy_send_suite			'WWW?'

// ===================== RECEIVING ==========================

// ================== Miscelaneous events

// ****************** OpenURL
#define AE_spy_openURL		'OURL'	// typeChar OpenURL

#define AE_spy_openURL_flag 'FLGS'	// typeLongInteger flags
#define AE_spy_openURL_wind	'WIND'	// typeLongInteger windowID

#if 0 // Not supported in Mozilla

#define AE_spy_openURL_into 'INTO'	// typeFSS into
#define AE_spy_openURL_post	'POST'	// typeWildCard post data
#define AE_spy_openURL_mime 'MIME'	// typeChar MIME type
#define AE_spy_openURL_prog 'PROG'	// typePSN Progress app

// ****************** ShowFile
#define AE_spy_showFile		'SHWF'	// typeAlias file spec
#define AE_spy_showFile_mime 'MIME'	// typeChar MIME type
#define AE_spy_showFile_win	'WIND'	// WindowID
#define AE_spy_showFile_url 'URL '	// URL
// ****************** ParseAnchor
#define AE_spy_parse		'PRSA'	// typeChar main URL
#define AE_spy_parse_rel	'RELA'	// typeChar relative URL

// ****************** Progress (receiving)
#define AE_spy_CancelProgress 'CNCL' // typeLongInteger transactionID
#define AE_spy_CancelProgress_win 'WIND' // typeLongInteger windowID

// ****************** FindURL
#define AE_spy_findURL		'FURL'	// typeFSS file spec. Returns the URL of the file

// =================== Windows

// ****************** Activate
#define AE_spy_activate				'ACTV'	// typeLong window ID
#define AE_spy_activate_flags		'FLGS'	// typeLong unused flags
// ****************** ListWindows
#define AE_spy_listwindows			'LSTW'	// no arguments
// ****************** GetWindowInfo
#define AE_spy_getwindowinfo		'WNFO'	// typeLong window

//
// ================== Registration events
//

// ****************** RegisterURLEcho
#define AE_spy_registerURLecho		'RGUE'	// typeApplSignature application
// ****************** UnregisterURLEcho
#define AE_spy_unregisterURLecho	'UNRU'	// typeApplSignature application

// ****************** RegisterViewer
#define AE_spy_registerViewer		'RGVW'	//  typeSign	Application
#define AE_spy_registerViewer_mime	'MIME'	// typeChar		Mime type
#define AE_spy_registerViewer_flag	'MTHD'	// typeLongInteger Flags
#define AE_spy_registerViewer_ftyp	'FTYP'	// file type
// ****************** UnregisterViewer
#define AE_spy_unregisterViewer		'UNRV'	// typeApplSignature application
#define AE_spy_unregisterViewer_mime 'MIME'	// MIME type

// ****************** Register protocol
#define AE_spy_register_protocol		'RGPR'	// typeApplSignature application
#define AE_spy_register_protocol_pro 	'PROT'	// typeChar protocol
// ****************** Unregister protocol
#define AE_spy_unregister_protocol		'UNRP'	// typeApplSignature application
#define AE_spy_register_protocol_pro 	'PROT'	// typeChar protocol

// ****************** RegisterWindowClose
#define AE_spy_registerWinClose		'RGWC'	// typeApplSignature application
#define AE_spy_registerWinClose_win	'WIND'// typeLong window
// ****************** UnregisterWindowClose
#define AE_spy_unregisterWinClose		'UNRC'	// typeApplSignature application
#define AE_spy_unregisterWinClose_win	'WIND'// typeLong window


// ****************** SetOption
#define AE_spy_setOption			'SOPT'	// typeChar option name
#define AE_spy_setOption_value		'OPTV'	// type depends upon the option
// ****************** GetOption
#define AE_spy_getOption			'GOPT'	// typeChar option name
// ****************** ListOptions
#define AE_spy_listOptions			'LOPT'	// no arguments

//
// ===================== SENDING ============================
//
// ViewDocFile
#define AE_spy_viewDocFile			'VDOC'	// typeAlias	fileSpec
#define AE_spy_viewDocFile_url		'URL '	// typeChar	url
#define AE_spy_viewDocFile_mime		'MIME'	// typeChar mimeType
#define AE_spy_viewDocFile_wind		'WIND'	// typeLongInteger Window ID
// BeginProgress
#define AE_spy_beginProgress		'PRBG'	// typeLongInteger windowID
#define AE_spy_beginProgress_msg	'PMSG'	// typeChar message
// SetProgressRange
#define AE_spy_setProgressRange		'PRSR'	// typeLongInteger transactionID
#define AE_spy_setProgressRange_max 'MAXV'	// typeLongInteger max
// MakingProgress
#define AE_spy_makingProgress		'PRMK'	// typeLongInteger transactionID
#define AE_spy_makingProgress_msg	'PMSG'	// typeChar message
#define AE_spy_makingProgress_curr	'CURR'	// typeLongInteger current data size
// EndProgress
#define AE_spy_endProgress			'PREN'	// typeLongInteger transactionID
// QueryViewer
#define AE_spy_queryViewer			'QVWR'	// typeChar url
#define AE_spy_queryViewer_mime		'MIME'	// typeChar MIME type
// URLEcho
#define AE_spy_URLecho				'URLE'	// typeChar url
#define AE_spy_URLecho_mime			'MIME'	// typeChar MIME type
#define AE_spy_URLecho_win			'WIND'	// typeLongInt windowID
#define AE_spy_URLecho_referer		'RFRR'	// typeChar referer
// Window closed
#define AE_spy_winClosed			'WNDC'	// typeLong windowID
#define AE_spy_winClosedExiting		'EXIT'	// typeBoolean are we quitting?

#endif // 0 - not supported in Mozilla.

/*--------------------------------------------------------------------------*/
/*					---- Eudora Suite  ----									*/
/*--------------------------------------------------------------------------*/	

/*----------------------------------------------------------------------------
	Client applications can manipulate our Mail system to send, receive
	and do other mail operations by remote contro.
	We can also 
----------------------------------------------------------------------------*/

//	----  Class Definitions for objects Eudora can manipulate
//----------------------------------------------------------------------------------------

#define cEuMailfolder     'euMF'  // Class: 			folder for mailboxes and mail folders
#define pEuTopLevel       'euTL'  // Property boolean:  is top-level of Eudora Folder?
#define pEuFSS            'euFS'  // Property alias:  	FSS for file

#define cEuMailbox        'euMB'  /* mailbox */
#define pEuMailboxType    'euMT'  /* in, out, trash, ... */
#define pEuWasteSpace     'euWS'  /* space wasted in mailbox */
#define pEuNeededSpace    'euNS'  /* space needed by messages in mailbox */
#define pEuTOCFSS         'eTFS'  /* FSS for toc file (pEuFSS is for mailbox) */

#define cEuNotify         'eNot'  /* applications to notify */
                                  /* pEuFSS is the fsspec */

#define cEuMessage        'euMS'  /* message */
#define pEuPriority       'euPY'  /* priority */
#define pEuStatus         'euST'  /* message status */
#define pEuSender         'euSe'  /* sender */
#define pEuDate           'euDa'  /* date */
#define pEuSize           'euSi'  /* size */
#define pEuSubject        'euSu'  /* subject */
#define pEuOutgoing       'euOu'  /* outgoing? */
#define pEuSignature      'eSig'  /* signature? */
#define pEuWrap           'eWrp'  /* wrap? */
#define pEuFakeTabs       'eTab'  /* fake tabs? */
#define pEuKeepCopy       'eCpy'  /* keep copy? */
#define pEuHqxText        'eXTX'  /* HQX -> TEXT? */
#define pEuMayQP          'eMQP'  /* may use quoted-printable? */
#define pEuAttachType     'eATy'  /* attachment type; 0 double, 1 single, 2 hqx, 3 uuencode */
#define pEuShowAll        'eBla'  /* show all headers */
#define pEuTableId        'eTbl'  /* resource id of table */
#define pEuBody           'eBod'  /* resource id of table */
#define pEuSelectedText   'eStx'  /* the text selected now */
#define pEuWillFetch      'eWFh'  /* is on list to fetch next time */
#define pEuWillDelete     'eWDl'  /* is on list to delete next time */
#define pEuReturnReceipt  'eRRR'  /* return receipt requested */
#define pEuLabel          'eLbl'  /* label index */

#define cEuField          'euFd'  /* field in message */

#define cEu822Address     'e822'  /* RFC 822 address */

#define cEuTEInWin        'EuWT'  /* the teh of a window */
#define cEuWTEText        'eWTT'  /* text from the teh of a window */

#define cEuPreference     'ePrf'  /* a preference string */

#define kEudoraSuite      'CSOm'  /* Eudora suite */
#define keyEuNotify       'eNot'  /* Notify of new mail */
#define kEuNotify         keyEuNotify
#define kEuInstallNotify  'nIns'  /* install a notification */
#define kEuRemoveNotify   'nRem'  /* remove a notification */
#define keyEuWhatHappened 'eWHp'  /* what happened */
#define keyEuMessList     'eMLs'  /* Message list */

#define eMailArrive       'wArv'  /* mail has arrived */
#define eMailSent         'wSnt'  /* mail has been sent */
#define eWillConnect      'wWCn'  /* will connect */
#define eHasConnected     'wHCn'  /* has connected */

#define kEuReply          'eRep'  /* Reply */
#define keyEuToWhom       'eRWh'  /* Reply to anyone in particular? */
#define keyEuReplyAll     'eRAl'  /* Reply to all? */
#define keyEuIncludeSelf  'eSlf'  /* Include self? */
#define keyEuQuoteText    'eQTx'  /* Quote original message text? */

#define kEuForward        'eFwd'  /* Forward */

#define kEuRedirect       'eRdr'  /* Redirect */

#define kEuSalvage        'eSav'  /* Salvage a message */

#define kEuAttach         'eAtc'  /* Attach a document */
#define keyEuDocumentList 'eDcl'  /* List of dox to attach */

#define kEuQueue          'eQue'  /* Queue a message */
#define keyEuWhen         'eWhn'  /* When to send message */

#define kEuUnQueue        'eUnQ'  /* Unqueue a message */

#define kEuConnect        'eCon'  /* Connect (send/queue) */
#define keyEuSend         'eSen'
#define keyEuCheck        'eChk'
#define keyEuOnIdle       'eIdl'  /* wait until Eudora is idle? */

#define kEuNewAttach      'euAD'  /* attach document, new style */
#define keyEuToWhat       'euMS'  /* attach to what message? */

#define typeVDId          'VDId'  /* vref & dirid */

#define kIn               IN
#define kOut              OUT
#define kTrash            TRASH
#define KRegular          0



























