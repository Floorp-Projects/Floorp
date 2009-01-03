// Copyright (c) 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// PreCompile.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#ifndef CLIENT_WINDOWS_TESTS_CRASH_GENERATION_APP_PRECOMPILE_H__
#define CLIENT_WINDOWS_TESTS_CRASH_GENERATION_APP_PRECOMPILE_H__

// Modify the following defines if you have to target a platform prior to
// the ones specified below. Refer to MSDN for the latest info on
// corresponding values for different platforms.

// Allow use of features specific to Windows XP or later.
#ifndef WINVER
// Change this to the appropriate value to target other versions of Windows.
#define WINVER 0x0501
#endif

// Allow use of features specific to Windows XP or later.
#ifndef _WIN32_WINNT
// Change this to the appropriate value to target other versions of Windows.
#define _WIN32_WINNT 0x0501
#endif

// Allow use of features specific to Windows 98 or later.
#ifndef _WIN32_WINDOWS
// Change this to the appropriate value to target Windows Me or later.
#define _WIN32_WINDOWS 0x0410
#endif

// Allow use of features specific to IE 6.0 or later.
#ifndef _WIN32_IE
// Change this to the appropriate value to target other versions of IE.
#define _WIN32_IE 0x0600
#endif

// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <DbgHelp.h>
#include <malloc.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>

#include <cassert>
#include <list>

#include "client/windows/common/ipc_protocol.h"
#include "client/windows/crash_generation/client_info.h"
#include "client/windows/crash_generation/crash_generation_client.h"
#include "client/windows/crash_generation/crash_generation_server.h"
#include "client/windows/crash_generation/minidump_generator.h"
#include "client/windows/handler/exception_handler.h"
#include "client/windows/tests/crash_generation_app/abstract_class.h"
#include "client/windows/tests/crash_generation_app/crash_generation_app.h"
#include "google_breakpad/common/minidump_format.h"

#endif  // CLIENT_WINDOWS_TESTS_CRASH_GENERATION_APP_PRECOMPILE_H__
