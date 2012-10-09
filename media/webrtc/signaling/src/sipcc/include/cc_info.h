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
