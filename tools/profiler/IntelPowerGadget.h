/*
 * Copyright 2013, Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: Joe Olivas <joseph.k.olivas@intel.com>
 */

#ifndef profiler_IntelPowerGadget_h
#define profiler_IntelPowerGadget_h

#ifdef _MSC_VER
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif
#include "prlink.h"

typedef int (*IPGInitialize) ();
typedef int (*IPGGetNumNodes) (int *nNodes);
typedef int (*IPGGetNumMsrs) (int *nMsr);
typedef int (*IPGGetMsrName) (int iMsr, wchar_t *szName);
typedef int (*IPGGetMsrFunc) (int iMsr, int *pFuncID);
typedef int (*IPGReadMSR) (int iNode, unsigned int address, uint64_t *value);
typedef int (*IPGWriteMSR) (int iNode, unsigned int address, uint64_t value);
typedef int (*IPGGetIAFrequency) (int iNode, int *freqInMHz);
typedef int (*IPGGetTDP) (int iNode, double *TDP);
typedef int (*IPGGetMaxTemperature) (int iNode, int *degreeC);
typedef int (*IPGGetThresholds) (int iNode, int *degree1C, int *degree2C);
typedef int (*IPGGetTemperature) (int iNode, int *degreeC);
typedef int (*IPGReadSample) ();
typedef int (*IPGGetSysTime) (void *pSysTime);
typedef int (*IPGGetRDTSC) (uint64_t *pTSC);
typedef int (*IPGGetTimeInterval) (double *pOffset);
typedef int (*IPGGetBaseFrequency) (int iNode, double *pBaseFrequency);
typedef int (*IPGGetPowerData) (int iNode, int iMSR, double *pResult, int *nResult);
typedef int (*IPGStartLog) (wchar_t *szFileName);
typedef int (*IPGStopLog) ();

#if defined(__x86_64__) || defined(__x86_64) || defined(_M_AMD64)
#define PG_LIBRARY_NAME "EnergyLib64"
#else
#define PG_LIBRARY_NAME "EnergyLib32"
#endif


class IntelPowerGadget
{
public:

    IntelPowerGadget();
    ~IntelPowerGadget();

    // Fails if initialization is incomplete
    bool Init();

    // Returns the number of packages on the system
    int GetNumberNodes();

    // Returns the number of MSRs being tracked
    int GetNumberMsrs();

    // Given a node, returns the temperature
    int GetCPUFrequency(int);

    // Returns the TDP of the given node
    double GetTdp(int);

    // Returns the maximum temperature for the given node
    int GetMaxTemp(int);

    // Returns the current temperature in degrees C
    // of the given node
    int GetTemp(int);

    // Takes a sample of data. Must be called before
    // any current data is retrieved.
    int TakeSample();

    // Gets the timestamp of the most recent sample
    uint64_t GetRdtsc();

    // returns number of seconds between the last
    // two samples
    double GetInterval();

    // Returns the base frequency for the given node
    double GetCPUBaseFrequency(int node);

    // Returns the combined package power for all
    // packages on the system for the last sample.
    double GetTotalPackagePowerInWatts();
    double GetPackagePowerInWatts(int node);

    // Returns the combined CPU power for all
    // packages on the system for the last sample.
    // If the reading is not available, returns 0.0
    double GetTotalCPUPowerInWatts();
    double GetCPUPowerInWatts(int node);

    // Returns the combined GPU power for all
    // packages on the system for the last sample.
    // If the reading is not available, returns 0.0
    double GetTotalGPUPowerInWatts();
    double GetGPUPowerInWatts(int node);

private:

    PRLibrary *libpowergadget;
    IPGInitialize Initialize;
    IPGGetNumNodes GetNumNodes;
    IPGGetNumMsrs GetNumMsrs;
    IPGGetMsrName GetMsrName;
    IPGGetMsrFunc GetMsrFunc;
    IPGReadMSR ReadMSR;
    IPGWriteMSR WriteMSR;
    IPGGetIAFrequency GetIAFrequency;
    IPGGetTDP GetTDP;
    IPGGetMaxTemperature GetMaxTemperature;
    IPGGetThresholds GetThresholds;
    IPGGetTemperature GetTemperature;
    IPGReadSample ReadSample;
    IPGGetSysTime GetSysTime;
    IPGGetRDTSC GetRDTSC;
    IPGGetTimeInterval GetTimeInterval;
    IPGGetBaseFrequency GetBaseFrequency;
    IPGGetPowerData GetPowerData;
    IPGStartLog StartLog;
    IPGStopLog StopLog;

    int packageMSR;
    int cpuMSR;
    int freqMSR;
    int tempMSR;
};

#endif // profiler_IntelPowerGadget_h
