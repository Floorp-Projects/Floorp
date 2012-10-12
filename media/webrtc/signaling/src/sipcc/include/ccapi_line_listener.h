/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CCAPI_LINE_LISTENER_H_
#define _CCAPI_LINE_LISTENER_H_

#include "cc_constants.h"

/**
 * Line event notification
 * @param [in] line_event - event
 * @param [in] line - line id
 * @param [in] lineInfo - line info reference
 * @return cc_call_handle_t - handle of the call created
 * NOTE: The memory associated with deviceInfo will be freed immediately upon
 * return from this method. If the application wishes to retain a copy it should
 * invoke CCAPI_Line_retainlineInfo() API. once the info is retained it can be
 * released by invoking CCAPI_Line_releaseLineInfo() API
 */
void CCAPI_LineListener_onLineEvent(ccapi_line_event_e line_event, cc_lineid_t line, cc_lineinfo_ref_t lineInfo);

#endif /* _CCAPIAPI_LINE_LISTENER_H_ */
