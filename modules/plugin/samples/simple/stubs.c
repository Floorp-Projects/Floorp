/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

