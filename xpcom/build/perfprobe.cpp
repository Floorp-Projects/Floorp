/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
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
 * The Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  David Rajchenbach-Teller <dteller@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*****************************
 Windows implementation of probes, using xperf
 *****************************/
#include <windows.h>
#include <wmistr.h>
#include <evntrace.h>

#include "perfprobe.h"
#include "nsAutoPtr.h"

namespace mozilla {
namespace probes {

#if defined(MOZ_LOGGING)
static PRLogModuleInfo *gProbeLog = PR_NewLogModule("SysProbe");
#define LOG(x)  PR_LOG(gProbeLog, PR_LOG_DEBUG, x)
#else
#define LOG(x)
#endif

//Utility function
GUID CID_to_GUID(const nsCID &aCID)
{
  GUID result;
  result.Data1 = aCID.m0;
  result.Data2 = aCID.m1;
  result.Data3 = aCID.m2;
  for (int i = 0; i < 8; ++i)
    result.Data4[i] = aCID.m3[i];
  return result;
}



// Implementation of Probe

Probe::Probe(const nsCID &aGUID,
             const nsACString &aName,
             ProbeManager *aManager)
  : mGUID(CID_to_GUID(aGUID))
  , mName(aName)
  , mManager(aManager)
{
}

nsresult Probe::Trigger()
{
  if (!(mManager->mIsActive)) {
    //Do not trigger if there is no session
    return NS_OK; 
  }
  
  _EVENT_TRACE_HEADER event;
  ZeroMemory(&event, sizeof(event));
  event.Size = sizeof(event);
  event.Flags = WNODE_FLAG_TRACED_GUID ;
  event.Guid = (const GUID)mGUID;
  event.Class.Type    = 1;
  event.Class.Version = 0;
  event.Class.Level   = TRACE_LEVEL_INFORMATION;
  
  ULONG result        = TraceEvent(mManager->mSessionHandle, &event);
  
  LOG(("Probes: Triggered %s, %s, %ld",
       mName.Data(),
       result==ERROR_SUCCESS ? "success":"failure",
       result));
  
  nsresult rv;
  switch(result)
  {
    case ERROR_SUCCESS:             rv = NS_OK; break;
    case ERROR_INVALID_FLAG_NUMBER:
    case ERROR_MORE_DATA:
    case ERROR_INVALID_PARAMETER:   rv = NS_ERROR_INVALID_ARG; break;
    case ERROR_INVALID_HANDLE:      rv = NS_ERROR_FAILURE;     break;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:         rv = NS_ERROR_OUT_OF_MEMORY; break;
    default:                        rv = NS_ERROR_UNEXPECTED;
  }
  return rv;
}


// Implementation of ProbeManager

ProbeManager::~ProbeManager()
{
  //If the manager goes out of scope, stop the session.
  if (mIsActive && mRegistrationHandle) {
    StopSession();
  }
}

ProbeManager::ProbeManager(const nsCID &aApplicationUID,
                           const nsACString &aApplicationName)
  : mApplicationUID(aApplicationUID)
  , mApplicationName(aApplicationName)
  , mSessionHandle(0)
  , mRegistrationHandle(0)
{
#if defined(MOZ_LOGGING)
  char cidStr[NSID_LENGTH];
  aApplicationUID.ToProvidedString(cidStr);
  LOG(("ProbeManager::Init for application %s, %s",
       aApplicationName.Data(), cidStr));
#endif
}

//Note: The Windows API is just a little bit scary there.
//The only way to obtain the session handle is to
//- ignore the session handle obtained from RegisterTraceGuids
//- pass a callback
//- in that callback, request the session handle through
//      GetTraceLoggerHandle and some opaque value received by the callback

ULONG WINAPI ControlCallback(
                             WMIDPREQUESTCODE RequestCode,
                             PVOID Context,
                             ULONG *Reserved,
                             PVOID Buffer
                             )
{
  ProbeManager* context = (ProbeManager*)Context;
  switch(RequestCode)
  {
  case WMI_ENABLE_EVENTS: 
    {
      context->mIsActive = true;
      TRACEHANDLE sessionHandle = GetTraceLoggerHandle(Buffer);
      //Note: We only accept one handle
      if ((HANDLE)sessionHandle == INVALID_HANDLE_VALUE) {
        ULONG result = GetLastError();
        LOG(("Probes: ControlCallback failed, %ul", result));
        return result;
      } else if (context->mIsActive && context->mSessionHandle
                && context->mSessionHandle != sessionHandle) {
        LOG(("Probes: Can only handle one context at a time, "
             "ignoring activation"));
        return ERROR_SUCCESS;
      } else {
        context->mSessionHandle = sessionHandle;
        LOG(("Probes: ControlCallback activated"));
        return ERROR_SUCCESS;
      }
    }

  case WMI_DISABLE_EVENTS:
    context->mIsActive      = false;
    context->mSessionHandle = 0;
    LOG(("Probes: ControlCallback deactivated"));
    return ERROR_SUCCESS;

  default:
    LOG(("Probes: ControlCallback does not know what to do with %d",
         RequestCode));
    return ERROR_INVALID_PARAMETER;
  }
}

already_AddRefed<Probe> ProbeManager::GetProbe(const nsCID &eventUID,
                               const nsACString &eventName)
{
  nsRefPtr<Probe> result(new Probe(eventUID, eventName, this));
  mAllProbes.AppendElement(result);
  return result.forget();
}

nsresult ProbeManager::StartSession()
{
  return StartSession(mAllProbes);
}

nsresult ProbeManager::StartSession(nsTArray<nsRefPtr<Probe>> &aProbes)
{
  const size_t probesCount = aProbes.Length();
  _TRACE_GUID_REGISTRATION* probes = new _TRACE_GUID_REGISTRATION[probesCount];
  for (unsigned int i = 0; i < probesCount; ++i) {
    const Probe *probe = aProbes[i];
    const Probe *probeX = static_cast<const Probe*>(probe);
    probes[i].Guid = (LPCGUID)&(probeX->mGUID);
  }
  ULONG result =
    RegisterTraceGuids(&ControlCallback
                     /*RequestAddress: Sets mSessions appropriately.*/,
                     this
                     /*RequestContext: Passed to ControlCallback*/,
                     (LPGUID)&mApplicationUID
                     /*ControlGuid:    Tracing GUID
                      the cast comes from MSDN examples*/,
                     probesCount
                     /*GuidCount:      Number of probes*/,
                     probes
                     /*TraceGuidReg:   Probes registration*/,
                     NULL
                     /*MofImagePath:   Must be NULL, says MSDN*/,
                     NULL
                     /*MofResourceName:Must be NULL, says MSDN*/,
                     &mRegistrationHandle
                     /*RegistrationHandle: Handler.
                      used only for unregistration*/
                     );
  delete[] probes;
  NS_ENSURE_TRUE(result == ERROR_SUCCESS, NS_ERROR_UNEXPECTED);
  return NS_OK;
}

nsresult ProbeManager::StopSession()
{
  LOG(("Probes: Stopping measures"));
  if (mSessionHandle != 0) {
    ULONG result = UnregisterTraceGuids(mSessionHandle);
    mSessionHandle = 0;
    if (result != ERROR_SUCCESS) {
      return NS_ERROR_INVALID_ARG;
    }
  }
  return NS_OK;
}


}
}
