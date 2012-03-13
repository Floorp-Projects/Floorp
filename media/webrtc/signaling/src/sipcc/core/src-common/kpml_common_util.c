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

#include "cpr_types.h"
#include "cpr_memory.h"
#include "cpr_timers.h"
#include "cpr_strings.h"
#include "phntask.h"
#include "ccsip_subsmanager.h"
#include "singly_link_list.h"
#include "ccapi.h"
#include "subapi.h"
#include "kpmlmap.h"
#include "dialplanint.h"
#include "uiapi.h"
#include "debug.h"
#include "phone_debug.h"
#include "kpml_common_util.h"

static kpml_status_type_t
add_char_to_bitmask (char character, unsigned long *bitmask)
{
    kpml_status_type_t rc = KPML_STATUS_OK;

    switch (character) {
    case 'x':
    case 'X':
        *bitmask |= REGEX_0_9;
        break;
    case '1':
        *bitmask |= REGEX_1;
        break;
    case '2':
        *bitmask |= REGEX_2;
        break;
    case '3':
        *bitmask |= REGEX_3;
        break;
    case '4':
        *bitmask |= REGEX_4;
        break;
    case '5':
        *bitmask |= REGEX_5;
        break;
    case '6':
        *bitmask |= REGEX_6;
        break;
    case '7':
        *bitmask |= REGEX_7;
        break;
    case '8':
        *bitmask |= REGEX_8;
        break;
    case '9':
        *bitmask |= REGEX_9;
        break;
    case '0':
        *bitmask |= REGEX_0;
        break;
    case '*':
        *bitmask |= REGEX_STAR;
        break;
    case '#':
        *bitmask |= REGEX_POUND;
        break;
    case 'A':
    case 'a':
        *bitmask |= REGEX_A;
        break;
    case 'B':
    case 'b':
        *bitmask |= REGEX_B;
        break;
    case 'C':
    case 'c':
        *bitmask |= REGEX_C;
        break;
    case 'D':
    case 'd':
        *bitmask |= REGEX_D;
        break;
    case '+':
        *bitmask |= REGEX_PLUS;
        break;

    default:

        rc = KPML_ERROR_INVALID_VALUE;
    }

    return (rc);
}

/*
 * handle_range_selector
 *     Range selector is in the form:
 *        [2-9]  - any of 2-9
 *
 * This function will set the bits in the bitmask according
 * to the specified range selector string.
 *
 * Input:  str = selector str (ex. "[2-9]")
 *         bitmask ptr - ptr to bitmask where bits should
 *                    be set
 * Returns: KPML_STATUS_OK -- success
 *          KPML_ERROR_INVALID_VALUE -- str value is not supported
 */
static kpml_status_type_t
handle_range_selector (char *str, unsigned long *bitmask)
{
    static const char *fname = "handle_range_selector";
    char *char_ptr;
    int first_digit = 0;
    int last_digit = 0;
    long first_shifted = 0;
    long last_shifted = 0;
    unsigned long temp_bitmask = 0;
    char digit[2];
    kpml_status_type_t rc = KPML_STATUS_OK;

    digit[1] = NUL;
    if (!str || !bitmask) {
        KPML_ERROR(KPML_F_PREFIX"Invalid input params", fname);
        return (KPML_ERROR_INTERNAL);
    }

    char_ptr = str;

    /* Parse [digit-digit] format */

    /* skip initial '[' */
    char_ptr++;
    digit[0] = *char_ptr;
    first_digit = atoi(digit);

    /* now check for '-' */
    char_ptr++;
    if (*char_ptr == '-') {
        char_ptr++;
        digit[0] = *char_ptr;
        last_digit = atoi(digit);

        /* make sure there is only 1 char after '-' */
        char_ptr++;
        if (*char_ptr != ']') {
            KPML_DEBUG(DEB_F_PREFIX"The Regex format %s is not supported.\n", 
                       DEB_F_PREFIX_ARGS(KPML_INFO, fname), str);
            rc = KPML_ERROR_INVALID_VALUE;

        } else if (first_digit > last_digit) {
            KPML_DEBUG(DEB_F_PREFIX"The Regex format %s is not "
                       "supported. First digit in the range must "
                       "be greater than the second.\n", DEB_F_PREFIX_ARGS(KPML_INFO, fname), str);
            rc = KPML_ERROR_INVALID_VALUE;
        }
    } else {
        KPML_DEBUG(DEB_F_PREFIX"The Regex format %s is not supported.\n", 
                   DEB_F_PREFIX_ARGS(KPML_INFO, fname), str);
        rc = KPML_ERROR_INVALID_VALUE;
    }

    if (rc == KPML_STATUS_OK) {

        /*
         * set bitmask range
         *
         *  ex) 2-4
         *    111111111100       (0-9 set, shifted left by 2)
         *   &  000001111111111  (0-9 set, shifted right by 9-4)
         *      ----------
         *      0000011100
         */

        first_shifted = REGEX_0_9;
        last_shifted = REGEX_0_9;
        first_shifted = (first_shifted << first_digit);
        last_shifted = (last_shifted >> (9 - last_digit));
        temp_bitmask = (first_shifted & last_shifted);
        *bitmask |= temp_bitmask;
    }

    KPML_DEBUG(DEB_F_PREFIX"1st/last digit=%d/%d, bitmask=%lu, "
               "return status = %d\n", DEB_F_PREFIX_ARGS(KPML_INFO, fname), first_digit, 
               last_digit, *bitmask, rc);

    return (rc);
}

/*
 * handle_character_selector
 *     Character selector is in 2 possible forms:
 *        [17#]  - 1, 7, or #
 *        [^01]  - any of 2-9
 *
 * This function will set the bits in the bitmask according
 * to the specified character selector string.
 *
 * Input:  str = selector str (ex. "[17#]")
 *         bitmask ptr - ptr to bitmask where bits should
 *                    be set
 * Returns: KPML_STATUS_OK -- success
 *          KPML_ERROR_INVALID_VALUE -- str value is not supported
 */
static kpml_status_type_t
handle_character_selector (char *str, unsigned long *bitmask)
{
    static const char *fname = "handle_character_selector";
    char *char_ptr;
    boolean negative_selector = FALSE;
    unsigned long temp_bitmask = 0;
    unsigned long all_digits = REGEX_0_9;
    kpml_status_type_t rc = KPML_STATUS_OK;

    char_ptr = str;

    /*
     * Check for the default dtmf string [x*#ABCD].  This is the
     * default regex used by the SIP gws.
     */
    if (cpr_strcasecmp(str, KPML_DEFAULT_DTMF_REGEX) == 0) {
        *bitmask |= REGEX_0_9 | REGEX_STAR | REGEX_POUND |
                    REGEX_A | REGEX_B | REGEX_C | REGEX_D;

        return (rc);
    }

    /* skip initial '[' */
    char_ptr++;

    if (*char_ptr == '^') {
        /* skip the '^' */
        char_ptr++;
        negative_selector = TRUE;
    }


    /* add each character to the bitmask */
    while ((rc == KPML_STATUS_OK) && (*char_ptr)) {

        if (*char_ptr != ']') {
            rc = add_char_to_bitmask(*char_ptr, &temp_bitmask);
            char_ptr++;
        } else {
            /* There should not be any characters after the ']' */
            char_ptr++;
            if (*char_ptr) {
                KPML_DEBUG(DEB_F_PREFIX"The Regex format %s is not supported.\n", 
                            DEB_F_PREFIX_ARGS(KPML_INFO, fname), str);
                rc = KPML_ERROR_INVALID_VALUE;
            }
        }
    }

    if (rc == KPML_STATUS_OK) {
        if (!negative_selector) {
            *bitmask |= temp_bitmask;
        } else {
            /* only digits are allowed with negative selector */
            temp_bitmask = ~temp_bitmask;
            *bitmask |= (temp_bitmask & all_digits);
        }
    }

    KPML_DEBUG(DEB_F_PREFIX"bitmask=%lu, return status = %d\n", 
                DEB_F_PREFIX_ARGS(KPML_INFO, fname), *bitmask, rc);

    return (rc);

}

/*
 * kpml_parser_regex_str
 *
 * This function parses a regex string based on the rules in
 * IETF KPML draft - http://www.ietf.org/internet-drafts/
 *                         draft-ietf-sipping-kpml-04.txt
 * Currently only single character patterns are supported.
 * This function will set the bits in the regex_match bitmask.
 *
 * Input:  regex_str = regular expression string (ex. "[x*#ABCD]")
 *         regex_match = structure which contains a bitmask of
 *             kpml characters.  The appropriate bits will be set
 *             by this function.
 *
 * Returns: KPML_STATUS_OK -- success
 *          KPML_ERROR_INVALID_VALUE -- the regex format is not supported
 *          KPML_ERROR_INTERNAL -- internal error
 */
kpml_status_type_t
kpml_parse_regex_str (char *regex_str, kpml_regex_match_t *regex_match)
{
    static const char *fname = "kpml_parse_regex_str";
    int len;
    boolean single_char = FALSE;
    kpml_status_type_t rc = KPML_STATUS_OK;

    if (!regex_str || !regex_match) {
        KPML_DEBUG(DEB_F_PREFIX"Invalid input params. \n", DEB_F_PREFIX_ARGS(KPML_INFO, fname));
        return (KPML_ERROR_INTERNAL);
    }

    regex_match->num_digits = 1;
    regex_match->u.single_digit_bitmask = 0;

    /*
     * This function currently only handles single-digit pattern
     * matches.
     *
     * Examples:
     *  x       any single digit 0-9
     *  2       the digit 2
     *  [17#]   1,7, or #
     *  [^01]   any single digit from 2-9
     *  [2-9]   any single digit from 2-9
     *  x{1}    any single digit 0-9
     *  x{1,1}  any single digit 0-9
     */

    len = strlen(regex_str);

    if (len == 1) {
        single_char = TRUE;
    } else {
        if (regex_str[1] == '{') {
            if ((strncmp(&regex_str[1], "{1}", 3) == 0) ||
                (strncmp(&regex_str[1], "{1,1}", 5) == 0)) {
                single_char = TRUE;
            } else {

                KPML_DEBUG(DEB_F_PREFIX"The Regex format %s is not supported.\n",
                           DEB_F_PREFIX_ARGS(KPML_INFO, fname), regex_str);
                return (KPML_ERROR_INVALID_VALUE);
            }
        }
    }

    if (single_char) {
        /*  x, x{1}, or x{1,1} format */
        regex_match->num_digits = 1;
        rc = add_char_to_bitmask(regex_str[0],
                                 &(regex_match->u.single_digit_bitmask));

    } else if (regex_str[0] == '[') {

        if (strchr(regex_str, '-')) {
            /* [2-9] format */
            rc = handle_range_selector(regex_str,
                                       &(regex_match->u.single_digit_bitmask));
        } else {

            /* [17#] or [^01] format */
            rc = handle_character_selector(regex_str,
                                           &(regex_match->u.single_digit_bitmask));
        }


    } else {
        /* The regex format is not supported. */
        KPML_DEBUG(DEB_F_PREFIX"The Regex format %s is not supported.\n",
                DEB_F_PREFIX_ARGS(KPML_INFO, fname), regex_str);
        rc = KPML_ERROR_INVALID_VALUE;
    }

    return (rc);
}
