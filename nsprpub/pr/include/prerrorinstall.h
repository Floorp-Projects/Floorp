/*
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

#ifndef prerrorinstall_h___
#define prerrorinstall_h___

#include "prerror.h"

PR_BEGIN_EXTERN_C

/**********************************************************************/
/************************* TYPES AND CONSTANTS ************************/
/**********************************************************************/

/*
 * struct PRErrorMessage --
 *
 *    An error message in an error table.
 */
struct PRErrorMessage {
    const char * name;    /* Macro name for error */
    const char * en_text; /* Default English text */
};

/*
 * struct PRErrorTable --
 *
 *    An error table, provided by a library.
 */
struct PRErrorTable {
    const struct PRErrorMessage * msgs; /* Array of error information */

    const char *name; /* Name of error table source */
    PRErrorCode base; /* Error code for first error in table */
    int n_msgs; /* Number of codes in table */
};

/*
 * struct PRErrorCallbackPrivate --
 *
 *    A private structure for the localization plugin 
 */
struct PRErrorCallbackPrivate;

/*
 * struct PRErrorCallbackTablePrivate --
 *
 *    A data structure under which the localization plugin may store information,
 *    associated with an error table, that is private to itself.
 */
struct PRErrorCallbackTablePrivate;

/*
 * PRErrorCallbackLookupFn --
 *
 *    A function of PRErrorCallbackLookupFn type is a localization
 *    plugin callback which converts an error code into a description
 *    in the requested language.  The callback is provided the
 *    appropriate error table, private data for the plugin and the table.
 *    The callback returns the appropriate UTF-8 encoded description, or NULL
 *    if no description can be found.
 */
typedef const char *
PRErrorCallbackLookupFn(PRErrorCode code, PRLanguageCode language, 
		   const struct PRErrorTable *table,
		   struct PRErrorCallbackPrivate *cb_private,
		   struct PRErrorCallbackTablePrivate *table_private);

/*
 * PRErrorCallbackNewtableFn --
 *
 *    A function PRErrorCallbackNewtableFn type is a localization plugin
 *    callback which is called once with each error table registered
 *    with NSPR.  The callback is provided with the error table and
 *    the plugin's private structure.  The callback returns any table private
 *    data it wishes to associate with the error table.  Does not need to be thread
 *    safe.
 */
typedef struct PRErrorCallbackTablePrivate *
PRErrorCallbackNewtableFn(const struct PRErrorTable *table,
			struct PRErrorCallbackPrivate *cb_private);

/**********************************************************************/
/****************************** FUNCTIONS *****************************/
/**********************************************************************/

/***********************************************************************
** FUNCTION:    PR_ErrorInstallTable
** DESCRIPTION:
**  Registers an error table with NSPR.  Must be done exactly once per
**  table.  Memory pointed to by `table' must remain valid for the life
**  of the process.
**
**  NOT THREAD SAFE!
**  
***********************************************************************/
PR_EXTERN(PRErrorCode) PR_ErrorInstallTable(const struct PRErrorTable *table);


/***********************************************************************
** FUNCTION:    PR_ErrorInstallCallback
** DESCRIPTION:
**  Registers an error localization plugin with NSPR.  May be called
**  at most one time.  `languages' contains the language codes supported
**  by this plugin.  Languages 0 and 1 must be "i-default" and "en"
**  respectively.  `lookup' and `newtable' contain pointers to
**  the plugin callback functions.  `cb_private' contains any information
**  private to the plugin functions.
**
**  NOT THREAD SAFE!
**
***********************************************************************/
PR_EXTERN(void) PR_ErrorInstallCallback(const char * const * languages,
			      PRErrorCallbackLookupFn *lookup, 
			      PRErrorCallbackNewtableFn *newtable,
			      struct PRErrorCallbackPrivate *cb_private);

PR_END_EXTERN_C

#endif /* prerrorinstall_h___ */
