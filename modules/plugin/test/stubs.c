/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "_jri/netscape_plugin_Plugin.c"

/*
** Define IMPLEMENT_Simple in order to pull the stub code in when
** compiling the Simple.c file. Without doing this, we'd get a
** UnsatisfiedLinkError whenever we tried to call one of the native
** methods:
*/
#define IMPLEMENT_Simple

/*
** Include the native stubs, initialization routines and
** debugging code. You should be building with DEBUG defined in order to
** take advantage of the diagnostic code that javah creates for
** you. Otherwise your life will be hell.
*/
#include "_jri/Simple.c"

#include "prtypes.h"
#include "npapi.h"

// Include the old API glue file.
#ifdef XP_UNIX
#   include "npunix.c"
#endif

