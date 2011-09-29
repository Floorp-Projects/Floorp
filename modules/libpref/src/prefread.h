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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is Darin Fisher.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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

#ifndef prefread_h__
#define prefread_h__

#include "prtypes.h"
#include "prefapi.h"

PR_BEGIN_EXTERN_C

/**
 * Callback function used to notify consumer of preference name value pairs.
 * The pref name and value must be copied by the implementor of the callback
 * if they are needed beyond the scope of the callback function.
 *
 * @param closure
 *        user data passed to PREF_InitParseState
 * @param pref
 *        preference name
 * @param val
 *        preference value
 * @param type
 *        preference type (PREF_STRING, PREF_INT, or PREF_BOOL)
 * @param defPref
 *        preference type (PR_TRUE: default, PR_FALSE: user preference)
 */
typedef void (*PrefReader)(void       *closure,
                           const char *pref,
                           PrefValue   val,
                           PrefType    type,
                           bool        defPref);

/* structure fields are private */
typedef struct PrefParseState {
    PrefReader  reader;
    void       *closure;
    int         state;      /* PREF_PARSE_...                */
    int         nextstate;  /* sometimes used...             */
    const char *smatch;     /* string to match               */
    int         sindex;     /* next char of smatch to check  */
                            /* also, counter in \u parsing   */
    PRUnichar   utf16[2];   /* parsing UTF16  (\u) escape    */
    int         esclen;     /* length in esctmp              */
    char        esctmp[6];  /* raw escape to put back if err */
    char        quotechar;  /* char delimiter for quotations */
    char       *lb;         /* line buffer (only allocation) */
    char       *lbcur;      /* line buffer cursor            */
    char       *lbend;      /* line buffer end               */
    char       *vb;         /* value buffer (ptr into lb)    */
    PrefType    vtype;      /* PREF_STRING,INT,BOOL          */
    bool        fdefault;   /* true if (default) pref     */
} PrefParseState;

/**
 * PREF_InitParseState
 *
 * Called to initialize a PrefParseState instance.
 * 
 * @param ps
 *        PrefParseState instance.
 * @param reader
 *        PrefReader callback function, which will be called once for each
 *        preference name value pair extracted.
 * @param closure
 *        PrefReader closure.
 */
void PREF_InitParseState(PrefParseState *ps, PrefReader reader, void *closure);

/**
 * PREF_FinalizeParseState
 *
 * Called to release any memory in use by the PrefParseState instance.
 *
 * @param ps
 *        PrefParseState instance.
 */        
void PREF_FinalizeParseState(PrefParseState *ps);

/**
 * PREF_ParseBuf
 *
 * Called to parse a buffer containing some portion of a preference file.  This
 * function may be called repeatedly as new data is made available.  The
 * PrefReader callback function passed PREF_InitParseState will be called as
 * preference name value pairs are extracted from the data.
 *
 * @param ps
 *        PrefParseState instance.  Must have been initialized.
 * @param buf
 *        Raw buffer containing data to be parsed.
 * @param bufLen
 *        Length of buffer.
 *
 * @return PR_FALSE if buffer contains malformed content.
 */
bool PREF_ParseBuf(PrefParseState *ps, const char *buf, int bufLen);

PR_END_EXTERN_C
#endif /* prefread_h__ */
