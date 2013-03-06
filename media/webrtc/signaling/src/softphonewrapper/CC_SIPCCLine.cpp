/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"

#include "CC_Common.h"

#include "CC_SIPCCLine.h"
#include "CC_SIPCCCall.h"
#include "CC_SIPCCLineInfo.h"

extern "C"
{
#include "ccapi_line.h"
#include "ccapi_line_listener.h"
}

using namespace std;
using namespace CSF;

CSF_IMPLEMENT_WRAP(CC_SIPCCLine, cc_lineid_t);

cc_lineid_t CC_SIPCCLine::getID()
{
    return lineId;
}

CC_LineInfoPtr CC_SIPCCLine::getLineInfo ()
{
    cc_lineinfo_ref_t lineInfoRef = CCAPI_Line_getLineInfo(lineId);
    CC_LineInfoPtr lineInfoPtr = CC_SIPCCLineInfo::wrap(lineInfoRef).get();

    //A call to CCAPI_Line_getLineInfo() needs a matching call to CCAPI_Line_releaseLineInfo()
    //However, the CC_SIPCCLineInfo() ctor/dtor does a retain/release internally, so I need to explicitly release
    //here to match up with the call to CCAPI_Line_getLineInfo().

    CCAPI_Line_releaseLineInfo(lineInfoRef);

    //CCAPI_Line_getLineInfo() --> requires release be called.
    //CC_SIPCCLineInfo::CC_SIPCCLineInfo() -> Call retain (wrapped in smart_ptr)
    //CCAPI_Line_releaseLineInfo() --> this maps to the call to CCAPI_Line_getLineInfo()
    //CC_SIPCCLineInfo::~CC_SIPCCLineInfo() --> CCAPI_Line_releaseLineInfo() (when smart pointer destroyed)

    return lineInfoPtr;
}

CC_CallPtr CC_SIPCCLine::createCall ()
{
    cc_call_handle_t callHandle = CCAPI_Line_CreateCall(lineId);

    return CC_SIPCCCall::wrap(callHandle).get();
}


