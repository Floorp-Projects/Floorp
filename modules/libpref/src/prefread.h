/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
 *        preference type (true: default, false: user preference)
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
 * @return false if buffer contains malformed content.
 */
bool PREF_ParseBuf(PrefParseState *ps, const char *buf, int bufLen);

PR_END_EXTERN_C
#endif /* prefread_h__ */
