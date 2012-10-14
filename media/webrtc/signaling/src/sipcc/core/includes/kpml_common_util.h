/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __KPML_COMMON_UTIL_H__
#define __KPML_COMMON_UTIL_H__

/* KPML status code for kpml decoder */
typedef enum {
    KPML_STATUS_OK,
    KPML_ERROR_ENCODE,
    KPML_ERROR_INVALID_ATTR,
    KPML_ERROR_INVALID_ELEM,
    KPML_ERROR_INVALID_VALUE,
    KPML_ERROR_INTERNAL,
    KPML_ERROR_MISSING_ATTR,
    KPML_ERROR_MISSING_FIELD,
    KPML_ERROR_NOMEM,
    KPML_ERROR_XML_PARSER
} kpml_status_type_t;


/* single_digit_bitmask  */
#define REGEX_0             1
#define REGEX_1             (1<<1)
#define REGEX_2             (1<<2)
#define REGEX_3             (1<<3)
#define REGEX_4             (1<<4)
#define REGEX_5             (1<<5)
#define REGEX_6             (1<<6)
#define REGEX_7             (1<<7)
#define REGEX_8             (1<<8)
#define REGEX_9             (1<<9)
#define REGEX_STAR          (1<<10)
#define REGEX_POUND         (1<<11)
#define REGEX_A             (1<<12)
#define REGEX_B             (1<<13)
#define REGEX_C             (1<<14)
#define REGEX_D             (1<<15)
#define REGEX_PLUS          (1<<16)

#define REGEX_0_9             REGEX_0|REGEX_1|REGEX_2|REGEX_3|REGEX_4|REGEX_5|REGEX_6|REGEX_7|REGEX_8|REGEX_9

#define KPML_DEFAULT_DTMF_REGEX  "[x*#ABCD]"

kpml_status_type_t kpml_parse_regex_str(char *regex_str,
                                        kpml_regex_match_t *regex_match);

#endif /* __KPML_COMMON_UTIL_H__ */
