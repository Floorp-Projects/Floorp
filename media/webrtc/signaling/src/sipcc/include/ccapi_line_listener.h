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
