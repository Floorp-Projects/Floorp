/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
#ifndef _PREFS_H_
#define _PREFS_H_

#if 0
#include "prtypes.h"
#include "ssmdefs.h"

typedef struct PrefFileStr PrefFile;

/************************************************************
** FUNCTION: PREF_OpenPrefs
**
** DESCRIPTION: Creates a PrefFile object for the preferences file.
**              If the file does not exist, it does not create it, and
**              returns a default set of preferences.  To create the
**              preference file you must call PREF_WritePrefs() or
**              PREF_ClosePrefs().  If the file has already been
**              opened, the reference count is incremented and the
**              same object is returned.
**              
** INPUTS:
**   filename
**     The name of the preferences file to open.
**       
** RETURNS:
**   If successful, returns a pointer to a PrefFile object.
**   If failed, returns NULL.
**
*************************************************************/
PrefFile *
PREF_OpenPrefs(char *filename);


/************************************************************
** FUNCTION: PREF_WritePrefs
**
** DESCRIPTION: Writes out the contents of prefs to the preferences
**              file.  If the file does not exist, it is created.
**              
** INPUTS:
**   prefs
**     The PrefFile object to write.
**       
** RETURNS:
**   If successful, returns PR_SUCCESS.
**   If failed, returns PR_FAILURE.
**
*************************************************************/
SSMStatus
PREF_WritePrefs(PrefFile *prefs);


/************************************************************
** FUNCTION: PREF_ClosePrefs
**
** DESCRIPTION: Closes the PrefFile object.  If this is the last
**              reference to to object, the contents are written out
**              and the object is destroyed.
**              
** INPUTS:
**   prefs
**     The PrefFile object to close.
**       
** RETURNS:
**   If successful, returns PR_SUCCESS.
**   If failed, returns PR_FAILURE.
**
*************************************************************/
SSMStatus
PREF_ClosePrefs(PrefFile *prefs);


/************************************************************
** FUNCTION: PREF_SetStringPref
**
** DESCRIPTION: Sets a preference with a string value.  The preference
**              will be created if it does not already exist.
**              
** INPUTS:
**   prefs
**     The PrefFile object.
**   name
**     The name of the preference.
**   value
**     The value of the preference.
**       
** RETURNS:
**   If successful, returns PR_SUCCESS.
**   If failed, returns PR_FAILURE.
**
*************************************************************/
SSMStatus
PREF_SetStringPref(PrefFile *prefs, char *name, char *value);


/************************************************************
** FUNCTION: PREF_GetStringPref
**
** DESCRIPTION: Gets the value of a string preference.  The returned
**              string must be freed with PR_Free().
**
** INPUTS:
**   prefs
**     The PrefFile object.
**   name
**     The name of the preference.
**       
** RETURNS:
**   If successful, returns a new string containing the value of
**     the preference.
**   If failed, returns NULL.
**
*************************************************************/
char *
PREF_GetStringPref(PrefFile *prefs, char *name);


#else    /* PREFS LITE (tm) */
/* XXX sjlee
 * The following set of APIs describe the preference management system which
 * we will use for the time being until a fully-functional prefs/profs library
 * comes online, thus the name PREFS LITE.
 */
#include "prtypes.h"
#include "ssmdefs.h"
#include "plhash.h"

/* pref types used for sending (and receiving) pref messages */
#define STRING_PREF 0
#define BOOL_PREF 1
#define INT_PREF 2

typedef struct PrefSetStr PrefSet;

/*
 * Function: PrefSet* PREF_NewPrefs()
 * Purpose: Creates a PrefSet object.  The object is not associated with a
 *          file, and the values are initially filled with default values.
 * Return values:
 * - a pointer to a PrefSet object; if failure, returns NULL
 */
PrefSet* PREF_NewPrefs(void);

/*
 * Function: SSMStatus PREF_ClosePrefs()
 * Purpose: Closes the PrefSet object and frees the memory.  We do not write
 *          changes back to the client at this time.
 * Arguments and return values:
 * - prefs: the PrefSet object to close
 * - returns: PR_SUCCESS if successful; PR_FAILURE if failure
 */
SSMStatus PREF_ClosePrefs(PrefSet* prefs);

/*
 * Function: SSMStatus PREF_GetStringPref()
 * Purpose: Retrieves the string pref for (key) from (prefs).
 * Arguments and return values:
 * - prefs: the pref set
 * - key: the key for the pref item
 * - value: on return, filled with a pointer to the string pref value (or it
 *          be NULL if NULL was the legally assigned value).  Since this 
 *          points to an existing string (not a new string), the caller
 *          should not free it.  If the pref item does not exist, (*value) is
 *          NULL.
 * - returns: PR_SUCCESS if successful; error code otherwise
 *
 * Notes: Note that NULL is a legal string value for a pref item.  One should
 *        check the return value (SSMStatus) to see if an error occurred.
 */
SSMStatus PREF_GetStringPref(PrefSet* prefs, char* key, char** value);

/*
 * Function: SSMStatus PREF_CopyStringPref()
 * Purpose: identical to PREF_GetStringPref() except that it creates a new
 *          string
 * Arguments and return values:
 * - prefs: the pref set
 * - key: the key for the pref item
 * - value: on return, filled with a pointer to the string newly allocated
 *          that contains the pref value (or NULL if the value is NULL).
 * - returns: PR_SUCCESS if successful; error code otherwise
 *
 * Notes: Use this function instead of PREF_GetStringPref() if you need a
 *        newly allocated string.  The caller is responsible for freeing
 *        the string when done.
 */
SSMStatus PREF_CopyStringPref(PrefSet* prefs, char* key, char** value);

/*
 * Function: SSMStatus PREF_SetStringPref()
 * Purpose: Sets the string pref for (key) in (prefs).
 * Arguments and return values:
 * - prefs: the pref set
 * - key: the key for the pref item
 * - value: the string value to set for the key.  NULL may be used for a
 *          value.
 * - returns: PR_SUCCESS if successful; error code otherwise
 */
SSMStatus PREF_SetStringPref(PrefSet* prefs, char* key, char* value);

/*
 * Function: PRBool PREF_StringPrefChanged()
 * Purpose: Compares the value in the set with the given value and determines
 *          if it has changed
 * Arguments and return values:
 * - prefs: the pref set
 * - key: the key for the pref item
 * - value: the string value to compare.  NULL may be used for a value.
 * - returns: PR_TRUE if the value does not match or the key was not found
 *            (i.e. change needs to be saved); otherwise PR_FALSE
 *
 * Notes: this is useful when one wants to determine the items that need
 *        to be permanently saved.
 */
PRBool PREF_StringPrefChanged(PrefSet* prefs, char* key, char* value);

/*
 * Function: SSMStatus PREF_GetBoolPref()
 * Purpose: Retrieves the boolean pref for (key) from (prefs).
 * Arguments and return values:
 * - prefs: the pref set
 * - key: the key for the pref item
 * - value: on return, filled with the boolean pref value.
 * - returns: PR_SUCCESS if successful; error code otherwise
 */
SSMStatus PREF_GetBoolPref(PrefSet* prefs, char* key, PRBool* value);

/*
 * Function: SSMStatus PREF_SetBoolPref()
 * Purpose: Sets the boolean pref for (key) in (prefs).
 * Arguments and return values:
 * - prefs: the pref set
 * - key: the key for the pref item
 * - value: the boolean value to set for the key.
 * - returns: PR_SUCCESS if successful; error code otherwise
 */
SSMStatus PREF_SetBoolPref(PrefSet* prefs, char* key, PRBool value);

/*
 * Function: PRBool PREF_BoolPrefChanged()
 * Purpose: Compares the value in the set with the given value and determines
 *          if it has changed
 * Arguments and return values:
 * - prefs: the pref set
 * - key: the key for the pref item
 * - value: the boolean value to compare.
 * - returns: PR_TRUE if the value does not match or the key was not found
 *            (i.e. change needs to be saved); otherwise PR_FALSE
 *
 * Notes: this is useful when one wants to determine the items that need
 *        to be permanently saved.
 */
PRBool PREF_BoolPrefChanged(PrefSet* prefs, char* key, PRBool value);

/*
 * Function: SSMStatus PREF_GetIntPref()
 * Purpose: Retrieves the integer pref for (key) from (prefs).
 * Arguments and return values:
 * - prefs: the pref set
 * - key: the key for the pref item
 * - value: on return, filled with the integer pref value
 * - returns: PR_SUCCESS if successful; error code otherwise
 */
SSMStatus PREF_GetIntPref(PrefSet* prefs, char* key, PRIntn* value);

/*
 * Function: SSMStatus PREF_SetIntPref()
 * Purpose: Sets the integer pref (key) in (prefs).
 * Arguments and return values:
 * - prefs: the prefs set
 * - key: the key for the pref item
 * - value: the integer value to set for the key
 * - returns: PR_SUCCESS if successful; error code otherwise
 */
SSMStatus PREF_SetIntPref(PrefSet* prefs, char* key, PRIntn value);

/*
 * Function: PRBool PREF_IntPrefChanged()
 * Purpose: Compares the value in the set with the given value and determines
 *          if it has changed
 * Arguments and return values:
 * - prefs: the pref set
 * - key: the key for the pref item
 * - value: the integer value to compare.
 * - returns: PR_TRUE if the value does not match or the key was not found
 *            (i.e. change needs to be saved); otherwise PR_FALSE
 *
 * Notes: this is useful when one wants to determine the items that need
 *        to be permanently saved.
 */
PRBool PREF_IntPrefChanged(PrefSet* prefs, char* key, PRIntn value);

/****
 * Functions and typedefs needed to iterated over multiple similar pref items.
 * Used to find all specified LDAP servers for UI lookup.
 ***/
typedef struct
{
        char*            childList;
        char*            parent;
        unsigned int bufsize;
} PrefChildIter;


SSMStatus PREF_CreateChildList(PrefSet * prefs, const char* parent_key, 
                               char ***child_list);
SSMStatus pref_append_value(char *** list, char * value);


#endif    /* PREFS LITE (tm) */
#endif /* _PREFS_H_ */
