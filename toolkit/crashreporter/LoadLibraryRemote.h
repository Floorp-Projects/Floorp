/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LoadLibraryRemote_h
#define LoadLibraryRemote_h

#include <windows.h>

/**
 * Inject a library into a remote process. This injection has the following
 * restrictions:
 *
 * - The DLL being injected must only depend on kernel32 and user32.
 * - The entry point of the DLL is not run. If the DLL uses the CRT, it is
 *   the responsibility of the caller to make sure that _CRT_INIT is called.
 * - There is no support for unloading a library once it has been loaded.
 * - The symbol must be a named symbol and not an ordinal.
 */
void* LoadRemoteLibraryAndGetAddress(HANDLE hRemoteProcess,
                                     const WCHAR* library, const char* symbol);

#endif  // LoadLibraryRemote_h
