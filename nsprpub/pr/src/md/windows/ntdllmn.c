/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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
 * The DLL entry point (DllMain) for NSPR.
 *
 * The only reason we use DLLMain() now is to find out whether
 * the NSPR DLL is statically or dynamically loaded.  When
 * dynamically loaded, we cannot use static thread-local storage.
 * However, static TLS is faster than the TlsXXX() functions.
 * So we want to use static TLS whenever we can.  A global
 * variable _pr_use_static_tls is set in DllMain() during process
 * attachment to indicate whether it is safe to use static TLS
 * or not.
 */

#include <windows.h>
#include <primpl.h>

extern BOOL _pr_use_static_tls;  /* defined in ntthread.c */

BOOL WINAPI DllMain(
    HINSTANCE hinstDLL,
    DWORD fdwReason,
    LPVOID lpvReserved)
{
PRThread *me;

    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            /*
             * If lpvReserved is NULL, we are dynamically loaded
             * and therefore can't use static thread-local storage.
             */
            if (lpvReserved == NULL) {
                _pr_use_static_tls = FALSE;
            } else {
                _pr_use_static_tls = TRUE;
            }
            break;
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            if (_pr_initialized) {
                me = _MD_GET_ATTACHED_THREAD();
                if ((me != NULL) && (me->flags & _PR_ATTACHED))
                    _PRI_DetachThread();
            }
            break;
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}
