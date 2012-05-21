/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
#include <windows.h>
#include "nsToolkit.h"

#if defined(__GNUC__)
// If DllMain gets name mangled, it won't be seen.
extern "C" {
#endif

BOOL APIENTRY DllMain(  
                      HINSTANCE hModule, 
                      DWORD reason, 
                      LPVOID lpReserved )
{
    switch( reason ) {
        case DLL_PROCESS_ATTACH:
            nsToolkit::Startup((HINSTANCE)hModule);
            break;

        case DLL_THREAD_ATTACH:
            break;
    
        case DLL_THREAD_DETACH:
            break;
    
        case DLL_PROCESS_DETACH:
            nsToolkit::Shutdown();
            break;

    }

    return TRUE;
}

#if defined(__GNUC__)
} // extern "C"
#endif
