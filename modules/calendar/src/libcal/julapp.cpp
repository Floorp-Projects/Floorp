/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

/*
 * julapp.cpp
 * John Sun
 * 4/28/98 9:48:56 AM
 */

#include "julinit.h"
#include <nlscol.h>
#include <nlsenc.h>
#include <nlsuni.h>
#include <nlsutl.h>

//#include <prefapi.h>

//---------------------------------------------------------------------

void JulianApplication::init()
{
    char * dir = NULL;
    // TODO: finish
    // somehow set dir to the distribution directory
    // $(MOZ_SRC)/ns/dist/nls.
    // need to append final '/' (UNIX) or '\\' (WINDOWS) to dir
    // look through preferences.  get locale directory from prefs.js
    // using Prefs API.
    NLS_Initialize(NULL, dir);
}

//---------------------------------------------------------------------

JulianApplication::JulianApplication()
{
    init();
}
