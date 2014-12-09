/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CC_INFO_H_
#define _CC_INFO_H_

#include "cc_constants.h"

/**
 * Defines to be used for info_package info_type and info body in CC_Info_sendInfo()
 * to send FAST PICTURE UPDATE INFO out
 */
#define FAST_PIC_INFO_PACK  ""
#define FAST_PIC_CTYPE  "application/media_control+xml"
#define FAST_PIC_MSG_BODY "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n\
        <media_control>\n\
          <vc_primitive>\n\
            <to_encoder>\n\
              <picture_fast_update/>\n\
            </to_encoder>\n\
          </vc_primitive>\n\
        </media_control>"


/**
 * Send SIP INFO message with the specified event_type and xml body
 * @param call_handle call handle
 * @param info_package the Info-Package header of the INFO message
 * @param info_type the Content-Type header of the INFO message
 * @param info_body the message body of the INFO message
 * @return void
 */
void CC_Info_sendInfo(cc_call_handle_t call_handle,
		cc_string_t info_package,
		cc_string_t info_type,
		cc_string_t info_body);

#endif /* _CC_INFO_H_ */
