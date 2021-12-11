/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
                if ((me != NULL) && (me->flags & _PR_ATTACHED)) {
                    _PRI_DetachThread();
                }
            }
            break;
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}
