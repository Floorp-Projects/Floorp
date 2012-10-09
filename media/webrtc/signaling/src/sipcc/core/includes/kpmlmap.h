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

#ifndef KPMLMAP_H
#define KPMLMAP_H

#include "xml_parser_defines.h"
#include "singly_link_list.h"
#include "subapi.h"

#define KPML_ESCAPE      '\\'
#define MAX_DIALSTRING   256
#define DIAL_TIMEOUT      10
#define MAX_KPML_TAG_STRING 32
#define MAX_KPML_TEXT_STRING 16
#define KPML_BUFFERS_NUM    100

#define KPML_VER_STR "1.0"
#define KPML_ENTER_STR "#"
#define NUM_OF_REGX 1

typedef enum {
    KPML_NOMATCH = 0,
    KPML_GIVETONE,
    KPML_WILDPATTERN,
    KPML_FULLPATTERN,
    KPML_FULLMATCH,
    KPML_IMMEDIATELY
} kpml_match_action_e;

typedef enum {
    KPML_SUCCESS = 200,
    KPML_USER_TERM_NOMATCH = 402,
    KPML_TIMER_EXPIRE = 423,
    KPML_NO_DIALOG = 481,
    KPML_SUB_EXPIRE = 487,
    KPML_BAD_EVENT = 489,
    KPML_BAD_DOC = 501,
    KPML_NO_PERSISTENT = 531,
    KPML_NO_MULTIPLE = 532,
    KPML_NO_MULITPLE_CALL_LEG = 533
} kpml_resp_code_e;

typedef enum {
    KPML_NONE = 0x0,
    KPML_SIGNAL_ONLY = 0x1,
    KPML_DTMF_ONLY = 0x2,
    KPML_BOTH = 0x3
} kpml_config_e;

#define KPML_SUCCESS_STR "OK"
#define KPML_USER_TERM_NOMATCH_STR "No Match"
#define KPML_TIMER_EXPIRE_STR "Timer Expired"
#define KPML_SUB_EXPIRE_STR "Subscription Expired"
#define KPML_ATTR_NOT_SUPPORTED_STR "Attribute not supported"
#define KPML_TRYING_STR "Trying"


typedef enum {
    KPML_ONE_SHOT = 0,
    KPML_PERSISTENT,
    KPML_SINGLY_NOTIFY
} kpml_sub_type_e;

typedef enum {
    KPML_NO_TIMER,
    KPML_INTERDIGIT_TIMER,
    KPML_CRITICAL_TIMER,
    KPML_EXTRADIGIT_TIMER,
    KPML_LONGDIGIT_TIMER
} kpml_timer_e;

typedef enum {
    NO_SUB_DATA = 0,
    SUB_DATA_FOUND = 1,
    NOTIFY_SENT
} kpml_state_e;

typedef struct kpml_Q_digit_t_ {
    char digits[MAX_DIALSTRING];
    int dig_head;
    int dig_tail;
} kpml_Q_dig;

typedef struct {
    line_t line;
    callid_t call_id;
    void *timer;
} kpml_key_t;

typedef struct kpml_regex_match_s {
    int num_digits;
    union {
        unsigned long single_digit_bitmask;
    } u;
} kpml_regex_match_t;

typedef struct kpml_data_t_ {

    line_t line;
    callid_t call_id;
    sub_id_t sub_id;

    unsigned long sub_duration;
    void *sub_timer;
    kpml_key_t subtimer_key;

    /* Hold regx received by SUB */
    struct Regex regex[NUM_OF_REGX];

    kpml_regex_match_t regex_match[NUM_OF_REGX];

    /* If this is set then send out 402 response after # key */
    boolean enterkey;

	uint32_t kpml_id;

    /* if this persistent */
    kpml_sub_type_e persistent;
    int flush;

    /* timeout duration */
    xml_unsigned32 inttimeout;
    xml_unsigned32 crittimeout;
    xml_unsigned32 extratimeout;

    /* timer pointers for digits */
    void *inter_digit_timer;
    void *critical_timer;
    void *extra_digit_timer;

    kpml_key_t inttime_key;
    kpml_key_t crittime_key;
    kpml_key_t extratime_key;

    xml_unsigned16 longhold;
    xml_unsigned8 longrepeat;
    xml_unsigned8 nopartial;

    /* Store dialed digits */
    char kpmlDialed[MAX_DIALSTRING];
    boolean last_dig_bkspace;

    char q_digits[MAX_DIALSTRING];
    int dig_head;
    int dig_tail;

    boolean sub_reject; /* If this varilable is set reject
                         * the subscription. There are cases
                         * where INV has been sent out premeturaly
                         * If the B2BUA asks for more digit by
                         * subsctiption, then reject thoese
                         */

    boolean pending_sub;

    boolean enable_backspace;
    /* Setting this variable will enable backspace
     */

} kpml_data_t;

kpml_state_e kpml_update_dialed_digits(line_t line, callid_t call_id,
                                       char digit);
void kpml_flush_quarantine_buffer(line_t line, callid_t call_id);
void kpml_sm_register(void);
void kpml_quarantine_digits(line_t line, callid_t call_id, char digit);
void kpml_init(void);
void kpml_shutdown(void);
void kpml_set_subscription_reject(line_t line, callid_t call_id);
boolean kpml_get_state(void);
void kpml_inter_digit_timer_callback(void *);
void kpml_subscription_timer_callback(void *);
boolean kpml_is_subscribed (callid_t call_id, line_t line);

#endif //KPMLMAP_H
