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

#ifndef __XML_PARSER_H__
#define __XML_PARSER_H__

#include "cc_constants.h"
#include "xml_parser_defines.h"

/**
 * Initializes the XML parser.
 * @param [out] xml_parser_handle handle to the XML parser.
 * @return CC_SUCCESS or CC_FAILURE
 */
int ccxmlInitialize(void ** xml_parser_handle);

/**
 * Reset and free any memory allocated by XML parser.
 * @param [in] xml_parser_handle handle to the XML parser.
 * @return void
 */
void ccxmlDeInitialize(void ** xml_parser_handle);

/**
 * Frame an XML string based on data passed to this function.
 * @param [in] xml_parser_handle handle to the XML parser.
 * @param [in] event_datap pointer to the data to be encoded into an XML string.
 * @return non-null char pointer if encoding succeeded, else returns null.
 * Calling function must free the char pointer when not needed.
 */
char * ccxmlEncodeEventData (void * xml_parser_handle, ccsip_event_data_t *event_datap);

/**
 * Parse the XML string and populate the data structure based on the XML tag
 * value received in the XML string.
 * @param [in] xml_parser_handle handle to the XML parser.
 * @param [in] msg_type type of message that need to be parsed.
 * @param [in] msg_body pointer to message that need to be parsed.
 * @param [in] msg_length lenght of message.
 * @param [out] event_datap pointer to a pointer of data structure that will be populated after
 * parsing the message.
 * is an remote_cc response.
 * @return CC_SUCCESS or CC_FAILURE.
 */
int ccxmlDecodeEventData (void * xml_parser_handle, cc_subscriptions_ext_t msg_type, const char *msg_body, int msg_length, ccsip_event_data_t ** event_datap);
#endif
