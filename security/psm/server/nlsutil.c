/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
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
#include "nlsutil.h"
#include "serv.h" /* for SSM_DEBUG */
#include "ssmerrs.h"
#include "resource.h"

/* atoi() for a UnicodeString. (sigh) */
PRInt32
SSMTextGen_atoi(char *str)
{
    PRInt32 result = -1;

    /* Extract the text by converting to ASCII. */
    if (str)
    {
        /* Convert the text into a number, if possible. */
        PR_sscanf(str, "%ld", &result);
    }

    return result;
}

/* Debug a UnicodeString. */
#ifdef DEBUG
void
SSM_DebugUTF8String(char *prefix, char *ustr)
{
    char *ch;

    if (ustr)
    {
        ch = ustr;
        if (ch)
            SSM_DEBUG("%s:\n%s\n"
                      "End of text\n", (prefix ? prefix : ""), ch);
    }
    else
        SSM_DEBUG("%s: (null string)\n", (prefix ? prefix : ""));
}
#endif

/* Clear a UnicodeString. */
void
SSMTextGen_UTF8StringClear(char **str)
{
    if (str && *str)
    {
        PR_Free(*str);
        *str = NULL;
    }
}


SSMStatus 
SSM_ConcatenateUTF8String(char **prefix, char *suffix)
{
    char *tmp;

    if (prefix == NULL || suffix == NULL) {
        return SSM_FAILURE;
    }
    tmp = PR_smprintf("%s%s", (*prefix) ? *prefix : "", suffix);
    if (tmp == NULL) {
        return SSM_FAILURE;
    }
    PR_Free(*prefix);
    *prefix = tmp;
    return SSM_SUCCESS;
}
