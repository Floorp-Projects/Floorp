/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/*
// <pre>
*/
#ifndef PREFAPI_H
#define PREFAPI_H

#include "xp_core.h"
#include "prtypes.h"
#include "jscompat.h"
#include "jspubtd.h"

#ifdef XP_WIN
#include "plhash.h"
#endif

#define NEW_PREF_ARCH

NSPR_BEGIN_EXTERN_C

/*
// <font color=blue>
// Error codes
// </font>
*/

typedef int PROFILE_ERROR;


/* Error numbers between -100 and -999 are reserved for individual stores */
/* Error numbers less than -1000 are reserved for the profile manager */

typedef enum {
    PREF_BAD_PASSWORD			    = -12,
    PREF_CANT_DELETE_NET_PROFILE    = -11,
    PREF_NETPROFILE_DIR_EXISTS      = -10,
    PREF_PROFILE_DIR_EXISTS         = -9,
    PREF_PROFILE_EXISTS             = -8,
    PREF_BAD_PARAMETER              = -7,
    PREF_DOES_NOT_EXIST             = -6,
	PREF_OUT_OF_MEMORY		= -5,
	PREF_TYPE_CHANGE_ERR	= -4,
	PREF_NOT_INITIALIZED	= -3,
	PREF_BAD_LOCKFILE		= -2,
	PREF_ERROR				= -1,
	PREF_NOERROR			= 0,
	PREF_OK					= 0,	/* same as PREF_NOERROR */
	PREF_VALUECHANGED		        = 1,
	PREF_PROFILE_UPGRADE	        = 2
} PrefResult;

#if defined(XP_PC) 
// horrible pre-declaration...so kill me.
PrefResult pref_InitInitialObjects(JSContext *js_context,JSObject *js_object);
#endif

/*
// <font color=blue>
// The Init function sets up of the JavaScript preference context and creates
// the basic preference and pref_array objects:

// It also takes the filename of the preference file to use.  This allows the
// module to create its own pref file if that is so desired.  If a filename isn't
// passed, the pref module will either use the current name and file (if known) or
// will call (FE_GetPrefFileName() ?) to have a module prompt the user to determine
// which user it is (on a mulit-user system).  In general, the FE should pass the
// filename and the sub-modules should pass NULL
//
// At the moment, the Init function is defining 3 JavaScript functions that can be
// called in arbitrary JavaScript (like that passed to the EvaluateJS function below).  
// This may increase in the future...
// 
// pref(pref_name,default_value)      : Setup the initial preference storage and default
// user_pref(pref_name, user_value)   : Set a user preference
// lock_pref(pref_name)				  : Lock a preference (prevent it from being modifyed by user)
// </font>
*/

PR_EXTERN(PrefResult)
PREF_ReadUserJSFile(const char *filename);

PR_EXTERN(PrefResult)
PREF_ReadLIJSFile(const char *filename);

#if defined (XP_PC) || defined (XP_MAC)
/* PREF_Init actually returns a profile object */
PR_EXTERN(void *)
PREF_InitProfile(void);

PR_EXTERN(void *)
PREF_GetCurrentProfile(void);
#endif

PR_EXTERN(PRBool) 
PREF_Init(const char *filename);

PR_EXTERN(PrefResult)
PREF_LockPref(const char *key);

PR_EXTERN(PrefResult)
PREF_GetConfigContext(JSContext **js_context);

PR_EXTERN(PrefResult)
PREF_GetGlobalConfigObject(JSObject **js_object);

PR_EXTERN(PrefResult)
PREF_GetPrefConfigObject(JSObject **js_object);

/*
// Cleanup should be called at program exit to free the 
// list of registered callbacks.
*/
PR_EXTERN(void)
PREF_Cleanup();

/*
// <font color=blue>
// Given a path to a local Lock file, unobscures the file (not implemented yet)
// and verifies the MD5 hash.  Returns PREF_BAD_LOCKFILE if hash failed;
// otherwise, evaluates the contents of the file as a JS buffer.
// </font>
*/
PR_EXTERN(PrefResult)
PREF_ReadLockFile(const char *filename);

/*
// <font color=blue>
// Pass an arbitrary JS buffer to be evaluated in the Preference context.
// On startup modules will want to setup their preferences with reasonable
// defaults.  For example netlib might call with a buffer of:
//
// pref("network.tcpbufsize",4096);
// pref("network.max_connections", 4);
// pref("network.proxy.http_host", "");
// pref("network.proxy.http_port". 0);
// ...etc...
//
// This routine generates callbacks to functions registered with 
// PREF_RegisterCallback() for any user values that have changed.
// </font>
*/
PR_EXTERN(JSBool)
PREF_EvaluateJSBuffer(const char * js_buffer, size_t length);

/*
// Like the above but does not generate callbacks.
*/
PR_EXTERN(JSBool)
PREF_QuietEvaluateJSBuffer(const char * js_buffer, size_t length);

/*
// Like the above but does not generate callbacks and executes in scope of global config object
*/
PR_EXTERN(JSBool)
PREF_QuietEvaluateJSBufferWithGlobalScope(const char * js_buffer, size_t length);

/*
// This routine is newer than the above which are not being called externally,
// as far as I know.  The following is used from mkautocf to evaluate a 
// config URL file with callbacks.
*/
PR_EXTERN(JSBool)
PREF_EvaluateConfigScript(const char * js_buffer, size_t length,
	const char* filename, PRBool bGlobalContext, PRBool bCallbacks,
	PRBool skipFirstLine);


/*
// <font color=blue>
// Actions that can be passed to various functions to perform an operation on a pref key
// </font>
*/

typedef enum { PREF_SETDEFAULT, PREF_SETUSER, 
			   PREF_LOCK, PREF_SETCONFIG, PREF_SETLI } PrefAction;

/*
// <font color=blue>
// Preference flags, including the native type of the preference
// </font>
*/

typedef enum { PREF_INVALID = 0,
               PREF_LOCKED = 1, PREF_USERSET = 2, PREF_CONFIG = 4, PREF_REMOTE = 8,
			   PREF_LILOCAL = 16, PREF_STRING = 32, PREF_INT = 64, PREF_BOOL = 128,
			   PREF_VALUETYPE_MASK = (PREF_STRING | PREF_INT | PREF_BOOL)
			  } PrefType;

/*
// <font color=blue>
// Set the various types of preferences.  These functions take a dotted
// notation of the preference name (e.g. "browser.startup.homepage").  
// Note that this will cause the preference to be saved to the file if
// it is different from the default.  In other words, these are used
// to set the _user_ preferences.
// Each set returns PREF_VALUECHANGED if the user value changed
// (triggering a callback), or PREF_NOERROR if the value was unchanged.
// </font>
*/
PR_EXTERN(PrefResult) PREF_SetCharPref(const char *pref,const char* value);
PR_EXTERN(PrefResult) PREF_SetIntPref(const char *pref,PRInt32 value);
PR_EXTERN(PrefResult) PREF_SetBoolPref(const char *pref,PRBool value);
PR_EXTERN(PrefResult) PREF_SetBinaryPref(const char *pref,void * value, long size);
PR_EXTERN(PrefResult) PREF_SetColorPref(const char *pref_name, PRUint8 red, PRUint8 green, PRUint8 blue);
PR_EXTERN(PrefResult) PREF_SetColorPrefDWord(const char *pref_name, PRUint32 colorref);
PR_EXTERN(PrefResult) PREF_SetRectPref(const char *pref_name, PRInt16 left, PRInt16 top, PRInt16 right, PRInt16 bottom);

/*
// <font color=blue>
// Set the default for various types of preferences.  These functions take a dotted
// notation of the preference name (e.g. "browser.startup.homepage")
// This will only affect the program behavior if the user does not have a value
// saved over it for the particular preference.  In addition, these will never
// be saved out to disk.
// </font>
*/
PR_EXTERN(PrefResult) PREF_SetDefaultCharPref(const char *pref,const char* value);
PR_EXTERN(PrefResult) PREF_SetDefaultIntPref(const char *pref,PRInt32 value);
PR_EXTERN(PrefResult) PREF_SetDefaultBoolPref(const char *pref,PRBool value);
PR_EXTERN(PrefResult) PREF_SetDefaultBinaryPref(const char *pref,void * value, long size);
PR_EXTERN(PrefResult) PREF_SetDefaultColorPref(const char *pref_name, PRUint8 red, PRUint8 green, PRUint8 blue);
PR_EXTERN(PrefResult) PREF_SetDefaultRectPref(const char *pref_name, PRInt16 left, PRInt16 top, PRInt16 right, PRInt16 bottom);

/*
// <font color=blue>
// Get the various types of preferences.  These functions take a dotted
// notation of the preference name (e.g. "browser.startup.homepage")
//
// They also take a pointer to fill in with the return value and return an
// error value.  At the moment, this is simply an int but it may
// be converted to an enum once the global error strategy is worked out.
// In addition, the GetChar and GetBinary versions take an (int *) which
// should contain the length of the buffer which is passed to be filled
// in.  If the length passed in is 0, the function will not copy the
// preference but will instead return the length necessary for the buffer,
// including null terminator.
//
// They will perform conversion if the type doesn't match what was requested.
// (if it is reasonably possible)
// </font>
*/
PR_EXTERN(PrefResult) PREF_GetCharPref(const char *pref, char * return_buf, int * buf_length);
PR_EXTERN(PrefResult) PREF_GetIntPref(const char *pref, PRInt32 * return_int);	
PR_EXTERN(PrefResult) PREF_GetBoolPref(const char *pref, PRBool * return_val);	
PR_EXTERN(PrefResult) PREF_GetBinaryPref(const char *pref, void * return_val, int * buf_length);	
PR_EXTERN(PrefResult) PREF_GetColorPref(const char *pref_name, PRUint8 *red, PRUint8 *green, PRUint8 *blue);
PR_EXTERN(PrefResult) PREF_GetColorPrefDWord(const char *pref_name, PRUint32 *colorref);
PR_EXTERN(PrefResult) PREF_GetRectPref(const char *pref_name, PRInt16 *left, PRInt16 *top, PRInt16 *right, PRInt16 *bottom);

/*
// <font color=blue>
// These functions are similar to the above "Get" version with the significant
// difference that the preference module will alloc the memory (e.g. XP_STRDUP) and
// the caller will need to be responsible for freeing it...
// </font>
*/
PR_EXTERN(PrefResult) PREF_CopyCharPref(const char *pref, char ** return_buf);
PR_EXTERN(PrefResult) PREF_CopyBinaryPref(const char *pref_name, void ** return_value, int *size);

PR_EXTERN(PrefResult) PREF_CopyDefaultCharPref( const char *pref_name,  char ** return_buffer );
PR_EXTERN(PrefResult) PREF_CopyDefaultBinaryPref(const char *pref, void ** return_val, int * size);	

/*
// <font color=blue>
// Get and set encoded full file/directory pathname strings
// (i.e. file URLs without the file:// part).
// On Windows and Unix, these are just stored as string preferences.
// On Mac, paths should be stored as aliases.  These calls convert
// between paths and aliases flattened into binary strings.
// </font>
*/
PR_EXTERN(PrefResult) PREF_CopyPathPref(const char *pref, char ** return_buf);
PR_EXTERN(PrefResult) PREF_SetPathPref(const char *pref_name, const char *path, PRBool set_default);

/*
// <font color=blue>
// Same as the previous "Get" functions but will always return the
// default value regardless of what the user has set.  These are designed
// to be used by functions which "reset" the preferences
//
// </font>
*/
PR_EXTERN(PrefResult) PREF_GetDefaultCharPref(const char *pref, char * return_buf, int * buf_length);
PR_EXTERN(PrefResult) PREF_GetDefaultIntPref(const char *pref, PRInt32 * return_int);	
PR_EXTERN(PrefResult) PREF_GetDefaultBoolPref(const char *pref, PRBool * return_val);	
PR_EXTERN(PrefResult) PREF_GetDefaultBinaryPref(const char *pref, void * return_val, int * buf_length);	
PR_EXTERN(PrefResult) PREF_GetDefaultColorPref(const char *pref_name, PRUint8 *red, PRUint8 *green, PRUint8 *blue);
PR_EXTERN(PrefResult) PREF_GetDefaultColorPrefDWord(const char *pref_name, PRUint32 *colorref);
PR_EXTERN(PrefResult) PREF_GetDefaultRectPref(const char *pref_name, PRInt16 *left, PRInt16 *top, PRInt16 *right, PRInt16 *bottom);

/*
// <font color=blue>
// Administration Kit support
//
// These fetch a given configuration parameter.
// If the parameter is not defined, an error code will be returned;
// a JavaScript error will not be generated (unlike the above Get routines).
//
// IndexConfig fetches an indexed button or menu string, e.g.
//		PREF_CopyIndexConfigString( "menu.help.item", 3, "label", &buf );
// to fetch the label of Help menu item 3.
// The caller is responsible for freeing the returned string.
// </font>
*/
PR_EXTERN(PrefResult) PREF_CopyConfigString(const char *obj_name, char **return_buffer);
PR_EXTERN(PrefResult) PREF_CopyIndexConfigString(const char *obj_name, int index,
	const char *field, char **return_buffer);
PR_EXTERN(PrefResult) PREF_GetConfigInt(const char *obj_name, PRInt32 *return_int);
PR_EXTERN(PrefResult) PREF_GetConfigBool(const char *obj_name, PRBool *return_bool);

/* OLD:: */PR_EXTERN(PrefResult) PREF_GetConfigString(const char *obj_name, char * return_buffer, int size,
	int index, const char *field);

/*
 * Listpref API
 */
PR_EXTERN(PrefResult) PREF_GetListPref(const char *pref_name, char*** list);
PR_EXTERN(PrefResult) PREF_SetListPref(const char *pref_name, char** list);
PR_EXTERN(PrefResult) PREF_AppendListPref(const char *pref_name, const char *value);
PR_EXTERN(PrefResult) PREF_FreeListPref(char*** list);

/*
// <font color=blue>
// PRBool function that returns whether or not the preference is locked and therefore
// cannot be changed.
// </font>
*/
PR_EXTERN(PRBool) PREF_PrefIsLocked(const char *pref_name);

PR_EXTERN(PrefType) PREF_GetPrefType(const char *pref_name);

/*
// <font color=blue>
// Cause the preference file to be written to disk
// </font>
*/
PR_EXTERN(PrefResult) PREF_SavePrefFile(void);
PR_EXTERN(PrefResult) PREF_SavePrefFileAs(const char *filename);

/* LI_STUFF */
PR_EXTERN(PrefResult) PREF_SaveLIPrefFile(const char *filename);


/*
 * Called to handle the "about:config" command.
 * Currently dumps out some debugging information.
 */
PR_EXTERN(char *) PREF_AboutConfig();

/*
 * Delete a branch of the tree
 */
PR_EXTERN(PrefResult) PREF_DeleteBranch(const char *branch_name);

/*
 * Clears the given pref (reverts it to its default value)
 */
PR_EXTERN(PrefResult) PREF_ClearUserPref(const char *pref_name);

/*
 * Creates an iterator over the children of a node.  Sample code:
 	char* children;
	if ( PREF_CreateChildList("mime", &children) == 0 )
	{	
		int index = 0;
		while (char* child = PREF_NextChild(children, &index)) {
			...
		}
		XP_FREE(children);
	}
 *	e.g. subsequent calls to Next() return
 *     "mime.image_gif", then
 *     "mime.image_jpeg", etc.
 */
PR_EXTERN(PrefResult) PREF_CreateChildList(const char* parent_node, char **child_list);
PR_EXTERN(char*) PREF_NextChild(char *child_list, int *index);

/*  The following function parts of the hierarchy from one root to another.
 *	For example, PREF_CopyPrefsTree("mail","newmail") copies all
 *	the "mail." prefs to "newmail." prefs. It does not delete the
 *	source tree; you should do that yourself.
 *	
 *	Either srcRoot or destRoot can be empty strings, to denote 
 *	the root of the entire tree, but cannot be NULL.
 */
PR_EXTERN(PrefResult) PREF_CopyPrefsTree(const char *srcRoot, const char *destRoot);

/*
// <font color=blue>
// The callback function will get passed the pref_node which triggered the call
// and the void * instance_data which was passed to the register callback function.
// Return a non-zero result (a PrefResult enumerated value) to pass an error up to the caller.
// </font>
*/
/* Temporarily conditionally compile PrefChangedFunc typedef.
** During migration from old libpref to nsIPref we need it in
** both header files.  Eventually prefapi.h will become a private
** file.  The two types need to be in sync for now.  Certain
** compilers were having problems with multiple definitions.
*/
#ifndef nsIPref_h__
typedef int (*PrefChangedFunc) (const char *, void *); 
#endif /* nsIPref_h__ */

/*
// <font color=blue>
// Register a callback.  This takes a node in the preference tree and will
// call the callback function if anything below that node is modified.
// Unregister returns PREF_NOERROR if a callback was found that
// matched all the parameters; otherwise it returns PREF_ERROR.
// </font>
*/
PR_EXTERN(void) PREF_RegisterCallback( const char* domain,
								PrefChangedFunc callback, void* instance_data );
PR_EXTERN(PrefResult) PREF_UnregisterCallback( const char* domain,
								PrefChangedFunc callback, void* instance_data );

/*
// Front ends implement to determine whether AutoAdmin library is installed.
*/
PR_EXTERN(PRBool) PREF_IsAutoAdminEnabled(void);

#ifdef XP_UNIX
struct fe_icon_data;
typedef void* XmStringPtr;
typedef void* KeySymPtr;
/* PR_EXTERN(void) PREF_AlterSplashIcon(struct fe_icon_data*); */
PR_EXTERN(PRBool) PREF_GetLabelAndMnemonic(char*, char**, XmStringPtr xmstring, KeySymPtr keysym);
PR_EXTERN(PRBool) PREF_GetUrl(char*, char**);
PR_EXTERN(void) PREF_SetCallbacksStatus(PRBool status);
PR_EXTERN(PrefResult) PREF_LoadLDAPPrefs(void);
#endif

NSPR_END_EXTERN_C
#endif
