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
