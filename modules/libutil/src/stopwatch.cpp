#include "windows.h"
#include <stdio.h>
#include "stopwatch.h"

Stopwatch::Stopwatch() {

#ifdef R__UNIX
   if (!gTicks) gTicks = (clock_t)sysconf(_SC_CLK_TCK);
#endif
   fState         = kUndefined;
   fTotalCpuTime  = 0;
   fTotalRealTime = 0;
   Start();
}


void Stopwatch::Start(bool reset) {
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


void Stopwatch::Continue() {

  if (fState != kUndefined) {

    if (fState == kStopped) {
      fTotalCpuTime  -= fStopCpuTime  - fStartCpuTime;
      fTotalRealTime -= fStopRealTime - fStartRealTime;
    }

    fState = kRunning;
  }
}


double Stopwatch::RealTime() {

  if (fState != kUndefined) {
    if (fState == kRunning)
      Stop();
  }

  return fTotalRealTime;
}

double Stopwatch::CpuTime() {
  if (fState != kUndefined) {

    if (fState == kRunning)
      Stop();

  }
  return fTotalCpuTime;
}


double Stopwatch::GetRealTime(){ 
#if defined(R__MAC)
   return(double)clock() / gTicks;
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
   return(double)clock() / gTicks;
#elif defined(R__UNIX)
   struct tms cpt;
   times(&cpt);
   return (double)(cpt.tms_utime+cpt.tms_stime) / gTicks;
#elif defined(R__VMS)
   return(double)clock()/gTicks;
#elif defined(WIN32)

  OSVERSIONINFO OsVersionInfo;

//*-*         Value                      Platform
//*-*  ----------------------------------------------------
//*-*  VER_PLATFORM_WIN32_WINDOWS       Win32 on Windows 95
//*-*  VER_PLATFORM_WIN32_NT            Windows NT
//*-*
  OsVersionInfo.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
  GetVersionEx(&OsVersionInfo);
  if (OsVersionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT) {
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
    if (ret != TRUE){
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

      return (double) (ftKernel.ftInt64 + ftUser.ftInt64) * gTicks;
  }
  else
      return GetRealTime();

#endif
}


void Stopwatch::Print(void) {
   // Print the real and cpu time passed between the start and stop events.

   double  realt = RealTime();

   int  hours = int(realt / 3600);
   realt -= hours * 3600;
   int  min   = int(realt / 60);
   realt -= min * 60;
   int  sec   = int(realt);
   printf("Real time %d:%d:%d, CP time %.3f", hours, min, sec, CpuTime());
}

