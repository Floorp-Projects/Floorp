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

#ifndef __nsStopWatch_H
#define __nsStopWatch_H
#include "prlog.h"
#include "nsDeque.h"

//
// nsStopWatch
//
// All times stored and returned are in millisecs
//

class nsStopWatch {

private:
    enum EState { kUndefined, kStopped, kRunning };

    // All state variables store time in millisecs
    double         fStartRealTime;   //wall clock start time
    double         fStopRealTime;    //wall clock stop time
    double         fStartCpuTime;    //cpu start time
    double         fStopCpuTime;     //cpu stop time
    double         fTotalCpuTime;    //total cpu time
    double         fTotalRealTime;   //total real time
    EState         fState;           //nsStopWatch state
    nsDeque*       mSavedStates;     //stack of saved states
    PRBool         mCreatedStack;    //Initially false.  Set to true in first SaveState() call.
    
public:
    nsStopWatch();
    virtual ~nsStopWatch();
    
    // Stop watch controls: Start, Stop, Continue, Reset
    // Start : Starts the stopwatch
    // Stop : Stops the stopwatch
    // Continue : Undo the last Stop command on the stopwatch.
    // Reset : Resets the stopwatch to 0
    void Start(PRBool reset = PR_TRUE);
    void Stop();
    void Continue();
    void Reset() { ResetCpuTime(); ResetRealTime(); }
    void ResetCpuTime(double aTime = 0) { Stop();  fTotalCpuTime = aTime; }
    void ResetRealTime(double aTime = 0) { Stop(); fTotalRealTime = aTime; }
    
    // Returns real and CPU times in millisecs elapsed in the current stopWatch
    int  GetTime(double *realTime, double *cpuTime);
    void Print(PRLogModuleInfo* log = NULL);
    
    // State management functions. The current state of the stopwatch
    // can be pushed into a stack and retrieved.
    void SaveState();
    void RestoreState();
    
private:
    // static functions that get the real and cpu time from the system
    static double GetSystemRealTime();
    static double GetSystemCPUTime();
    static int    GetSystemTime(double *real, double *cpu);
};
#endif
