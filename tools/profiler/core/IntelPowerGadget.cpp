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

#include "nsDebug.h"
#include "nsString.h"
#include "IntelPowerGadget.h"
#include "prenv.h"

IntelPowerGadget::IntelPowerGadget() :
    libpowergadget(nullptr),
    Initialize(nullptr),
    GetNumNodes(nullptr),
    GetMsrName(nullptr),
    GetMsrFunc(nullptr),
    ReadMSR(nullptr),
    WriteMSR(nullptr),
    GetIAFrequency(nullptr),
    GetTDP(nullptr),
    GetMaxTemperature(nullptr),
    GetThresholds(nullptr),
    GetTemperature(nullptr),
    ReadSample(nullptr),
    GetSysTime(nullptr),
    GetRDTSC(nullptr),
    GetTimeInterval(nullptr),
    GetBaseFrequency(nullptr),
    GetPowerData(nullptr),
    StartLog(nullptr),
    StopLog(nullptr),
    GetNumMsrs(nullptr),
    packageMSR(-1),
    cpuMSR(-1),
    freqMSR(-1),
    tempMSR(-1)
{
}

bool
IntelPowerGadget::Init()
{
    bool success = false;
    const char *path = PR_GetEnv("IPG_Dir");
    nsCString ipg_library;
    if (path && *path) {
        ipg_library.Append(path);
        ipg_library.Append('/');
        ipg_library.AppendLiteral(PG_LIBRARY_NAME);
        libpowergadget = PR_LoadLibrary(ipg_library.get());
    }

    if(libpowergadget) {
        Initialize = (IPGInitialize) PR_FindFunctionSymbol(libpowergadget, "IntelEnergyLibInitialize");
        GetNumNodes = (IPGGetNumNodes) PR_FindFunctionSymbol(libpowergadget, "GetNumNodes");
        GetMsrName = (IPGGetMsrName) PR_FindFunctionSymbol(libpowergadget, "GetMsrName");
        GetMsrFunc = (IPGGetMsrFunc) PR_FindFunctionSymbol(libpowergadget, "GetMsrFunc");
        ReadMSR = (IPGReadMSR) PR_FindFunctionSymbol(libpowergadget, "ReadMSR");
        WriteMSR = (IPGWriteMSR) PR_FindFunctionSymbol(libpowergadget, "WriteMSR");
        GetIAFrequency = (IPGGetIAFrequency) PR_FindFunctionSymbol(libpowergadget, "GetIAFrequency");
        GetTDP = (IPGGetTDP) PR_FindFunctionSymbol(libpowergadget, "GetTDP");
        GetMaxTemperature = (IPGGetMaxTemperature) PR_FindFunctionSymbol(libpowergadget, "GetMaxTemperature");
        GetThresholds = (IPGGetThresholds) PR_FindFunctionSymbol(libpowergadget, "GetThresholds");
        GetTemperature = (IPGGetTemperature) PR_FindFunctionSymbol(libpowergadget, "GetTemperature");
        ReadSample = (IPGReadSample) PR_FindFunctionSymbol(libpowergadget, "ReadSample");
        GetSysTime = (IPGGetSysTime) PR_FindFunctionSymbol(libpowergadget, "GetSysTime");
        GetRDTSC = (IPGGetRDTSC) PR_FindFunctionSymbol(libpowergadget, "GetRDTSC");
        GetTimeInterval = (IPGGetTimeInterval) PR_FindFunctionSymbol(libpowergadget, "GetTimeInterval");
        GetBaseFrequency = (IPGGetBaseFrequency) PR_FindFunctionSymbol(libpowergadget, "GetBaseFrequency");
        GetPowerData = (IPGGetPowerData) PR_FindFunctionSymbol(libpowergadget, "GetPowerData");
        StartLog = (IPGStartLog) PR_FindFunctionSymbol(libpowergadget, "StartLog");
        StopLog = (IPGStopLog) PR_FindFunctionSymbol(libpowergadget, "StopLog");
        GetNumMsrs = (IPGGetNumMsrs) PR_FindFunctionSymbol(libpowergadget, "GetNumMsrs");
    }

    if(Initialize) {
        Initialize();
        int msrCount = GetNumberMsrs();
        wchar_t name[1024] = {0};
        for(int i = 0; i < msrCount; ++i) {
            GetMsrName(i, name);
            int func = 0;
            GetMsrFunc(i, &func);
            // MSR for frequency
            if(wcscmp(name, L"CPU Frequency") == 0 && (func == 0)) {
                this->freqMSR = i;
            }
            // MSR for Package
            else if(wcscmp(name, L"Processor") == 0 && (func == 1)) {
                this->packageMSR = i;
            }
            // MSR for CPU
            else if(wcscmp(name, L"IA") == 0 && (func == 1)) {
                this->cpuMSR = i;
            }
            // MSR for Temperature
            else if(wcscmp(name, L"Package") == 0 && (func == 2)) {
                this->tempMSR = i;
            }
        }
        // Grab one sample at startup for a diff
        TakeSample();
        success = true;
    }
    return success;
}

IntelPowerGadget::~IntelPowerGadget()
{
    if(libpowergadget) {
        NS_WARNING("Unloading PowerGadget library!\n");
        PR_UnloadLibrary(libpowergadget);
        libpowergadget = nullptr;
        Initialize = nullptr;
        GetNumNodes = nullptr;
        GetMsrName = nullptr;
        GetMsrFunc = nullptr;
        ReadMSR = nullptr;
        WriteMSR = nullptr;
        GetIAFrequency = nullptr;
        GetTDP = nullptr;
        GetMaxTemperature = nullptr;
        GetThresholds = nullptr;
        GetTemperature = nullptr;
        ReadSample = nullptr;
        GetSysTime = nullptr;
        GetRDTSC = nullptr;
        GetTimeInterval = nullptr;
        GetBaseFrequency = nullptr;
        GetPowerData = nullptr;
        StartLog = nullptr;
        StopLog = nullptr;
        GetNumMsrs = nullptr;
    }
}

int
IntelPowerGadget::GetNumberNodes()
{
    int nodes = 0;
    if(GetNumNodes) {
        int ok = GetNumNodes(&nodes);
    }
    return nodes;
}

int
IntelPowerGadget::GetNumberMsrs()
{
    int msrs = 0;
    if(GetNumMsrs) {
        int ok = GetNumMsrs(&msrs);
    }
    return msrs;
}

int
IntelPowerGadget::GetCPUFrequency(int node)
{
    int frequency = 0;
    if(GetIAFrequency) {
        int ok = GetIAFrequency(node, &frequency);
    }
    return frequency;
}

double
IntelPowerGadget::GetTdp(int node)
{
    double tdp = 0.0;
    if(GetTDP) {
        int ok = GetTDP(node, &tdp);
    }
    return tdp;
}

int
IntelPowerGadget::GetMaxTemp(int node)
{
    int maxTemperatureC = 0;
    if(GetMaxTemperature) {
        int ok = GetMaxTemperature(node, &maxTemperatureC);
    }
    return maxTemperatureC;
}

int
IntelPowerGadget::GetTemp(int node)
{
    int temperatureC = 0;
    if(GetTemperature) {
        int ok = GetTemperature(node, &temperatureC);
    }
    return temperatureC;
}

int
IntelPowerGadget::TakeSample()
{
    int ok = 0;
    if(ReadSample) {
        ok = ReadSample();
    }
    return ok;
}

uint64_t
IntelPowerGadget::GetRdtsc()
{
    uint64_t rdtsc = 0;
    if(GetRDTSC) {
        int ok = GetRDTSC(&rdtsc);
    }
    return rdtsc;
}

double
IntelPowerGadget::GetInterval()
{
    double interval = 0.0;
    if(GetTimeInterval) {
        int ok = GetTimeInterval(&interval);
    }
    return interval;
}

double
IntelPowerGadget::GetCPUBaseFrequency(int node)
{
    double freq = 0.0;
    if(GetBaseFrequency) {
        int ok = GetBaseFrequency(node, &freq);
    }
    return freq;
}

double
IntelPowerGadget::GetTotalPackagePowerInWatts()
{
    int nodes = GetNumberNodes();
    double totalPower = 0.0;
    for(int i = 0; i < nodes; ++i) {
        totalPower += GetPackagePowerInWatts(i);
    }
    return totalPower;
}

double
IntelPowerGadget::GetPackagePowerInWatts(int node)
{
    int numResult = 0;
    double result[] = {0.0, 0.0, 0.0};
    if(GetPowerData && packageMSR != -1) {
        int ok = GetPowerData(node, packageMSR, result, &numResult);
    }
    return result[0];
}

double
IntelPowerGadget::GetTotalCPUPowerInWatts()
{
    int nodes = GetNumberNodes();
    double totalPower = 0.0;
    for(int i = 0; i < nodes; ++i) {
        totalPower += GetCPUPowerInWatts(i);
    }
    return totalPower;
}

double
IntelPowerGadget::GetCPUPowerInWatts(int node)
{
    int numResult = 0;
    double result[] = {0.0, 0.0, 0.0};
    if(GetPowerData && cpuMSR != -1) {
        int ok = GetPowerData(node, cpuMSR, result, &numResult);
    }
    return result[0];
}

double
IntelPowerGadget::GetTotalGPUPowerInWatts()
{
    int nodes = GetNumberNodes();
    double totalPower = 0.0;
    for(int i = 0; i < nodes; ++i) {
        totalPower += GetGPUPowerInWatts(i);
    }
    return totalPower;
}

double
IntelPowerGadget::GetGPUPowerInWatts(int node)
{
    return 0.0;
}

