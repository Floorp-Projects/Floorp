/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "rl.h"
#include "plstr.h"

PR_IMPLEMENT(void)
RLInit(void)
{
}

PR_IMPLEMENT(void)
RLTrace(const char* fmtstr, ...)
{
    char* str;
    va_list ap;
    va_start(ap, fmtstr);

    str = PR_vsmprintf(fmtstr, ap);
    if (str) {
        PR_Write(PR_STDERR, str, PL_strlen(str));
        PR_smprintf_free(str);
    }
}
