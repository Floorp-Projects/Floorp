/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
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

#ifndef _MOZSTORAGESTATEMENTPARAMS_H_
#define _MOZSTORAGESTATEMENTPARAMS_H_

#include "mozIStorageStatement.h"
#include "mozIStorageStatementWrapper.h"
#include "nsIXPCScriptable.h"

#include "jsapi.h"
#include "jsdate.h"

class mozStorageStatement;

class mozStorageStatementParams : public mozIStorageStatementParams,
                                  public nsIXPCScriptable
{
public:
    mozStorageStatementParams(mozIStorageStatement *aStatement);

    // interfaces
    NS_DECL_ISUPPORTS
    NS_DECL_MOZISTORAGESTATEMENTPARAMS
    NS_DECL_NSIXPCSCRIPTABLE

protected:
    mozIStorageStatement *mStatement;
    PRUint32 mParamCount;

    friend class mozStorageStatement;
};

static PRBool
JSValStorageStatementBinder (JSContext *cx,
                             mozIStorageStatement *aStatement,
                             int aIdx,
                             jsval val)
{
    if (JSVAL_IS_INT(val)) {
        int v = JSVAL_TO_INT(val);
        (void)aStatement->BindInt32Parameter(aIdx, v);
    } else if (JSVAL_IS_DOUBLE(val)) {
        double d = *JSVAL_TO_DOUBLE(val);
        (void)aStatement->BindDoubleParameter(aIdx, d);
    } else if (JSVAL_IS_STRING(val)) {
        JSString *str = JSVAL_TO_STRING(val);
        (void)aStatement->BindStringParameter(aIdx, nsDependentString(reinterpret_cast<PRUnichar*>(JS_GetStringChars(str)), JS_GetStringLength(str)));
    } else if (JSVAL_IS_BOOLEAN(val)) {
        (void)aStatement->BindInt32Parameter(aIdx, (val == JSVAL_TRUE) ? 1 : 0);
    } else if (JSVAL_IS_NULL(val)) {
        (void)aStatement->BindNullParameter(aIdx);
    } else if (JSVAL_IS_OBJECT(val)) {
        JSObject *obj = JSVAL_TO_OBJECT(val);
        // some special things
        if (js_DateIsValid (cx, obj)) {
            double msecd = js_DateGetMsecSinceEpoch(cx, obj);
            msecd *= 1000.0;
            PRInt64 msec;
            LL_D2L(msec, msecd);

            (void)aStatement->BindInt64Parameter(aIdx, msec);
        } else {
            return PR_FALSE;
        }
    } else {
        return PR_FALSE;
    }

    return PR_TRUE;
}

#endif /* _MOZSTORAGESTATEMENTPARAMS_H_ */
