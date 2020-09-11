/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
