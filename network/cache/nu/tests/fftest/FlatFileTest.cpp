/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include <prtypes.h>
#include <prio.h>
#include <plstr.h>
#include "nsFlatFile.h"
#include "nsTOC.h"

int main()
{
    PRFileDesc* pfd = PR_GetSpecialFD(PR_StandardOutput);
    
    nsFlatFile tf("CACHE.DAT");
    nsTOC toc("Cache.TOC", &tf);

    if (tf.IsValid() && toc.IsValid())
    {
        PR_Write(pfd, tf.Filename(), PL_strlen(tf.Filename()));
        PR_Write(pfd, "  ", 2);
        PR_Write(pfd, toc.Filename(), PL_strlen(toc.Filename()));

    }


    return 1;
}