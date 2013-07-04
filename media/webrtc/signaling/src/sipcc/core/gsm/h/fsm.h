/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _FSM_H_
#define _FSM_H_

#include "cpr_types.h"
#include "sm.h"
#include "ccapi.h"
#include "vcm.h"
#include "ccsip_core.h"
#include "sll_lite.h"
#include "sessionConstants.h"
#include "ccsdp.h"
#include "fsmdef_states.h"

/* TODO: BLASBERG
 * fsm.h only needs the following from ccsip_core.h
 * should put basic sip types into a separate hdr file
typedef enum {
    ALERTING_NONE,
    ALERTING_OLD,
    ALERTING_TONE,
    ALERTING_RING
} alertingType;
*/


#define FSMCNF_MAX_CCBS            (LSM_MAX_LINES)
#define FSMXFR_MAX_XCBS            (LSM_MAX_LINES)
#define FSM_NO_ID                  (0)
#define FSMDEF_NO_DCB              (NULL)
#define FSMDEF_ERR_ONHOOK_TMR_SECS (20)

#define FSMDEF_MAX_DIGEST_ALG_LEN  10
#define FSMDEF_MAX_DIGEST_LEN      32 * 3

// Should match define for SIP stack MAX_SIP_URL_LENGTH
#define FSMDEF_MAX_CALLER_ID_LEN (256)

#ifndef fim_icb_t__
#define fim_icb_t__
struct fim_icb_t_;
typedef struct fim_icb_t_ fim_icb_t;
#endif

typedef enum {
    PRIMARY,
    MONITOR,
    LOCAL_CONF,
    WHISPER_COACHING
} fsm_session_t;

typedef enum {
    DIAL_MODE_NUMERIC,
    DIAL_MODE_URL
} dialMode_t;

typedef enum {
    FSMDEF_CALL_TYPE_MIN = -1,
    FSMDEF_CALL_TYPE_NONE = CC_CALL_TYPE_NONE,
    FSMDEF_CALL_TYPE_INCOMING = CC_CALL_TYPE_INCOMING,
    FSMDEF_CALL_TYPE_OUTGOING = CC_CALL_TYPE_OUTGOING,
    FSMDEF_CALL_TYPE_FORWARD = CC_CALL_TYPE_FORWARDED,
    FSMDEF_CALL_TYPE_MAX
} fsmdef_call_types_t;

typedef enum {
    FSMDEF_MRTONE_NO_ACTION = 0,
    FSMDEF_MRTONE_PLAYED_MONITOR_TONE,
    FSMDEF_MRTONE_PLAYED_RECORDER_TONE,
    FSMDEF_MRTONE_PLAYED_BOTH_TONES,
    FSMDEF_MRTONE_RESUME_MONITOR_TONE,
    FSMDEF_MRTONE_RESUME_RECORDER_TONE,
    FSMDEF_MRTONE_RESUME_BOTH_TONES
} fsmdef_monrec_tone_action_e;

typedef enum {
    FSMDEF_PLAYTONE_NO_ACTION = 0,
    FSMDEF_PLAYTONE_ZIP
} fsmdef_play_tone_action_e;

/* Local crypto parameter is the local parameters to offer */
typedef struct fsmdef_crypto_param_t_ {
    int32_t                tag;           /* crypto attribute tag    */
    vcm_crypto_algorithmID algorithmID;   /* encryption algorithm.   */
    vcm_crypto_key_t       key;           /* local key               */
} fsmdef_crypto_param_t;

/* Negotiated crypto parameter */
#define FSMDEF_CRYPTO_TX_CHANGE   (1 << 0) /* crypto Tx parms. change */
#define FSMDEF_CRYPTO_RX_CHANGE   (1 << 1) /* crypto Tx parms. change */
typedef struct fsmdef_negotiated_crypto_t_ {
    int32_t                 tag;           /* crypto attribute tag    */
    vcm_crypto_algorithmID  algorithmID;   /* algorithm ID            */
    vcm_crypto_key_t        tx_key;        /* tx key                  */
    vcm_crypto_key_t        rx_key;        /* rx key                  */
    uint32_t                flags;         /* misc. flags.            */
    char                    algorithm[FSMDEF_MAX_DIGEST_ALG_LEN];
    char                    digest[FSMDEF_MAX_DIGEST_LEN];
} fsmdef_negotiated_crypto_t;

/*
 * Saved attributes of interest from previously received SDP
 */
typedef struct fsmdef_previous_sdp_ {
    uint16_t        dest_port;
    cpr_ip_addr_t   dest_addr;
    int32_t         avt_payload_type;

    /*
     * This field contains the number of elements in the payloads field.
     */
    int32_t num_payloads;
    vcm_payload_info_t* payloads;

    uint16_t        packetization_period;
    uint16_t        max_packetization_period;
    sdp_direction_e direction;
    int32_t         tias_bw;
    int32_t         profile_level;
} fsmdef_previous_sdp_t;

typedef struct fsmdef_media_t_ {
    sll_lite_node_t node;     /* link node, must be first member of struct */
    media_refid_t   refid;    /* media reference id                        */
    sdp_media_e     type;     /* audio, video etc. media                   */
    sdp_addrtype_e  addr_type;/* ipv4, ipv6                                */
    int32_t         avt_payload_type;
    vcm_vad_t       vad;
    uint16_t        packetization_period;
    uint16_t        max_packetization_period;
    uint16_t        mode;
    uint16_t        level;
    boolean         direction_set;
    sdp_direction_e direction;         /* current negotiated direction    */
    sdp_direction_e support_direction; /* supported direction             */
    sdp_transport_e transport;
    uint16_t        src_port;   /* source port for this media stream      */
    cpr_ip_addr_t   src_addr;   /* source addr for this media stream      */
    uint16_t        dest_port;  /* destination port for this media stream */
    cpr_ip_addr_t   dest_addr;  /* destination addr for this media straam */
    /* Flag to indicate if Multicast */
    boolean         is_multicast;
    uint16_t        multicast_port;
    /*
     * rcv_chan indicates if the receive media stream has been opened
     */
    boolean         rcv_chan;
    /*
     * xmit_chan indicates if the transmit media stream has been opened
     */
    boolean         xmit_chan;

    /*
     * SRTP support.
     */
    fsmdef_negotiated_crypto_t negotiated_crypto;

    /*
     * Local crypto holds the local offered crypto set, keeps it in the
     * dcb for fast access. The alternative to be kept in the
     * SDP structure but is slower in retrieving it. In the future,
     * it is possible that more than 1 crypto lines are offered but
     * for now it is one.
     */
    fsmdef_crypto_param_t local_crypto;

    /*
     * Used to track previously received SDP for comparisons to
     * newly received media to determine if RTP port recycling is
     * required. Eliminates unnecessary recycling of ports which
     * causes breaks in the audio stream.
     */
    fsmdef_previous_sdp_t previous_sdp;

    /*
     * hold tracks the hold state. The flag is a bit map so it is possible that
     * the phone may have multiple holding states, ie. local and remote
     */
    uint32_t hold;
    /*
     * Flags fields for various bit flags
     */
#define FSM_MEDIA_F_SUPPORT_SECURITY (1 << 0)  /* supported security */
    uint32_t flags;

    /*
     * capability index. The index into the media capbilty table
     * that this media entry is coresponding to.
     */
    uint8_t         cap_index;

    /* Values cached from attributes */
    int32_t         tias_bw;
    int32_t         profile_level;

    void *video;

    /* ICE Candidates */
    char **candidatesp;
    int candidate_ct;

    /*
     * rtcp-mux indicates media stream is muxed for RTP and RTCP
     */
    boolean        rtcp_mux;

    /*
     * port number used in m= data channel line
     */
    uint16_t       local_datachannel_port;
    uint16_t       remote_datachannel_port;

    /*
     * Data Channel properties
     */
#define WEBRTC_DATACHANNEL_STREAMS_DEFAULT 16
    uint32         datachannel_streams;
    char           datachannel_protocol[SDP_MAX_STRING_LEN + 1];

    /*
     * This field contains the number of elements in the payloads field.
     */
    int32_t num_payloads;

    /*
     * List of active lists of payloads negotiated
     */
    vcm_payload_info_t* payloads;

} fsmdef_media_t;

struct fsm_fcb_t_;

typedef struct {
    callid_t        call_id;
    callid_t        join_call_id;
    line_t          line;
    cc_caller_id_t  caller_id;
    groupid_t       group_id;
    int             digit_cnt;
    fsmdef_call_types_t call_type;
    fsm_session_t   session;
    boolean         send_release;
    int             msgs_sent;
    int             msgs_rcvd;
    boolean         onhook_received;

    /*
     * inband indicates if inband alerting is active
     */
    boolean inband;

    /*
     * inband_received indicates if inband alerting has been received.
     * Once set, this bool stays set until dcb is reset.
     */
    boolean inband_received;

    /*
     * outofband tracks the payload type for outofband DTMF
     */
    int outofband;

    /*
     * Boolean indication of whether call was originated by phone or
     * far end party.
     */
    boolean inbound;

    /*
     * The following data tracks the RTP info
     */
    boolean        remote_sdp_present;
    boolean        remote_sdp_in_ack;
    boolean        local_sdp_complete;
    uint16_t       src_sdp_version;
    cc_sdp_t       *sdp;

    /* media list corresponding to m lines */
    sll_lite_list_t media_list;

    /*
     * dial_mode tracks the state of the dialing mode icon, ie. alphanumeric or
     * numeric
     */
    dialMode_t dial_mode;

    /*
     * pd_updated tracks whether or not the personal directory has
     * been updated
     */
    boolean pd_updated;

    /* tracks the ringing pattern to play */
    vcm_ring_mode_t alerting_ring;

    /* tracks the tone to play */
    vcm_tones_t alerting_tone;

    /* tracks the direction to play the tone. */
    uint16_t tone_direction;

    /* Was an alert-info header present, if so what did it contain */
    cc_alerting_type alert_info;

    /* used to determine when SIP stack releases the call early. */
    boolean early_error_release;

    /* used to determine when we are played a tone via the dialplan */
    boolean dialplan_tone;

    /* active tone (i.e. tone currently being played or requested to be played) */
    vcm_tones_t active_tone;

    /* indicates the action of monitor and recorder tones */
    fsmdef_monrec_tone_action_e monrec_tone_action;

    /* monitor/recorder tone direction to play out to */
    uint16_t monitor_tone_direction;
    uint16_t recorder_tone_direction;

    /* indicates the action of play tone for a single time */
    fsmdef_play_tone_action_e play_tone_action;

    struct fsm_fcb_t_ *fcb;

    /* Feature that is currently active */
    cc_features_t active_feature;

    /* Reason for hold */
    cc_hold_resume_reason_e hold_reason;

    /* Feature invocation state.
     * Each feature will correspond to unique bit in variable.
     * Bit will be Set if feature is invoked and awaiting feature ACK.
     * Bit will be Cleared if feature is ACKed or not yet invoked.
     * Each array element will hold invocation state for 32 features.
     * The resource manager utility is utilized to maintain the bit settings.
     */
    void *feature_invocation_state;

    /* TRUE if CCM has requested phone to show ringout UI */
    boolean spoof_ringout_requested;
    /* TRUE if GSM has applied ringout due to CCMs request to show ringout UI */
    boolean spoof_ringout_applied;

    /* Timer to go on hook after any call error */
    cprTimer_t err_onhook_tmr;

    /* Request pending timer */
    cprTimer_t req_pending_tmr;

    /* Ringback delay timer */
    cprTimer_t ringback_delay_tmr;

    /*
     * save of orientation from callInfo to update UI at any time
     * other than during call info. update such as after Tx start in
     * order to update security icon.
     */
    cc_orientation_e orientation;

    /*
     * This boolean is used to short circuit sending UI update requests to the platform
     * so that requests are only made when one of the call ui components requires
     * updating. The same is done for placed call history.
     */
    boolean ui_update_required;
    boolean placed_call_update_required;

    boolean is_conf_call;

    cc_security_e security;
    cc_policy_e policy;

    /* auto answer timer */
    cprTimer_t autoAnswerTimer;
    int32_t    reversionInterval;
    cprTimer_t revertTimer;

    boolean dsp_out_of_resources;

    boolean selected;

    boolean select_pending;

    boolean call_not_counted_in_mnc_bt;

    /*
     * The media_cap holds the current media caps of the call
     */
    cc_media_cap_table_t *media_cap_tbl;

    /*
     * Holds the remote stream track information to be passed to UI
     */
    cc_media_remote_stream_table_t *remote_media_stream_tbl;

    /*
     * Holds the local stream track information passed in from the UI
     */
    cc_media_local_track_table_t *local_media_track_tbl;

#define FSMDEF_F_HOLD_REQ_PENDING  (1 << 0)/* hold feature pending    */
#define FSMDEF_F_XFER_COMPLETE     (1 << 1)/* hold feature pending    */
    uint32_t                flags;         /* misc. flags.            */

    int log_disp;

    uint8_t cur_video_avail;
    sdp_direction_e  video_pref;
    unsigned int callref;      /* Callref (CI) from CUCM */

    char peerconnection[PC_HANDLE_SIZE];  /* A handle to the peerconnection */
    boolean peerconnection_set;

    char *ice_ufrag;
    char *ice_pwd;
    char ice_default_candidate_addr[MAX_IPADDR_STR_LEN];

    char digest_alg[FSMDEF_MAX_DIGEST_ALG_LEN];
    char digest[FSMDEF_MAX_DIGEST_LEN];

} fsmdef_dcb_t;

typedef enum fsm_types_t_ {
    FSM_TYPE_MIN = -1,
    FSM_TYPE_NONE = FSM_TYPE_MIN,
    FSM_TYPE_HEAD,
    FSM_TYPE_CNF,
    FSM_TYPE_B2BCNF,
    FSM_TYPE_XFR,
    FSM_TYPE_DEF,
    FSM_TYPE_MAX
} fsm_types_t;

typedef enum fsm_hold_t_ {
    FSM_HOLD_MIN = -1,
    FSM_HOLD_NONE = 0,
    FSM_HOLD_LCL = 1,
    FSM_HOLD_MAX = 2
} fsm_hold_t;

typedef struct fsm_data_def_t_ {
    int hold;
} fsm_data_def_t;

typedef struct fsm_data_xfr_t_ {
    int xfr_id;
} fsm_data_xfr_t;

typedef struct fsm_data_t_ {
    union {
        fsm_data_def_t def;
        fsm_data_xfr_t xfr;
    } data;
} fsm_data_t;

typedef struct fsmcnf_ccb_t_ {
    cc_srcs_t cnf_orig;
    int      cnf_id;
    callid_t cnf_call_id;
    callid_t cns_call_id;
    line_t   cnf_line;
    line_t   cns_line;
    boolean  active;
    boolean  bridged;

/* The following field encodes flags */
#define JOINED  0x1
#define XFER    0x2
#define LCL_CNF    0x4
    uint32_t flags;
    boolean  cnf_ftr_ack;
} fsmcnf_ccb_t;

typedef enum fsmxfr_types_t_ {
    FSMXFR_TYPE_MIN = -1,
    FSMXFR_TYPE_NONE,
    FSMXFR_TYPE_XFR,
    FSMXFR_TYPE_BLND_XFR,
    FSMXFR_TYPE_DIR_XFR,
    FSMXFR_TYPE_MAX
} fsmxfr_types_t;

typedef enum fsmxfr_modes_t_ {
    FSMXFR_MODE_MIN = -1,
    FSMXFR_MODE_TRANSFEROR,
    FSMXFR_MODE_TRANSFEREE,
    FSMXFR_MODE_TARGET
} fsmxfr_modes_t;

struct fsmxfr_xcb_t_;
typedef struct fsmxfr_xcb_t_ {
    cc_srcs_t         xfr_orig;
    int               xfr_id;
    callid_t          xfr_call_id;
    callid_t          cns_call_id;
    line_t            xfr_line;
    line_t            cns_line;
    fsmxfr_types_t    type;
    cc_xfer_methods_t method;
    char              *dialstring;
    char              *queued_dialstring;
    char              *referred_by;
    boolean           active;
    boolean           cnf_xfr;
    boolean           xfer_comp_req;
    fsmxfr_modes_t    mode;
    struct fsmxfr_xcb_t_ *xcb2;
} fsmxfr_xcb_t;


typedef struct fsm_fcb_t_ {
    callid_t call_id;

    int state;
    int old_state;

    fsm_types_t fsm_type;


    /*
     * fsmdef specific data
     */
    fsmdef_dcb_t *dcb;

    /*
     * fsmxfr specific data
     */

    fsmxfr_xcb_t *xcb;
    /*
     * fsmcnf specific data
     */
    fsmcnf_ccb_t *ccb;

    /*
     * fsmb2bcnf specific data
     */
    fsmcnf_ccb_t *b2bccb;

} fsm_fcb_t;

typedef enum {
    FSMDEF_MSG_MIN              = -1,
    FSMDEF_MSG_NONE             = 0,
    FSMDEF_MSG_SETUP            = 1,
    FSMDEF_MSG_SETUP_ACK        = 2,
    FSMDEF_MSG_PROCEEDING       = 4,
    FSMDEF_MSG_ALERTING         = 8,
    FSMDEF_MSG_CONNECTED        = 16,
    FSMDEF_MSG_CONNECTED_ACK    = 32,
    FSMDEF_MSG_RELEASE          = 64,
    FSMDEF_MSG_RELEASE_COMPLETE = 128,
    FSMDEF_MSG_MAX
} fsmdef_msgs_t;

#define FSM_FOR_ALL_CBS(cb, cbs, max_cbs) \
    for ((cb) = (cbs); (cb) <= &((cbs)[(max_cbs-1)]); (cb)++)

#define FSM_CHK_FLAGS(flags, flag)   ((flags) &   (flag))
#define FSM_SET_FLAGS(flags, flag)   ((flags) |=  (flag))
#define FSM_RESET_FLAGS(flags, flag) ((flags) &= ~(flag))
void         *fsmdef_feature_timer_timeout(cc_features_t feature_id, void *);
void         fsmdef_end_call(fsmdef_dcb_t *dcb, cc_causes_t cause);
void         fsm_sm_ftr(cc_features_t ftr_id, cc_srcs_t src_id);
void         fsm_sm_ignore_ftr(fsm_fcb_t *fcb, int fname,
                               cc_features_t ftr_id);
void fsm_sm_ignore_src(fsm_fcb_t *fcb, int fname, cc_srcs_t src_id);
const char *fsm_state_name(fsm_types_t type, int id);
const char *fsm_type_name(fsm_types_t type);
fsmdef_dcb_t *fsm_get_dcb(callid_t call_id);
void fsm_init_scb(fim_icb_t *icb, callid_t call_id);
fsm_fcb_t *fsm_get_fcb_by_call_id(callid_t call_id);
fsm_fcb_t *fsm_get_fcb_by_call_id_and_type(callid_t call_id, fsm_types_t type);
void
fsm_get_fcb_by_selected_or_connected_call_fcb(callid_t call_id, fsm_fcb_t **con_fcb_found,
                                               fsm_fcb_t **sel_fcb_found);
fsm_fcb_t *fsm_get_new_fcb(callid_t call_id, fsm_types_t fsm_type);
void fsm_init(void);
void fsm_shutdown(void);
void fsm_release(fsm_fcb_t *fcb, int fname, cc_causes_t cause);
void fsm_change_state(fsm_fcb_t *fcb, int fname, int new_state);
void fsm_init_fcb(fsm_fcb_t *fcb, callid_t call_id, fsmdef_dcb_t *dcb,
                  fsm_types_t type);
void fsm_display_no_free_lines(void);
void fsm_display_use_line_or_join_to_complete(void);
void fsm_display_feature_unavailable(void);
void fsm_set_call_status_feature_unavailable(callid_t call_id, line_t line);

cc_causes_t fsm_get_new_outgoing_call_context(callid_t call_id, line_t line,
                                              fsm_fcb_t *fcb, boolean expline);
cc_causes_t fsm_get_new_incoming_call_context(callid_t call_id, fsm_fcb_t *fcb,
                                              const char *called_number,
                                              boolean expline);
sm_rcs_t fsmdef_release(fsm_fcb_t *fcb, cc_causes_t cause,
                        boolean send_release);
int fsmdef_get_call_type_by_call_id(callid_t call_id);
fsmdef_call_types_t fsmdef_get_call_id_by_call_ref(int call_ref);
fsmdef_dcb_t *fsmdef_get_dcb_by_call_id(callid_t call_id);
void fsmdef_init_dcb(fsmdef_dcb_t *dcb, callid_t call_id,
                     fsmdef_call_types_t call_type,
                     const char *called_number, line_t line,
                     fsm_fcb_t *fcb);
cc_causes_t fsm_set_fcb_dcbs (fsmdef_dcb_t *dcb);
fsmdef_dcb_t *fsmdef_get_new_dcb(callid_t call_id);
void fsmdef_init(void);
int fsmdef_get_active_call_cnt(callid_t callId);
fsmdef_dcb_t *fsmdef_get_connected_call(void);
boolean fsmdef_are_join_calls_on_same_line(line_t line);
boolean fsmdef_are_there_selected_calls_onotherline(line_t line);
fsmdef_dcb_t *fsmdef_get_other_dcb_by_line(callid_t call_id, line_t line);
int fsmdef_get_dcbs_in_held_state(fsmdef_dcb_t **dcb,
                                  callid_t ignore_call_id);
sm_rcs_t fsmdef_offhook(fsm_fcb_t *fcb, cc_msgs_t msg_id, callid_t call_id,
                        line_t line, const char *dial_string, sm_event_t *event,
                        char *global_call_id, callid_t prim_call_id,
                        cc_hold_resume_reason_e consult_reason,
                        monitor_mode_t monitor_mode);

sm_rcs_t fsmdef_dialstring(fsm_fcb_t *fcb, const char *dialstring,
                           cc_redirect_t *redirect, boolean replace,
                           cc_call_info_t *call_info);

fsmcnf_ccb_t *fsmcnf_get_ccb_by_call_id(callid_t call_id);
callid_t fsmcnf_get_other_call_id(fsmcnf_ccb_t *ccb, callid_t call_id);

void fsmxfr_update_xfr_context(fsmxfr_xcb_t *xcb, callid_t old_call_id,
                               callid_t new_call_id);
fsmxfr_xcb_t *fsmxfr_get_xcb_by_call_id(callid_t call_id);
callid_t fsmxfr_get_other_call_id(fsmxfr_xcb_t *xcb, callid_t call_id);
fsmxfr_types_t fsmxfr_get_xfr_type(callid_t call_id);
cc_features_t fsmxfr_type_to_feature(fsmxfr_types_t type);

#ifdef _WIN32
extern void NotifyStateChange(callid_t callid, int32_t state);
#define NOTIFY_STATE_CHANGE(fcb,callid,state) NotifyStateChange(callid,state);dcsm_update_gsm_state(fcb,callid,state)
#else
#define NOTIFY_STATE_CHANGE(fcb,callid,state) dcsm_update_gsm_state(fcb,callid,state)
#endif

const char *fsmdef_state_name(int id);
const char *fsmxfr_state_name(int id);
const char *fsmcnf_state_name(int id);

void fsm_cac_init(void);
void fsmcnf_init(void);
void fsmxfr_init(void);
void fsmdef_init(void);
void fsmcnf_shutdown(void);
void fsmxfr_shutdown(void);
void fsmdef_shutdown(void);
void fsm_cac_shutdown(void);
void fsm_cac_call_release_cleanup(callid_t call_id);

void fsmdef_reversion_timeout(callid_t call_id);
void fsm_cac_process_bw_fail_timer(void *tmr_data);
void fsmdef_auto_answer_timeout(void *);
const char *fsmb2bcnf_state_name(int id);
void fsmb2bcnf_init(void);
void fsmb2bcnf_shutdown(void);
int fsmutil_is_b2bcnf_consult_call(callid_t call_id);
callid_t fsmxfr_get_consult_call_id(callid_t call_id);
callid_t fsmb2bcnf_get_consult_call_id(callid_t call_id);
callid_t fsmb2bcnf_get_primary_call_id(callid_t call_id);
boolean fsmdef_check_if_ok_for_dial_call(line_t line);
boolean fsmdef_check_if_ok_to_ans_call(line_t line, callid_t call_id);
boolean fsmdef_check_if_ok_to_resume_call(line_t line, callid_t call_id);
boolean fsmdef_check_if_ok_to_hold_call(line_t line, callid_t call_id);
boolean fsmdef_check_if_ok_to_monitor_update_call(line_t line, callid_t call_id);
boolean fsmdef_check_if_ok_to_run_feature(line_t line, callid_t call_id);
fsmdef_dcb_t *fsmdef_get_dcb_by_call_instance_id(line_t line,
                                                 uint16 call_instance_id);
boolean fsmb2bcnf_check_if_ok_to_setup_conf (callid_t call_id);
boolean fsmdef_check_if_chaperone_call_exist (void);
void fsmdef_call_cc_state_dialing(fsmdef_dcb_t *dcb, boolean suppressStutter);
/* This macro is to identify incoming joining call */
#define fsm_is_joining_call(feat_data)  \
      ((feat_data.newcall.join.join_call_id != CC_NO_CALL_ID) && \
      ((feat_data.newcall.cause == CC_CAUSE_BARGE) || \
      (feat_data.newcall.cause == CC_CAUSE_MONITOR)))
/* These macros are for SRTP support */
#define FSM_GET_SECURITY_STATUS(dcb) (dcb->security)
#define FSM_SET_SECURITY_STATUS(dcb, status) (dcb->security = status)
#define FSM_GET_POLICY(dcb) (dcb->policy)
#define FSM_SET_POLICY(dcb, status) (dcb->policy = status)
#define FSM_GET_CACHED_ORIENTATION(dcb) (dcb->orientation)
#define FSM_SET_CACHED_ORIENTATION(dcb, value) (dcb->orientation = value)
#define FSM_NEGOTIATED_CRYPTO_ALGORITHM_ID(media)                  \
       ((media->transport == SDP_TRANSPORT_RTPSAVP  ||       \
         media->transport == SDP_TRANSPORT_RTPSAVPF) ?        \
        media->negotiated_crypto.algorithmID : VCM_NO_ENCRYPTION)
#define FSM_NEGOTIATED_CRYPTO_RX_KEY(media)       \
       &media->negotiated_crypto.rx_key
#define FSM_NEGOTIATED_CRYPTO_TX_KEY(media)       \
       &media->negotiated_crypto.tx_key

#define FSM_NEGOTIATED_CRYPTO_DIGEST_ALGORITHM(media)       \
       media->negotiated_crypto.algorithm

#define FSM_NEGOTIATED_CRYPTO_DIGEST(media)       \
       media->negotiated_crypto.digest

int fsmutil_get_call_attr(fsmdef_dcb_t *dcb, line_t line, callid_t call_id);
uint16_t fsmutil_get_ci_id(line_t line);
void fsmutil_init_ci_map(void);
void fsmdef_platform_dcb_init(fsmdef_dcb_t *dcb);
void fsmutil_free_all_ci_id(void);
void fsmutil_free_ci_id(uint16_t id, line_t line);
void fsmutil_set_ci_id(uint16_t id, line_t line);
void fsmutil_free_ci_map(void);
void fsmutil_show_ci_map(void);
void fsmutil_init_shown_calls_ci_map(void);
void fsmutil_free_all_shown_calls_ci_map(void);
void fsmutil_clear_shown_calls_ci_element(uint16_t id, line_t line);
void fsmutil_set_shown_calls_ci_element(uint16_t id, line_t line);
boolean fsmutil_is_shown_calls_ci_element_set(uint16_t id, line_t line);


callid_t fsmxfr_get_primary_call_id(callid_t call_id);
uint16_t fsmutil_get_num_selected_calls(void);
int fsmutil_is_cnf_consult_call(callid_t call_id);
int fsmutil_is_cnf_consult_leg(callid_t call_id, fsmcnf_ccb_t *fsmcnf_ccbs,
                               uint16_t max_ccbs);
int fsmutil_is_xfr_consult_call(callid_t call_id);
int fsmutil_is_xfr_consult_leg(callid_t call_id, fsmxfr_xcb_t *fsmxfr_xcbs,
                               uint16_t max_xcbs);
void fsmutil_init_groupid(fsmdef_dcb_t *dcb, callid_t call_id,
                          fsmdef_call_types_t call_type);
void fsmutil_process_feature_ack(fsmdef_dcb_t *dcb, cc_features_t feature_id);
void fsmutil_clear_all_feature_invocation_state(fsmdef_dcb_t *dcb);
void fsmutil_init_feature_invocation_state(fsmdef_dcb_t *dcb);
void fsmutil_free_feature_invocation_state(fsmdef_dcb_t *dcb);

void fsmdef_error_onhook_timeout(void *data);

int fsmutil_is_xfr_leg(callid_t call_id, fsmxfr_xcb_t *fsmxfr_xcbs,
                       unsigned short max_xcbs);
int fsmutil_is_cnf_leg(callid_t call_id, fsmcnf_ccb_t *fsmcnf_ccbs,
                       unsigned short max_ccbs);

void fsm_display_control_ringin_calls(boolean hide);
void fsmdef_update_media_cap_feature_event(cc_feature_t *msg);
boolean fsmcnd_conf_call_id_valid(fsmcnf_ccb_t   *ccb);

boolean fsmdef_check_retain_fwd_info_state(void);
#endif
