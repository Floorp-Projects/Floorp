/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#if defined(WIN16)
#include <windows.h>
#endif
#include "prtypes.h"

extern PRIntn my_global;

PR_IMPLEMENT(PRIntn) My_GetValue()
{
    return my_global;
}

#if defined(WIN16)
int CALLBACK LibMain( HINSTANCE hInst, WORD wDataSeg, 
                      WORD cbHeapSize, LPSTR lpszCmdLine )
{
    return TRUE;
}
#endif /* WIN16 */

