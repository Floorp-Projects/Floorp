/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil -*- */
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
