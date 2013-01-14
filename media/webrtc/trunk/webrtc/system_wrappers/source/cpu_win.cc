/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "cpu_win.h"

#define _WIN32_DCOM

#include <assert.h>
#include <iostream>
#include <Wbemidl.h>

#pragma comment(lib, "wbemuuid.lib")

#include "condition_variable_wrapper.h"
#include "critical_section_wrapper.h"
#include "event_wrapper.h"
#include "thread_wrapper.h"

namespace webrtc {
WebRtc_Word32 CpuWindows::CpuUsage()
{
    if (!has_initialized_)
    {
        return -1;
    }
    // Last element is the average
    return cpu_usage_[number_of_objects_ - 1];
}

WebRtc_Word32 CpuWindows::CpuUsageMultiCore(WebRtc_UWord32& num_cores,
                                            WebRtc_UWord32*& cpu_usage)
{
    if (has_terminated_) {
        num_cores = 0;
        cpu_usage = NULL;
        return -1;
    }
    if (!has_initialized_)
    {
        num_cores = 0;
        cpu_usage = NULL;
        return -1;
    }
    num_cores = number_of_objects_ - 1;
    cpu_usage = cpu_usage_;
    return cpu_usage_[number_of_objects_-1];
}

CpuWindows::CpuWindows()
    : cpu_polling_thread(NULL),
      initialize_(true),
      has_initialized_(false),
      terminate_(false),
      has_terminated_(false),
      cpu_usage_(NULL),
      wbem_enum_access_(NULL),
      number_of_objects_(0),
      cpu_usage_handle_(0),
      previous_processor_timestamp_(NULL),
      timestamp_sys_100_ns_handle_(0),
      previous_100ns_timestamp_(NULL),
      wbem_service_(NULL),
      wbem_service_proxy_(NULL),
      wbem_refresher_(NULL),
      wbem_enum_(NULL)
{
    // All resources are allocated in PollingCpu().
    if (AllocateComplexDataTypes())
    {
        StartPollingCpu();
    }
    else
    {
        assert(false);
    }
}

CpuWindows::~CpuWindows()
{
    // All resources are reclaimed in StopPollingCpu().
    StopPollingCpu();
    DeAllocateComplexDataTypes();
}

bool CpuWindows::AllocateComplexDataTypes()
{
    cpu_polling_thread = ThreadWrapper::CreateThread(
        CpuWindows::Process,
        reinterpret_cast<void*>(this),
        kNormalPriority,
        "CpuWindows");
    init_crit_ = CriticalSectionWrapper::CreateCriticalSection();
    init_cond_ = ConditionVariableWrapper::CreateConditionVariable();
    terminate_crit_ = CriticalSectionWrapper::CreateCriticalSection();
    terminate_cond_ = ConditionVariableWrapper::CreateConditionVariable();
    sleep_event = EventWrapper::Create();
    return (cpu_polling_thread != NULL) && (init_crit_ != NULL) &&
           (init_cond_ != NULL) && (terminate_crit_ != NULL) &&
           (terminate_cond_ != NULL) && (sleep_event != NULL);
}

void CpuWindows::DeAllocateComplexDataTypes()
{
    if (sleep_event != NULL)
    {
        delete sleep_event;
        sleep_event = NULL;
    }
    if (terminate_cond_ != NULL)
    {
        delete terminate_cond_;
        terminate_cond_ = NULL;
    }
    if (terminate_crit_ != NULL)
    {
        delete terminate_crit_;
        terminate_crit_ = NULL;
    }
    if (init_cond_ != NULL)
    {
        delete init_cond_;
        init_cond_ = NULL;
    }
    if (init_crit_ != NULL)
    {
        delete init_crit_;
        init_crit_ = NULL;
    }
    if (cpu_polling_thread != NULL)
    {
        delete cpu_polling_thread;
        cpu_polling_thread = NULL;
    }
}

void CpuWindows::StartPollingCpu()
{
    unsigned int dummy_id = 0;
    if (!cpu_polling_thread->Start(dummy_id))
    {
        initialize_ = false;
        has_terminated_ = true;
        assert(false);
    }
}

bool CpuWindows::StopPollingCpu()
{
    {
        // If StopPollingCpu is called immediately after StartPollingCpu() it is
        // possible that cpu_polling_thread is in the process of initializing.
        // Let initialization finish to avoid getting into a bad state.
        CriticalSectionScoped cs(init_crit_);
        while(initialize_)
        {
            init_cond_->SleepCS(*init_crit_);
        }
    }

    CriticalSectionScoped cs(terminate_crit_);
    terminate_ = true;
    sleep_event->Set();
    while (!has_terminated_)
    {
        terminate_cond_->SleepCS(*terminate_crit_);
    }
    cpu_polling_thread->Stop();
    delete cpu_polling_thread;
    cpu_polling_thread = NULL;
    return true;
}

bool CpuWindows::Process(void* thread_object)
{
    return reinterpret_cast<CpuWindows*>(thread_object)->ProcessImpl();
}

bool CpuWindows::ProcessImpl()
{
    {
        CriticalSectionScoped cs(terminate_crit_);
        if (terminate_)
        {
            Terminate();
            terminate_cond_->WakeAll();
            return false;
        }
    }
    // Initialize on first iteration
    if (initialize_)
    {
        CriticalSectionScoped cs(init_crit_);
        initialize_ = false;
        const bool success = Initialize();
        init_cond_->WakeAll();
        if (!success || !has_initialized_)
        {
            has_initialized_ = false;
            terminate_ = true;
            return true;
        }
    }
    // Approximately one seconds sleep for each CPU measurement. Precision is
    // not important. 1 second refresh rate is also used by Performance Monitor
    // (perfmon).
    if(kEventTimeout != sleep_event->Wait(1000))
    {
        // Terminating. No need to update CPU usage.
        assert(terminate_);
        return true;
    }

    // UpdateCpuUsage() returns false if a single (or more) CPU read(s) failed.
    // Not a major problem if it happens.
    UpdateCpuUsage();
    return true;
}

bool CpuWindows::CreateWmiConnection()
{
    IWbemLocator* service_locator = NULL;
    HRESULT hr = CoCreateInstance(CLSID_WbemLocator, NULL,
                                  CLSCTX_INPROC_SERVER, IID_IWbemLocator,
                                  reinterpret_cast<void**> (&service_locator));
    if (FAILED(hr))
    {
        return false;
    }
    // To get the WMI service specify the WMI namespace.
    BSTR wmi_namespace = SysAllocString(L"\\\\.\\root\\cimv2");
    if (wmi_namespace == NULL)
    {
        // This type of failure signifies running out of memory.
        service_locator->Release();
        return false;
    }
    hr = service_locator->ConnectServer(wmi_namespace, NULL, NULL, NULL, 0L,
                                        NULL, NULL, &wbem_service_);
    SysFreeString(wmi_namespace);
    service_locator->Release();
    return !FAILED(hr);
}

// Sets up WMI refresher and enum
bool CpuWindows::CreatePerfOsRefresher()
{
    // Create refresher.
    HRESULT hr = CoCreateInstance(CLSID_WbemRefresher, NULL,
                                  CLSCTX_INPROC_SERVER, IID_IWbemRefresher,
                                  reinterpret_cast<void**> (&wbem_refresher_));
    if (FAILED(hr))
    {
        return false;
    }
    // Create PerfOS_Processor enum.
    IWbemConfigureRefresher* wbem_refresher_config = NULL;
    hr = wbem_refresher_->QueryInterface(
        IID_IWbemConfigureRefresher,
        reinterpret_cast<void**> (&wbem_refresher_config));
    if (FAILED(hr))
    {
        return false;
    }

    // Create a proxy to the IWbemServices so that a local authentication
    // can be set up (this is needed to be able to successfully call 
    // IWbemConfigureRefresher::AddEnum). Setting authentication with
    // CoInitializeSecurity is process-wide (which is too intrusive).
    hr = CoCopyProxy(static_cast<IUnknown*> (wbem_service_),
                     reinterpret_cast<IUnknown**> (&wbem_service_proxy_));
    if(FAILED(hr))
    {
        return false;
    }
    // Set local authentication.
    // RPC_C_AUTHN_WINNT means using NTLM instead of Kerberos which is default.
    hr = CoSetProxyBlanket(static_cast<IUnknown*> (wbem_service_proxy_),
                           RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                           RPC_C_AUTHN_LEVEL_DEFAULT,
                           RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
    if(FAILED(hr))
    {
        return false;
    }

    // Don't care about the particular id for the enum.
    long enum_id = 0;
    hr = wbem_refresher_config->AddEnum(wbem_service_proxy_,
                                        L"Win32_PerfRawData_PerfOS_Processor",
                                        0, NULL, &wbem_enum_, &enum_id);
    wbem_refresher_config->Release();
    wbem_refresher_config = NULL;
    return !FAILED(hr);
}

// Have to pull the first round of data to be able set the handles.
bool CpuWindows::CreatePerfOsCpuHandles()
{
    // Update the refresher so that there is data available in wbem_enum_.
    wbem_refresher_->Refresh(0L);

    // The number of enumerators is the number of processor + 1 (the total).
    // This is unknown at this point.
    DWORD number_returned = 0;
    HRESULT hr = wbem_enum_->GetObjects(0L, number_of_objects_,
                                        wbem_enum_access_, &number_returned);
    // number_returned indicates the number of enumerators that are needed.
    if (hr == WBEM_E_BUFFER_TOO_SMALL &&
        number_returned > number_of_objects_)
    {
        // Allocate the number IWbemObjectAccess asked for by the
        // GetObjects(..) function.
        wbem_enum_access_ = new IWbemObjectAccess*[number_returned];
        cpu_usage_ = new WebRtc_UWord32[number_returned];
        previous_processor_timestamp_ = new unsigned __int64[number_returned];
        previous_100ns_timestamp_ = new unsigned __int64[number_returned];
        if ((wbem_enum_access_ == NULL) || (cpu_usage_ == NULL) ||
            (previous_processor_timestamp_ == NULL) ||
            (previous_100ns_timestamp_ == NULL))
        {
            // Out of memory.
            return false;
        }

        SecureZeroMemory(wbem_enum_access_, number_returned *
                         sizeof(IWbemObjectAccess*));
        memset(cpu_usage_, 0, sizeof(int) * number_returned);
        memset(previous_processor_timestamp_, 0, sizeof(unsigned __int64) *
               number_returned);
        memset(previous_100ns_timestamp_, 0, sizeof(unsigned __int64) *
               number_returned);

        number_of_objects_ = number_returned;
        // Read should be successfull now that memory has been allocated.
        hr = wbem_enum_->GetObjects(0L, number_of_objects_, wbem_enum_access_,
                                    &number_returned);
        if (FAILED(hr))
        {
            return false;
        }
    }
    else
    {
        // 0 enumerators should not be enough. Something has gone wrong here.
        return false;
    }

    // Get the enumerator handles that are needed for calculating CPU usage.
    CIMTYPE cpu_usage_type;
    hr = wbem_enum_access_[0]->GetPropertyHandle(L"PercentProcessorTime",
                                                 &cpu_usage_type,
                                                 &cpu_usage_handle_);
    if (FAILED(hr))
    {
        return false;
    }
    CIMTYPE timestamp_sys_100_ns_type;
    hr = wbem_enum_access_[0]->GetPropertyHandle(L"TimeStamp_Sys100NS",
                                                 &timestamp_sys_100_ns_type,
                                                 &timestamp_sys_100_ns_handle_);
    return !FAILED(hr);
}

bool CpuWindows::Initialize()
{
    if (terminate_)
    {
        return false;
    }
    // Initialize COM library.
    HRESULT hr = CoInitializeEx(NULL,COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        return false;
    }
    if (FAILED(hr))
    {
        return false;
    }

    if (!CreateWmiConnection())
    {
        return false;
    }
    if (!CreatePerfOsRefresher())
    {
        return false;
    }
    if (!CreatePerfOsCpuHandles())
    {
        return false;
    }
    has_initialized_ = true;
    return true;
}

bool CpuWindows::Terminate()
{
    if (has_terminated_)
    {
        return false;
    }
    // Reverse order of Initialize().
    // Some compilers complain about deleting NULL though it's well defined
    if (previous_100ns_timestamp_ != NULL)
    {
        delete[] previous_100ns_timestamp_;
        previous_100ns_timestamp_ = NULL;
    }
    if (previous_processor_timestamp_ != NULL)
    {
        delete[] previous_processor_timestamp_;
        previous_processor_timestamp_ = NULL;
    }
    if (cpu_usage_ != NULL)
    {
        delete[] cpu_usage_;
        cpu_usage_ = NULL;
    }
    if (wbem_enum_access_ != NULL)
    {
        for (DWORD i = 0; i < number_of_objects_; i++)
        {
            if(wbem_enum_access_[i] != NULL)
            {
                wbem_enum_access_[i]->Release();
            }
        }
        delete[] wbem_enum_access_;
        wbem_enum_access_ = NULL;
    }
    if (wbem_enum_ != NULL)
    {
        wbem_enum_->Release();
        wbem_enum_ = NULL;
    }    
    if (wbem_refresher_ != NULL)
    {
        wbem_refresher_->Release();
        wbem_refresher_ = NULL;
    }
    if (wbem_service_proxy_ != NULL)
    {
        wbem_service_proxy_->Release();
        wbem_service_proxy_ = NULL;
    }
    if (wbem_service_ != NULL)
    {
        wbem_service_->Release();
        wbem_service_ = NULL;
    }
    // CoUninitialized should be called once for every CoInitializeEx.
    // Regardless if it failed or not.
    CoUninitialize();
    has_terminated_ = true;
    return true;
}

bool CpuWindows::UpdateCpuUsage()
{
    wbem_refresher_->Refresh(0L);
    DWORD number_returned = 0;
    HRESULT hr = wbem_enum_->GetObjects(0L, number_of_objects_,
                                        wbem_enum_access_,&number_returned);
    if (FAILED(hr))
    {
        // wbem_enum_access_ has already been allocated. Unless the number of
        // CPUs change runtime this should not happen.
        return false;
    }
    unsigned __int64 cpu_usage = 0;
    unsigned __int64 timestamp_100ns = 0;
    bool returnValue = true;
    for (DWORD i = 0; i < number_returned; i++)
    {
        hr = wbem_enum_access_[i]->ReadQWORD(cpu_usage_handle_,&cpu_usage);
        if (FAILED(hr))
        {
            returnValue = false;
        }
        hr = wbem_enum_access_[i]->ReadQWORD(timestamp_sys_100_ns_handle_,
                                             &timestamp_100ns);
        if (FAILED(hr))
        {
            returnValue = false;
        }
        wbem_enum_access_[i]->Release();
        wbem_enum_access_[i] = NULL;

        const bool wrapparound =
            (previous_processor_timestamp_[i] > cpu_usage) ||
            (previous_100ns_timestamp_[i] > timestamp_100ns);
        const bool first_time = (previous_processor_timestamp_[i] == 0) ||
                                (previous_100ns_timestamp_[i] == 0);
        if (wrapparound || first_time)
        {
            previous_processor_timestamp_[i] = cpu_usage;
            previous_100ns_timestamp_[i] = timestamp_100ns;
            continue;
        }
        const unsigned __int64 processor_timestamp_delta =
            cpu_usage - previous_processor_timestamp_[i];
        const unsigned __int64 timestamp_100ns_delta =
            timestamp_100ns - previous_100ns_timestamp_[i];

        if (processor_timestamp_delta >= timestamp_100ns_delta)
        {
            cpu_usage_[i] = 0;
        } else {
            // Quotient must be float since the division is guaranteed to yield
            // a value between 0 and 1 which is 0 in integer division.
            const float delta_quotient =
                static_cast<float>(processor_timestamp_delta) /
                static_cast<float>(timestamp_100ns_delta);
            cpu_usage_[i] = 100 - static_cast<WebRtc_UWord32>(delta_quotient *
                                                              100);
        }
        previous_processor_timestamp_[i] = cpu_usage;
        previous_100ns_timestamp_[i] = timestamp_100ns;
    }
    return returnValue;
}
} // namespace webrtc
