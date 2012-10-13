/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CC_INFO_LISTENER_H_
#define _CC_INFO_LISTENER_H_

#include "cc_constants.h"

/**
 * Recieve SIP INFO for the specified session
 * @param call_handle call handle
 * @param pack_id the info package id number
 * @param info_package the Info-Package header of the Info Package
 * @param info_type the Content-Type header of the Info Package
 * @param info_body the message body of the Info Package
 * @return void
 */
void CC_InfoListener_receivedInfo(cc_call_handle_t call_handle, int pack_id,
		cc_string_t info_package,
		cc_string_t info_type,
		cc_string_t info_body);

#endif /* _CC_INFO_LISTENER_H_ */
