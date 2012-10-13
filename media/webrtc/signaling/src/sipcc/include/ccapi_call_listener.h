/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CCAPI_CALL_LISTENER_H_
#define _CCAPI_CALL_LISTENER_H_

#include "ccapi_types.h"

/**
 * Call Listener callback
 * @param [in] event - call event
 * @param [in] handle - call handle for the event
 * @param [in] info - reference to call info
 * @return void
 * NOTE: The memory associated with callInfo will be freed immediately upon return from this method.
 * If the application wishesd to retain a copy it should invoke CCAPI_Call_retainCallInfo() API. Once retained
 * it can be released by invoking the CCAPI_Call_releaseCallInfo API.
 */
void CCAPI_CallListener_onCallEvent(ccapi_call_event_e event, cc_call_handle_t handle, cc_callinfo_ref_t info);

#endif /* _CCAPIAPI_CALL_LISTENER_H_ */
