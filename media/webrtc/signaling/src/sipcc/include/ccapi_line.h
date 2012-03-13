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

#ifndef _CCAPI_LINE_H_
#define _CCAPI_LINE_H_

#include "ccapi_types.h"

/**
 * Get reference handle for the line
 * @param [in] line - lineID on which to create call
 * @return cc_call_handle_t - handle of the call created
 * NOTE: The info returned by this method must be released using CCAPI_Line_releaseLineInfo()
 */
cc_lineinfo_ref_t CCAPI_Line_getLineInfo(cc_uint32_t line);

/**
 * Create a call on the line
 * @param [in] line - lineID on which to create call
 * @return cc_call_handle_t - handle of the call created
 */
cc_call_handle_t CCAPI_Line_CreateCall(cc_lineid_t line);

/** 
 * Retain the lineInfo snapshot - this info shall not be freed until a 
 * CCAPI_Line_releaseLineInfo() API releases this resource.
 * @param [in] ref - reference to the block to be retained
 * @return void
 * NOTE: Application may use this API to retain the device info using this API inside 
 * CCAPI_LineListener_onLineEvent() App must release the Info using 
 * CCAPI_Line_releaseLineInfo() once it is done with it.
 */
void CCAPI_Line_retainLineInfo(cc_lineinfo_ref_t ref);

/**
 * Release the lineInfo snapshot that was retained using the CCAPI_Line_retainLineInfo API
 * @param [in] ref - line info reference to be freed
 * @return void
 */
void CCAPI_Line_releaseLineInfo(cc_lineinfo_ref_t ref);

#endif /* _CCAPIAPI_LINE_H_ */
