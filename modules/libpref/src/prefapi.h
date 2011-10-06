/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
// <pre>
*/
#ifndef PREFAPI_H
#define PREFAPI_H

#include "nscore.h"
#include "pldhash.h"

PR_BEGIN_EXTERN_C

typedef union
{
    char*       stringVal;
    PRInt32     intVal;
    PRBool      boolVal;
} PrefValue;

struct PrefHashEntry : PLDHashEntryHdr
{
    const char *key;
    PrefValue defaultPref;
    PrefValue userPref;
    PRUint16  flags;
};

/*
// <font color=blue>
// The Init function initializes the preference context and creates
// the preference hashtable.
// </font>
*/
nsresult    PREF_Init();

/*
// Cleanup should be called at program exit to free the 
// list of registered callbacks.
*/
void        PREF_Cleanup();
void        PREF_CleanupPrefs();

/*
// <font color=blue>
// Preference flags, including the native type of the preference
// </font>
*/

typedef enum { PREF_INVALID = 0,
               PREF_LOCKED = 1, PREF_USERSET = 2, PREF_CONFIG = 4, PREF_REMOTE = 8,
               PREF_LILOCAL = 16, PREF_STRING = 32, PREF_INT = 64, PREF_BOOL = 128,
               PREF_HAS_DEFAULT = 256,
               PREF_VALUETYPE_MASK = (PREF_STRING | PREF_INT | PREF_BOOL)
             } PrefType;

/*
// <font color=blue>
// Set the various types of preferences.  These functions take a dotted
// notation of the preference name (e.g. "browser.startup.homepage").  
// Note that this will cause the preference to be saved to the file if
// it is different from the default.  In other words, these are used
// to set the _user_ preferences.
//
// If set_default is set to PR_TRUE however, it sets the default value.
// This will only affect the program behavior if the user does not have a value
// saved over it for the particular preference.  In addition, these will never
// be saved out to disk.
//
// Each set returns PREF_VALUECHANGED if the user value changed
// (triggering a callback), or PREF_NOERROR if the value was unchanged.
// </font>
*/
nsresult PREF_SetCharPref(const char *pref,const char* value, PRBool set_default = PR_FALSE);
nsresult PREF_SetIntPref(const char *pref,PRInt32 value, PRBool set_default = PR_FALSE);
nsresult PREF_SetBoolPref(const char *pref,PRBool value, PRBool set_default = PR_FALSE);

PRBool   PREF_HasUserPref(const char* pref_name);

/*
// <font color=blue>
// Get the various types of preferences.  These functions take a dotted
// notation of the preference name (e.g. "browser.startup.homepage")
//
// They also take a pointer to fill in with the return value and return an
// error value.  At the moment, this is simply an int but it may
// be converted to an enum once the global error strategy is worked out.
//
// They will perform conversion if the type doesn't match what was requested.
// (if it is reasonably possible)
// </font>
*/
nsresult PREF_GetIntPref(const char *pref,
                           PRInt32 * return_int, PRBool get_default);	
nsresult PREF_GetBoolPref(const char *pref, PRBool * return_val, PRBool get_default);	
/*
// <font color=blue>
// These functions are similar to the above "Get" version with the significant
// difference that the preference module will alloc the memory (e.g. XP_STRDUP) and
// the caller will need to be responsible for freeing it...
// </font>
*/
nsresult PREF_CopyCharPref(const char *pref, char ** return_buf, PRBool get_default);
/*
// <font color=blue>
// PRBool function that returns whether or not the preference is locked and therefore
// cannot be changed.
// </font>
*/
PRBool PREF_PrefIsLocked(const char *pref_name);

/*
// <font color=blue>
// Function that sets whether or not the preference is locked and therefore
// cannot be changed.
// </font>
*/
nsresult PREF_LockPref(const char *key, PRBool lockIt);

PrefType PREF_GetPrefType(const char *pref_name);

/*
 * Delete a branch of the tree
 */
nsresult PREF_DeleteBranch(const char *branch_name);

/*
 * Clears the given pref (reverts it to its default value)
 */
nsresult PREF_ClearUserPref(const char *pref_name);

/*
 * Clears all user prefs
 */
nsresult PREF_ClearAllUserPrefs();


/*
// <font color=blue>
// The callback function will get passed the pref_node which triggered the call
// and the void * instance_data which was passed to the register callback function.
// Return a non-zero result (nsresult) to pass an error up to the caller.
// </font>
*/
/* Temporarily conditionally compile PrefChangedFunc typedef.
** During migration from old libpref to nsIPref we need it in
** both header files.  Eventually prefapi.h will become a private
** file.  The two types need to be in sync for now.  Certain
** compilers were having problems with multiple definitions.
*/
#ifndef have_PrefChangedFunc_typedef
typedef nsresult (*PrefChangedFunc) (const char *, void *); 
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
nsresult PREF_UnregisterCallback( const char* domain,
								PrefChangedFunc callback, void* instance_data );

/*
 * Used by nsPrefService as the callback function of the 'pref' parser
 */
void PREF_ReaderCallback( void *closure,
                          const char *pref,
                          PrefValue   value,
                          PrefType    type,
                          PRBool      isDefault);

PR_END_EXTERN_C
#endif
