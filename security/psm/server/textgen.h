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
/*
    Include file for libnls related utilities. Separated because there
    are a lot of headers for libnls, and only those objects which override
    the default Print function (see resource.h) should be including
    these files.

    mwelch - 1999 January
 */

#ifndef __textgen_h__
#define __textgen_h__

#include "nlsutil.h"
#include "minihttp.h"
#include "resource.h"
#include "ctrlconn.h"

/* Indices for the resource bundles. 
   Keep in sync with (resBundleNames) below. */
typedef enum
{
    SSM_RESBNDL_TEXT = (PRInt32) 0,
    SSM_RESBNDL_UI,
    SSM_RESBNDL_BIN,
    SSM_RESBNDL_DOC,
    SSM_RESBNDL_COUNT
} SSMResourceBundleIndex;

extern char *resBundleNames[];

/* Text generator state, used by keyword handlers */
typedef struct SSMTextGenContext
{
    struct SSMTextGenContext *m_caller;  /* who called us, if at all */
    HTTPRequest *             m_request; /* the HTTP request being processed */
    char*                     m_keyword; /* the keywd by which we are called */
    SSMCollection *           m_params;  /* params to this keyword */
    char*                     m_result;  /* where we store the result */
} SSMTextGenContext;

/* Create/destroy a textgen context. */
SSMStatus SSMTextGen_NewContext(SSMTextGenContext *caller, /* can be NULL */
                                HTTPRequest *req,
                                char *keyword,
                                char **params,
                                SSMTextGenContext **result);

/* Create a top-level textgen context. */
SSMStatus SSMTextGen_NewTopLevelContext(HTTPRequest *req,
                                        SSMTextGenContext **result);

void SSMTextGen_DestroyContext(SSMTextGenContext *victim);

/* Helper routines used with a TextGenContext */

SSMResource *SSMTextGen_GetTargetObject(SSMTextGenContext *cx);
SSMControlConnection *SSMTextGen_GetControlConnection(SSMTextGenContext *cx);

/* 
   Top level routine called by non-NLS-using parts of Cartman. 
   Retrieves a string, expands it, then formats it according to the
   _Print method of the target object. 
*/
SSMStatus SSM_GetUTF8Text(SSMTextGenContext *cx,  /* can be NULL */
                          const char *key, 
                          char **resultText);

/* Get a numeric parameter from the properties file. */
SSMStatus SSM_GetNumber(SSMTextGenContext *cx, char *key, PRInt32 *param);

/* Just gets and expands a string. Can pass NULL for (res). */
SSMStatus SSM_GetAndExpandText(SSMTextGenContext *cx, 
                               const char *key, char **result);

SSMStatus SSM_GetAndExpandTextKeyedByString(SSMTextGenContext *cx,
                                            const char *key, 
                                            char **result);

/* Keyword processing function. */
typedef SSMStatus (*KeywordHandlerFunc)(SSMTextGenContext *cx);

SSMStatus SSM_HelloKeywordHandler(SSMTextGenContext *cx);

/* Register a keyword callback. */
SSMStatus SSM_RegisterKeywordHandler(char *keyword, 
                                     KeywordHandlerFunc func);


/* Perform NLS-specific initialization. (dataDirectory) contains the path
   of the directory containing the "conv" folder with conversion libraries
   for the current locale. */
void SSM_InitNLS(char *dataDirectory);

/* Given a key and a context, cycle through resource bundles until
   we find a match for the desired key. */
SSMStatus
SSM_FindUTF8StringInBundles(SSMTextGenContext *cx,
                            const char *key,
                            char **utf8result);

SSMStatus SSMTextGen_SubstituteString(SSMTextGenContext *cx, 
                                      char *str, 
                                      char **result);

SSMStatus 
SSM_WarnPKCS12Incompatibility(SSMTextGenContext *cx);

SSMStatus
SSM_PassVariable(SSMTextGenContext *cx);

#endif
