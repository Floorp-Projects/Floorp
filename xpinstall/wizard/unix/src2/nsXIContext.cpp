/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Samir Gehani <sgehani@netscape.com>
 */

#include "nsXIContext.h"

nsXIContext::nsXIContext()
{
    me = NULL;

    ldlg = NULL;
    wdlg = NULL;
    sdlg = NULL;
    cdlg = NULL;
    idlg = NULL;

    window = NULL;
    back = NULL;
    next = NULL;
    logo = NULL; 
}

nsXIContext::~nsXIContext()
{
    // NOTE: don't try to delete "me" cause I control thee

    XI_IF_DELETE(ldlg);
    XI_IF_DELETE(wdlg);
    XI_IF_DELETE(sdlg);
    XI_IF_DELETE(cdlg);
    XI_IF_DELETE(idlg);

    XI_IF_FREE(window);
    XI_IF_FREE(back);
    XI_IF_FREE(next);
    XI_IF_FREE(logo);
}
