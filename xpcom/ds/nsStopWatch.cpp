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
 */

#include "nsStopWatch.h"
#include <stdio.h>
#include <time.h>
#ifdef XP_UNIX
#include <unistd.h>
#include <sys/times.h>
#endif
#ifdef XP_WIN
#include "windows.h"
#endif

// All state variables store time in millesecs

// gTicks : is the number of clock ticks in a second. All system calls
// 			are expected to return time in ticks
//
#if defined(XP_UNIX)
static const double gTicks = CLK_TCK;
#elif defined(XP_WIN)
// System returns time as a number of 100 nanoticks
static const double gTicks = 10e+7;
#else
static const double gTicks = 10e-4;
#endif /* XP_UNIX */

#define NS_TICKS_TO_MILLISECS(tick) ((tick) * (1000L / gTicks))
#define NS_MILLISECS_TO_SECS(millisecs) (millisecs * 1000L)

nsStopWatch::nsStopWatch()
{
    fState         = kUndefined;
    fTotalCpuTime  = 0;
    fTotalRealTime = 0;
    mCreatedStack = PR_FALSE;
    mSavedStates = nsnull;
    Start();
}

nsStopWatch::~nsStopWatch()
{
    EState* state = 0;
    if (mSavedStates)
    {
        while ((state = (EState*) mSavedStates->Pop()))
        {
            delete state;
        }
        delete mSavedStates;
    }
}

///////////////////////////////////////////////////////////////////////////
// StopWatch controls: Start, Stop, Continue

void nsStopWatch::Start(PRBool reset)
{
    if (reset)
    {
        fTotalCpuTime  = 0;
        fTotalRealTime = 0;
    }
    if (fState != kRunning)
    {
        GetSystemTime(&fStartRealTime, &fStartCpuTime);
        fState = kRunning;
    }
}

void nsStopWatch::Stop() {
    GetSystemTime(&fStopRealTime, &fStopCpuTime);
    if (fState == kRunning)
    {
        fTotalCpuTime  += fStopCpuTime  - fStartCpuTime;
        fTotalRealTime += fStopRealTime - fStartRealTime;
    }
    fState = kStopped;
}


void nsStopWatch::Continue()
{
    if (fState != kUndefined)
    {
        if (fState == kStopped)
        {
            fTotalCpuTime  -= fStopCpuTime  - fStartCpuTime;
            fTotalRealTime -= fStopRealTime - fStartRealTime;
        }
        fState = kRunning;
    }
}

///////////////////////////////////////////////////////////////////////////
// Getters

int nsStopWatch::GetTime(double *realStopWatchTime, double *cpuStopWatchTime)
{
    double realTime = 0;
    double cpuTime = 0;

    if (fState == kUndefined)
        return -1;

    if (fState == kStopped)
    {
        realTime = fTotalRealTime;
        cpuTime = fTotalCpuTime;
    }
    else
    {
        // We haven't stopped. Just snapshot the current time.
        GetSystemTime(&realTime, &cpuTime);
        realTime = fTotalRealTime + realTime - fStartRealTime;
        cpuTime = fTotalCpuTime + cpuTime - fStartCpuTime;
    }

    if (realStopWatchTime) *realStopWatchTime = realTime;
    if (cpuStopWatchTime) *cpuStopWatchTime = cpuTime;

    return 0;
}

///////////////////////////////////////////////////////////////////////////
// Print

void nsStopWatch::Print(PRLogModuleInfo* log)
{
    // Print the real and cpu time passed between the start and stop events.
    double  realt, cput;
    GetTime(&realt, &cput);

    int  hours = int(realt / 3600000);
    realt -= hours * 3600000;
    int  min   = int(realt / 60000);
    realt -= min * 60000;
    int  sec   = int(realt/1000);
    realt -= sec * 1000;
    int ms     = int(realt);
    if (log)
        PR_LOG(log, PR_LOG_DEBUG,
               ("Real time %d:%d:%d.%d, CP time %.3f\n", hours, min, sec, ms, cput));
    else
        printf("Real time %d:%d:%d.%d, CP time %.3f\n", hours, min, sec, ms, cput);
}

///////////////////////////////////////////////////////////////////////////
// State save and restore functions

void nsStopWatch::SaveState()
{
    if (!mCreatedStack)
    {
        mSavedStates = new nsDeque(nsnull);
        mCreatedStack = PR_TRUE;
    }
    EState* state = new EState();
    *state = fState;
    mSavedStates->PushFront((void*) state);
}


void nsStopWatch::RestoreState()
{
    EState* state = nsnull;
    state = (EState*) mSavedStates->Pop();
    if (state)
    {
        if (*state == kRunning && fState == kStopped)
            Start(PR_FALSE);
        else if (*state == kStopped && fState == kRunning)
            Stop();
        delete state;
    }
    else
    {
        PR_ASSERT("nsStopWatch::RestoreState(): The saved state stack is empty.\n");
    }
}

///////////////////////////////////////////////////////////////////////////
// Static functions to get time in millisecs from the system

//
// GetSystemTime()
// returns both real and cpu times in one shot.
int nsStopWatch::GetSystemTime(double *real, double *cpu)
{
#if defined(XP_UNIX)
    double realSysTime, cpuSysTime;
    struct tms cpt;
    realSysTime = NS_TICKS_TO_MILLISECS(times(&cpt));
    cpuSysTime = NS_TICKS_TO_MILLISECS((cpt.tms_utime+cpt.tms_stime));
    if (real) *real = realSysTime;
    if (cpu) *cpu = cpuSysTime;
#else
    if (real) *real = GetSystemRealTime();
    if (cpu) *cpu = GetSystemCpuTime();
#endif
    return 0;
}

double nsStopWatch::GetSystemRealTime()
{
#if defined(XP_MAC)
    return(double)clock() / 1000000L;
#elif defined(XP_UNIX)
    struct tms cpt;
    times(&cpt);
    return NS_TICKS_TO_MILLISECS((double)times(&cpt));
#elif defined(R__VMS)
    return(double)clock()/gTicks;
#elif defined(XP_WIN)
    union {
        FILETIME ftFileTime;
        __int64  ftInt64;
    } ftRealTime; // time the process has spent in kernel mode
    SYSTEMTIME st;
    GetSystemTime(&st);
    SystemTimeToFileTime(&st,&ftRealTime.ftFileTime);
    return NS_TICKS_TO_MILLISECS((double)ftRealTime.ftInt64);
#endif
}


double nsStopWatch::GetSystemCPUTime()
{
#if defined(XP_MAC)
   return(double)clock();
#elif defined(XP_UNIX)
   struct tms cpt;
   times(&cpt);
   return NS_TICKS_TO_MILLISECS((double)(cpt.tms_utime+cpt.tms_stime));
#elif defined(R__VMS)
   return(double)clock()/gTicks;
#elif defined(XP_WIN)

  OSVERSIONINFO OsVersionInfo;

//*-*         Value                      Platform
//*-*  ----------------------------------------------------
//*-*  VER_PLATFORM_WIN32_WINDOWS       Win32 on Windows 95
//*-*  VER_PLATFORM_WIN32_NT            Windows NT
//*-*
  OsVersionInfo.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
  GetVersionEx(&OsVersionInfo);
  if (OsVersionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
  {
      DWORD       ret;
      FILETIME    ftCreate;       // when the process was created
      FILETIME    ftExit;         // when the process exited

      union {
          FILETIME ftFileTime;
          __int64  ftInt64;
      } ftKernel; // time the process has spent in kernel mode

    union {
        FILETIME ftFileTime;
        __int64  ftInt64;
    } ftUser;   // time the process has spent in user mode

    HANDLE hProcess = GetCurrentProcess();
    ret = GetProcessTimes (hProcess, &ftCreate, &ftExit,
                           &ftKernel.ftFileTime,
                           &ftUser.ftFileTime);
    if (ret != PR_TRUE)
    {
        ret = GetLastError ();
        printf("%s 0x%lx\n"," Error on GetProcessTimes", (int)ret);
    }

    /*
     * Process times are returned in a 64-bit structure, as the number of
     * 100 nanosecond ticks since 1 January 1601.  User mode and kernel mode
     * times for this process are in separate 64-bit structures.
     * To convert to floating point seconds, we will:
     *
     *          Convert sum of high 32-bit quantities to 64-bit int
     */

      return NS_TICKS_TO_MILLISECS((double) (ftKernel.ftInt64 + ftUser.ftInt64));
  }
  else
      return GetSystemRealTime();

#endif
}

