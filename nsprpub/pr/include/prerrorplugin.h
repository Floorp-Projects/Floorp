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

#ifndef prerrorplugin_h___
#define prerrorplugin_h___

#include "prerror.h"

PR_BEGIN_EXTERN_C

/**********************************************************************/
/************************* TYPES AND CONSTANTS ************************/
/**********************************************************************/

/*
 * struct PRErrorTable --
 *
 *    An error table, provided by a library.
 */
struct PRErrorTable {
    struct PRErrorMessage {
	const char * const name;    /* Macro name for error */
	const char * const en_text; /* default english text */
    } const * msgs; /* Array of error information */

    const char *name; /* Name of error table source */
    PRErrorCode base; /* Error code for first error in table */
    int n_msgs; /* Number of codes in table */
};

/*
 * struct PRErrorPluginRock --
 *
 *    A rock, under which the localization plugin may store information
 *    that is private to itself.
 */
struct PRErrorPluginRock;

/*
 * struct PRErrorPluginTableRock --
 *
 *    A rock, under which the localization plugin may store information,
 *    associated with an error table, that is private to itself.
 */
struct PRErrorPluginTableRock;

/*
 * PRErrorPluginLookupFn --
 *
 *    A function of PRErrorPluginLookupFn type is a localization
 *    plugin callback which converts an error code into a description
 *    in the requested language.  The callback is provided the
 *    appropriate error table, rock, and table rock.  The callback
 *    returns the appropriate UTF-8 encoded description, or NULL if no
 *    description can be found.
 */
typedef const char *
PRErrorPluginLookupFn(PRErrorCode code, PRLanguageCode language, 
		   const struct PRErrorTable *table,
		   struct PRErrorPluginRock *rock,
		   struct PRErrorPluginTableRock *table_rock);

/*
 * PRErrorPluginNewtableFn --
 *
 *    A function PRErrorPluginNewtableFn type is a localization plugin
 *    callback which is called once with each error table registered
 *    with NSPR.  The callback is provided with the error table and
 *    the plugin rock.  The callback returns any table rock it wishes
 *    to associate with the error table.  Does not need to be thread
 *    safe.
 */
typedef struct PRErrorPluginTableRock *
PRErrorPluginNewtableFn(const struct PRErrorTable *table,
			struct PRErrorPluginRock *rock);

/**********************************************************************/
/****************************** FUNCTIONS *****************************/
/**********************************************************************/

/***********************************************************************
** FUNCTION:    PR_ErrorInstallTable
** DESCRIPTION:
**  Registers an error table with NSPR.  Must be done exactly once per
**  table.
**  NOT THREAD SAFE!
**  
***********************************************************************/
PR_EXTERN(PRErrorCode) PR_ErrorInstallTable(const struct PRErrorTable *table);


/***********************************************************************
** FUNCTION:    PR_ErrorInstallPlugin
** DESCRIPTION:
**  Registers an error localization plugin with NSPR.  May be called
**  at most one time.  `languages' contains the language codes supported
**  by this plugin.  Languages 0 and 1 must be "i-default" and "en"
**  respectively.  `lookup' and `newtable' contain pointers to
**  the plugin callback functions.  `rock' contains any information
**  private to the plugin functions.
**  NOT THREAD SAFE!
***********************************************************************/
PR_EXTERN(void) PR_ErrorInstallPlugin(const char * const * languages,
			      PRErrorPluginLookupFn *lookup, 
			      PRErrorPluginNewtableFn *newtable,
			      struct PRErrorPluginRock *rock);

PR_END_EXTERN_C

#endif /* prerrorplugin_h___ */
