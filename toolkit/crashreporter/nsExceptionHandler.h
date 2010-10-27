/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Breakpad integration
 *
 * The Initial Developer of the Original Code is
 * Ted Mielczarek <ted.mielczarek@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsExceptionHandler_h__
#define nsExceptionHandler_h__

#include "nscore.h"
#include "nsDataHashtable.h"
#include "nsXPCOM.h"
#include "nsStringGlue.h"

#include "nsIFile.h"

#if defined(XP_WIN32)
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#if defined(XP_MACOSX)
#include <mach/mach.h>
#endif

namespace CrashReporter {
nsresult SetExceptionHandler(nsILocalFile* aXREDirectory, bool force=false);
nsresult UnsetExceptionHandler();
bool     GetEnabled();
bool     GetServerURL(nsACString& aServerURL);
nsresult SetServerURL(const nsACString& aServerURL);
bool     GetMinidumpPath(nsAString& aPath);
nsresult SetMinidumpPath(const nsAString& aPath);
nsresult AnnotateCrashReport(const nsACString& key, const nsACString& data);
nsresult AppendAppNotesToCrashReport(const nsACString& data);
nsresult SetRestartArgs(int argc, char** argv);
nsresult SetupExtraData(nsILocalFile* aAppDataDirectory,
                        const nsACString& aBuildID);

// Functions for working with minidumps and .extras
typedef nsDataHashtable<nsCStringHashKey, nsCString> AnnotationTable;

bool GetMinidumpForID(const nsAString& id, nsILocalFile** minidump);
bool GetIDFromMinidump(nsILocalFile* minidump, nsAString& id);
bool GetExtraFileForID(const nsAString& id, nsILocalFile** extraFile);
bool GetExtraFileForMinidump(nsILocalFile* minidump, nsILocalFile** extraFile);
bool AppendExtraData(const nsAString& id, const AnnotationTable& data);
bool AppendExtraData(nsILocalFile* extraFile, const AnnotationTable& data);

#ifdef XP_WIN32
  nsresult WriteMinidumpForException(EXCEPTION_POINTERS* aExceptionInfo);
#endif
#ifdef XP_MACOSX
  nsresult AppendObjCExceptionInfoToAppNotes(void *inException);
#endif
nsresult GetSubmitReports(PRBool* aSubmitReport);
nsresult SetSubmitReports(PRBool aSubmitReport);

#ifdef MOZ_IPC
// Out-of-process crash reporter API.

// Return true iff a dump was found for |childPid|, and return the
// path in |dump|.  The caller owns the last reference to |dump| if it
// is non-NULL.
bool TakeMinidumpForChild(PRUint32 childPid,
                          nsILocalFile** dump NS_OUTPARAM);

#if defined(XP_WIN)
typedef HANDLE ProcessHandle;
typedef DWORD ThreadId;
#elif defined(XP_MACOSX)
typedef task_t ProcessHandle;
typedef mach_port_t ThreadId;
#else
typedef int ProcessHandle;
typedef int ThreadId;
#endif

// Return the current thread's ID.
//
// XXX: this is a somewhat out-of-place interface to expose through
// crashreporter, but it takes significant work to call sys_gettid()
// correctly on Linux and breakpad has already jumped through those
// hoops for us.
ThreadId CurrentThreadId();

// Create new minidumps that are snapshots of the state of this parent
// process and |childPid|.  Return true on success along with the
// minidumps and a new UUID that can be used to correlate the dumps.
//
// If this function fails, it's the caller's responsibility to clean
// up |childDump| and |parentDump|.  Either or both can be created and
// returned non-null on failure.
bool CreatePairedMinidumps(ProcessHandle childPid,
                           ThreadId childBlamedThread,
                           nsAString* pairGUID NS_OUTPARAM,
                           nsILocalFile** childDump NS_OUTPARAM,
                           nsILocalFile** parentDump NS_OUTPARAM);

#  if defined(XP_WIN32) || defined(XP_MACOSX)
// Parent-side API for children
const char* GetChildNotificationPipe();

// Child-side API
bool SetRemoteExceptionHandler(const nsACString& crashPipe);

#  elif defined(XP_LINUX)
// Parent-side API for children

// Set the outparams for crash reporter server's fd (|childCrashFd|)
// and the magic fd number it should be remapped to
// (|childCrashRemapFd|) before exec() in the child process.
// |SetRemoteExceptionHandler()| in the child process expects to find
// the server at |childCrashRemapFd|.  Return true iff successful.
//
// If crash reporting is disabled, both outparams will be set to -1
// and |true| will be returned.
bool CreateNotificationPipeForChild(int* childCrashFd, int* childCrashRemapFd);

// Child-side API
bool SetRemoteExceptionHandler();

#endif  // XP_WIN32

bool UnsetRemoteExceptionHandler();
#endif // MOZ_IPC

#if defined(__ANDROID__)
// Android builds use a custom library loader, so /proc/<pid>/maps
// will just show anonymous mappings for all the non-system
// shared libraries. This API is to work around that by providing
// info about the shared libraries that are mapped into these anonymous
// mappings.
void AddLibraryMapping(const char* library_name,
                       const char* file_id,
                       uintptr_t   start_address,
                       size_t      mapping_length,
                       size_t      file_offset);
#endif
}

#endif /* nsExceptionHandler_h__ */
