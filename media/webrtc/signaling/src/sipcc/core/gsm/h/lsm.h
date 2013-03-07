/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _LSM_H_
#define _LSM_H_

#include "cpr_types.h"
#include "ccapi.h"
#include "uiapi.h"
#include "fsm.h"


#define LSM_NO_LINE     (0)
#define LSM_NO_INSTANCE (0)
#define NO_LINES_AVAILABLE (0)

/* FIXME GS - Breaking my own rule - need to figure out how to do a platform
 * specific include file here
 */
#define LSM_MAX_LINES         MAX_REG_LINES
#define LSM_MAX_INSTANCES     (MAX_CALLS_PER_LINE-1)
#define LSM_MAX_EXP_INSTANCES MAX_CALLS_PER_LINE
#define LSM_MAX_CALLS         MAX_CALLS

#define NO_FREE_LINES_TIMEOUT  (10)
#define CALL_ALERT_TIMEOUT  (10)

/*
 * Used to play busy verfication tone
 */
#define BUSY_VERIFICATION_DELAY       (10000)

/*
 * Temp until added to config
 */
#define TOH_DELAY                (10000)

/*
 * Used to play msg waiting and stutter dialtones.
 * Both tones are 100ms on/off repeating 10 and
 * 3 times respectively, followed by steady dialtone.
 * Due to DSP limitations we first tell the DSP to
 * play the 100ms on/off pairs the correct number of
 * times, set a timer, and then tell it to play dialtone.
 */
#define MSG_WAITING_DELAY   (2050)
#define STUTTER_DELAY       (650)

/*
 * Used to play the busy verfication tone which
 * is two seconds of dialtone followed by the
 * callwaiting tone every ten seconds.
 */
#define BUSY_VERIFY_DELAY   (12000)


/* LSM states */
typedef enum {
    LSM_S_MIN  = -1,
    LSM_S_NONE = LSM_S_MIN,
    LSM_S_IDLE,
    LSM_S_PENDING,
    LSM_S_OFFHOOK,
    LSM_S_ONHOOK,
    LSM_S_PROCEED,
    LSM_S_RINGOUT,
    LSM_S_RINGIN,
    LSM_S_CONNECTED,
    LSM_S_BUSY,
    LSM_S_CONGESTION,
    LSM_S_HOLDING,
    LSM_S_CWT,
    LSM_S_XFER,
    LSM_S_ATTN_XFER,
    LSM_S_CONF,
    LSM_S_INVALID_NUMBER,
    LSM_S_MAX
} lsm_states_t;

boolean lsm_is_phone_idle(void);
int lsm_get_instances_available_cnt(line_t line, boolean expline);
void lsm_increment_call_chn_cnt(line_t line);
void lsm_decrement_call_chn_cnt(line_t line);
line_t lsm_get_newcall_line(line_t line);
callid_t lsm_get_active_call_id(void);
line_t lsm_get_line_by_call_id(callid_t call_id);
/*
 * The lsm_get_facility_by_called_number() and the lsm_get_facility_by_line()
 * below are defined to allow be used by fsm state mechine to bind the
 * LSM to the FSM default state machine only.
 */
cc_causes_t lsm_get_facility_by_called_number(callid_t call_id,
                                              const char *called_number,
                                              line_t *free_line,
                                              boolean expline, void *dcb);
cc_causes_t lsm_get_facility_by_line(callid_t call_id, line_t line,
                                     boolean exp, void *dcb);
char lsm_digit2ch(int digit);
void lsm_set_active_call_id(callid_t call_id);
void lsm_init(void);
void lsm_shutdown(void);
void lsm_reset(void);
void lsm_ui_display_notify(const char *pNotifyStr, unsigned long timeout);
void lsm_ui_display_status(const char *pStatusStr, line_t line,
                           callid_t call_id);
string_t lsm_parse_displaystr(string_t displaystr);
void lsm_speaker_mode(short mode);
void lsm_add_remote_stream (line_t line, callid_t call_id, fsmdef_media_t *media, int *pc_stream_id);

#ifdef _WIN32
void terminate_active_calls(void);
#endif

const char *lsm_state_name(lsm_states_t id);
void lsm_set_cfwd_all_nonccm(line_t line, char *callfwd_dialstring);
void lsm_set_cfwd_all_ccm(line_t line, char *callfwd_dialstring);
void lsm_clear_cfwd_all_nonccm(line_t line);
void lsm_clear_cfwd_all_ccm(line_t line);
int lsm_check_cfwd_all_nonccm(line_t line);
int lsm_check_cfwd_all_ccm(line_t line);
char *lsm_is_phone_forwarded(line_t line);
void lsm_tmr_tones_callback(void *);
void lsm_update_placed_callinfo(void *dcb);
void lsm_start_multipart_tone_timer(vcm_tones_t tone, uint32_t delay,
                                    callid_t callId);
void lsm_start_continuous_tone_timer(vcm_tones_t tone, uint32_t delay,
                                     callid_t callId);
void lsm_start_tone_duration_timer(vcm_tones_t tone, uint32_t delay,
                                   cc_call_handle_t call_handle);
void lsm_stop_multipart_tone_timer(void);
void lsm_stop_continuous_tone_timer(void);
void lsm_stop_tone_duration_timer(void);
void lsm_tone_duration_tmr_callback(void *data);
void lsm_tone_start_with_duration (vcm_tones_t tone, short alert_info, cc_call_handle_t call_handle, groupid_t group_id,
                                   streamid_t stream_id, uint16_t direction, uint32_t duration);
void lsm_update_active_tone(vcm_tones_t tone, callid_t call_id);
boolean lsm_is_tx_channel_opened(callid_t call_id);
void lsm_update_monrec_tone_action (vcm_tones_t tone, callid_t call_id, uint16_t direction);
void lsm_downgrade_monrec_tone_action(vcm_tones_t tone, callid_t call_id);
char *lsm_get_gdialed_digits();
void lsm_set_hold_ringback_status(callid_t call_id, boolean ringback_status);

extern callid_t lsm_get_ui_id(callid_t call_id);
extern cc_call_handle_t lsm_get_ms_ui_call_handle(line_t line, callid_t call_id, callid_t ui_id);
extern void lsm_set_ui_id(callid_t call_id, callid_t ui_id);
extern lsm_states_t lsm_get_state(callid_t call_id);

extern void lsm_set_lcb_dusting_call(callid_t call_id);
extern void lsm_set_lcb_call_priority(callid_t call_id);
extern boolean lsm_is_it_priority_call(callid_t call_id);
extern void lsm_play_tone(cc_features_t feature_id);
extern boolean lsm_callwaiting(void);
extern void lsm_display_control_ringin_call (callid_t call_id, line_t line, boolean hide);
extern line_t lsm_find_next_available_line(line_t line, boolean same_dn, boolean incoming);
extern line_t lsm_get_available_line (boolean incoming);
extern boolean lsm_is_line_available(line_t line, boolean incoming);
extern void lsm_ui_display_notify_str_index(int str_index);
extern cc_causes_t lsm_allocate_call_bandwidth(callid_t call_id, int sessions);
extern void lsm_update_gcid(callid_t call_id, char * gcid);
extern void lsm_set_lcb_prevent_ringing(callid_t call_id);
extern void lsm_remove_lcb_prevent_ringing(callid_t call_id);
extern void lsm_set_lcb_dialed_str_flag(callid_t call_id);
void lsm_update_video_avail(line_t line, callid_t call_id, int dir);
void lsm_update_video_offered(line_t line, callid_t call_id, int dir);
callid_t lsm_get_callid_from_ui_id (callid_t uid);
void lsm_set_video_mute (callid_t call_id, int mute);
int lsm_get_video_mute (callid_t call_id);
void lsm_set_video_window (callid_t call_id, int flags, int x, int y, int h, int w);
void lsm_get_video_window (callid_t call_id, int *flags, int *x, int *y, int *h, int *w);
boolean lsm_is_kpml_subscribed (callid_t call_id);
void
lsm_util_tone_start_with_speaker_as_backup (vcm_tones_t tone, short alert_info,
                                    cc_call_handle_t call_handle, groupid_t group_id,
                                    streamid_t stream_id, uint16_t direction);

void lsm_initialize_datachannel (fsmdef_dcb_t *dcb, fsmdef_media_t *media);

#endif //_LSM_H_

