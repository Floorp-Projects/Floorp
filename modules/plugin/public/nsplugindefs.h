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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

////////////////////////////////////////////////////////////////////////////////
/**
 * <B>INTERFACE TO NETSCAPE COMMUNICATOR PLUGINS (NEW C++ API).</B>
 *
 * <P>This superscedes the old plugin API (npapi.h, npupp.h), and 
 * eliminates the need for glue files: npunix.c, npwin.cpp and npmac.cpp that
 * get linked with the plugin. You will however need to link with the "backward
 * adapter" (badapter.cpp) in order to allow your plugin to run in pre-5.0
 * browsers. 
 *
 * <P>See nsplugin.h for an overview of how this fits with the 
 * overall plugin architecture.
 */
////////////////////////////////////////////////////////////////////////////////

#ifndef nsplugindefs_h___
#define nsplugindefs_h___

#ifdef __OS2__
#pragma pack(1)
#endif

#ifdef XP_MAC
	#include <Quickdraw.h>
	#include <Events.h>
#endif

#ifdef XP_UNIX
	#include <X11/Xlib.h>
	#include <X11/Xutil.h>
#endif

#ifdef XP_PC
	#include <windef.h>
#endif

////////////////////////////////////////////////////////////////////////////////

/* The OS/2 version of Netscape uses RC_DATA to define the
   mime types, file extentions, etc that are required.
   Use a vertical bar to seperate types, end types with \0.
   FileVersion and ProductVersion are 32bit ints, all other
   entries are strings the MUST be terminated wwith a \0.

AN EXAMPLE:

RCDATA NS_INFO_ProductVersion { 1,0,0,1,}

RCDATA NS_INFO_MIMEType    { "video/x-video|",
                             "video/x-flick\0" }
RCDATA NS_INFO_FileExtents { "avi|",
                             "flc\0" }
RCDATA NS_INFO_FileOpenName{ "MMOS2 video player(*.avi)|",
                             "MMOS2 Flc/Fli player(*.flc)\0" }

RCDATA NS_INFO_FileVersion       { 1,0,0,1 }
RCDATA NS_INFO_CompanyName       { "Netscape Communications\0" }
RCDATA NS_INFO_FileDescription   { "NPAVI32 Extension DLL\0"
RCDATA NS_INFO_InternalName      { "NPAVI32\0" )
RCDATA NS_INFO_LegalCopyright    { "Copyright Netscape Communications \251 1996\0"
RCDATA NS_INFO_OriginalFilename  { "NVAPI32.DLL" }
RCDATA NS_INFO_ProductName       { "NPAVI32 Dynamic Link Library\0" }

*/


/* RC_DATA types for version info - required */
#define NS_INFO_ProductVersion      1
#define NS_INFO_MIMEType            2
#define NS_INFO_FileOpenName        3
#define NS_INFO_FileExtents         4

/* RC_DATA types for version info - used if found */
#define NS_INFO_FileDescription     5
#define NS_INFO_ProductName         6

/* RC_DATA types for version info - optional */
#define NS_INFO_CompanyName         7
#define NS_INFO_FileVersion         8
#define NS_INFO_InternalName        9
#define NS_INFO_LegalCopyright      10
#define NS_INFO_OriginalFilename    11

#ifndef RC_INVOKED

////////////////////////////////////////////////////////////////////////////////
// Structures and definitions

#ifdef XP_MAC
#pragma options align=mac68k
#endif

typedef const char*     nsMIMEType;

struct nsByteRange {
    PRInt32             offset; 	/* negative offset means from the end */
    PRUint32            length;
    struct nsByteRange* next;
};

struct nsRect {
    PRUint16            top;
    PRUint16            left;
    PRUint16            bottom;
    PRUint16            right;
};

////////////////////////////////////////////////////////////////////////////////
// Unix specific structures and definitions

#ifdef XP_UNIX

#include <stdio.h>

/*
 * Callback Structures.
 *
 * These are used to pass additional platform specific information.
 */
enum nsPluginCallbackType {
    nsPluginCallbackType_SetWindow = 1,
    nsPluginCallbackType_Print
};

struct nsPluginAnyCallbackStruct {
    PRInt32     type;
};

struct nsPluginSetWindowCallbackStruct {
    PRInt32     type;
    Display*    display;
    Visual*     visual;
    Colormap    colormap;
    PRUint32    depth;
};

struct nsPluginPrintCallbackStruct {
    PRInt32     type;
    FILE*       fp;
};

#endif /* XP_UNIX */

////////////////////////////////////////////////////////////////////////////////

// List of variable names for which NPP_GetValue shall be implemented
enum nsPluginVariable {
    nsPluginVariable_NameString = 1,
    nsPluginVariable_DescriptionString,
    nsPluginVariable_WindowBool,        // XXX go away
    nsPluginVariable_TransparentBool,   // XXX go away?
    nsPluginVariable_JavaClass,         // XXX go away
    nsPluginVariable_WindowSize,
    nsPluginVariable_TimerInterval
    // XXX add MIMEDescription (for unix) (but GetValue is on the instance, not the class)
};

// List of variable names for which NPN_GetValue is implemented by Mozilla
enum nsPluginManagerVariable {
    nsPluginManagerVariable_XDisplay = 1,
    nsPluginManagerVariable_XtAppContext,
    nsPluginManagerVariable_NetscapeWindow,
    nsPluginManagerVariable_JavascriptEnabledBool,      // XXX prefs accessor api
    nsPluginManagerVariable_ASDEnabledBool,             // XXX prefs accessor api
    nsPluginManagerVariable_IsOfflineBool               // XXX prefs accessor api
};

////////////////////////////////////////////////////////////////////////////////

enum nsPluginType {
    nsPluginType_Embedded = 1,
    nsPluginType_Full
};

// XXX this can go away now
enum NPStreamType {
    NPStreamType_Normal = 1,
    NPStreamType_Seek,
    NPStreamType_AsFile,
    NPStreamType_AsFileOnly
};

#define NP_STREAM_MAXREADY	(((unsigned)(~0)<<1)>>1)

/*
 * The type of a NPWindow - it specifies the type of the data structure
 * returned in the window field.
 */
enum nsPluginWindowType {
    nsPluginWindowType_Window = 1,
    nsPluginWindowType_Drawable
};

struct nsPluginWindow {
    void*       window;         /* Platform specific window handle */
                                /* OS/2: x - Position of bottom left corner  */
                                /* OS/2: y - relative to visible netscape window */
    PRUint32    x;              /* Position of top left corner relative */
    PRUint32    y;              /*	to a netscape page.					*/
    PRUint32    width;          /* Maximum window size */
    PRUint32    height;
    nsRect      clipRect;       /* Clipping rectangle in port coordinates */
                                /* Used by MAC only.			  */
#ifdef XP_UNIX
    void*       ws_info;        /* Platform-dependent additonal data */
#endif /* XP_UNIX */
    nsPluginWindowType type;    /* Is this a window or a drawable? */
};

struct nsPluginFullPrint {
    PRBool      pluginPrinted;	/* Set TRUE if plugin handled fullscreen */
                                /*	printing							 */
    PRBool      printOne;       /* TRUE if plugin should print one copy  */
                                /*	to default printer					 */
    void*       platformPrint;  /* Platform-specific printing info */
};

struct nsPluginEmbedPrint {
    nsPluginWindow    window;
    void*       platformPrint;	/* Platform-specific printing info */
};

struct nsPluginPrint {
    nsPluginType      mode;     /* NP_FULL or nsPluginType_Embedded */
    union
    {
        nsPluginFullPrint     fullPrint;	/* if mode is NP_FULL */
        nsPluginEmbedPrint    embedPrint;	/* if mode is nsPluginType_Embedded */
    } print;
};

struct nsPluginEvent {

#if defined(XP_MAC)
    EventRecord* event;
    void*       window;

#elif defined(XP_PC)
    uint16      event;
    uint32      wParam;
    uint32      lParam;

#elif defined(XP_OS2)
    uint32      event;
    uint32      wParam;
    uint32      lParam;

#elif defined(XP_UNIX)
    XEvent      event;

#endif
};

#ifdef XP_MAC
typedef RgnHandle nsRegion;
#elif defined(XP_PC)
typedef HRGN nsRegion;
#elif defined(XP_UNIX)
typedef Region nsRegion;
#else
typedef void *nsRegion;
#endif

////////////////////////////////////////////////////////////////////////////////
// Mac-specific structures and definitions.

#ifdef XP_MAC

struct NPPort {
    CGrafPtr    port;   /* Grafport */
    PRInt32     portx;  /* position inside the topmost window */
    PRInt32     porty;
};

/*
 *  Non-standard event types that can be passed to HandleEvent
 */
#define getFocusEvent           (osEvt + 16)
#define loseFocusEvent          (osEvt + 17)
#define adjustCursorEvent       (osEvt + 18)
#define menuCommandEvent		(osEvt + 19)

#endif /* XP_MAC */

////////////////////////////////////////////////////////////////////////////////
// Error and Reason Code definitions

enum nsPluginError {
    nsPluginError_Base = 0,
    nsPluginError_NoError = 0,
    nsPluginError_GenericError,
    nsPluginError_InvalidInstanceError,
    nsPluginError_InvalidFunctableError,
    nsPluginError_ModuleLoadFailedError,
    nsPluginError_OutOfMemoryError,
    nsPluginError_InvalidPluginError,
    nsPluginError_InvalidPluginDirError,
    nsPluginError_IncompatibleVersionError,
    nsPluginError_InvalidParam,
    nsPluginError_InvalidUrl,
    nsPluginError_FileNotFound,
    nsPluginError_NoData,
    nsPluginError_StreamNotSeekable
};

#define NPCallFailed( code ) ((code) != nsPluginError_NoError)

enum nsPluginReason {
    nsPluginReason_Base = 0,
    nsPluginReason_Done = 0,
    nsPluginReason_NetworkErr,
    nsPluginReason_UserBreak,
    nsPluginReason_NoReason
};

////////////////////////////////////////////////////////////////////////////////
// Classes
////////////////////////////////////////////////////////////////////////////////

// Classes that must be implemented by the plugin DLL:
struct nsIPlugin;                       // plugin class (MIME-type handler)
class nsILiveConnectPlugin;             // subclass of nsIPlugin
class nsIPluginInstance;                // plugin instance
class nsIPluginStream;                  // stream to receive data from the browser

// Classes that are implemented by the browser:
class nsIPluginManager;                 // minimum browser requirements
class nsIFileUtilities;                 // file utilities (accessible from nsIPluginManager)
class nsIPluginInstancePeer;            // parts of nsIPluginInstance implemented by the browser
class nsIWindowlessPluginInstancePeer;  // subclass of nsIPluginInstancePeer for windowless plugins
class nsIPluginTagInfo;                 // describes html tag (accessible from nsIPluginInstancePeer)
class nsILiveConnectPluginInstancePeer; // subclass of nsIPluginInstancePeer
class nsIPluginStreamPeer;              // parts of nsIPluginStream implemented by the browser
class nsISeekablePluginStreamPeer;      // seekable subclass of nsIPluginStreamPeer

////////////////////////////////////////////////////////////////////////////////

#ifdef XP_MAC
#pragma options align=reset
#endif

#endif /* RC_INVOKED */
#ifdef __OS2__
#pragma pack()
#endif

#endif // nsplugindefs_h___
