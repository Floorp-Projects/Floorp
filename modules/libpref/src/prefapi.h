/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
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
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/*
// <pre>
*/
#ifndef PREFAPI_H
#define PREFAPI_H

#include "xp_core.h"
#include "prtypes.h"
#include "jsapi.h"

#ifdef XP_WIN
#include "plhash.h"
#endif

#define NEW_PREF_ARCH

#if defined(VMS)
/* Deal with case naming conflicts */
#define pref_CopyCharPref prefl_CopyCharPref
#define pref_GetBoolPref prefl_GetBoolPref
#define pref_GetCharPref prefl_GetCharPref
#define pref_GetIntPref prefl_GetIntPref
#define pref_LockPref prefl_LockPref
#endif /* VMS */

NSPR_BEGIN_EXTERN_C

/*
// <font color=blue>
// Error codes
// </font>
*/

typedef int PROFILE_ERROR;


/* Error numbers between -100 and -999 are reserved for individual stores */
/* Error numbers less than -1000 are reserved for the profile manager */

/* IMPORTANT:  if you add error codes to this, make sure you update convertRes() in
 * nsPref.cpp, otherwise the error won't get turned into the right nsresult value
 */
typedef enum {
	PREF_DEFAULT_VALUE_NOT_INITIALIZED	    = -13,
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

#if defined (XP_WIN) || defined (XP_MAC) || defined(XP_OS2)
/* PREF_Init actually returns a profile object */
void *
PREF_InitProfile(void);

void *
PREF_GetCurrentProfile(void);
#endif

PRBool
PREF_Init(const char *filename);

PrefResult
PREF_LockPref(const char *key);

PrefResult
PREF_GetConfigContext(JSContext **js_context);

PrefResult
PREF_GetGlobalConfigObject(JSObject **js_object);

PrefResult
PREF_GetPrefConfigObject(JSObject **js_object);

/*
// Cleanup should be called at program exit to free the 
// list of registered callbacks.
*/
void
PREF_Cleanup();

void
PREF_CleanupPrefs();

/*
// <font color=blue>
// Given a path to a local Lock file, unobscures the file (not implemented yet)
// and verifies the MD5 hash.  Returns PREF_BAD_LOCKFILE if hash failed;
// otherwise, evaluates the contents of the file as a JS buffer.
// </font>
*/
PrefResult
PREF_ReadLockFile(const char *filename);

JSBool
PREF_EvaluateConfigScript(const char * js_buffer, size_t length,
	const char* filename, PRBool bGlobalContext, PRBool bCallbacks,
	PRBool skipFirstLine);
/*
// This routine is newer than the above which are not being called externally,
// as far as I know.  The following is used from mkautocf to evaluate a 
// config URL file with callbacks.

// <font color=blue>
// Pass an arbitrary JS buffer to be evaluated in the Preference context.
// On startup modules will want to set up their preferences with reasonable
// defaults.  For example netlib might call with a buffer of:
//
// pref("network.proxy.http_host", "");
// pref("network.proxy.http_port". 0);
// ...etc...
//
// This routine generates callbacks to functions registered with 
// PREF_RegisterCallback() for any user values that have changed.
// </font>
*/


/*
// <font color=blue>
// Actions that can be passed to various functions to perform an operation on a pref key
// </font>
*/

typedef enum { PREF_SETDEFAULT, PREF_SETUSER, 
			   PREF_LOCK, PREF_SETCONFIG} PrefAction;

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
PrefResult PREF_SetCharPref(const char *pref,const char* value);
PrefResult PREF_SetIntPref(const char *pref,PRInt32 value);
PrefResult PREF_SetBoolPref(const char *pref,PRBool value);
PrefResult PREF_SetBinaryPref(const char *pref,void * value, long size);
PrefResult PREF_SetColorPref(const char *pref_name, PRUint8 red, PRUint8 green, PRUint8 blue);
PrefResult PREF_SetColorPrefDWord(const char *pref_name, PRUint32 colorref);
PrefResult PREF_SetRectPref(const char *pref_name, PRInt16 left, PRInt16 top, PRInt16 right, PRInt16 bottom);

/*
// <font color=blue>
// Set the default for various types of preferences.  These functions take a dotted
// notation of the preference name (e.g. "browser.startup.homepage")
// This will only affect the program behavior if the user does not have a value
// saved over it for the particular preference.  In addition, these will never
// be saved out to disk.
// </font>
*/
PrefResult PREF_SetDefaultCharPref(const char *pref,const char* value);
PrefResult PREF_SetDefaultIntPref(const char *pref,PRInt32 value);
PrefResult PREF_SetDefaultBoolPref(const char *pref,PRBool value);
PrefResult PREF_SetDefaultBinaryPref(const char *pref,void * value, long size);
PrefResult PREF_SetDefaultColorPref(const char *pref_name, PRUint8 red, PRUint8 green, PRUint8 blue);
PrefResult PREF_SetDefaultRectPref(const char *pref_name, PRInt16 left, PRInt16 top, PRInt16 right, PRInt16 bottom);

PRBool PREF_HasUserPref(const char* pref_name);

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
PrefResult PREF_GetIntPref(const char *pref,
                           PRInt32 * return_int, PRBool isDefault);	
PrefResult PREF_GetBoolPref(const char *pref, PRBool * return_val, PRBool isDefault);	
PrefResult PREF_GetBinaryPref(const char *pref, void * return_val, int * buf_length, PRBool isDefault);
PrefResult PREF_GetColorPref(const char *pref_name, PRUint8 *red, PRUint8 *green, PRUint8 *blue, PRBool isDefault);
PrefResult PREF_GetColorPrefDWord(const char *pref_name, PRUint32 *colorref, PRBool isDefault);
/*
// <font color=blue>
// These functions are similar to the above "Get" version with the significant
// difference that the preference module will alloc the memory (e.g. XP_STRDUP) and
// the caller will need to be responsible for freeing it...
// </font>
*/
PrefResult PREF_CopyCharPref(const char *pref, char ** return_buf, PRBool isDefault);
PrefResult PREF_CopyBinaryPref(const char *pref_name, void ** return_value, int *size, PRBool isDefault);

/*
// <font color=blue>
// Get and set encoded full file/directory pathname strings
// (i.e. file URLs without the file:// part).
// On Windows and Unix, these are just stored as string preferences.
// On Mac, paths should be stored as aliases.  These calls convert
// between paths and aliases flattened into binary strings.
// </font>
*/
PrefResult PREF_CopyPathPref(const char *pref, char ** return_buf, PRBool isDefault);
PrefResult PREF_SetPathPref(const char *pref_name, const char *path, PRBool set_default);

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
PrefResult PREF_CopyConfigString(const char *obj_name, char **return_buffer);
PrefResult PREF_CopyIndexConfigString(const char *obj_name, int indx,
	const char *field, char **return_buffer);
PrefResult PREF_GetConfigInt(const char *obj_name, PRInt32 *return_int);
PrefResult PREF_GetConfigBool(const char *obj_name, PRBool *return_bool);

/* OLD:: */PrefResult PREF_GetConfigString(const char *obj_name, char * return_buffer, int size,
	int indx, const char *field);

/*
 * Listpref API
 */
PrefResult PREF_GetListPref(const char *pref_name, char*** list, PRBool isDefault);
PrefResult PREF_SetListPref(const char *pref_name, char** list);
PrefResult PREF_AppendListPref(const char *pref_name, const char *value);
PrefResult PREF_FreeListPref(char*** list);

/*
// <font color=blue>
// PRBool function that returns whether or not the preference is locked and therefore
// cannot be changed.
// </font>
*/
PRBool PREF_PrefIsLocked(const char *pref_name);

PrefType PREF_GetPrefType(const char *pref_name);

/*
// <font color=blue>
// Cause the preference file to be written to disk
// </font>
*/
PrefResult PREF_SavePrefFile(void);

/*
 * Called to handle the "about:config" command.
 * Currently dumps out some debugging information.
 */
char * PREF_AboutConfig();

/*
 * Delete a branch of the tree
 */
PrefResult PREF_DeleteBranch(const char *branch_name);

/*
 * Clears the given pref (reverts it to its default value)
 */
PrefResult PREF_ClearUserPref(const char *pref_name);

/*
 * Clears all user prefs
 */
PrefResult PREF_ClearAllUserPrefs();

/*
 * Creates an iterator over the children of a node.  Sample code:
 	char* children;
	if ( PREF_CreateChildList("mime", &children) == 0 )
	{	
		int indx = 0;
		while (char* child = PREF_NextChild(children, &indx)) {
			...
		}
		XP_FREE(children);
	}
 *	e.g. subsequent calls to Next() return
 *     "mime.image_gif", then
 *     "mime.image_jpeg", etc.
 */
PrefResult PREF_CreateChildList(const char* parent_node, char **child_list);
char* PREF_NextChild(char *child_list, int *indx);

/*  The following function parts of the hierarchy from one root to another.
 *	For example, PREF_CopyPrefsTree("mail","newmail") copies all
 *	the "mail." prefs to "newmail." prefs. It does not delete the
 *	source tree; you should do that yourself.
 *	
 *	Either srcRoot or destRoot can be empty strings, to denote 
 *	the root of the entire tree, but cannot be NULL.
 */
PrefResult PREF_CopyPrefsTree(const char *srcRoot, const char *destRoot);

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
#ifndef have_PrefChangedFunc_typedef
typedef int (*PrefChangedFunc) (const char *, void *); 
#define have_PrefChangedFunc_typedef
#endif

/*
// <font color=blue>
// Register a callback.  This takes a node in the preference tree and will
// call the callback function if anything below that node is modified.
// Unregister returns PREF_NOERROR if a callback was found that
// matched all the parameters; otherwise it returns PREF_ERROR.
// </font>
*/
void PREF_RegisterCallback( const char* domain,
								PrefChangedFunc callback, void* instance_data );
PrefResult PREF_UnregisterCallback( const char* domain,
								PrefChangedFunc callback, void* instance_data );

/*
// Front ends implement to determine whether AutoAdmin library is installed.
*/
PRBool PREF_IsAutoAdminEnabled(void);

#ifdef XP_UNIX
struct fe_icon_data;
typedef void* XmStringPtr;
typedef void* KeySymPtr;
/* void PREF_AlterSplashIcon(struct fe_icon_data*); */
PRBool PREF_GetLabelAndMnemonic(char*, char**, XmStringPtr xmstring, KeySymPtr keysym);
PRBool PREF_GetUrl(char*, char**);
void PREF_SetCallbacksStatus(PRBool status);
PrefResult PREF_LoadLDAPPrefs(void);
#endif

NSPR_END_EXTERN_C
#endif
