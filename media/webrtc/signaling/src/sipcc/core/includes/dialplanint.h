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

#ifndef DIALPLANINT_H
#define DIALPLANINT_H

#include "phone_types.h"
#include "dialplan.h"

#define BKSPACE_KEY 90
#define DEFAULT_DIALTIMEOUT 30
#define CISCO_PLAR_STRING  "x-cisco-serviceuri-offhook"

typedef struct timer_index_ {
    line_t line;
    callid_t call_id;
} timer_index_t;

typedef union timer_info_t_ {
    void *whole;
    timer_index_t index;
} timer_info_t;

/* dialplan interface data structure */
typedef struct dp_int_t_ {
    line_t   line;
    callid_t call_id;
    char     digit;
    char     digit_str[MAX_DIALSTRING];
    boolean  collect_more;
    void    *tmr_ptr;
    char     global_call_id[CC_GCID_LEN];
    monitor_mode_t monitor_mode;
} dp_int_t;

typedef enum {
    DP_NONE_TIMER,
    DP_OFFHOOK_TIMER,
    DP_INTERDIGIT_TIMER
} dp_timer_type_e;

typedef struct dp_data_t_ {
    char         gDialed[MAX_DIALSTRING];
    char         gReDialed[MAX_DIALSTRING];
    line_t       gRedialLine;
    char         empty_rewrite[MAX_DIALSTRING];

    uint32_t     offhook_timeout; /* timeout configured to wait after offhook */
    void        *dial_timer;
    boolean      gDialplanDone;
                 /* Indicates that local dialplan match has been completed */
    boolean      allow_proceed;
                 /* under certain conditions proceed has to be passed to
                  * UI even KPML is enabled. For example redial, which not
                  * collect more digits so freeze the screen by sending
                  * proceed to UI
                  */

    line_t       line; /* Line where which dialing is currently active */
    callid_t     call_id;        /* call id of currently dialing call */
    boolean      url_dialing;    /* Indicates dialing is url dialing */
    timer_info_t timer_info;
    dp_timer_type_e gTimerType;
} dp_data_t;

void dp_dial_timeout(void *);
void dp_int_init_dialing_data(line_t line, callid_t call_id);
void dp_int_update_key_string(line_t line, callid_t call_id, char *digits);
void dp_int_store_digit_string(line_t line, callid_t call_id, char *digit_str);
void dp_int_update_keypress(line_t line, callid_t call_id, unsigned char digit);
void dp_int_dial_immediate(line_t line, callid_t call_id, boolean collect_more,
                           char *digit_str, char *g_call_id, 
                           monitor_mode_t monitor_mode);
void dp_int_do_redial(line_t line, callid_t call_id);
void dp_int_onhook(line_t line, callid_t call_id);
void dp_int_offhook(line_t line, callid_t call_id);
void dp_int_update(line_t line, callid_t call_id, string_t called_num);
int dp_init_template(const char *dp_file_name, int maxSize);
void dp_int_cancel_offhook_timer(line_t line, callid_t call_id);


boolean dp_offhook (line_t line, callid_t call_id);
boolean dp_get_kpml_state(void);
void dp_delete_last_digit(line_t line_id, callid_t call_id);
void dp_store_digits(line_t line, callid_t call_id, unsigned char digit);
char *dp_get_gdialed_digits(void);
line_t dp_get_redial_line(void);
void dp_reset(void);
boolean dp_check_for_plar_line(line_t line);
void dp_cancel_offhook_timer(void);

#endif /* DIALPLANINT_H */
