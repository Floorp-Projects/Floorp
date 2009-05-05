/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Jeff Walden <jwalden+code@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
 * Test harness for XPCOM objects, providing a scoped XPCOM initializer,
 * nsCOMPtr, nsRefPtr, do_CreateInstance, do_GetService, ns(Auto|C|)String,
 * and stdio.h/stdlib.h.
 */

#ifndef TestHarness_h__
#define TestHarness_h__

#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsStringGlue.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

static PRUint32 gFailCount = 0;

/**
 * Prints the given failure message and arguments using printf, prepending
 * "TEST-UNEXPECTED-FAIL " for the benefit of the test harness and
 * appending "\n" to eliminate having to type it at each call site.
 */
void fail(const char* msg, ...)
{
  va_list ap;

  printf("TEST-UNEXPECTED-FAIL | ");

  va_start(ap, msg);
  vprintf(msg, ap);
  va_end(ap);

  putchar('\n');
  ++gFailCount;
}

/**
 * Prints the given string prepending "TEST-PASS | " for the benefit of
 * the test harness and with "\n" at the end, to be used at the end of a
 * successful test function.
 */
void passed(const char* test)
{
  printf("TEST-PASS | %s\n", test);
}

//-----------------------------------------------------------------------------
// Code profiling
//
static const char* gCurrentProfile;
static PRBool gProfilerTriedInit = PR_FALSE;
static PRBool gProfilerInited = PR_FALSE;

// Platform profilers must implement these functions.
// Init and deinit are guaranteed to only be called once, and
// StartProfile/StopProfile may assume that they are only called
// when the profiler has successfully been initialized.
static PRBool _PlatformInitProfiler();
static PRBool _PlatformStartProfile(const char* profileName);
static PRBool _PlatformStopProfile(const char* profileName);
static PRBool _PlatformDeinitProfiler();

/**
 * If the build has been configured properly, start the best code profiler
 * available on this platform.
 *
 * This is NOT thread safe.
 *
 * @precondition Profiling is not started
 * @param profileName A descriptive name for this profiling run.  Every 
 *                    attempt is made to name the profile data according
 *                    to this name, but check your platform's profiler
 *                    documentation for what this means.
 * @return PR_TRUE if profiling was available and successfully started.
 * @see StopProfiling
 */
static PRBool
StartProfiling(const char* profileName)
{
    if (!gProfilerTriedInit) {
        gProfilerTriedInit = PR_TRUE;
        gProfilerInited = _PlatformInitProfiler();
    }
    if (!gProfilerInited)
        return PR_FALSE;

    NS_ASSERTION(profileName, "need a name for this profile");
    NS_PRECONDITION(!gCurrentProfile, "started a new profile before stopping another");

    PRBool rv = _PlatformStartProfile(profileName);
    gCurrentProfile = profileName;
    return rv;
}

/**
 * Stop the platform's profiler.  For what this means, what happens after
 * stopping, and how the profile data can be accessed, check the 
 * documentation of your platform's profiler.
 *
 * This is NOT thread safe.
 *
 * @precondition Profiling was started
 * @return PR_TRUE if profiling was successfully stopped.
 * @see StartProfiling
 */
static PRBool
StopProfiling()
{
    NS_ASSERTION(gProfilerTriedInit, "tried to stop profile before starting one");
    if (!gProfilerInited)
        return PR_FALSE;

    NS_PRECONDITION(gCurrentProfile, "tried to stop profile before starting one");
    
    const char* profileName = gCurrentProfile;
    gCurrentProfile = 0;
    return _PlatformStopProfile(profileName);
}

//--------------------------------------------------
// Shark impl
#if defined(MOZ_SHARK)
#include <CHUD/CHUD.h>

static PRBool
_PlatformInitProfiler()
{
    if (chudSuccess != chudInitialize())
        return PR_FALSE;
    if (chudSuccess != chudAcquireRemoteAccess()) {
        NS_WARNING("Couldn't connect to Shark.  Is it running and in Programmatic mode (Shift-Cmd-R)?");
        return PR_FALSE;
    }
   return PR_TRUE;
}

static PRBool
_PlatformStartProfile(const char* profileName)
{
    return (chudSuccess == chudStartRemotePerfMonitor(profileName)) ?
        PR_TRUE : PR_FALSE;
}

static PRBool
_PlatformStopProfile(const char* profileName)
{
    return (chudSuccess == chudStopRemotePerfMonitor()) ?
        PR_TRUE : PR_FALSE;
}

static PRBool
_PlatformDeinitProfiler()
{
    return (chudIsRemoteAccessAcquired() 
            && chudSuccess == chudReleaseRemoteAccess()) ?
        PR_TRUE : PR_FALSE;
}

//--------------------------------------------------
// Default, no-profiler impl
#else 

static PRBool
_PlatformInitProfiler()
{
    NS_WARNING("Profiling is not available/configured for your platform.");
    return PR_FALSE;
}
static PRBool
_PlatformStartProfile(const char* profileName)
{
    NS_WARNING("Profiling is not available/configured for your platform.");
    return PR_FALSE;
}
static PRBool
_PlatformStopProfile(const char* profileName)
{
    NS_WARNING("Profiling is not available/configured for your platform.");
    return PR_FALSE;
}
static PRBool
_PlatformDeinitProfiler()
{
    NS_WARNING("Profiling is not available/configured for your platform.");
    return PR_FALSE;
}

#endif

//-----------------------------------------------------------------------------

class ScopedXPCOM
{
  public:
    ScopedXPCOM(const char* testName,
                nsIDirectoryServiceProvider *dirSvcProvider = NULL)
    {
      mTestName = testName;
      printf("Running %s tests...\n", mTestName);

      nsresult rv = NS_InitXPCOM2(&mServMgr, NULL, dirSvcProvider);
      if (NS_FAILED(rv))
      {
        fail("NS_InitXPCOM2 returned failure code 0x%x", rv);
        mServMgr = NULL;
      }
    }

    ~ScopedXPCOM()
    {
      if (gProfilerInited)
        if (!_PlatformDeinitProfiler())
          NS_WARNING("Problem shutting down profiler");

      if (mServMgr)
      {
        NS_RELEASE(mServMgr);
        nsresult rv = NS_ShutdownXPCOM(NULL);
        if (NS_FAILED(rv))
        {
          fail("XPCOM shutdown failed with code 0x%x", rv);
          exit(1);
        }
      }

      printf("Finished running %s tests.\n", mTestName);
    }

    PRBool failed()
    {
      return mServMgr == NULL;
    }

  private:
    const char* mTestName;
    nsIServiceManager* mServMgr;
};

#endif  // TestHarness_h__
