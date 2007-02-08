#include "stopwatch.h"
#include <stdio.h>
#include <time.h>
#ifdef XP_UNIX
#include <unistd.h>
#include <sys/times.h>
#endif
#ifdef XP_WIN
#include "windows.h"
#endif
#include "nsDebug.h"

// #define MILLISECOND_RESOLUTION to track time with greater precision
//  If not defined the resolution is to the second only
//
#define MILLISECOND_RESOLUTION
#ifdef MILLISECOND_RESOLUTION
double gTicks = 1.0e-4; // for millisecond resolution
#else
double gTicks = 1.0e-7; // for second resolution
#endif

Stopwatch::Stopwatch() {

#ifdef R__UNIX
   if (!gTicks) gTicks = (clock_t)sysconf(_SC_CLK_TCK);
#endif
   fState         = kUndefined;   
   fTotalCpuTime  = 0;
   fTotalRealTime = 0;   
   mCreatedStack = PR_FALSE;
   mSavedStates = nsnull;
   Start();
}

Stopwatch::~Stopwatch() {
  EState* state = 0;
  if (mSavedStates) {
    while ((state = (EState*) mSavedStates->Pop())) {
      delete state;
    } 
    delete mSavedStates;
  }
}

void Stopwatch::Start(PRBool reset) {
   if (reset) {
      fTotalCpuTime  = 0;
      fTotalRealTime = 0;
   }
   if (fState != kRunning) {
#ifndef R__UNIX
      fStartRealTime = GetRealTime();
      fStartCpuTime  = GetCPUTime();
#else
      struct tms cpt;
      fStartRealTime = (double)times(&cpt) / gTicks;
      fStartCpuTime  = (double)(cpt.tms_utime+cpt.tms_stime) / gTicks;
#endif
   }
   fState = kRunning;
}

void Stopwatch::Stop() {

#ifndef R__UNIX
   fStopRealTime = GetRealTime();
   fStopCpuTime  = GetCPUTime();
#else
   struct tms cpt;
   fStopRealTime = (double)times(&cpt) / gTicks;
   fStopCpuTime  = (double)(cpt.tms_utime+cpt.tms_stime) / gTicks;
#endif
   if (fState == kRunning) {
      fTotalCpuTime  += fStopCpuTime  - fStartCpuTime;
      fTotalRealTime += fStopRealTime - fStartRealTime;
   }
   fState = kStopped;
}


void Stopwatch::SaveState() {
  if (!mCreatedStack) {
    mSavedStates = new nsDeque(nsnull);
    if (!mSavedStates)
      return;
    mCreatedStack = PR_TRUE;
  }
  EState* state = new EState();
  if (state) {
    *state = fState;
    mSavedStates->PushFront((void*) state);
  }
}

void Stopwatch::RestoreState() {
  EState* state = nsnull;
  state = (EState*) mSavedStates->Pop();
  if (state) {
    if (*state == kRunning && fState == kStopped)
      Start(PR_FALSE);
    else if (*state == kStopped && fState == kRunning)
      Stop();
    delete state;
  }
  else {
    NS_WARNING("Stopwatch::RestoreState(): The saved state stack is empty.\n");
  }
}

void Stopwatch::Continue() {

  if (fState != kUndefined) {

    if (fState == kStopped) {
      fTotalCpuTime  -= fStopCpuTime  - fStartCpuTime;
      fTotalRealTime -= fStopRealTime - fStartRealTime;
    }

    fState = kRunning;
  }
}


// NOTE: returns seconds regardless of the state of the MILLISECOND_RESOLUTION #define
//
double Stopwatch::RealTime() {

  if (fState != kUndefined) {
    if (fState == kRunning)
      Stop();
  }

#ifdef MILLISECOND_RESOLUTION
  return fTotalRealTime/1000;
#else
  return fTotalRealTime;
#endif
}

// NOTE: returns milliseconds regardless of the state of the MILLISECOND_RESOLUTION #define
//
double Stopwatch::RealTimeInMilliseconds() {

  if (fState != kUndefined) {
    if (fState == kRunning)
      Stop();
  }

#ifdef MILLISECOND_RESOLUTION
  return fTotalRealTime;
#else
  return fTotalRealTime * 1000; // we don;t have milliseconds, so fake it
#endif
}

// NOTE: returns seconds regardless of the state of the MILLISECOND_RESOLUTION define
//
double Stopwatch::CpuTime() {
  if (fState != kUndefined) {

    if (fState == kRunning)
      Stop();

  }
#ifdef MILLISECOND_RESOLUTION
  return fTotalCpuTime / 1000;  // adjust from milliseconds to seconds
#else
  return fTotalCpuTime;
#endif
}


double Stopwatch::GetRealTime(){ 
#if defined(R__MAC)
// return(double)clock() / gTicks;
   return(double)clock() / 1000000L;
#elif defined(R__UNIX)
   struct tms cpt;
   return (double)times(&cpt) / gTicks;
#elif defined(R__VMS)
  return(double)clock()/gTicks;
#elif defined(WIN32)
  union     {FILETIME ftFileTime;
             __int64  ftInt64;
            } ftRealTime; // time the process has spent in kernel mode
  SYSTEMTIME st;
  GetSystemTime(&st);
  SystemTimeToFileTime(&st,&ftRealTime.ftFileTime);
  return (double)ftRealTime.ftInt64 * gTicks;
#endif
}


double Stopwatch::GetCPUTime(){ 
#if defined(R__MAC)
// return(double)clock() / gTicks;
   return(double)clock();
#elif defined(R__UNIX)
   struct tms cpt;
   times(&cpt);
   return (double)(cpt.tms_utime+cpt.tms_stime) / gTicks;
#elif defined(R__VMS)
   return(double)clock()/gTicks;
#elif defined(WINCE)
   return 0;
#elif defined(WIN32)

  DWORD       ret;
  FILETIME    ftCreate,       // when the process was created
              ftExit;         // when the process exited

  union     {FILETIME ftFileTime;
             __int64  ftInt64;
            } ftKernel; // time the process has spent in kernel mode

  union     {FILETIME ftFileTime;
             __int64  ftInt64;
            } ftUser;   // time the process has spent in user mode

  HANDLE hProcess = GetCurrentProcess();
  ret = GetProcessTimes (hProcess, &ftCreate, &ftExit,
                                   &ftKernel.ftFileTime,
                                   &ftUser.ftFileTime);
  if (ret != PR_TRUE){
    ret = GetLastError ();
#ifdef DEBUG
    printf("%s 0x%lx\n"," Error on GetProcessTimes", (int)ret);
#endif
    }

  /*
   * Process times are returned in a 64-bit structure, as the number of
   * 100 nanosecond ticks since 1 January 1601.  User mode and kernel mode
   * times for this process are in separate 64-bit structures.
   * To convert to floating point seconds, we will:
   *
   *          Convert sum of high 32-bit quantities to 64-bit int
   */

    return (double) (ftKernel.ftInt64 + ftUser.ftInt64) * gTicks;

#endif
}


void Stopwatch::Print(void) {
   // Print the real and cpu time passed between the start and stop events.

   double  realt = RealTimeInMilliseconds();

   int  hours = int(realt / 3600000);
   realt -= hours * 3600000;
   int  min   = int(realt / 60000);
   realt -= min * 60000;
   int  sec   = int(realt/1000);
   realt -= sec * 1000;
#ifdef MOZ_PERF_METRICS
  int ms = int(realt);
   RAPTOR_STOPWATCH_TRACE(("Real time %d:%d:%d.%d, CP time %.3f\n", hours, min, sec, ms, CpuTime()));
#elif defined(DEBUG)
  int ms = int(realt);
   printf("Real time %d:%d:%d.%d, CP time %.3f\n", hours, min, sec, ms, CpuTime());
#endif
}
