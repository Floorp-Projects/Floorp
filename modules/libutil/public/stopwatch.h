const double gTicks = 1.0e-7;

#ifdef RAPTOR_PERF_METRICS
#  define NS_RESET_AND_START_STOPWATCH(_sw)          \
    _sw.Start(PR_TRUE);

#  define NS_START_STOPWATCH(_sw)                    \
    _sw.Start(PR_FALSE);

#  define NS_STOP_STOPWATCH(_sw)                     \
    _sw.Stop();

#  define NS_SAVE_STOPWATCH_STATE(_sw)               \
    _sw.SaveState();

#  define NS_RESTORE_STOPWATCH_STATE(_sw)            \
    _sw.RestoreState();

#else
#  define NS_RESET_AND_START_STOPWATCH(_sw) 
#  define NS_START_STOPWATCH(_sw)
#  define NS_STOP_STOPWATCH(_sw)
#  define NS_SAVE_STOPWATCH_STATE(_sw)
#  define NS_RESTORE_STOPWATCH_STATE(_sw)
#endif

class Stopwatch {

private:
   enum EState { kUndefined, kStopped, kRunning };

   double         fStartRealTime;   //wall clock start time
   double         fStopRealTime;    //wall clock stop time
   double         fStartCpuTime;    //cpu start time
   double         fStopCpuTime;     //cpu stop time
   double         fTotalCpuTime;    //total cpu time
   double         fTotalRealTime;   //total real time
   EState         fState;           //stopwatch state
   EState         fSavedState;      //last saved state

public:
   Stopwatch();
   void           Start(PRBool reset = PR_TRUE);
   void           Stop();
   void           Continue();
   void           SaveState();      // record current state of stopwatch
   void           RestoreState();   // restore last recored state of stopwatch
   double         RealTime();
   void           Reset() { ResetCpuTime(); ResetRealTime(); }
   void           ResetCpuTime(double time = 0) { Stop();  fTotalCpuTime = time; }
   void           ResetRealTime(double time = 0) { Stop(); fTotalRealTime = time; }
   double         CpuTime();
   void           Print(void);
   static double  GetRealTime();
   static double  GetCPUTime();

};
