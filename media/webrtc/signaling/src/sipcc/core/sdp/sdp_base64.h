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

#ifndef _SDP_BASE64_H_
#define _SDP_BASE64_H_

/*
 * base64_result_t
 *  Enumeration of the result codes for Base64 conversion.
 */
typedef enum base64_result_t_ {
    BASE64_INVALID=-1,
    BASE64_SUCCESS=0,
    BASE64_BUFFER_OVERRUN,
    BASE64_BAD_DATA,
    BASE64_BAD_PADDING,
    BASE64_BAD_BLOCK_SIZE,
    BASE64_RESULT_MAX
} base64_result_t;

#define MAX_BASE64_STRING_LEN 60

/* Result code string table */
extern char *base64_result_table[];

/*
 * BASE64_RESULT_TO_STRING
 *  Macro to convert a Base64 result code into a human readable string.
 */
#define BASE64_RESULT_TO_STRING(_result) (((_result)>=0 && (_result)<BASE64_RESULT_MAX)?(base64_result_table[_result]):("UNKNOWN Result Code"))

/* Prototypes */

int base64_est_encode_size_bytes(int raw_size_bytes);
int base64_est_decode_size_bytes(int base64_size_bytes);

base64_result_t base64_encode(unsigned char *src, int src_bytes, unsigned char *dest, int *dest_bytes);

base64_result_t base64_decode(unsigned char *src, int src_bytes, unsigned char *dest, int *dest_bytes);

#endif /* _SDP_BASE64_H_ */
