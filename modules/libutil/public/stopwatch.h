const double gTicks = 1.0e-7;

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

public:
   Stopwatch();
   void           Start(bool reset = true);
   void           Stop();
   void           Continue();
   double         RealTime();
   void           Reset() { ResetCpuTime(); ResetRealTime(); }
   void           ResetCpuTime(double time = 0) { Stop();  fTotalCpuTime = time; }
   void           ResetRealTime(double time = 0) { Stop(); fTotalRealTime = time; }
   double         CpuTime();
   void           Print(void);
   static double  GetRealTime();
   static double  GetCPUTime();

};
