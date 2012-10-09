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
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
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

#include "CC_Common.h"

#include "csf_common.h"
#include "CC_SIPCCLineInfo.h"
#include "CC_SIPCCLine.h"
#include "CC_SIPCCCall.h"

extern "C"
{
#include "cpr_types.h"
#include "ccapi_line.h"
#include "ccapi_line_info.h"
}

using namespace std;
using namespace CSF;

#include "CSFLog.h"

#define MAX_SUPPORTED_NUM_CALLS 100

//FIXME: Header file "ccapi_line.h" has misnamed the retain function as "CCAPI_Device_retainLineInfo"
//       Checked the source file and it's declared correctly there, so I have to declare it myself here.

//Also CCAPI_LineInfo_getCalls() in source file and CCAPI_lineInfo_getCalls() in header (lowercase 'l' in lineInfo)

extern "C" void CCAPI_Line_retainLineInfo(cc_lineinfo_ref_t ref);
extern "C" void CCAPI_LineInfo_getCalls(cc_lineid_t line, cc_call_handle_t handles[], int *count);
extern "C" void CCAPI_LineInfo_getCallsByState(cc_lineid_t line, cc_call_state_t state, cc_call_handle_t handles[], int *count);

CSF_IMPLEMENT_WRAP(CC_SIPCCLineInfo, cc_lineinfo_ref_t);

CC_SIPCCLineInfo::CC_SIPCCLineInfo (cc_lineinfo_ref_t lineinfo) : lineinfo(lineinfo)
{
    CCAPI_Line_retainLineInfo(lineinfo);
}

CC_SIPCCLineInfo::~CC_SIPCCLineInfo()
{
    CCAPI_Line_releaseLineInfo(lineinfo);
}

string CC_SIPCCLineInfo::getName()
{
    return CCAPI_lineInfo_getName(lineinfo);
}

string CC_SIPCCLineInfo::getNumber()
{
    return CCAPI_lineInfo_getNumber(lineinfo);
}

cc_uint32_t CC_SIPCCLineInfo::getButton()
{
    return CCAPI_lineInfo_getButton(lineinfo);
}

cc_line_feature_t CC_SIPCCLineInfo::getLineType()
{
    return CCAPI_lineInfo_getLineType(lineinfo);
}

bool CC_SIPCCLineInfo::getRegState()
{
    if (CCAPI_lineInfo_getRegState(lineinfo))
    {
    	return true;
    }
    else
    {
    	return false;
    }
}

bool CC_SIPCCLineInfo::isCFWDActive()
{
    if (CCAPI_lineInfo_isCFWDActive(lineinfo))
    {
    	return true;
    }
    else
    {
    	return false;
    }
}

string CC_SIPCCLineInfo::getCFWDName()
{
    return CCAPI_lineInfo_getCFWDName(lineinfo);
}

vector<CC_CallPtr> CC_SIPCCLineInfo::getCalls (CC_LinePtr linePtr)
{
    vector<CC_CallPtr> callsVector;

    cc_call_handle_t handles[MAX_SUPPORTED_NUM_CALLS] = {};
    int numCalls = csf_countof(handles);

    CCAPI_LineInfo_getCalls(linePtr->getID(), handles, &numCalls) ;

    for (int i=0; i<numCalls; i++)
    {
        CC_CallPtr callPtr = CC_SIPCCCall::wrap(handles[i]);
        callsVector.push_back(callPtr);
    }

    return callsVector;
}

vector<CC_CallPtr> CC_SIPCCLineInfo::getCallsByState (CC_LinePtr linePtr, cc_call_state_t state)
{
    vector<CC_CallPtr> callsVector;

    cc_call_handle_t handles[MAX_SUPPORTED_NUM_CALLS] = {};
    int numCalls = csf_countof(handles);

    CCAPI_LineInfo_getCallsByState(linePtr->getID(), state, handles, &numCalls) ;

    for (int i=0; i<numCalls; i++)
    {
        CC_CallPtr callPtr = CC_SIPCCCall::wrap(handles[i]);
        callsVector.push_back(callPtr);
    }

    return callsVector;
}

bool CC_SIPCCLineInfo::getMWIStatus()
{
    return (CCAPI_lineInfo_getMWIStatus(lineinfo) != 0);
}

cc_uint32_t CC_SIPCCLineInfo::getMWIType()
{
    return CCAPI_lineInfo_getMWIType(lineinfo);
}

cc_uint32_t CC_SIPCCLineInfo::getMWINewMsgCount()
{
    return CCAPI_lineInfo_getMWINewMsgCount(lineinfo);
}

cc_uint32_t CC_SIPCCLineInfo::getMWIOldMsgCount()
{
    return CCAPI_lineInfo_getMWIOldMsgCount(lineinfo);
}

cc_uint32_t CC_SIPCCLineInfo::getMWIPrioNewMsgCount()
{
    return CCAPI_lineInfo_getMWIPrioNewMsgCount(lineinfo);
}

cc_uint32_t CC_SIPCCLineInfo::getMWIPrioOldMsgCount()
{
    return CCAPI_lineInfo_getMWIPrioOldMsgCount(lineinfo);
}

bool CC_SIPCCLineInfo::hasCapability(ccapi_call_capability_e capability)
{
    return CCAPI_LineInfo_hasCapability(lineinfo, (cc_int32_t) capability) == TRUE;
}

bitset<CCAPI_CALL_CAP_MAX> CC_SIPCCLineInfo::getCapabilitySet()
{
    bitset<CCAPI_CALL_CAP_MAX> lineCaps;

    for (int i=0; i<CCAPI_CALL_CAP_MAX; i++)
    {
        if (hasCapability((ccapi_call_capability_e) i))
        {
            lineCaps.set(i, true);
        }
    }

    return lineCaps;
}

