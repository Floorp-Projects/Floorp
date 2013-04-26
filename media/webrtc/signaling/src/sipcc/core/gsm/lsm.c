/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <time.h>
#include "cpr_types.h"
#include "cpr_stdlib.h"
#include "cpr_locks.h"
#include "cpr_stdio.h"
#include "cpr_timers.h"
#include "cpr_in.h"
#include "cpr_errno.h"
#include "phone_types.h"
#include "phone.h"
#include "sdp.h"
#include "lsm.h"
#include "phone_debug.h"
#include "fsm.h"
#include "gsm_sdp.h"
#include "vcm.h"
#include "ccsip_pmh.h"
#include "dtmf.h"
#include "debug.h"
#include "rtp_defs.h"
#include "lsm_private.h"
#include "dialplanint.h"
#include "kpmlmap.h"
#include "prot_configmgr.h"
#include "dialplan.h"
#include "sip_interface_regmgr.h"
#include "gsm.h"
#include "phntask.h"
#include "fim.h"
#include "util_string.h"
#include "platform_api.h"

static const char* logTag = "lsm";

#ifndef NO
#define NO  (0)
#endif

#ifndef YES
#define YES (1)
#endif

#define CALL_INFO_NONE  (string_t)""

#define FROM_NOTIFY_PRI   1 // Same as SCCP phone behavior
#define LSM_DISPLAY_STR_LEN 256

static cc_rcs_t lsm_stop_tone (lsm_lcb_t *lcb, cc_action_data_tone_t *data);

static lsm_lcb_t *lsm_lcbs;
static uint32_t lsm_call_perline[MAX_REG_LINES];

/* This variable is used locally to reflect the CFA state (set/clear)
 * when in CCM mode.
 */
static boolean cfwdall_state_in_ccm_mode[MAX_REG_LINES+1] ;

static const char *lsm_state_names[LSM_S_MAX] = {
    "IDLE",
    "PENDING",
    "OFFHOOK",
    "ONHOOK",
    "PROCEED",
    "RINGOUT",
    "RINGIN",
    "CONNECTED",
    "BUSY",
    "CONGESTION",
    "HOLDING",
    "CWT",
    "XFER",
    "ATTN_XFER",
    "CONF",
    "INVALID_NUMBER"
};

static const char *cc_state_names[] = {
    "OFFHOOK",
    "DIALING",
    "DIALING_COMPLETED",
    "CALL_SENT",
    "FAR_END_PROCEEDING",
    "FAR_END_ALERTING",
    "CALL_RECEIVED",
    "ALERTING",
    "ANSWERED",
    "CONNECTED",
    "HOLD",
    "RESUME",
    "ONHOOK",
    "CALL_FAILED",
    "HOLD_REVERT",
    "STATE_UNKNOWN"
};


static const char *cc_action_names[] = {
    "SPEAKER",
    "DIAL_MODE",
    "MWI",
    "MWI_LAMP",
    "OPEN_RCV",
    "UPDATE_UI",
    "MEDIA",
    "RINGER",
    "LINE_RINGER",
    "PLAY_TONE",
    "STOP_TONE",
    "STOP_MEDIA",
    "START_RCV",
    "ANSWER_PENDING",
    "PLAY_BLF_ALERT_TONE"
};

/* names are corresponds to vcm_ring_mode_t structure */
static const char *vm_alert_names[] = {
    "NONE",
//    "RINGER_OFF",
    "VCM_RING_OFF",
    "VCM_INSIDE_RING",
    "VCM_OUTSIDE_RING",
    "VCM_FEATURE_RING",
    "VCM_BELLCORE_DR1",
    "VCM_RING_OFFSET",
    "VCM_BELLCORE_DR2",
    "VCM_BELLCORE_DR3",
    "VCM_BELLCORE_DR4",
    "VCM_BELLCORE_DR5",
    "VCM_BELLCORE_MAX",
    "VCM_FLASHONLY_RING",
    "VCM_STATION_PRECEDENCE_RING",
    "VCM_MAX_RING"
};

/* Enum just to make code read better */
/* the following values must be in sync with the values listed in edcs-387610 */
typedef enum {
    DISABLE = 1,
    FLASH_ONLY = 2,
    RING_ONCE = 3,
    RING = 4,
    BEEP_ONLY = 5
} config_value_type_t;

cprTimer_t lsm_tmr_tones;
cprTimer_t lsm_continuous_tmr_tones;
cprTimer_t lsm_tone_duration_tmr;
static uint32_t lsm_tmr_tones_ticks;
static int callWaitingDelay;
static int ringSettingIdle;
static int ringSettingActive;

/* Ring mode set by remote-cc app */
static cc_rcc_ring_mode_e cc_line_ringer_mode[MAX_REG_LINES+1] =
    {CC_RING_DEFAULT};
// Following data has to be non-stack b/c the way SIP stack uses it.
// It is used by the lsm_is_phone_forwarded() function only.
static char cfwdall_url[MAX_URL_LENGTH];

static void lsm_update_inalert_status(line_t line, callid_t call_id,
                                      cc_state_data_alerting_t * data,
                                      boolean notify);
static void lsm_util_start_tone(vcm_tones_t tone, short alert_info,
        cc_call_handle_t call_handle, groupid_t group_id,
        streamid_t stream_id, uint16_t direction);

const char *
lsm_state_name (lsm_states_t id)
{
    if ((id <= LSM_S_MIN) || (id >= LSM_S_MAX)) {
        return get_debug_string(GSM_UNDEFINED);
    }

    return (lsm_state_names[id]);
}


static const char *
cc_state_name (cc_states_t id)
{
    if ((id <= CC_STATE_MIN) || (id >= CC_STATE_MAX)) {
        return (get_debug_string(GSM_UNDEFINED));
    }

    return (cc_state_names[id]);
}


static const char *
cc_action_name (cc_actions_t id)
{
    if ((id <= CC_ACTION_MIN) || (id >= CC_ACTION_MAX)) {
        return (get_debug_string(GSM_UNDEFINED));
    }

    return (cc_action_names[id]);
}


char
lsm_digit2ch (int digit)
{
    switch (digit) {
    case 0x0:
    case 0x1:
    case 0x2:
    case 0x3:
    case 0x4:
    case 0x5:
    case 0x6:
    case 0x7:
    case 0x8:
    case 0x9:
        return (char)(digit + '0');
    case 0x0e:
        return ('*');
    case 0x0f:
        return ('#');
    default:
        return ('x');
    }
}


void
lsm_debug_entry (callid_t call_id, line_t line, const char *fname)
{
    LSM_DEBUG(get_debug_string(LSM_DBG_ENTRY), call_id, line, fname);
}

static void
lsm_ui_call_state (call_events event, line_t line, lsm_lcb_t *lcb, cc_causes_t cause)
{
    if (lcb->previous_call_event != event) {
        lcb->previous_call_event = event;

        /* For local conference case, the second call is hidden
         * so do not show that when the call is held, resumed,
         * or moved to other state. This should be done only
         * when local bridge is active
         */
        ui_call_state(event, line, lcb->ui_id, cause);
    }
    else if(event == evConnected) {
	//This is for Chaperone Conference case, if conference changed to a normal call,
	//then need to update the call state to refresh the key's status. like re-enable
	//Confrn key
        ui_call_state(event, line, lcb->ui_id, cause);
    }
}

/**
 * This function will control the display of the ringingin call based on the hide arg.
 *
 * @param[in] call_id -  call id
 * @param[in] line - line on which call is ringing.
 * @param[in] hide - whether to hide or not
 *
 * @return none
 *
 * @pre (call_id != CC_NO_CALL_ID) and (line != 0)
 */
void lsm_display_control_ringin_call (callid_t call_id, line_t line, boolean hide)
{
    lsm_lcb_t *lcb;

    lcb = lsm_get_lcb_by_call_id(call_id);
    if (lcb != NULL) {
        ui_call_state(evRingIn, line, lcb->ui_id, CC_CAUSE_NORMAL);
    }
}

/**
 * This function will be invoked by DEF SM to set if it is a dusting call.
 * @param[in] call_id - GSM call id.
 *
 * @return none
 *
 * @pre (call_id != CC_NO_CALL_ID)
 */
void lsm_set_lcb_dusting_call (callid_t call_id)
{
    lsm_lcb_t *lcb;

    lcb = lsm_get_lcb_by_call_id(call_id);
    if (lcb != NULL) {
        FSM_SET_FLAGS(lcb->flags, LSM_FLAGS_DUSTING);
    }
}

/**
 * This function will be invoked by DEF SM to set call priority.
 *
 * @param[in] call_id - GSM call id.
 *
 * @return none
 *
 * @pre (call_id != CC_NO_CALL_ID)
 */
void lsm_set_lcb_call_priority (callid_t call_id)
{
    lsm_lcb_t *lcb;

    lcb = lsm_get_lcb_by_call_id(call_id);
    if (lcb != NULL) {
        FSM_SET_FLAGS(lcb->flags, LSM_FLAGS_CALL_PRIORITY_URGENT);
    }
}


/**
 * This function sets the LSM_FLAGS_DIALED_STRING bit in lcb->flags
 *
 * @param[in] call_id - GSM call id.
 *
 * @return none
 *
 * @pre (call_id != CC_NO_CALL_ID)
 */
void lsm_set_lcb_dialed_str_flag (callid_t call_id)
{
    lsm_lcb_t *lcb;

    lcb = lsm_get_lcb_by_call_id(call_id);
    if (lcb != NULL) {
        FSM_SET_FLAGS(lcb->flags, LSM_FLAGS_DIALED_STRING);
    }
}

/**
 * This function will be invoked by DEF SM to set gcid in lcb.
 *
 * @param[in] call_id - GSM call id.
 * @param[in] gcid - GCID provided by CUCM.
 *
 * @return none
 *
 * @pre (call_id != CC_NO_CALL_ID)
 */
void lsm_update_gcid (callid_t call_id, char * gcid)
{
    lsm_lcb_t *lcb;

    lcb = lsm_get_lcb_by_call_id(call_id);
    if (lcb != NULL) {
        if (lcb->gcid == NULL) {
            lcb->gcid = (char *)cpr_malloc(CC_GCID_LEN);
            sstrncpy(lcb->gcid, gcid, CC_GCID_LEN);
        }
    }

}
/**
 * This function will be invoked by DEF SM.
 * it will check if there is a RINGIN call
 * with the same GCID. If so, it will set a flag to prevent ringing.
 *
 * @param[in] call_id - GSM call id.
 *
 * @return none
 *
 * @pre (call_id != CC_NO_CALL_ID)
 */
void lsm_set_lcb_prevent_ringing (callid_t call_id)
{
    lsm_lcb_t *lcb;
    char *gcid;

    lcb = lsm_get_lcb_by_call_id(call_id);
    if (lcb == NULL) {
        return;
    }

    gcid = lcb->gcid;
    if (gcid == NULL) {
        return;
    }

    LSM_DEBUG(DEB_L_C_F_PREFIX"gcid=%s",
              DEB_L_C_F_PREFIX_ARGS(LSM, lcb->line, call_id, "lsm_set_lcb_prevent_ringing"), gcid);

    FSM_FOR_ALL_CBS(lcb, lsm_lcbs, LSM_MAX_LCBS) {
        if (lcb->state == LSM_S_RINGIN) {
            if ((lcb->gcid != NULL) && (strncmp(gcid, lcb->gcid, CC_GCID_LEN) == 0)) {
                LSM_DEBUG(DEB_L_C_F_PREFIX"found ringing call, gcid %s",
                          DEB_L_C_F_PREFIX_ARGS(LSM, lcb->line, lcb->call_id, "lsm_set_lcb_prevent_ringing"), gcid);
                FSM_SET_FLAGS(lcb->flags, LSM_FLAGS_PREVENT_RINGING);
            }
            break;
        }
    }
}

void lsm_remove_lcb_prevent_ringing (callid_t call_id)
{
    lsm_lcb_t *lcb;
    char *gcid;

    lcb = lsm_get_lcb_by_call_id(call_id);
    if (lcb == NULL) {
        return;
    }

    gcid = lcb->gcid;
    if (gcid == NULL) {
        return;
    }

    LSM_DEBUG(DEB_L_C_F_PREFIX"gcid=%s",
              DEB_L_C_F_PREFIX_ARGS(LSM, lcb->line, call_id, "lsm_remove_lcb_prevent_ringing"), gcid);

    FSM_FOR_ALL_CBS(lcb, lsm_lcbs, LSM_MAX_LCBS) {
        if (lcb->state == LSM_S_RINGIN) {
            if ((lcb->gcid != NULL) && (strncmp(gcid, lcb->gcid, CC_GCID_LEN) == 0)) {
                //FSM_RESET_FLAGS(lcb->flags, LSM_FLAGS_ANSWER_PENDING);
                lcb->flags = 0;
                LSM_DEBUG(DEB_L_C_F_PREFIX"found ringing call, gcid=%s, lcb->flags=%d.",
                          DEB_L_C_F_PREFIX_ARGS(LSM, lcb->line, lcb->call_id, "lsm_remove_lcb_prevent_ringing"), gcid, lcb->flags);
            }
            break;
        }
    }
}

/**
 * This function finds if the call is a priority call.
 *
 * @param[in] call_id - GSM call id.
 *
 * @return none
 *
 * @pre (call_id != CC_NO_CALL_ID)
 */
boolean lsm_is_it_priority_call (callid_t call_id)
{
    lsm_lcb_t *lcb;

    lcb = lsm_get_lcb_by_call_id(call_id);
    if (lcb == NULL) {
        return FALSE;
    }
    if (FSM_CHK_FLAGS(lcb->flags, LSM_FLAGS_CALL_PRIORITY_URGENT)) {
        return TRUE;
    }
    return FALSE;
}

/*
 *  Function: lsm_internal_update_call_info
 *
 *  Parameters:
 *     lcb - pointer to lsm_lcb_t.
 *     dcb - pointer to fsmdef_dcb_t.
 *
 *  Description: This is an internal function of LSM for updating call
 *               information to the UI that is not directly driven by the
 *               the explicit CALL INFO event from call control.
 *
 *               It is a convenient function that used by some of the
 *               LSM handling function that needs to update call information
 *               to the UI during media changes.
 *
 *  Returns:
 *     None.
 *
 */
static void
lsm_internal_update_call_info (lsm_lcb_t *lcb, fsmdef_dcb_t *dcb)
{
    boolean inbound;
    calltype_t call_type;
    fsmcnf_ccb_t   *ccb;

    if ((lcb == NULL) || (dcb == NULL)) {
        return;
    }

    if (!dcb->ui_update_required) {
        return;
    }

    /* For local conference, do not update the primary
     * call bubbles call-info. Primary call is already
     * displaying To conference in this case
     * But dcb-> caller_id should be updated to
     * refresh the UI when the call is dropped
     */
    ccb = fsmcnf_get_ccb_by_call_id(lcb->call_id);
    if (ccb && (ccb->flags & LCL_CNF) && (ccb->active)
             && (ccb->cnf_call_id == lcb->call_id)) {
        return;
    }
    dcb->ui_update_required = FALSE;

    /* Derive orientation of the call */
    switch (dcb->orientation) {
    case CC_ORIENTATION_FROM:
        inbound = TRUE;
        break;

    case CC_ORIENTATION_TO:
        inbound = FALSE;
        break;

    default:
        /*
         * No orientation available, use the direction when call was started
         */
        inbound = dcb->inbound;
        break;
    }

    if ((dcb->call_type == FSMDEF_CALL_TYPE_FORWARD)
        && fsmdef_check_retain_fwd_info_state()) {
        call_type = (inbound) ? (calltype_t)dcb->call_type:FSMDEF_CALL_TYPE_OUTGOING;
    } else {
        if (inbound) {
            call_type = FSMDEF_CALL_TYPE_INCOMING;
        } else {
            call_type = FSMDEF_CALL_TYPE_OUTGOING;
        }
    }

    ui_call_info(dcb->caller_id.calling_name,
                 dcb->caller_id.calling_number,
                 dcb->caller_id.alt_calling_number,
                 dcb->caller_id.display_calling_number,
                 dcb->caller_id.called_name,
                 dcb->caller_id.called_number,
                 dcb->caller_id.display_called_number,
                 dcb->caller_id.orig_called_name,
                 dcb->caller_id.orig_called_number,
                 dcb->caller_id.last_redirect_name,
                 dcb->caller_id.last_redirect_number,
                 call_type,
                 lcb->line, lcb->ui_id,
                 dcb->caller_id.call_instance_id,
                 FSM_GET_SECURITY_STATUS(dcb),
                 FSM_GET_POLICY(dcb));

}

/**
 * The function opens receive channel or allocates receive port. It depends
 * on the "keep" member of the cc_action_data_open_rcv_t structure set
 * up by the caller to whether opens a receive channel or just
 * to allocate a receive port.
 *
 * @param[in] lcb       - pointer to the lsm_lcb_t.
 * @param[in/out] data  - pointer to the cc_action_data_open_rcv_t.
 *                        Upon a successful return, the port element
 *                        of this structure will be filled with the actual
 *                        receive port.
 * @param[in]     media - pointer to the fsmdef_media_t if a specific
 *                        media to be operated on.
 *
 * @return              CC_RC_ERROR or CC_RC_SUCCESS.
 *
 * @pre (lcb is_not NULL) and (data is_not NULL)
 */
static cc_rcs_t
lsm_open_rx (lsm_lcb_t *lcb, cc_action_data_open_rcv_t *data,
             fsmdef_media_t *media)
{
    static const char fname[] = "lsm_open_rx";
    int           port_allocated = 0;
    cc_rcs_t      rc = CC_RC_ERROR;
    fsmdef_dcb_t *dcb;
    int           sdpmode = 0;

    dcb = lcb->dcb;
    if (dcb == NULL) {
        return (rc);
    }

    /*
     * P2: At this point, it is a guess that refid from media structure
     *     may be needed. If it turns out to be not the case, then the
     *     code below that looks up media should be removed including
     *     the media parameter that is passed in.
     */
    if (media == NULL) {
        /* no explicit media parameter specified, look up based on refID */
        if (data->media_refid != CC_NO_MEDIA_REF_ID) {
            media = gsmsdp_find_media_by_refid(dcb,
                                               data->media_refid);
        }
        if (media == NULL) {
            LSM_DEBUG(get_debug_string(LSM_DBG_INT1), lcb->call_id,
                      lcb->line, fname, "no media refID %d found",
                      data->media_refid);
            return (rc);
        }
    }

    LSM_DEBUG(get_debug_string(LSM_DBG_INT1), lcb->call_id, lcb->line, fname,
              "requested port", data->port);


    sdpmode = 0;
    config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));

    if (data->keep == TRUE) {
      if (sdpmode && strlen(dcb->peerconnection)) {
        /* If we are doing ICE, don't try to re-open */
        port_allocated = data->port;
      }
      else {
        //Todo IPv6: Add interface call for IPv6
        (void) vcmRxOpen(media->cap_index, dcb->group_id, media->refid,
          lsm_get_ms_ui_call_handle(lcb->line, lcb->call_id, lcb->ui_id), data->port,
          media->is_multicast ? &media->dest_addr:&media->src_addr, data->is_multicast,
          &port_allocated);
      }
      if (port_allocated != -1) {
        data->port = (uint16_t)port_allocated;
        rc = CC_RC_SUCCESS;
      }
    } else {

      if (sdpmode) {
        if (!strlen(dcb->peerconnection)) {
          vcmRxAllocPort(media->cap_index, dcb->group_id, media->refid,
            lsm_get_ms_ui_call_handle(lcb->line, lcb->call_id, lcb->ui_id),
            data->port,
            &port_allocated);
          if (port_allocated != -1) {
            data->port = (uint16_t)port_allocated;
            rc = CC_RC_SUCCESS;
          }
        } else {
          char **candidates;
          int candidate_ct;
          char *default_addr;
          short status;

          status = vcmRxAllocICE(media->cap_index, dcb->group_id, media->refid,
            lsm_get_ms_ui_call_handle(lcb->line, lcb->call_id, lcb->ui_id),
            dcb->peerconnection,
            media->level,
            &default_addr, &port_allocated,
            &candidates, &candidate_ct);

          if (!status) {
            sstrncpy(dcb->ice_default_candidate_addr, default_addr, sizeof(dcb->ice_default_candidate_addr));

            data->port = (uint16_t)port_allocated;
            media->candidate_ct = candidate_ct;
            media->candidatesp = candidates;
            rc = CC_RC_SUCCESS;
          }
        }
      }
    }

    if (rc == CC_RC_SUCCESS) {
      LSM_DEBUG(get_debug_string(LSM_DBG_INT1), lcb->call_id, lcb->line, fname,
                "allocated port", port_allocated);
    }

    return (rc);
}
/*
 * This function updates the dscp value based on whether video is enable or not
 * and video is active or not.
 * @param[in] dcb       - pointer to the fsmdef_dcb.
 */

void lsm_update_dscp_value(fsmdef_dcb_t   *dcb)
{
    static const char fname[] = "lsm_update_dscp_value";
    int dscp = 184;   /* default 184 used for DSCP */
    // depending upon video is enabled or disabled ,set the dscp value.
    if (dcb != NULL && dcb->cur_video_avail != SDP_DIRECTION_INACTIVE ) {
        config_get_value(CFGID_DSCP_VIDEO, (int *)&dscp, sizeof(dscp));
    } else {
        config_get_value(CFGID_DSCP_AUDIO, (int *)&dscp, sizeof(dscp));
    }
    // We would use DSCP for video for both audio and video streams if this is a video call
    if (dcb != NULL) {
        LSM_DEBUG(DEB_L_C_F_PREFIX"Setting dscp=%d for Rx group_id=%d",
            DEB_L_C_F_PREFIX_ARGS(LSM, dcb->line, dcb->call_id, fname), dscp,  dcb->group_id);
        vcmSetRtcpDscp(dcb->group_id, dscp);
    }
}

/**
 * The function closes receive channel for a given media entry.
 * The receive channel may not be closed if the caller intents to
 * fresh the channel i.e close if needed but otherwise leave it open.
 * When the caller indicates refreshing, the receive channel
 * will be closed only when there is a difference in current SDP and
 * the previous SDP.
 *
 * @param[in] lcb       - pointer to the lsm_lcb_t.
 * @param[in] refresh   - channel to be refreshed i.e. close if necessary.
 * @param[in] media     - pointer to the fsmdef_media_t for the
 *                        media entry to be refresh.
 *
 *                        If the value of media is NULL, it indicates that
 *                        all current inused media entries.
 *
 * @return              None.
 *
 * @pre (lcb is_not NULL)
 */
static void
lsm_close_rx (lsm_lcb_t *lcb, boolean refresh, fsmdef_media_t *media)
{
    static const char fname[] = "lsm_close_rx";
    fsmdef_media_t *start_media, *end_media;
    fsmdef_dcb_t   *dcb;
    int             sdpmode = 0;

    dcb = lcb->dcb;
    if (dcb == NULL) {
        LSM_ERR_MSG(get_debug_string(DEBUG_INPUT_NULL), fname);
        return;
    }

    LSM_DEBUG(DEB_L_C_F_PREFIX"Called with refresh set to %d",
              DEB_L_C_F_PREFIX_ARGS(LSM, dcb->line, dcb->call_id, fname), refresh);

    if (media == NULL) {
        /* NULL value of the given media indicates for all media */
        start_media = GSMSDP_FIRST_MEDIA_ENTRY(dcb);
        end_media   = NULL; /* NULL means till the end of the list */
    } else {
        /* given media, uses the provided media */
        start_media = media;
        end_media   = media;
    }

    /* close receive port on the media(s) */
    GSMSDP_FOR_MEDIA_LIST(media, start_media, end_media, dcb) {
        if (media->rcv_chan) {
            /*
             * If caller is releasing the port or if the caller is
             * recycling the receive port and the codec has changed, close the
             * receive port. Also stop bridging of media streams.
             */
            if (!refresh ||
                (refresh &&
                 gsmsdp_sdp_differs_from_previous_sdp(TRUE, media))) {
                LSM_DEBUG(get_debug_string(LSM_DBG_INT1), dcb->call_id,
                          dcb->line, fname, "port closed",
                          media->src_port);

                config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));
                if (!sdpmode) {

                    vcmRxClose(media->cap_index, dcb->group_id, media->refid,
                             lsm_get_ms_ui_call_handle(lcb->line, lcb->call_id, lcb->ui_id));
                }
                media->rcv_chan = FALSE;
            }
        }
    }
}

/**
 * The function closes transmit channel for a given media entry.
 * The transmit channel may not be closed if the caller intents to
 * fresh the port i.e close if needed but otherwise leave it open.
 * When the caller indicates refreshing, the transmit channel
 * will be closed only when there is a difference in current SDP and
 * the previous SDP.
 *
 * @param[in] lcb       - pointer to the lsm_lcb_t.
 * @param[in] refresh   - channel to be refreshed i.e. close if necessary.
 * @param[in] media     - pointer to the fsmdef_media_t for the
 *                        media entry to be refresh.
 *
 *                        If the value of media is NULL, it indicates that
 *                        all current inused media entries.
 *
 * @return              None.
 *
 * @pre (lcb is_not NULL)
 */
static void
lsm_close_tx (lsm_lcb_t *lcb, boolean refresh, fsmdef_media_t *media)
{
    fsmdef_media_t *start_media, *end_media;
    fsmdef_dcb_t   *dcb;
    static const char fname[] = "lsm_close_tx";
    int             sdpmode = 0;

    dcb = lcb->dcb;
    if (dcb == NULL) {
        LSM_ERR_MSG(get_debug_string(DEBUG_INPUT_NULL), fname);
        return;
    }
    LSM_DEBUG(DEB_L_C_F_PREFIX"called with refresh set to %d",
              DEB_L_C_F_PREFIX_ARGS(LSM, dcb->line, dcb->call_id, fname), refresh);

    config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));

    if (media == NULL) {
        /* NULL value of the given media indicates for all media */
        start_media = GSMSDP_FIRST_MEDIA_ENTRY(dcb);
        end_media   = NULL; /* NULL means till the end of the list */
    } else {
        /* given media, uses the provided media */
        start_media = media;
        end_media   = media;
    }

    /*
     * Close the RTP, but only if this call is using it and the capabilities
     * have changed.
     */
    GSMSDP_FOR_MEDIA_LIST(media, start_media, end_media, dcb) {
        if (media->xmit_chan == TRUE) {

            if (!refresh ||
                (refresh &&
                 gsmsdp_sdp_differs_from_previous_sdp(FALSE, media))) {

                if (!sdpmode) {
                    vcmTxClose(media->cap_index, dcb->group_id, media->refid,
                        lsm_get_ms_ui_call_handle(lcb->line, lcb->call_id, lcb->ui_id));
                }

                if (dcb->active_tone == VCM_MONITORWARNING_TONE || dcb->active_tone == VCM_RECORDERWARNING_TONE) {
                    LSM_DEBUG(DEB_L_C_F_PREFIX"%s: Found active_tone: %d being played, current monrec_tone_action: %d. Need stop tone.",
                              DEB_L_C_F_PREFIX_ARGS(LSM, dcb->line, dcb->call_id, fname), fname,
                              dcb->active_tone, dcb->monrec_tone_action);
                    (void) lsm_stop_tone(lcb, NULL);
                }
                media->xmit_chan = FALSE;
                LSM_DEBUG(DEB_L_C_F_PREFIX"closed",
                          DEB_L_C_F_PREFIX_ARGS(LSM, dcb->line, dcb->call_id, fname));
            }
        }
    }
}

/**
 * The function starts receive channel for a given media entry.
 *
 * @param[in] lcb       - pointer to the lsm_lcb_t.
 * @param[in] fname     - pointer to to const. char for the name
 *                        of the function that calls to this function.
 *                        It is for debuging purpose.
 * @param[in] media     - pointer to the fsmdef_media_t for the
 *                        media entry to be refresh.
 *
 *                        If the value of media is NULL, it indicates that
 *                        all current inused media entries.
 *
 * @return              None.
 *
 * @pre (lcb is_not NULL)
 */
static void
lsm_rx_start (lsm_lcb_t *lcb, const char *fname, fsmdef_media_t *media)
{
    static const char fname1[] = "lsm_rx_start";
    cc_action_data_open_rcv_t open_rcv;
    uint16_t port;
    groupid_t         group_id = CC_NO_GROUP_ID;
    callid_t          call_id  = lcb->call_id;
    vcm_mixing_mode_t mix_mode = VCM_NO_MIX;
    vcm_mixing_party_t mix_party = VCM_PARTY_NONE;
    int ret_val;
    fsmdef_media_t *start_media, *end_media;
    boolean has_checked_conference = FALSE;
    fsmdef_dcb_t   *dcb, *grp_id_dcb;
    vcm_mediaAttrs_t attrs;
    int              sdpmode = 0;
    int pc_stream_id = 0;
    int pc_track_id = 0;
    attrs.video.opaque = NULL;

    dcb = lcb->dcb;
    if (dcb == NULL) {
        LSM_ERR_MSG(get_debug_string(DEBUG_INPUT_NULL), fname1);
        return;
    }
    group_id = dcb->group_id;
    if (media == NULL) {
        /* NULL value of the given media indicates for all media */
        start_media = GSMSDP_FIRST_MEDIA_ENTRY(dcb);
        end_media   = NULL; /* NULL means till the end of the list */
    } else {
        /* given media, uses the provided media */
        start_media = media;
        end_media   = media;
    }

    /* Start receive channel for the media(s) */
    GSMSDP_FOR_MEDIA_LIST(media, start_media, end_media, dcb) {
        if (!GSMSDP_MEDIA_ENABLED(media)) {
            /* this entry is not enabled */
            continue;
        }

        /*
         * Check to see if the receive port can be opened.
         * For SRTP, the receive can not be opened if the remote's crypto
         * parameters are not received yet.
         */
        if (media->type != SDP_MEDIA_APPLICATION &&
            !gsmsdp_is_crypto_ready(media, TRUE)) {
            LSM_DEBUG(DEB_L_C_F_PREFIX"%s: Not ready to open receive port (%d)",
                      DEB_L_C_F_PREFIX_ARGS(LSM, dcb->line, dcb->call_id, fname1), fname, media->src_port);
            continue;
        }

        /* TODO(ekr@rtfm.com): Needs changing for when we
           have > 2 streams. (adam@nostrum.com): For now,
           we use all the same stream so pc_stream_id == 0
           and the tracks are assigned in order and are
           equal to the level in the media objects */
        pc_stream_id = 0;
        pc_track_id = media->level;

        /*
         * Open the RTP receive channel if it is not already open.
         */
        LSM_DEBUG(get_debug_string(LSM_DBG_INT1), dcb->call_id, dcb->line,
                  fname1, "rcv chan", media->rcv_chan);
        if (media->rcv_chan == FALSE) {

            memset(&open_rcv, 0, sizeof(open_rcv));
            port = media->src_port;

            if (media->is_multicast &&
                (media->direction == SDP_DIRECTION_RECVONLY)) {
                open_rcv.is_multicast = media->is_multicast;
                open_rcv.listen_ip    = media->dest_addr;
                port                  = media->multicast_port;
            }
            open_rcv.port = port;
            open_rcv.keep = TRUE;
            open_rcv.media_type = media->type;

            if (!has_checked_conference) {
                switch(dcb->session)
                {
                    case WHISPER_COACHING:
                        mix_mode  = VCM_MIX;
                        mix_party = VCM_PARTY_TxBOTH_RxNONE;
                        grp_id_dcb = fsmdef_get_dcb_by_call_id(dcb->join_call_id);
                        if (grp_id_dcb == NULL) {
                            LSM_ERR_MSG(get_debug_string(DEBUG_INPUT_NULL), fname1);
                        } else {
                            group_id = grp_id_dcb->group_id;
                        }
                        break;

                    case MONITOR:
                    case LOCAL_CONF:
                    	//AgentGreeting is MIX RXBOTH, SilentMonitoring is MIX TXBOTH
                    	//so we have to use VCM_PARTY_BOTH for case MONITOR
                        mix_mode  = VCM_MIX;
                        mix_party = VCM_PARTY_BOTH;
                        break;
                    case PRIMARY:
                    default:
                        mix_mode  = VCM_NO_MIX;
                        mix_party = VCM_PARTY_NONE;
                        break;
                }
                has_checked_conference = TRUE;
            }

            if (lsm_open_rx(lcb, &open_rcv, media) != CC_RC_SUCCESS) {
                LSM_ERR_MSG(LSM_L_C_F_PREFIX"%s: open receive port (%d) failed.",
                            dcb->line, dcb->call_id, fname1,
							fname, media->src_port);
            } else {
                /* successful open receive channel */
                media->rcv_chan = TRUE; /* recevied channel is created */
                /* save the source RX port */
                if (media->is_multicast) {
                    media->multicast_port = open_rcv.port;
                } else {
                    media->src_port = open_rcv.port;
                }

                if ( media->cap_index == CC_VIDEO_1 ) {
                    attrs.video.opaque = media->video;
                } else {
                    attrs.audio.packetization_period = media->packetization_period;
                    attrs.audio.max_packetization_period = media->max_packetization_period;
                    attrs.audio.avt_payload_type = media->avt_payload_type;
                    attrs.audio.mixing_mode = mix_mode;
                    attrs.audio.mixing_party = mix_party;
                }
                dcb->cur_video_avail &= ~CC_ATTRIB_CAST;

                config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));
                if (dcb->peerconnection) {
                    ret_val = vcmRxStartICE(media->cap_index, group_id, media->refid,
                    media->level,
                    pc_stream_id,
                    pc_track_id,
                    lsm_get_ms_ui_call_handle(dcb->line, call_id, CC_NO_CALL_ID),
                    dcb->peerconnection,
                    media->num_payloads,
                    media->payloads,
                    FSM_NEGOTIATED_CRYPTO_DIGEST_ALGORITHM(media),
                    FSM_NEGOTIATED_CRYPTO_DIGEST(media),
                    &attrs);
                } else if (!sdpmode) {
                    if (media->payloads == NULL) {
                        LSM_ERR_MSG(get_debug_string(DEBUG_INPUT_NULL), fname1);
                        return;
                    }
                    ret_val =  vcmRxStart(media->cap_index, group_id, media->refid,
                                          lsm_get_ms_ui_call_handle(dcb->line, call_id, CC_NO_CALL_ID),
                                          media->payloads,
                                          media->is_multicast ? &media->dest_addr:&media->src_addr,
                                          port,
                                          FSM_NEGOTIATED_CRYPTO_ALGORITHM_ID(media),
                                          FSM_NEGOTIATED_CRYPTO_RX_KEY(media),
                                          &attrs);
                    if (ret_val == -1) {
                        dcb->dsp_out_of_resources = TRUE;
                        return;
                    }
                } else {
                    ret_val = CC_RC_ERROR;
                }

                lsm_update_dscp_value(dcb);

                if (dcb->play_tone_action == FSMDEF_PLAYTONE_ZIP)
                {
                    vcm_tones_t tone = VCM_ZIP;
                    uint16_t    direction = dcb->tone_direction;

                    LSM_DEBUG(DEB_L_C_F_PREFIX"%s: Found play_tone_action: %d. Need to play tone.",
                              DEB_L_C_F_PREFIX_ARGS(LSM, dcb->line, dcb->call_id, fname), fname, dcb->play_tone_action);

                    // reset to initialized values
                    dcb->play_tone_action = FSMDEF_PLAYTONE_NO_ACTION;
                    dcb->tone_direction = VCM_PLAY_TONE_TO_EAR;

                    lsm_util_tone_start_with_speaker_as_backup(tone, VCM_ALERT_INFO_OFF,
                                                               lsm_get_ms_ui_call_handle(dcb->line, dcb->call_id, CC_NO_CALL_ID),
                                                               dcb->group_id,
                                                               ((media != NULL) ? media->refid : CC_NO_MEDIA_REF_ID),
                                                               direction);
                }
            }
        }

        if (media->type == SDP_MEDIA_APPLICATION) {
          /* Enable datachannels
             Datachannels are always two-way so initializing only here in rx_start.
          */
          lsm_initialize_datachannel(dcb, media, pc_track_id);
        }
    }
}

/**
 * The function starts transmit channel for a given media entry.
 *
 * @param[in] lcb       - pointer to the lsm_lcb_t.
 * @param[in] fname     - pointer to to const. char for the name
 *                        of the function that calls to this function.
 *                        It is for debuging purpose.
 * @param[in] media     - pointer to the fsmdef_media_t for the
 *                        media entry to be refresh.
 *
 *                        If the value of media is NULL, it indicates that
 *                        all current inused media entries.
 *
 * @return              None.
 *
 * @pre (lcb is_not NULL)
 */

#define LSM_TMP_VAD_LEN 64

static void
lsm_tx_start (lsm_lcb_t *lcb, const char *fname, fsmdef_media_t *media)
{
    static const char fname1[] = "lsm_tx_start";
    int           dscp = 184;   /* default 184 used for DSCP */
    char          tmp[LSM_TMP_VAD_LEN];
    fsmcnf_ccb_t *ccb = NULL;
    groupid_t         group_id;
    callid_t          call_id  = lcb->call_id;
    vcm_mixing_mode_t mix_mode = VCM_NO_MIX;
    vcm_mixing_party_t mix_party = VCM_PARTY_NONE;
    fsmdef_media_t *start_media, *end_media;
    boolean        has_checked_conference = FALSE;
    fsmdef_dcb_t   *dcb;
    vcm_mediaAttrs_t attrs;
    int              sdpmode;
    long strtol_result;
    char *strtol_end;

    attrs.video.opaque = NULL;

    dcb = lcb->dcb;
    if (dcb == NULL) {
        LSM_ERR_MSG(get_debug_string(DEBUG_INPUT_NULL), fname1);
        return;
    }
    // Set the DSCP value for RTP stream.
    if ( media != NULL ){
        // We would use DSCP for video for both audio and video streams if this
        // is a video call
        if ( dcb->cur_video_avail != SDP_DIRECTION_INACTIVE ) {
            config_get_value(CFGID_DSCP_VIDEO, (int *)&dscp, sizeof(dscp));
        } else if ( CC_IS_AUDIO(media->cap_index)){
            // audio stream for audio only call shall use the DSCP for audio
            // value.
            config_get_value(CFGID_DSCP_AUDIO, (int *)&dscp, sizeof(dscp));
        }
    }
    group_id = dcb->group_id;
    LSM_DEBUG(DEB_L_C_F_PREFIX"invoked", DEB_L_C_F_PREFIX_ARGS(LSM, dcb->line, dcb->call_id, fname1));

    if (media == NULL) {
        /* NULL value of the given media indicates for all media */
        start_media = GSMSDP_FIRST_MEDIA_ENTRY(dcb);
        end_media   = NULL; /* NULL means till the end of the list */
    } else {
        /* given media, uses the provided media */
        start_media = media;
        end_media   = media;
    }

    /* Start receive channel for the media(s) */
    GSMSDP_FOR_MEDIA_LIST(media, start_media, end_media, dcb) {
        if (!GSMSDP_MEDIA_ENABLED(media)) {
            /* this entry is not enabled */
            continue;
        }
        /*
         * Check to see if the transmit port can be opened.
         * For SRTP, the transmit port can not be opened if the remote's crypto
         * parameters are not received yet.
         */
        if (!gsmsdp_is_crypto_ready(media, FALSE)) {
            LSM_DEBUG(DEB_L_C_F_PREFIX"%s: Not ready to open transmit port",
                      DEB_L_C_F_PREFIX_ARGS(LSM, dcb->line, dcb->call_id, fname1), fname);
            continue;
        }
        if (media->xmit_chan == FALSE && dcb->remote_sdp_present &&
            media->dest_addr.type != CPR_IP_ADDR_INVALID && media->dest_port) {

            /* evaluate the mode and group id once for all media entries */
            if (!has_checked_conference) {
                switch(dcb->session)
                {
                    case WHISPER_COACHING:
                        mix_mode  = VCM_MIX;
                        mix_party = VCM_PARTY_TxBOTH_RxNONE;
                        group_id = fsmdef_get_dcb_by_call_id(dcb->join_call_id)->group_id;
                        break;

                    case MONITOR:
                    case LOCAL_CONF:
                    	//AgentGreeting is MIX RXBOTH, SilentMonitoring is MIX TXBOTH
                    	//so we have to use VCM_PARTY_BOTH for case MONITOR
                        mix_mode  = VCM_MIX;
                        mix_party = VCM_PARTY_BOTH;
                        break;
                    case PRIMARY:
                    default:
                        mix_mode  = VCM_NO_MIX;
                        mix_party = VCM_PARTY_NONE;
                        break;
                }
                has_checked_conference = TRUE;
            }

            /*
             *   Set the VAD value.
             */
            /* can't use vad on conference calls - the dsp can't handle it. */
            ccb = fsmcnf_get_ccb_by_call_id(lcb->call_id);
            if (ccb != NULL) {
                media->vad = VCM_VAD_OFF;
            } else {
                config_get_string(CFGID_ENABLE_VAD, tmp, sizeof(tmp));

                errno = 0;

                strtol_result = strtol(tmp, &strtol_end, 10);

                if (errno || tmp == strtol_end ||
                    strtol_result < VCM_VAD_OFF || strtol_result > VCM_VAD_ON) {
                    LSM_ERR_MSG("%s parse error of vad: %s", __FUNCTION__, tmp);
                    return;
                }

                media->vad = (vcm_vad_t) strtol_result;
            }

            /*
             * Open the transmit port and start sending, but only if we have
             * the SDP for the remote end.
             */

            sdpmode = 0;
        	config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));
        	if (!sdpmode) {

        		if (vcmTxOpen(media->cap_index, dcb->group_id, media->refid,
                            lsm_get_ms_ui_call_handle(lcb->line, lcb->call_id, lcb->ui_id)) != 0) {
        			LSM_DEBUG(DEB_L_C_F_PREFIX"%s: vcmTxOpen failed",
                          DEB_L_C_F_PREFIX_ARGS(LSM, dcb->line, dcb->call_id, fname1), fname);
        			continue;
            	}
        	}

            media->xmit_chan = TRUE;

            attrs.mute = FALSE;
            if ( CC_IS_VIDEO(media->cap_index)) {
                attrs.video.opaque = media->video;
                if (lcb->vid_mute) {
                    attrs.mute = TRUE;
                }

            } else if ( CC_IS_AUDIO(media->cap_index)){
                attrs.audio.packetization_period = media->packetization_period;
                attrs.audio.max_packetization_period = media->max_packetization_period;
                attrs.audio.avt_payload_type = media->avt_payload_type;
                attrs.audio.vad = media->vad;
                attrs.audio.mixing_mode = mix_mode;
                attrs.audio.mixing_party = mix_party;
            }

            dcb->cur_video_avail &= ~CC_ATTRIB_CAST;

            if (media->payloads == NULL) {
                LSM_ERR_MSG(get_debug_string(DEBUG_INPUT_NULL), fname1);
                return;
            }
            if (!strlen(dcb->peerconnection)){
              if (vcmTxStart(media->cap_index, group_id,
                  media->refid,
                  lsm_get_ms_ui_call_handle(dcb->line, call_id, CC_NO_CALL_ID),
                  media->payloads,
                  (short)dscp,
                  &media->src_addr,
                  media->src_port,
                  &media->dest_addr,
                  media->dest_port,
                  FSM_NEGOTIATED_CRYPTO_ALGORITHM_ID(media),
                  FSM_NEGOTIATED_CRYPTO_TX_KEY(media),
                  &attrs) == -1)
              {
                LSM_DEBUG(DEB_L_C_F_PREFIX"%s: vcmTxStart failed",
                  DEB_L_C_F_PREFIX_ARGS(LSM, dcb->line, dcb->call_id, fname1), fname);
                dcb->dsp_out_of_resources = TRUE;
                return;
              }
            }
            else {
              if (vcmTxStartICE(media->cap_index, group_id,
                  media->refid,
                  media->level,
                  /* TODO(emannion): his perhaps needs some error checking for validity.
                     See gsmsdp_get_media_cap_entry_by_index. */
                  dcb->media_cap_tbl->cap[media->cap_index].pc_stream,
                  dcb->media_cap_tbl->cap[media->cap_index].pc_track,
                  lsm_get_ms_ui_call_handle(dcb->line, call_id, CC_NO_CALL_ID),
                  dcb->peerconnection,
                  media->payloads,
                  (short)dscp,
                  FSM_NEGOTIATED_CRYPTO_DIGEST_ALGORITHM(media),
                  FSM_NEGOTIATED_CRYPTO_DIGEST(media),
                  &attrs) == -1)
              {
                LSM_DEBUG(DEB_L_C_F_PREFIX"%s: vcmTxStartICE failed",
                  DEB_L_C_F_PREFIX_ARGS(LSM, dcb->line, dcb->call_id, fname1), fname);
                dcb->dsp_out_of_resources = TRUE;
                return;
              }
            }

            lsm_update_dscp_value(dcb);

            LSM_DEBUG(DEB_L_C_F_PREFIX"%s: vcmTxStart started",
                  DEB_L_C_F_PREFIX_ARGS(LSM, dcb->line, dcb->call_id, fname1), fname);

            if ( dcb->monrec_tone_action != FSMDEF_MRTONE_NO_ACTION)
            {
                vcm_tones_t tone = VCM_NO_TONE;
                uint16_t    direction = VCM_PLAY_TONE_TO_EAR;
                boolean     play_both_tones = FALSE;

                LSM_DEBUG(DEB_L_C_F_PREFIX"%s: Found monrec_tone_action: %d. Need to restart playing tone.",
                          DEB_L_C_F_PREFIX_ARGS(LSM, dcb->line, dcb->call_id, fname), fname, dcb->monrec_tone_action);

                switch (dcb->monrec_tone_action) {
                    case FSMDEF_MRTONE_RESUME_MONITOR_TONE:
                        tone = VCM_MONITORWARNING_TONE;
                        direction = dcb->monitor_tone_direction;
                        break;

                    case FSMDEF_MRTONE_RESUME_RECORDER_TONE:
                        tone = VCM_RECORDERWARNING_TONE;
                        direction = dcb->recorder_tone_direction;
                        break;

                    case FSMDEF_MRTONE_RESUME_BOTH_TONES:
                        play_both_tones = TRUE;
                        tone = VCM_MONITORWARNING_TONE;
                        direction = dcb->monitor_tone_direction;
                        break;

                    default:
                        break;
                }

                if (play_both_tones == TRUE) {
                    lsm_util_tone_start_with_speaker_as_backup(VCM_RECORDERWARNING_TONE, VCM_ALERT_INFO_OFF,
                                                          lsm_get_ms_ui_call_handle(dcb->line, dcb->call_id, CC_NO_CALL_ID),
                                                          dcb->group_id,
                                                          ((media != NULL) ? media->refid : CC_NO_MEDIA_REF_ID),
                                                          dcb->recorder_tone_direction);
                }

                lsm_util_tone_start_with_speaker_as_backup(tone, VCM_ALERT_INFO_OFF, lsm_get_ms_ui_call_handle(dcb->line, dcb->call_id, CC_NO_CALL_ID),
                                                    dcb->group_id,
                                                    ((media != NULL) ? media->refid : CC_NO_MEDIA_REF_ID),
                                                    direction);
            }
        }
    }
}


static cc_rcs_t
lsm_start_tone (lsm_lcb_t *lcb, cc_action_data_tone_t *data)
{
    callid_t call_id = lcb->call_id;
    fsmdef_media_t *media;

    if (lcb->dcb == NULL) {
        /* No dcb to work with */
        return (CC_RC_ERROR);
    }
    media = gsmsdp_find_audio_media(lcb->dcb);

    lsm_util_start_tone(data->tone, VCM_ALERT_INFO_OFF, lsm_get_ms_ui_call_handle(lcb->line, call_id, CC_NO_CALL_ID), lcb->dcb->group_id,
                   ((media != NULL) ? media->refid : CC_NO_MEDIA_REF_ID),
                   VCM_PLAY_TONE_TO_EAR);

    return (CC_RC_SUCCESS);
}

static cc_rcs_t
lsm_stop_tone (lsm_lcb_t *lcb, cc_action_data_tone_t *data)
{
    /* NOTE: For now, ignore data input parameter that may contain the tone type.
     *       We'll check active_tone in the dcb for the call_id and see if there
     *       is a valid tone playing. If so, then and only then issue tone stop.
     */
    static const char fname[] = "lsm_stop_tone";
    callid_t      call_id;
    fsmdef_dcb_t *dcb;

    if (lcb == NULL) {
        LSM_DEBUG(DEB_F_PREFIX"NULL lcb passed", DEB_F_PREFIX_ARGS(LSM, fname));
        return (CC_RC_ERROR);
    }
    call_id = lcb->call_id;

    dcb = lcb->dcb;
    if (dcb == NULL) {
        LSM_DEBUG(DEB_F_PREFIX" NULL dcb passed for call_id = %d", DEB_F_PREFIX_ARGS(LSM, fname), call_id);
        return (CC_RC_ERROR);
    }

    /* for tnp do call stop only if active_tone is other than VCM_NO_TONE */
    if (dcb->active_tone != VCM_NO_TONE) {
            fsmdef_media_t *media = gsmsdp_find_audio_media(lcb->dcb);
            vcmToneStop(dcb->active_tone, dcb->group_id,
                          ((media != NULL) ? media->refid : CC_NO_MEDIA_REF_ID),
                                  lsm_get_ms_ui_call_handle(lcb->line, lcb->call_id, lcb->ui_id));
        /*
         * Both periodic tones, recording and monitoring, can be active at the
         * same time. And because we only keep track of last tone, requested to
         * play, through active_tone, so when the tone to be stopped is of
         * periodic type, then it could be that both type of periodic tones
         * could be playing and both should be stopped. If the second periodic
         * tone is not playing then media server will ignore the stop request.
         */
        if (dcb->active_tone == VCM_RECORDERWARNING_TONE ||
        dcb->active_tone == VCM_MONITORWARNING_TONE)
        {
            vcmToneStop(dcb->active_tone == VCM_RECORDERWARNING_TONE ?
                VCM_MONITORWARNING_TONE : VCM_RECORDERWARNING_TONE,
                dcb->group_id,
                ((media != NULL) ? media->refid : CC_NO_MEDIA_REF_ID),
                lsm_get_ms_ui_call_handle(lcb->line, lcb->call_id, lcb->ui_id));

            /* in case need to play back the tone again when tx channel active */
            switch (dcb->monrec_tone_action) {
                case FSMDEF_MRTONE_PLAYED_MONITOR_TONE:
                    dcb->monrec_tone_action = FSMDEF_MRTONE_RESUME_MONITOR_TONE;
                    break;

                case FSMDEF_MRTONE_PLAYED_RECORDER_TONE:
                    dcb->monrec_tone_action = FSMDEF_MRTONE_RESUME_RECORDER_TONE;
                    break;

                case FSMDEF_MRTONE_PLAYED_BOTH_TONES:
                    dcb->monrec_tone_action = FSMDEF_MRTONE_RESUME_BOTH_TONES;
                    break;

                default:
                    break;
            }

            LSM_DEBUG(DEB_L_C_F_PREFIX"%s: Setting monrec_tone_action: %d so resume to play correct tone.",
                              DEB_L_C_F_PREFIX_ARGS(LSM, dcb->line, dcb->call_id, fname), fname,
			                  dcb->monrec_tone_action);
        }
        dcb->active_tone = VCM_NO_TONE;
    } else {
        LSM_DEBUG(DEB_L_C_F_PREFIX"Ignoring tone stop request",
                  DEB_L_C_F_PREFIX_ARGS(LSM, dcb->line, call_id, fname));
    }

    return (CC_RC_SUCCESS);
}

/*
 * Function
 *
 * @param[in] tone       - tone type
 * @param[in] alert_info - alertinfo header
 * @param[in] call_handle- call handle
 * @param[in] direction  - network, speaker, both
 * @param[in] duration   - length of time for tone to be played
 *
 * @return none
 */
void
lsm_tone_start_with_duration (vcm_tones_t tone, short alert_info,
                              cc_call_handle_t call_handle, groupid_t group_id,
                              streamid_t stream_id, uint16_t direction,
				    		  uint32_t duration)
{

    static const char *fname = "lsm_tone_start_with_duration";

    DEF_DEBUG(DEB_L_C_F_PREFIX"tone=%-2d: direction=%-2d duration=%-2d",
              DEB_L_C_F_PREFIX_ARGS(LSM, GET_LINE_ID(call_handle), GET_CALL_ID(call_handle), fname),
              tone, direction, duration);

    /*
     * play the tone. audio path is always set by MSUI module.
     */
    vcmToneStart (tone, alert_info, call_handle, group_id, stream_id, direction);

    lsm_update_active_tone (tone, GET_CALL_ID(call_handle));

    lsm_start_tone_duration_timer (tone, duration, call_handle);
}

/*
 * Function: lsm_get_used_instances_cnt
 *
 * @param line - line number
 *
 * Description: find the number of used instances for this particular line
 *
 * @return number of used instances
 *
 */
int lsm_get_used_instances_cnt (line_t line)
{
    static const char fname[] = "lsm_get_used_instances_cnt";
    int             used_instances = 0;
    lsm_lcb_t      *lcb;

    if (!sip_config_check_line(line)) {
        LSM_ERR_MSG(LSM_F_PREFIX"invalid line (%d)", fname, line);

        return (-1);
    }

    /*
     * Count home many instances are already in use for this particular line.
     */
    FSM_FOR_ALL_CBS(lcb, lsm_lcbs, LSM_MAX_LCBS) {
        if ((lcb->call_id != CC_NO_CALL_ID) &&
            (lcb->line == line) &&
            (lcb->state != LSM_S_IDLE)) {
            used_instances++;
        }
    }

    return (used_instances);
}

/*
 * Function: lsm_get_all_used_instances_cnt
 *
 * Description: find the number of used instances for all lines
 *
 * @return number of used instances for all lines
 *
 */
int lsm_get_all_used_instances_cnt ()
{
    int             used_instances = 0;
    lsm_lcb_t      *lcb;

    /*
     * Count home many instances are already in use for all lines.
     */
    FSM_FOR_ALL_CBS(lcb, lsm_lcbs, LSM_MAX_LCBS) {
        if ((lcb->call_id != CC_NO_CALL_ID) &&
            (lcb->state != LSM_S_IDLE)) {
            used_instances++;
        }
    }

    return (used_instances);
}

/*
 * Function: lsm_increment_call_chn_cnt
 *
 * @param line - line number
 *
 * Description:
 *
 * @return none
 *
 */
void lsm_increment_call_chn_cnt (line_t line)
{
    if ( line <=0 || line > MAX_REG_LINES ) {
        LSM_ERR_MSG(LSM_F_PREFIX"invalid line (%d)", __FUNCTION__, line);
        return;
    }
    lsm_call_perline[line-1]++;

    LSM_DEBUG(DEB_F_PREFIX"number of calls on line[%d]=%d",
        DEB_F_PREFIX_ARGS(LSM, __FUNCTION__),
        line, lsm_call_perline[line-1]);
}

/*
 * Function: lsm_decrement_call_chn_cnt
 *
 * @param line - line number
 *
 * Description:
 *
 * @return none
 *
 */
void lsm_decrement_call_chn_cnt (line_t line)
{
    if ( line <=0 || line > MAX_REG_LINES ) {
        LSM_ERR_MSG(LSM_F_PREFIX"invalid line (%d)", __FUNCTION__, line);
        return;
    }

    lsm_call_perline[line-1]--;

    LSM_DEBUG(DEB_F_PREFIX"number of calls on line[%d]=%d",
        DEB_F_PREFIX_ARGS(LSM, __FUNCTION__),
        line, lsm_call_perline[line-1]);
}

#define NO_ROLLOVER 0
#define ROLLOVER_ACROSS_SAME_DN 1
#define ROLLOVER_NEXT_AVAILABLE_LINE 2

/*
 * Function: lsm_find_next_available_line
 *
 * @param line - line number
 * @param same_dn - whether lines with same DN to be looked at.
 * @param incoming - whether we are looking for an available line for an anticipated incoming call.
 *
 * Description:
 *
 *
 * @return found line number
 *
 */
line_t lsm_find_next_available_line (line_t line, boolean same_dn, boolean incoming)
{
    char current_line_dn_name[MAX_LINE_NAME_SIZE];
    char dn_name[MAX_LINE_NAME_SIZE];
    uint32_t line_feature;
    line_t  i, j;

    config_get_line_string(CFGID_LINE_NAME, current_line_dn_name, line, sizeof(current_line_dn_name));
    /* This line has exhausted its  limit, start rollover */
    /* First, search the lines on top of the current one */
    for (i=line+1; i <= MAX_REG_LINES; i++) {
        config_get_line_value(CFGID_LINE_FEATURE, &line_feature, sizeof(line_feature), i);

        /* if it is not a DN, skip it */
        if (line_feature != cfgLineFeatureDN) {
            continue;
        }

        if (same_dn == TRUE) {
            config_get_line_string(CFGID_LINE_NAME, dn_name, i, sizeof(dn_name));
            /* Does this line have the same DN */
            if (cpr_strcasecmp(dn_name, current_line_dn_name) == 0) {
                return (i);
            }
        } else {
            return (i);
        }
    }
    /*
     * We went up to the top and couldn't find an available line,
     * start from line 1 and search up to the current line, thus
     * we are treating the available lines as a circular pool
     */

    for (j=1; j <= line; j++) {
        config_get_line_value(CFGID_LINE_FEATURE, &line_feature, sizeof(line_feature), j);

        /* if it is not a DN, skip it */
        if (line_feature != cfgLineFeatureDN) {
            continue;
        }

        if (same_dn == TRUE) {
            config_get_line_string(CFGID_LINE_NAME, dn_name, j, sizeof(dn_name));
            /* Does this line have the same DN */
            if (cpr_strcasecmp(dn_name, current_line_dn_name) == 0) {
                return (j);
            }
        } else {
            return (j);
        }
    }

    return (NO_LINES_AVAILABLE);
}
/*
 * Function: lsm_get_newcall_line
 *
 * @param line - line number
 *
 * Description: find out a line that has room to make a
 *              new call based on the rollover settings
 *
 * @return found line number
 *
 */
line_t lsm_get_newcall_line (line_t line)
{
    return line;
}

/*
 * Function: lsm_get_available_line
 *
 * @param incoming - whether we are looking for an available line for an anticipated incoming call.
 *
 * Description: find out a line that has room to make a
 *              new call starting from the first line.
 *
 * @return found line number
 *
 */
line_t lsm_get_available_line (boolean incoming)
{
    return 1;
}

/*
 * Function: lsm_is_line_available_for_outgoing_call
 *
 * @param line
 * @param incoming - whether we are looking for an available line for an anticipated incoming call.
 *
 * Description: find out if the line  has room to make a new call
 *
 * @return TRUE/FALSE
 *
 */
boolean lsm_is_line_available (line_t line, boolean incoming)
{
    return TRUE;
}

/*
 * lsm_get_instances_available_cnt
 *
 * return the number of available instances for this particular line
 *
 * NOTE: The function can return negative values, which the user should read
 *       as no available lines.
 */
int
lsm_get_instances_available_cnt (line_t line, boolean expline)
{
    static const char fname[] = "lsm_get_instances_available_cnt";
    int             max_instances;
    int             used_instances = 0;
    int             free_instances;

    if (!sip_config_check_line(line)) {
        LSM_ERR_MSG(LSM_F_PREFIX"invalid line (%d)", fname, line);

        return (-1);
    }

    used_instances = lsm_get_used_instances_cnt(line);

    max_instances = (expline) ? (LSM_MAX_EXP_INSTANCES) : (LSM_MAX_INSTANCES);

    free_instances = max_instances - used_instances;

    if(free_instances > 0){
         int all_used_instances = lsm_get_all_used_instances_cnt();
         int all_max_instances = (expline) ? (LSM_MAX_CALLS) : (LSM_MAX_CALLS - 1);
         int all_free_instances = all_max_instances - all_used_instances;
         free_instances = ((free_instances < all_free_instances) ? free_instances : all_free_instances);
         LSM_DEBUG("lsm_get_instances_available_cnt: line=%d, expline=%d, free=%d, all_used=%d, all_max=%d, all_free=%d",
         	line, expline, free_instances, all_used_instances, all_max_instances, all_free_instances);

    }
    LSM_DEBUG("lsm_get_instances_available_cnt: line=%d, expline=%d, free_instances=%d",
         	line, expline, free_instances);
    return (free_instances);
}


static void
lsm_init_lcb (lsm_lcb_t *lcb)
{
    lcb->call_id  = CC_NO_CALL_ID;
    lcb->line     = LSM_NO_LINE;
    lcb->previous_call_event = evMaxEvent;
    lcb->state    = LSM_S_IDLE;
    lcb->mru      = 0;
    lcb->enable_ringback = TRUE;
    lcb->flags    = 0;
    lcb->dcb      = NULL;
    lcb->gcid     = NULL;
    lcb->vid_flags = 0; //set to not visible
    lcb->ui_id    = CC_NO_CALL_ID;
}

/**
 * Return the port back to Media service component
 * @param [in] lcb - lsm control block
 *
 */
static void lsm_release_port (lsm_lcb_t *lcb)
{
    static const char fname[] = "lsm_release_port";
    fsmdef_media_t *start_media, *end_media;
    fsmdef_dcb_t   *dcb;
    fsmdef_media_t *media;
    int            sdpmode = 0;

    dcb = lcb->dcb;
    if (dcb == NULL) {
        LSM_ERR_MSG(get_debug_string(DEBUG_INPUT_NULL), fname);
        return;
    }

    config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));

    LSM_DEBUG(DEB_L_C_F_PREFIX,
              DEB_L_C_F_PREFIX_ARGS(LSM, dcb->line, dcb->call_id, fname));

    start_media = GSMSDP_FIRST_MEDIA_ENTRY(dcb);
    end_media   = NULL; /* NULL means till the end of the list */

    GSMSDP_FOR_MEDIA_LIST(media, start_media, end_media, dcb) {
        if (!sdpmode) {
            vcmRxReleasePort(media->cap_index, dcb->group_id, media->refid,
                            lsm_get_ms_ui_call_handle(lcb->line, lcb->call_id, lcb->ui_id), media->src_port);
        }
    }
}

static void
lsm_free_lcb (lsm_lcb_t *lcb)
{
    lsm_release_port(lcb);
    cpr_free(lcb->gcid);
    lsm_init_lcb(lcb);
}


/**
 * lsm_get_free_lcb
 * return a free instance of the given line
 *
 * @param[in]call_id - gsm call id to allocate the lcb instance with.
 * @param[in]line    - line that the lcb instnce will be associated with
 * @param[in]dcb     - fsmdef_dcb_t structure that the lcb instance will
 *                     be associated with.
 * @return pointer to lsm_lcb_t if there is an available lcb otherwise
 *         returns NULL.
 * @pre    (dcb not_eq NULL)
 */
static lsm_lcb_t *
lsm_get_free_lcb (callid_t call_id, line_t line, fsmdef_dcb_t *dcb)
{
    static const char fname[] = "lsm_get_free_lcb";
    static int      mru = 0;
    lsm_lcb_t      *lcb;
    lsm_lcb_t      *lcb_found = NULL;

    if (!sip_config_check_line(line)) {
        LSM_ERR_MSG(LSM_F_PREFIX"invalid line (%d)", fname, line);

        return (NULL);
    }


    /*
     * Set mru (most recently used).
     * Used to determine which call came in first.
     */
    if (++mru < 0) {
        mru = 1;
    }

    /*
     * Find a free lcb.
     */
    FSM_FOR_ALL_CBS(lcb, lsm_lcbs, LSM_MAX_LCBS) {
        if ((lcb->call_id == CC_NO_CALL_ID) && (lcb->state == LSM_S_IDLE)) {
            lcb_found    = lcb;
            lcb->call_id = call_id;
            lcb->line    = line;
            lcb->state   = LSM_S_PENDING;
            lcb->mru     = mru;
            lcb->dcb     = dcb;
            // start unmuted if txPref is true
            lcb->vid_mute = cc_media_getVideoAutoTxPref() ? FALSE : TRUE;

            lcb->ui_id = call_id;   /* default UI ID is the same as call_id */
            break;
        }
    }

    return (lcb_found);
}


lsm_lcb_t *
lsm_get_lcb_by_call_id (callid_t call_id)
{
    lsm_lcb_t *lcb;
    lsm_lcb_t *lcb_found = NULL;
    LSM_DEBUG(DEB_L_C_F_PREFIX"call_id=%d.",
              DEB_L_C_F_PREFIX_ARGS(LSM, 0, call_id, "lsm_get_lcb_by_call_id"), call_id);

    FSM_FOR_ALL_CBS(lcb, lsm_lcbs, LSM_MAX_LCBS) {
        if (lcb->call_id == call_id) {
            lcb_found = lcb;
            break;
        }
    }

    return (lcb_found);
}

/**
 * This function returns the LSM state for the given call_id.
 *
 * @param[in] call_id -  call id
 *
 * @return            lsm_states_t of the given call_id. If the
 *                    there is no call associated with the given
 *                    call ID it returns the LSM_S_NONE.
 */
lsm_states_t
lsm_get_state (callid_t call_id)
{
    lsm_lcb_t *lcb;

    lcb = lsm_get_lcb_by_call_id(call_id);

    if (lcb == NULL) {
        /* there is no call for this call id */
        return (LSM_S_NONE);
    }
    return (lcb->state);
}

static void
lsm_change_state (lsm_lcb_t *lcb, int line_num, lsm_states_t new_state)
{
    static const char fname1[] = "lsm_change_state";
    LSM_DEBUG(DEB_L_C_F_PREFIX"%d: %s -> %s",
			  DEB_L_C_F_PREFIX_ARGS(LSM, lcb->line, lcb->call_id, fname1),
              line_num, lsm_state_name(lcb->state), lsm_state_name(new_state));

    lcb->state = new_state;
}

boolean
lsm_is_phone_idle (void)
{
	static const char fname[] = "lsm_is_phone_idle";
    boolean         idle = TRUE;
    lsm_lcb_t      *lcb;

	if(!lsm_lcbs){
		LSM_DEBUG(DEB_F_PREFIX"No lsm line cb", DEB_F_PREFIX_ARGS(LSM, fname));
		return (idle);
	}

    FSM_FOR_ALL_CBS(lcb, lsm_lcbs, LSM_MAX_LCBS) {
        if ((lcb->call_id != CC_NO_CALL_ID) && (lcb->state != LSM_S_IDLE)) {
            idle = FALSE;
            break;
        }
    }

    return (idle);
}



/*
 *  Function: lsm_is_phone_inactive
 *
 *  Parameters: None.
 *
 *  Description: Determines if the phone is inactive. Inactive means the phone
 *               as active at some point, but now it is not - there are still
 *               calls on the phone but they are probably in a holding state.
 *               This is different from idle, which means that there are not
 *               any calls on the phone.
 *
 *  Returns:
 *      inactive: FALSE: phone is not inactive
 *                TRUE:  phone is inactive
 */
boolean
lsm_is_phone_inactive (void)
{
    boolean         inactive = TRUE;
    lsm_lcb_t      *lcb;

    FSM_FOR_ALL_CBS(lcb, lsm_lcbs, LSM_MAX_LCBS) {
        if ((lcb->call_id != CC_NO_CALL_ID) &&
            ((lcb->state == LSM_S_OFFHOOK) ||
             (lcb->state == LSM_S_PENDING) ||
             (lcb->state == LSM_S_PROCEED) ||
             (lcb->state == LSM_S_RINGOUT) ||
             (lcb->state == LSM_S_RINGIN)  ||
             (lcb->state == LSM_S_CONNECTED))) {
            inactive = FALSE;
            break;
        }
    }

    return (inactive);
}

/*
 *  Function: lsm_callwaiting
 *
 *  Parameters: None.
 *
 *  Description: Determines if the phone is in a state that this
 *               call will be handled by the callwaiting code. TNP
 *               phones allow call-waiting when dialing digits while
 *               the legacy phones do not.
 *
 *  Returns:
 *      inactive: FALSE: Treat as a normal call on an idle phone
 *                TRUE:  Display incoming call and play call waiting tone
 */
boolean
lsm_callwaiting (void)
{
    lsm_lcb_t *lcb;

    FSM_FOR_ALL_CBS(lcb, lsm_lcbs, LSM_MAX_LCBS) {
        if (lcb->call_id != CC_NO_CALL_ID) {
            switch (lcb->state) {
            case LSM_S_OFFHOOK:
            case LSM_S_PROCEED:
            case LSM_S_RINGOUT:
            case LSM_S_CONNECTED:
                return (TRUE);

            default:
                break;
            }
        }
    }

    return (FALSE);
}

static callid_t
lsm_find_state (lsm_states_t state)
{
    callid_t        found_callid = CC_NO_CALL_ID;
    lsm_lcb_t      *lcb;

    FSM_FOR_ALL_CBS(lcb, lsm_lcbs, LSM_MAX_LCBS) {
        if ((lcb->call_id != CC_NO_CALL_ID) && (lcb->state == state)) {
            found_callid = lcb->call_id;
            break;
        }
    }

    return (found_callid);
}

/**
 * lsm_get_facility_by_called_number
 * return facility by the given called_number.
 *
 * @param[in]call_id            - gsm's call_id for a new call.
 * @param[in]called_number      - pointer to the called number.
 * @paran[in/out]free_line      - pointer to the line_t to store
 *                                the result line number corresponding
 *                                to the called number given.
 * @param[in]expline            - boolean indicating extra instance
 *                                is needed.
 * @param[in]dcb                - pointer to void but it must be
 *                                a pointer to fsmdef_dcb_t to bind with
 *                                the new LCB. The reason to use a void
 *                                pointer is the declaration of the function
 *                                is in lsm.h. The lsm.h file is used by
 *                                components outside gsm environment. Those
 *                                modules would need to include the fsm.h
 *                                which is not desirable. Using void pointer
 *                                avoids this problem.
 *
 * @return cc_cause_t
 *
 * @pre    (called_number not_eq NULL)
 * @pre    (free_line not_eq NULL)
 * @pre    (dcb not_eq NULL)
 */
cc_causes_t
lsm_get_facility_by_called_number (callid_t call_id,
                                   const char *called_number,
                                   line_t *free_line, boolean expline,
                                   void *dcb)
{
    static const char fname[] = "lsm_get_facility_by_called_number";
    line_t     line;
    lsm_lcb_t *lcb;
    int        free_instances;
    line_t     madn_line;

    lsm_debug_entry(call_id, 0, fname);
    LSM_DEBUG(DEB_F_PREFIX"called_number= %s", DEB_F_PREFIX_ARGS(LSM, fname), called_number);

    //line = sip_config_get_line_by_called_number(1, called_number);
    line = 1;
    if (line == 0) {
        return (CC_CAUSE_UNASSIGNED_NUM);
    }
    *free_line = line;

    /* check for a MADN line */
    madn_line = sip_config_get_line_by_called_number((line_t)(line + 1),
                                              called_number);

    /*
     * Check to see if we even have any available instances.
     */
    free_instances = lsm_get_instances_available_cnt(line, expline);

    /* if it is a MADN line and it already has a call, then go to next
     * line with this MADN number.
     */
    if ((madn_line) && (free_instances < 2)) {
        while (madn_line) {
            free_instances = lsm_get_instances_available_cnt(madn_line, expline);
            if (free_instances == 2) {
                *free_line = line = madn_line;
                break;
            }
            madn_line = sip_config_get_line_by_called_number((line_t)(madn_line + 1),
                                                      called_number);
        }
        if (madn_line == 0) {
            return (CC_CAUSE_BUSY);
        }
    }

    if (free_instances <= 0) {
        return (CC_CAUSE_BUSY);
    }

    lcb = lsm_get_free_lcb(call_id, line, (fsmdef_dcb_t *)dcb);
    if (lcb == NULL) {
        return (CC_CAUSE_NO_RESOURCE);
    }

    return (CC_CAUSE_OK);
}

/**
 * lsm_allocate_call_bandwidth
 *
 * @param[in] none.
 *
 * The wlan interface puts into unique situation where call control
 * has to allocate the worst case bandwith before creating a
 * inbound or outbound call. The function call will interface through
 * media API into wlan to get the call bandwidth. The function
 * return is asynchronous and will block till the return media
 * callback signals to continue the execution.
 *
 * @return true if the bandwidth can be allocated else false.
 * @pre    none
 */

cc_causes_t lsm_allocate_call_bandwidth (callid_t call_id, int sessions)
{
    //get line for vcm
    line_t line = lsm_get_line_by_call_id(call_id);
    //cc_feature(CC_SRC_GSM, call_id, 0, CC_FEATURE_CAC_RESP_PASS, NULL);

    /* Activate the wlan before allocating bandwidth */
    vcmActivateWlan(TRUE);

    if (vcmAllocateBandwidth(lsm_get_ms_ui_call_handle(line, call_id, CC_NO_CALL_ID), sessions)) {
        return(CC_CAUSE_OK);
    }

    return(CC_CAUSE_CONGESTION);
}

/**
 * lsm_get_facility_by_line
 * return facility by the given line
 *
 * @param[in]call_id  - gsm's call_id for a new call.
 * @param[in]line     - line
 * @param[in]expline  - boolean indicating extra instance
 *                      is needed.
 * @param[in]dcb      - pointer to void but it must be
 *                      a pointer to fsmdef_dcb_t to bind with
 *                      the new LCB. The reason to use a void
 *                      pointer is the declaration of the function
 *                      is in lsm.h. The lsm.h file is used by
 *                      components outside gsm environment. Those
 *                      modules would need to include the fsm.h
 *                      which is not desirable. Using void pointer
 *                      avoids this problem.
 *
 * @return cc_cause_t
 * @pre    (dcb not_eq NULL)
 */
cc_causes_t
lsm_get_facility_by_line (callid_t call_id, line_t line, boolean expline,
                          void *dcb)
{
    static const char fname[] = "lsm_get_facility_by_line";
    lsm_lcb_t      *lcb;
    int             free_instances;

    LSM_DEBUG(get_debug_string(LSM_DBG_INT1), call_id, line, fname,
              "exp", expline);

    /*
     * Check to see if we even have any available instances
     */
    free_instances = lsm_get_instances_available_cnt(line, expline);
    if (free_instances <= 0) {
        return (CC_CAUSE_BUSY);
    }

    lcb = lsm_get_free_lcb(call_id, line, (fsmdef_dcb_t *)dcb);
    if (lcb == NULL) {
        return (CC_CAUSE_NO_RESOURCE);
    }

    return (CC_CAUSE_OK);
}


#ifdef _WIN32
/* This function enumerates over the lcbs
 * and attempts to terminate the call
 * This is used by softphone when
 * it exits and the softphone is
 * still engaged in a call
 */
void
terminate_active_calls (void)
{
    callid_t        call_id = CC_NO_CALL_ID;
    lsm_lcb_t      *lcb;
    line_t          line;

    FSM_FOR_ALL_CBS(lcb, lsm_lcbs, LSM_MAX_LCBS) {
        if (lcb->call_id != CC_NO_CALL_ID) {
            line = lsm_get_line_by_call_id(lcb->call_id);
            /* Currently cc_feature does a better job of releasing the call
             * compared to cc_onhook.
             */
            //cc_onhook(CC_SRC_UI, call_id, line);
            cc_feature(CC_SRC_UI, call_id, line, CC_FEATURE_END_CALL, NULL);
            call_id = lcb->call_id;
        }
    }
}


#endif

line_t
lsm_get_line_by_call_id (callid_t call_id)
{
    fsmdef_dcb_t   *dcb;
    line_t          line;

    dcb = fsmdef_get_dcb_by_call_id(call_id);

    if (dcb != NULL) {
        line = dcb->line;
    } else {
        line = LSM_DEFAULT_LINE;
    }

    return (line);
}

/*
 * This is a callback function for those tones that are
 * played in two parts (stutter and msgwaiting) or played
 * every x seconds, but are not steady tones (call waiting).
 *
 * @param[in] data    The gsm ID (callid_t) of the call of the
 *                    tones timer has timeout.
 *
 * @return            N/A
 */
void
lsm_tmr_tones_callback (void *data)
{
    static const char fname[] = "lsm_tmr_tones_callback";
    callid_t     call_id;
    fsmdef_dcb_t *dcb = NULL;
    fsmdef_media_t *media;

    LSM_DEBUG(DEB_F_PREFIX"invoked", DEB_F_PREFIX_ARGS(LSM, fname));

    call_id = (callid_t)(long)data;
    if (call_id == CC_NO_CALL_ID) {
        /* Invalid call id */
        LSM_DEBUG(DEB_F_PREFIX"invalid call id", DEB_F_PREFIX_ARGS(LSM, fname));
        return;
    }

    /*
     * A call-waiting tone should be played if these conditions are met:
     * 1. A line must be ringing for an incoming call
     * 2. The phone must be in a state that we handle callwaiting
     */
    /* Retrieve dcb from call id */
    dcb = fsmdef_get_dcb_by_call_id(call_id);
    if (dcb == NULL) {
        LSM_DEBUG(DEB_F_PREFIX"no dcb found for call_id %d", DEB_F_PREFIX_ARGS(LSM, fname), call_id);
        return;
    }

    media = gsmsdp_find_audio_media(dcb);

    if ((lsm_find_state(LSM_S_RINGIN) > CC_NO_CALL_ID) && (lsm_callwaiting())) {

            /* Determine what tone/ringing pattern to play */
            switch (dcb->alert_info) {

            case ALERTING_RING:

                /* Need to map the alerting patterns to the call waiting patterns */
                switch (dcb->alerting_ring) {
                case VCM_BELLCORE_DR2:
                    lsm_util_start_tone(VCM_CALL_WAITING_2_TONE, NO, lsm_get_ms_ui_call_handle(dcb->line, dcb->call_id, CC_NO_CALL_ID), dcb->group_id,
                                   ((media != NULL) ? media->refid : CC_NO_MEDIA_REF_ID),
                                   dcb->tone_direction);
                    break;
                case VCM_BELLCORE_DR3:
                    lsm_util_start_tone(VCM_CALL_WAITING_3_TONE, NO, lsm_get_ms_ui_call_handle(dcb->line, dcb->call_id, CC_NO_CALL_ID), dcb->group_id,
                                   ((media != NULL) ? media->refid : CC_NO_MEDIA_REF_ID),
                                   dcb->tone_direction);
                    break;
                case VCM_BELLCORE_DR4:
                    lsm_util_start_tone(VCM_CALL_WAITING_4_TONE, NO, lsm_get_ms_ui_call_handle(dcb->line, dcb->call_id, CC_NO_CALL_ID), dcb->group_id,
                                   ((media != NULL) ? media->refid : CC_NO_MEDIA_REF_ID),
                                   dcb->tone_direction);
                    break;
                default:
                    lsm_util_start_tone(VCM_CALL_WAITING_TONE, NO, lsm_get_ms_ui_call_handle(dcb->line, dcb->call_id, CC_NO_CALL_ID), dcb->group_id,
                                   ((media != NULL) ? media->refid : CC_NO_MEDIA_REF_ID),
                                   dcb->tone_direction);
                }
                break;

            case ALERTING_TONE:

                /* Busy verify is just 2 secs of dialtone followed by
                 * a call waiting tone every 10 secs. The rest of the
                 * tones are just played once.
                 */
                switch (dcb->alerting_tone) {
                case VCM_BUSY_VERIFY_TONE:
                    lsm_util_start_tone(VCM_CALL_WAITING_TONE, NO, lsm_get_ms_ui_call_handle(dcb->line, dcb->call_id, CC_NO_CALL_ID), dcb->group_id,
                                   ((media != NULL) ? media->refid : CC_NO_MEDIA_REF_ID),
                                   dcb->tone_direction);
                    if (cprStartTimer(lsm_tmr_tones, BUSY_VERIFICATION_DELAY,
                        (void *)(long)dcb->call_id) == CPR_FAILURE) {
                        LSM_DEBUG(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                                  fname, "cprStartTimer", cpr_errno);
                    }
                    break;

                case VCM_CALL_WAITING_TONE:
                case VCM_CALL_WAITING_2_TONE:
                case VCM_CALL_WAITING_3_TONE:
                case VCM_CALL_WAITING_4_TONE:
                    lsm_util_start_tone(dcb->alerting_tone, NO, lsm_get_ms_ui_call_handle(dcb->line, dcb->call_id, CC_NO_CALL_ID), dcb->group_id,
                                   ((media != NULL) ? media->refid : CC_NO_MEDIA_REF_ID),
                                   dcb->tone_direction);
                    break;

                case VCM_MSG_WAITING_TONE:
                case VCM_STUTTER_TONE:
                    lsm_util_start_tone(VCM_INSIDE_DIAL_TONE, NO, lsm_get_ms_ui_call_handle(dcb->line, dcb->call_id, CC_NO_CALL_ID), dcb->group_id,
                                   ((media != NULL) ? media->refid : CC_NO_MEDIA_REF_ID),
                                   dcb->tone_direction);
                    lsm_tmr_tones_ticks = 0;
                    break;
                default:
                    break;
                }
                break;

            default:
                lsm_util_start_tone(VCM_CALL_WAITING_TONE, NO, lsm_get_ms_ui_call_handle(dcb->line, dcb->call_id, CC_NO_CALL_ID), dcb->group_id,
                               ((media != NULL) ? media->refid : CC_NO_MEDIA_REF_ID),
                               dcb->tone_direction);

                break;
            }

    } else if (dcb->dialplan_tone) {
        dcb->dialplan_tone = FALSE;
        switch (dcb->alert_info) {

        case ALERTING_TONE:
            /*
             * Currently the only supported multi-part tones
             * played via the dialplan are Message Waiting and
             * Stutter dialtones.
             */
            switch (dcb->alerting_tone) {
            case VCM_MSG_WAITING_TONE:
            case VCM_STUTTER_TONE:
                lsm_util_start_tone(VCM_INSIDE_DIAL_TONE, NO, lsm_get_ms_ui_call_handle(dcb->line, dcb->call_id, CC_NO_CALL_ID), dcb->group_id,
                               ((media != NULL) ? media->refid : CC_NO_MEDIA_REF_ID),
                               dcb->tone_direction);
                break;

            case VCM_HOLD_TONE:
                lsm_util_start_tone(dcb->alerting_tone, NO, lsm_get_ms_ui_call_handle(dcb->line, dcb->call_id, CC_NO_CALL_ID), dcb->group_id,
                               ((media != NULL) ? media->refid : CC_NO_MEDIA_REF_ID),
                               dcb->tone_direction);
                break;

            default:
                break;
            }

            break;

        default:
            break;
        }
    }
}

/*
 * Function   : lsm_start_multipart_tone_timer
 * Parameters : Tone: 2nd part of tone to play
 *              Delay: Time to delay between playing the 1st and 2nd parts of the tone
 *              CallId: Used to retrieve the dcb for this call
 * Purpose    : This function is used to set up the dcb to play the 2nd part of
 *              the tone. A timer is started to allow the 1st tone to played to
 *              completion before the 2nd part is started.
 */
void
lsm_start_multipart_tone_timer (vcm_tones_t tone,
                                uint32_t delay,
                                callid_t callId)
{
    static const char fname[] = "lsm_start_multipart_tone_timer";
    fsmdef_dcb_t *dcb;

    /* Set up dcb for timer callback function */
    dcb = fsmdef_get_dcb_by_call_id(callId);
    dcb->alert_info = ALERTING_TONE;
    dcb->alerting_tone = tone;
    dcb->dialplan_tone = TRUE;

    if (cprCancelTimer(lsm_tmr_tones) == CPR_FAILURE) {
        LSM_DEBUG(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                  fname, "cprCancelTimer", cpr_errno);
    }
    if (cprStartTimer(lsm_tmr_tones, delay, (void *)(long)dcb->call_id) ==
            CPR_FAILURE) {
        LSM_DEBUG(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                  fname, "cprStartTimer", cpr_errno);
    }
}

/*
 * Function   : lsm_stop_multipart_tone_timer
 * Parameters : None
 * Purpose    : Called from vcm_stop_tones. That function
 *              will stop the 1st part of the tone, this
 *              function cancels the timer so the 2nd part
 *              will never be played.
 */
void
lsm_stop_multipart_tone_timer (void)
{
    static const char fname[] = "lsm_stop_multipart_tone_timer";

    if (cprCancelTimer(lsm_tmr_tones) == CPR_FAILURE) {
        LSM_DEBUG(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                  fname, "cprCancelTimer", cpr_errno);
    }
}

/*
 * Function   : lsm_start_continuous_tone_timer
 * Parameters : Tone: tone to play
 *              Delay: Time to delay between playing the tone
 *              CallId: Used to retrieve the dcb for this call
 * Purpose    : This function is used to set up the dcb to play a tone continuously.
 *              An example being the tone on hold tone.
 */
void
lsm_start_continuous_tone_timer (vcm_tones_t tone,
                                 uint32_t delay,
                                 callid_t callId)
{
    static const char fname[] = "lsm_start_continuous_tone_timer";
    fsmdef_dcb_t *dcb;

    /* Set up dcb for timer callback function */
    dcb = fsmdef_get_dcb_by_call_id(callId);
    dcb->alert_info = ALERTING_TONE;
    dcb->alerting_tone = tone;
    dcb->dialplan_tone = TRUE;

    if (cprCancelTimer(lsm_continuous_tmr_tones) == CPR_FAILURE) {
        LSM_DEBUG(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                  fname, "cprCancelTimer", cpr_errno);
    }
    if (cprStartTimer(lsm_continuous_tmr_tones, delay, (void *)(long)dcb->call_id)
            == CPR_FAILURE) {
        LSM_DEBUG(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                  fname, "cprStartTimer", cpr_errno);
    }
}

/*
 * Function   : lsm_stop_continuous_tone_timer
 * Parameters : None
 * Purpose    : Called from vcm_stop_tones. That function
 *              will stop the the tone, this function cancels
 *              the timer subsequent playing of the tone is not
 *              performed
 */
void
lsm_stop_continuous_tone_timer (void)
{
    static const char fname[] = "lsm_stop_continuous_tone_timer";

    if (cprCancelTimer(lsm_continuous_tmr_tones) == CPR_FAILURE) {
        LSM_DEBUG(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                  fname, "cprCancelTimer", cpr_errno);
    }
}

/*
 * Function   : lsm_start_tone_duration_timer
 * Parameters : Tone: tone type to play
 *              Duration: length of time for tone to play
 *              call_handle: Used to retrieve the dcb for this call
 * Purpose    : This function is used to set up the dcb to play the tone for
 *              a specified length of time.
 */
void
lsm_start_tone_duration_timer (vcm_tones_t tone,
                                uint32_t duration,
                                cc_call_handle_t call_handle)
{
    static const char fname[] = "lsm_start_tone_duration_timer";
    fsmdef_dcb_t *dcb;

    /* Set up dcb for timer callback function */
    dcb = fsmdef_get_dcb_by_call_id(GET_CALL_ID(call_handle));

    if (cprCancelTimer(lsm_tone_duration_tmr) == CPR_FAILURE) {
        LSM_DEBUG(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                  fname, "cprCancelTimer", cpr_errno);
    }
    if (cprStartTimer(lsm_tone_duration_tmr, duration*1000, (void *)(long)dcb->call_id) ==
            CPR_FAILURE) {
        LSM_DEBUG(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                  fname, "cprStartTimer", cpr_errno);
    }
}

/*
 * Function   : lsm_stop_tone_duration_timer
 * Parameters : None
 * Purpose    : Called from vcm_stop_tones. That function
 *              will stop the tone.
 */
void
lsm_stop_tone_duration_timer (void)
{
    static const char fname[] = "lsm_stop_tone_duration_timer";

    if (cprCancelTimer(lsm_tone_duration_tmr) == CPR_FAILURE) {
        LSM_DEBUG(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                  fname, "cprCancelTimer", cpr_errno);
    }
}

/*
 * This is a callback function for those tones that are
 * played in two parts (stutter and msgwaiting) or played
 * every x seconds, but are not steady tones (call waiting).
 *
 * @param[in] data    The gsm ID (callid_t) of the call of the
 *                    tones timer has timeout.
 *
 * @return            N/A
 */
void
lsm_tone_duration_tmr_callback (void *data)
{
    static const char fname[] = "lsm_tone_duration_tmr_callback";
    callid_t     call_id;
    fsmdef_dcb_t *dcb = NULL;
    fsmdef_media_t *media;

    LSM_DEBUG(DEB_F_PREFIX"invoked", DEB_F_PREFIX_ARGS(LSM, fname));

    call_id = (callid_t)(long)data;
    if (call_id == CC_NO_CALL_ID) {
        /* Invalid call id */
        LSM_DEBUG(DEB_F_PREFIX"invalid call id", DEB_F_PREFIX_ARGS(LSM, fname));
        return;
    }

    /* Retrieve dcb from call id */
    dcb = fsmdef_get_dcb_by_call_id(call_id);
    if (dcb == NULL) {
        LSM_DEBUG(DEB_F_PREFIX"no dcb found for call_id %d", DEB_F_PREFIX_ARGS(LSM, fname), call_id);
        return;
    }

    media = gsmsdp_find_audio_media(dcb);

    vcmToneStop(dcb->active_tone, dcb->group_id,
              ((media != NULL) ? media->refid : CC_NO_MEDIA_REF_ID),
              lsm_get_ms_ui_call_handle(dcb->line, dcb->call_id, CC_NO_CALL_ID));

    /* Up until this point, only sip core has started the call release procedure        */
    /* since upon receipt of the BYE.  Now that tone is completed playing as requested  */
    /* in the BYE, need to continue processing with call clearing.                      */

    cc_int_release(CC_SRC_GSM, CC_SRC_GSM, call_id, dcb->line, CC_CAUSE_NORMAL, NULL, NULL);
}

/*
 * LSM internal function that checks if any calls are in a pending
 * answer condition. Such a condition occurs when the GSM has delayed
 * answering an incoming call while trying to clear other calls.
 *
 * @return              call_id if found, else CC_NO_CALL_ID.
 */
static callid_t
lsm_answer_pending (void)
{
    callid_t  found_callid = CC_NO_CALL_ID;
    lsm_lcb_t *lcb;

    FSM_FOR_ALL_CBS(lcb, lsm_lcbs, LSM_MAX_LCBS) {
        if ((lcb->call_id != CC_NO_CALL_ID) &&
            (FSM_CHK_FLAGS(lcb->flags, LSM_FLAGS_ANSWER_PENDING))) {

            found_callid = lcb->call_id;
            break;
        }
    }

    return (found_callid);
}

/**
 *
 * Hold Reversion Alert - plays the ringer once.
 *
 * @param lsm_lcb_t     lcb for this call
 * @param callid_t      gsm_id
 * @param line_t        line
 *
 * @return  none
 *
 * @pre     (lcb not_eq NULL)
 */
static void
lsm_reversion_ringer (lsm_lcb_t *lcb, callid_t call_id, line_t line)
{
    vcm_ring_mode_t ringerMode = VCM_INSIDE_RING;
    vcm_tones_t     toneMode = VCM_CALL_WAITING_TONE;

    if (!lsm_callwaiting()) {
        config_get_line_value(CFGID_LINE_RING_SETTING_IDLE,
                              &ringSettingIdle, sizeof(ringSettingIdle),
                              line);
        if (cc_line_ringer_mode[line] == CC_RING_DISABLE) {
            ringerMode = VCM_FLASHONLY_RING;
        } else if (ringSettingIdle == DISABLE) {
            ringerMode = VCM_RING_OFF;
        } else if (ringSettingIdle == FLASH_ONLY) {
            ringerMode = VCM_FLASHONLY_RING;
        }

        vcmControlRinger(ringerMode, YES, NO, line, call_id);

    } else {
        lsm_tmr_tones_ticks = 0;

        config_get_line_value(CFGID_LINE_RING_SETTING_ACTIVE,
                              &ringSettingActive, sizeof(ringSettingActive),
                              line);
        if (ringSettingActive == DISABLE) {
            ringerMode = VCM_RING_OFF;
        } else if (ringSettingActive == FLASH_ONLY) {
            ringerMode = VCM_FLASHONLY_RING;
        }

        if (ringSettingActive == BEEP_ONLY) {
            fsmdef_media_t *media = gsmsdp_find_audio_media(lcb->dcb);

            lsm_util_start_tone(toneMode, NO, lsm_get_ms_ui_call_handle(line, call_id, CC_NO_CALL_ID), lcb->dcb->group_id,
                           ((media != NULL) ? media->refid : CC_NO_MEDIA_REF_ID),
                           VCM_PLAY_TONE_TO_EAR);
        } else {
            vcmControlRinger(ringerMode, YES, NO, line, call_id);
        }
    }
}

/**
 * This function will set beep only settings.
 *
 * @param[in] dcb - DEF S/M control block
 * @param[out] toneMode_p - pointer to tone mode
 *
 * @return none
 */
static void
lsm_set_beep_only_settings (fsmdef_dcb_t *dcb, vcm_tones_t *toneMode_p)
{
    switch (dcb->alert_info) {
    /*
     * Map BTS requested ring pattern to corresponding call waiting
     * pattern if phone is already offhook. All call waiting tones
     * must be played every ten seconds and msg waiting and stutter
     * dialtone are multi-part tones that play and then after
     * 100 ms give steady dialtone. Set a timer to call the tone
     * callback function for those tones.
     */
    case ALERTING_RING:
        lsm_tmr_tones_ticks = callWaitingDelay;
        switch (dcb->alerting_ring) {
        case VCM_BELLCORE_DR2:
            *toneMode_p = VCM_CALL_WAITING_2_TONE;
            break;

        case VCM_BELLCORE_DR3:
            *toneMode_p = VCM_CALL_WAITING_3_TONE;
            break;

        case VCM_BELLCORE_DR4:
            *toneMode_p = VCM_CALL_WAITING_4_TONE;
            break;

        default:
            break;
        }
        break;

    /* BTS wishes to override call waiting tone */
    case ALERTING_TONE:
        /*
         * In violation of the spec, BTS will send tones in the
         * Alert-Info header and if the phone is offhook, wants
         * the phone to play the tone specified in the Alert-Info
         * header instead of the normal call waiting tone. If this
         * line is connected to a call manager follow the spec and
         * always play the call waiting tone regardless of what was
         * received in the Alert-Info header.
         */
        if (sip_regmgr_get_cc_mode(dcb->line) == REG_MODE_CCM) {
            dcb->alerting_tone = VCM_CALL_WAITING_TONE;
            LSM_DEBUG(DEB_F_PREFIX" - Overriding value in Alert-Info header as line %d is \
                      connected to a Call Manager.",
                      DEB_F_PREFIX_ARGS(LSM, __FUNCTION__), dcb->line);
        }
        *toneMode_p = dcb->alerting_tone;
        switch (dcb->alerting_tone) {
        case VCM_MSG_WAITING_TONE:
            lsm_tmr_tones_ticks = MSG_WAITING_DELAY;
            break;

        case VCM_STUTTER_TONE:
            lsm_tmr_tones_ticks = STUTTER_DELAY;
            break;

        case VCM_BUSY_VERIFY_TONE:
            lsm_tmr_tones_ticks = BUSY_VERIFY_DELAY;
            break;

        case VCM_CALL_WAITING_TONE:
        case VCM_CALL_WAITING_2_TONE:
        case VCM_CALL_WAITING_3_TONE:
        case VCM_CALL_WAITING_4_TONE:
            lsm_tmr_tones_ticks = callWaitingDelay;
            break;

        default:
            break;
        }
        break;

    default:
        lsm_tmr_tones_ticks = callWaitingDelay;
    }
}

/**
 *
 * Set ringer mode based on remote-cc input and configuration parameters. If there
 * any other call pending then it should play call waiting tone.
 *
 * @param lsm_lcb_t     lcb for this call
 * @param callid_t      gsm_id
 * @param line_t        line
 *
 * @return  none
 *
 * @pre     (lcb not_eq NULL)
 */
static void
lsm_set_ringer (lsm_lcb_t *lcb, callid_t call_id, line_t line, int alerting)
{
    static const char fname[] = "lsm_set_ringer";
    fsmdef_dcb_t   *dcb;
    boolean         ringer_set = FALSE;
    callid_t        other_call_id = CC_NO_CALL_ID;
    callid_t        priority_call_id = CC_NO_CALL_ID;
    int             callHoldRingback = 0;
    int             dcb_cnt = 0;
    int             i = 0;
    fsmxfr_xcb_t   *xcb;
    fsmcnf_ccb_t   *ccb;
    lsm_lcb_t *lcb2;
    fsmdef_dcb_t   *dcbs[LSM_MAX_CALLS];
    vcm_ring_mode_t ringerMode = VCM_INSIDE_RING;
    short           ringOnce = NO;
    boolean         alertInfo = NO;
    vcm_tones_t     toneMode = VCM_CALL_WAITING_TONE;
    fsmdef_media_t *media;
    boolean         isDusting = FSM_CHK_FLAGS(lcb->flags, LSM_FLAGS_DUSTING) ? TRUE : FALSE;
    int            sdpmode = 0;


    LSM_DEBUG(DEB_L_C_F_PREFIX"Entered, state=%d.",
              DEB_L_C_F_PREFIX_ARGS(LSM, line, call_id, fname), lcb->state);

    config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));

    /*
     * The ringer (or call-waiting tone) should be on if these
     * conditions are met:
     * 1. A line is ringing for an incoming call and no calls
     *    with a pending answer
     * 2. A line is on hold
     * and
     * 3. No lines are connected
     *
     * Otherwise, turn on the call-waiting tones.
     *
     */

    if (priority_call_id == CC_NO_CALL_ID) {
        /* get the call_id of the line that triggers this if it is ringing and
           pass down the correct line variable and its ring type and let the ring
           manager decides. Originally we only find line first line in ringing state
           which results in issue where Flash only line follows by audio ring line
           ringing simultaneously, the phone does not ring audibly.
        */
        if (lcb->state == LSM_S_RINGIN) {
            other_call_id = call_id;
        } else {
            other_call_id = lsm_find_state(LSM_S_RINGIN);
        }
    }

    if (((priority_call_id != CC_NO_CALL_ID) || (other_call_id != CC_NO_CALL_ID)) &&
        (lsm_answer_pending() == CC_NO_CALL_ID)) {
        /*sam
         * may need to add (ringout and rtp open) to this check.
         * It is possible that inband alerting is active for an outgoing call.
         */
        dcb = fsmdef_get_dcb_by_call_id((priority_call_id != CC_NO_CALL_ID) ?
                                        priority_call_id : other_call_id);
        lcb = lsm_get_lcb_by_call_id((priority_call_id != CC_NO_CALL_ID) ?
                                        priority_call_id : other_call_id);
        isDusting = ((lcb != NULL) && FSM_CHK_FLAGS(lcb->flags, LSM_FLAGS_DUSTING)) ? TRUE : FALSE;

        /*
         * TNP has line-based ringing so update the line parameter so
         * if reflects what line is ringing, not which line had an action
         * taken against it, i.e. if line 1 hangs up and line 2 is ringing,
         * line will be equal to 1 (since that line hung-up), but it needs
         * to be 2 since that is the line actually ringing. 40/60 can
         * get away with this since it is device-based ringing. If there
         * are multiple lines in the RINGIN state, the ringing will be
         * based on the first line in the RINGIN state found. Could add
         * the check if (other_call_id != callid) but line will equal
         * dcb->line if the callids are the same so save a few CPU cycles
         * by not having the check. No need to do a #ifdef TNP since
         * it does matter which line we use on the 40/60 as it is device based.
         */
        line = dcb->line;

        if (!lsm_callwaiting()) {

            LSM_DEBUG(DEB_L_C_F_PREFIX"No call waiting, lcb->line=%d, lcb->flag=%d.",
                      DEB_L_C_F_PREFIX_ARGS(LSM, line, lcb->call_id, fname),
                      lcb->line,
                      lcb->flags);

            ringer_set = TRUE;
            lsm_tmr_tones_ticks = 0;

            /*
             * CFGID_LINE_RING_SETTING_IDLE is a config parameter that
             * tells the phone what action to take for an incoming
             * call on a phone with no active calls.
             *
             */

            if (isDusting) {
                ringSettingIdle = FLASH_ONLY;
            }
            else if (FSM_CHK_FLAGS(lcb->flags, LSM_FLAGS_PREVENT_RINGING)) {
                /*
                 * If this phone is both calling and called device, do not play ring.
                 */
                ringSettingIdle = DISABLE;
            } else if (cc_line_ringer_mode[line] == CC_RING_DISABLE) {
                /*
                 * Disable - no ring or flash
                 */
                ringSettingIdle = FLASH_ONLY;
            } else if (cc_line_ringer_mode[line] == CC_RING_ENABLE) {
                ringSettingIdle = RING;
            } else {
                config_get_line_value(CFGID_LINE_RING_SETTING_IDLE,
                                      &ringSettingIdle, sizeof(ringSettingIdle),
                                      line);
            }
            LSM_DEBUG(DEB_L_C_F_PREFIX"Ring set mode=%d.",
                      DEB_L_C_F_PREFIX_ARGS(LSM, line, call_id, fname), ringSettingIdle);

            /*
             * Disable - no ring or flash
             */
            if (ringSettingIdle == DISABLE) {
                ringerMode = VCM_RING_OFF;

                /*
                 * Flash Only - No ringing, just flash.
                 */
            } else if (ringSettingIdle == FLASH_ONLY) {
                ringerMode = VCM_FLASHONLY_RING;

                /*
                 * Ring once - ring the phone once
                 */
            } else if (ringSettingIdle == RING_ONCE) {
                ringOnce = YES;

                /*
                 * Ring - normal operation. Ring the phone until answered,
                 * forwarded, or disconnected.
                 */
            } else if (ringSettingIdle == RING) {

                /* Determine what tone/ringing pattern to play */
                switch (dcb->alert_info) {
                case ALERTING_NONE:
                    /* This is the default case nothing to do */
                    break;

                case ALERTING_RING:
                    ringerMode = dcb->alerting_ring;
                    break;

                case ALERTING_OLD:
                default:
                    alertInfo = YES;
                }
            } else if (ringSettingIdle == BEEP_ONLY) {
                lsm_set_beep_only_settings (dcb, &toneMode);

            }
            LSM_DEBUG(DEB_L_C_F_PREFIX"Alert info=%d, ringSettingIdle=%d, ringerMode=%d",
                                  DEB_L_C_F_PREFIX_ARGS(LSM, line, call_id, fname),
                                  dcb->alert_info,
                                  ringSettingIdle,
                                  ringerMode);

            /*
             * If an active call is being held while there is an incoming
             * call AND ringSettingBusyStationPolicy is 0, this flag will
             * be false.
             */
            if (alerting) {
                /*
                 * If the line is connected to a CCM, Bellcore-Dr1 means
                 * play the defined ringer once.  Bellcore-dr2 means play
                 * the defined ringer twice.
                 */
                if (sip_regmgr_get_cc_mode(line) == REG_MODE_CCM) {
                    if (ringerMode == VCM_BELLCORE_DR1) {
                        ringerMode = VCM_INSIDE_RING;
                    } else if (ringerMode == VCM_BELLCORE_DR2) {
                        ringerMode = VCM_OUTSIDE_RING;
                    }
                }
                if (ringSettingIdle == BEEP_ONLY) {

                    LSM_DEBUG(DEB_L_C_F_PREFIX"Idle phone RING SETTING: Beep_only",
                              DEB_L_C_F_PREFIX_ARGS(LSM, line, call_id, fname));

                    media = gsmsdp_find_audio_media(lcb->dcb);
                    lsm_util_tone_start_with_speaker_as_backup(toneMode, NO, lsm_get_ms_ui_call_handle(line, call_id, CC_NO_CALL_ID),
                                                          lcb->dcb->group_id,
                          ((media != NULL) ? media->refid : CC_NO_MEDIA_REF_ID),
                                                          VCM_PLAY_TONE_TO_EAR);
                } else {
                    LSM_DEBUG(DEB_L_C_F_PREFIX"Idle phone RING SETTING: ringer Mode = %s,"
                              " Ring once = %d, alertInfo = %d\n",
                              DEB_L_C_F_PREFIX_ARGS(LSM, line, call_id, fname),
                              vm_alert_names[ringerMode], ringOnce, alertInfo);

                    vcmControlRinger(ringerMode, ringOnce, alertInfo, line, lcb->ui_id);
                }
            }

            if ( lcb->state != LSM_S_HOLDING &&
                 lcb->state != LSM_S_RINGIN ) {
                ui_set_call_status(platform_get_phrase_index_str(CALL_ALERTING),
                                   line, lcb->ui_id);
            }
        } else {

            // Ring off all lines.
	    FSM_FOR_ALL_CBS(lcb2, lsm_lcbs, LSM_MAX_LCBS) {
		 if ((lcb2->call_id != CC_NO_CALL_ID) &&
		     (lcb2->state == LSM_S_RINGIN) )
		 {
            LSM_DEBUG(DEB_L_C_F_PREFIX"Call waiting RING SETTING: "
                      "ringer Mode = RING_OFF, Ring once = NO, alertInfo = NO\n",
                      DEB_L_C_F_PREFIX_ARGS(LSM, lcb2->line, lcb2->call_id, fname));
                      vcmControlRinger(VCM_RING_OFF, NO, NO, lcb2->line, call_id);
                 }
            }
            ringer_set = TRUE;
            lsm_tmr_tones_ticks = 0;

            /*
             * ringSettingActive is a TNP only config parameter that
             * tells the phone what action to take for an incoming
             * call on a phone with an active call.
             */
            if (isDusting) {
                ringSettingActive = FLASH_ONLY;
            } else {
                config_get_line_value(CFGID_LINE_RING_SETTING_ACTIVE,
                                      &ringSettingActive, sizeof(ringSettingActive),
                                      line);
            }

            /*
             * Disable - no ring or flash
             */
            if (ringSettingActive == DISABLE) {
                ringerMode = VCM_RING_OFF;

                /*
                 * Flash Only - No ringing, just flash.
                 */
            } else if (ringSettingActive == FLASH_ONLY) {
                ringerMode = VCM_FLASHONLY_RING;

                /*
                 * Ring once - ring the phone once
                 */
            } else if (ringSettingActive == RING_ONCE) {
                ringOnce = YES;

                /*
                 * Ring - Ring the phone until answered, forwarded or
                 * disconnected.
                 *
                 * NOTE: This code is replicated above under checking
                 * RING_SETTING_IDLE above. Putting this common code
                 * in a function call saved a miniscule amount of memory
                 * at the cost of an additional function call for every
                 * call. It was decided it was not worth the cost, but
                 * has been documented in case the phone gets very,
                 * very low on memory in the future.
                 */
            } else if (ringSettingActive == RING) {

                /* Determine what tone/ringing pattern to play */
                switch (dcb->alert_info) {
                case ALERTING_NONE:
                    /* This is the default case nothing to do */
                    break;

                case ALERTING_RING:
                    ringerMode = dcb->alerting_ring;
                    break;

                case ALERTING_OLD:
                default:
                    alertInfo = YES;
                }

                /*
                 * BeepOnly - normal operation. Play call waiting tone.
                 */
            } else if (ringSettingActive == BEEP_ONLY) {
                lsm_set_beep_only_settings (dcb, &toneMode);

            }

            /*
             * If an active call is being held while there is an incoming
             * call AND ringSettingBusyStationPolicy is 0, this flag will
             * be false.
             */
            if (alerting) {
                /*
                 * The code above has set the variables to play either the ringer
                 * or a tone based on the ringSettingBusy. If the config variable
                 * is beeponly then call start_tone else call control_ringer.
                 */
                if (ringSettingActive == BEEP_ONLY) {
                    media = gsmsdp_find_audio_media(dcb);

                    lsm_util_start_tone(toneMode, NO, lsm_get_ms_ui_call_handle(line, call_id, CC_NO_CALL_ID), dcb->group_id,
                                   ((media != NULL) ? media->refid : CC_NO_MEDIA_REF_ID),
                                   VCM_PLAY_TONE_TO_EAR);
                } else {

                    LSM_DEBUG(DEB_L_C_F_PREFIX"Active call RING SETTING: "
                              "ringer Mode = %s, Ring once = %d, alertInfo = %d\n",
                              DEB_L_C_F_PREFIX_ARGS(LSM, line, call_id, fname),
                              vm_alert_names[ringerMode], ringOnce, alertInfo);

                    vcmControlRinger(ringerMode, ringOnce, alertInfo, line, lcb->ui_id);
                }
            }

            /*
             * Start a timer to play multiple part tones if needed.
             */
            if (lsm_tmr_tones_ticks > 0) {
                if (cprCancelTimer(lsm_tmr_tones) == CPR_FAILURE) {
                    LSM_DEBUG(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                              fname, "cprCancelTimer", cpr_errno);
                }
                if (cprStartTimer(lsm_tmr_tones, lsm_tmr_tones_ticks,
                                  (void *)(long)dcb->call_id) == CPR_FAILURE) {
                    LSM_DEBUG(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                              fname, "cprStartTimer", cpr_errno);
                }
            }
        }
    } else if (lcb->state == LSM_S_IDLE) {
        /*
         * This line just hungup so let's check to see if call hold ringback
         * is enabled and if we have any other holding lines. If so ring to
         * alert the user that a line is still around.
         */
        config_get_value(CFGID_CALL_HOLD_RINGBACK, &callHoldRingback,
                         sizeof(callHoldRingback));
        if (callHoldRingback & 0x1) {
            callid_t        ui_id;

            dcb_cnt = fsmdef_get_dcbs_in_held_state(dcbs, call_id);
            for (i = 0, dcb = dcbs[i]; i < dcb_cnt; i++, dcb = dcbs[i]) {
              	ccb = fsmcnf_get_ccb_by_call_id(call_id);
               	xcb = fsmxfr_get_xcb_by_call_id(call_id);
               	if ((lsm_is_phone_inactive() == TRUE) &&
	               	(ccb == NULL) && (xcb == NULL) &&
                   	(lcb->enable_ringback == TRUE)) {
                    LSM_DEBUG(DEB_L_C_F_PREFIX"Applying ringback",
                              	DEB_L_C_F_PREFIX_ARGS(LSM, lcb->line, lcb->call_id, fname));
                    ringer_set = TRUE;

                    LSM_DEBUG(DEB_L_C_F_PREFIX"Hold RINGBACK SETTING: ringer Mode = "
                        	      "VCM_INSIDE_RING, Ring once = YES, alertInfo = YES\n",
                            	  DEB_L_C_F_PREFIX_ARGS(LSM, line, dcb->call_id, fname));
                    vcmControlRinger(VCM_INSIDE_RING, YES, YES, line, call_id);

                    /* Find the corresponding LCB to get to the UI ID */
                    ui_id = lsm_get_ui_id(dcb->call_id);
                    ui_set_call_status(platform_get_phrase_index_str(CALL_INITIATE_HOLD),
                        	                  dcb->line, ui_id);
                }
            }
        }
    }

    if (ringer_set == FALSE) {

        LSM_DEBUG(DEB_L_C_F_PREFIX"Ringer_set = False : "
                  "ringer Mode = VCM_RING_OFF, Ring once = NO, alertInfo = NO\n",
                  DEB_L_C_F_PREFIX_ARGS(LSM, line, call_id, fname));


        if (!sdpmode) {
            vcmControlRinger(VCM_RING_OFF, NO, NO, line, call_id);
        }

    }
}

static cc_rcs_t
lsm_offhook (lsm_lcb_t *lcb, cc_state_data_offhook_t *data)
{
    callid_t        call_id = lcb->call_id;
    line_t          line = lcb->line;
    fsmxfr_xcb_t   *xcb;
    lsm_lcb_t      *lcb2;
    callid_t        call_id2;
    int             attr;
    fsmdef_dcb_t   *dcb;

    dcb = lcb->dcb;
    if (dcb == NULL) {
        return (CC_RC_ERROR);
    }

    lsm_change_state(lcb, __LINE__, LSM_S_OFFHOOK);

    /*
     * Disable the ringer since the user is going offhook. Only calls
     * in the RINGIN state should have ringing enabled.
     */
    FSM_FOR_ALL_CBS(lcb2, lsm_lcbs, LSM_MAX_LCBS) {
        if ((lcb2->call_id != CC_NO_CALL_ID) &&
            (lcb2->state == LSM_S_RINGIN)) {

            vcmControlRinger(VCM_RING_OFF, NO, NO, lcb2->line, lcb2->call_id);
        }
    }

    dp_offhook(line, call_id);

    attr = fsmutil_get_call_attr(dcb, line, call_id);

    ui_new_call(evOffHook, line, lcb->ui_id, attr,
                dcb->caller_id.call_instance_id,
                (boolean)FSM_CHK_FLAGS(lcb->flags, LSM_FLAGS_DIALED_STRING));

    xcb = fsmxfr_get_xcb_by_call_id(call_id);
    if (xcb != NULL) {
        call_id2 = ((lcb->call_id == xcb->xfr_call_id) ?
                    (xcb->cns_call_id) : (xcb->xfr_call_id));
        lcb2 = lsm_get_lcb_by_call_id(call_id2);
    }

    //vcmActivateWlan(TRUE);

    vcmEnableSidetone(YES);

    return (CC_RC_SUCCESS);
}


static cc_rcs_t
lsm_dialing (lsm_lcb_t *lcb, cc_state_data_dialing_t *data)
{
    fsmxfr_xcb_t   *xcb = NULL;
    int             stutterMsgWaiting = 0;
    fsmdef_dcb_t   *dcb = lcb->dcb;
    fsmdef_media_t *media = gsmsdp_find_audio_media(dcb);


    if ( dcb == NULL) {
        return (CC_RC_ERROR);
    }

    /* don't provide dial tone on transfer unless we are the transferor. */
    xcb = fsmxfr_get_xcb_by_call_id(lcb->call_id);
    if ((xcb != NULL) && (xcb->mode != FSMXFR_MODE_TRANSFEROR)) {
        return (CC_RC_SUCCESS);
    }

    /*
     * Start dial tone if no digits have been entered
     */
    if ((data->play_dt == TRUE)
        && (dp_check_for_plar_line(lcb->line) == FALSE)
        ) {

        /* get line based AMWI config */
        config_get_value(CFGID_LINE_MESSAGE_WAITING_AMWI + lcb->line - 1, &stutterMsgWaiting,
                         sizeof(stutterMsgWaiting));
        if ( stutterMsgWaiting != 1 && stutterMsgWaiting != 0) {
          /* AMWI is not configured. Fallback on config for stutter dial tone */
          config_get_value(CFGID_STUTTER_MSG_WAITING, &stutterMsgWaiting,
                         sizeof(stutterMsgWaiting));
          stutterMsgWaiting &= 0x1; /* LSB indicates on/off */
        }

        if ( (data->suppress_stutter == FALSE) &&
             (ui_line_has_mwi_active(lcb->line)) && /* has msgs waiting */
		      stutterMsgWaiting ) {
            lsm_util_start_tone(VCM_STUTTER_TONE, FALSE, lsm_get_ms_ui_call_handle(lcb->line, CC_NO_CALL_ID, lcb->ui_id),
                    dcb->group_id,
                           ((media != NULL) ? media->refid : CC_NO_MEDIA_REF_ID),
                           VCM_PLAY_TONE_TO_EAR);
        } else {
            lsm_util_start_tone(VCM_INSIDE_DIAL_TONE, FALSE, lsm_get_ms_ui_call_handle(lcb->line, CC_NO_CALL_ID, lcb->ui_id),
                    dcb->group_id,
                           ((media != NULL) ? media->refid : CC_NO_MEDIA_REF_ID),
                           VCM_PLAY_TONE_TO_EAR);
        }
    }

    /*
     * For round table phone, post WAITINGFORDIGITS event,
     * so that UI can pop up dialing screen.
     * For TNP, this event gets ignored.
     */
    ui_call_state(evWaitingForDigits, lcb->line, lcb->ui_id, CC_CAUSE_NORMAL);


    return (CC_RC_SUCCESS);
}


static cc_rcs_t
lsm_dialing_completed (lsm_lcb_t *lcb, cc_state_data_dialing_completed_t *data)
{
    line_t          line = lcb->line;
    fsmdef_dcb_t   *dcb = lcb->dcb;

    if (dcb == NULL) {
        return (CC_RC_ERROR);
    }

    lsm_change_state(lcb, __LINE__, LSM_S_PROCEED);

    /* If KPML is enabled then do not change UI state to
     * proceed, more digit to collect
     */
    if (dp_get_kpml_state()) {
        return (CC_RC_SUCCESS);
    }

    ui_call_info(data->caller_id.calling_name,
                 data->caller_id.calling_number,
                 data->caller_id.alt_calling_number,
                 data->caller_id.display_calling_number,
                 data->caller_id.called_name,
                 data->caller_id.called_number,
                 data->caller_id.display_called_number,
                 data->caller_id.orig_called_name,
                 data->caller_id.orig_called_number,
                 data->caller_id.last_redirect_name,
                 data->caller_id.last_redirect_number,
                 (calltype_t)dcb->call_type,
                 line, lcb->ui_id,
                 dcb->caller_id.call_instance_id,
                 FSM_GET_SECURITY_STATUS(dcb),
                 FSM_GET_POLICY(dcb));

    lsm_ui_call_state(evProceed, line, lcb, CC_CAUSE_NORMAL);

    (void) lsm_stop_tone(lcb, NULL);

    return (CC_RC_SUCCESS);
}


static cc_rcs_t
lsm_call_sent (lsm_lcb_t *lcb, cc_state_data_call_sent_t *data)
{
    line_t          line = lcb->line;
    fsmdef_dcb_t   *dcb;
    char            tmp_str[STATUS_LINE_MAX_LEN];
    fsmdef_media_t *media;
    static const char fname[] = "lsm_call_sent";

    dcb = lcb->dcb;
    if (dcb == NULL) {
        return (CC_RC_ERROR);
    }

    lsm_change_state(lcb, __LINE__, LSM_S_PROCEED);

    (void) lsm_stop_tone(lcb, NULL);

    /*
     * We go ahead and start a rx port if our local SDP indicates
     * the need in an attempt to be 3264 compliant. Since we have
     * not yet locked down the codec, we will use preferred codec if
     * configured. If not, we use the first codec in our local
     * list of supported codecs. The codec list was initialized
     * in fsmdef_init_local_sdp.
     */
    GSMSDP_FOR_ALL_MEDIA(media, dcb) {
         if (!GSMSDP_MEDIA_ENABLED(media)) {
             continue;
         }
         LSM_DEBUG(DEB_F_PREFIX"%d %d %d", DEB_F_PREFIX_ARGS(LSM, fname), media->direction_set,
                   media->direction, media->is_multicast);
         if ((media->direction_set) &&
             ((media->direction == SDP_DIRECTION_SENDRECV) ||
              (media->direction == SDP_DIRECTION_RECVONLY))) {

             lsm_rx_start(lcb, cc_state_name(CC_STATE_FAR_END_ALERTING),
                          media);
         }
    }

    if (!dp_get_kpml_state()) {
        if ((platGetPhraseText(STR_INDEX_CALLING,
                                     (char *) tmp_str,
                                     STATUS_LINE_MAX_LEN - 1)) == CPR_SUCCESS) {
            ui_set_call_status(tmp_str, line, lcb->ui_id);
        }
    }

    /*
     * cancel offhook to first digit timer.
     */
    dp_int_cancel_offhook_timer(line, lcb->call_id);

    return (CC_RC_SUCCESS);
}


static cc_rcs_t
lsm_far_end_proceeding (lsm_lcb_t *lcb,
                       cc_state_data_far_end_proceeding_t * data)
{
    line_t        line = lcb->line;
    fsmdef_dcb_t *dcb;

    lsm_change_state(lcb, __LINE__, LSM_S_PROCEED);

    if (!dp_get_kpml_state()) {
        ui_set_call_status(platform_get_phrase_index_str(CALL_PROCEEDING_IN),
                           line, lcb->ui_id);
        /*
         * update placed call info in call history with dialed digits
         */
        dcb = lcb->dcb;
        if (dcb != NULL && dcb->placed_call_update_required) {
            lsm_update_placed_callinfo(dcb);
            dcb->placed_call_update_required = FALSE;
        }
    }


    return (CC_RC_SUCCESS);
}


static cc_rcs_t
lsm_far_end_alerting (lsm_lcb_t *lcb, cc_state_data_far_end_alerting_t *data)
{
    static const char fname[] = "lsm_far_end_alerting";
    callid_t        call_id = lcb->call_id;
    line_t          line = lcb->line;
    fsmdef_dcb_t   *dcb;
    const char     *status = NULL;
    fsmcnf_ccb_t   *ccb;
    boolean         rcv_port_started = FALSE;
    char            tmp_str[STATUS_LINE_MAX_LEN];
    boolean         spoof_ringout;
    fsmdef_media_t  *media;
    call_events     call_state;
    fsmdef_media_t *audio_media;
    boolean         is_session_progress = FALSE;


    /*
     * Need to check if rcv_chan is already open and if we will be
     * receiving inband ringing. The recv_chan should always be
     * open since we always open a receive channel when initiating a
     * call. If inband ringing will be sent by the far end, we
     * will close the receive port and reopen it using the codec
     * negotiated when we received the SDP in the far ends call
     * proceeding message. We want to close the receive port well
     * ahead of reopening it due to some issue in the dsp where
     * a close followed immediately by an open causes a reset of
     * the DSP.
     */
    dcb = lcb->dcb;
    if (dcb == NULL) {
        return (CC_RC_ERROR);
    }
    audio_media =  gsmsdp_find_audio_media(dcb);

    if (dcb->inband) {
        /* close (with refresh) all media entries */
        lsm_close_rx(lcb, TRUE, NULL);
        lsm_close_tx(lcb, TRUE, NULL);
    }

    /*
     * Check to see if we need to spoof ring out in connected or holding
     * state.
     *
     * The LSM can be in holding state when the user is resuming
     * currently held call that was early transferred to another party
     * and the other party has not answered the call yet.
     */
    if (dcb->spoof_ringout_requested &&
        ((lcb->state == LSM_S_CONNECTED) || (lcb->state == LSM_S_HOLDING))) {
        /* Spoof ring out is requested in the connected/holding state */
        spoof_ringout = TRUE;
    } else {
        spoof_ringout = FALSE;
    }
    lsm_change_state(lcb, __LINE__, LSM_S_RINGOUT);

    /* Don't send the dialplan update msg if CFWD_ALL. Otherwise the invalid
     * redial numer is saved. (CSCsv08816)
     */
    if (dcb->active_feature != CC_FEATURE_CFWD_ALL) {
        dp_int_update(line, call_id, data->caller_id.called_number);
    }


    /*
     * Check for inband alerting or spoof ringout.
     * If no inband alerting and this is not a spoof ringout case,
     * just update the status line to show we are alerting. The local
     * ringback tone will not be started until the ringback delay timer
     * expires.
     */
    if (dcb->inband != TRUE || spoof_ringout) {
        status = platform_get_phrase_index_str(CALL_ALERTING_LOCAL);

        if (spoof_ringout) {

            if (audio_media) {

            /*
             * Ringback delay timer is not used for spoof ringout case
             * so start local ringback tone now.
             */
                lsm_util_start_tone(VCM_ALERTING_TONE, FALSE, lsm_get_ms_ui_call_handle(line, call_id, CC_NO_CALL_ID),
                           dcb->group_id,audio_media->refid,
                           VCM_PLAY_TONE_TO_EAR);
            }

        }
    } else {
    	is_session_progress = TRUE;
        (void) lsm_stop_tone(lcb, NULL);

        if ((platGetPhraseText(STR_INDEX_SESSION_PROGRESS,
                                     (char *) tmp_str,
                                     STATUS_LINE_MAX_LEN - 1)) == CPR_SUCCESS) {
            status = tmp_str;
        }

        /* start receive and transmit for all media entries that are active */
        GSMSDP_FOR_ALL_MEDIA(media, dcb) {
            if (!GSMSDP_MEDIA_ENABLED(media)) {
                /* this entry is not active */
                continue;
            }
            LSM_DEBUG(DEB_L_C_F_PREFIX"direction_set:%d direction:%d"
                      " dest_addr:%p is_multicast:%d",
                      DEB_L_C_F_PREFIX_ARGS(LSM, dcb->line, dcb->call_id, fname),
                      media->direction_set,
                      media->direction, &media->dest_addr,
                      media->is_multicast);

            if (media->direction_set) {
                if (media->direction == SDP_DIRECTION_SENDRECV ||
                    media->direction == SDP_DIRECTION_RECVONLY) {
                    lsm_rx_start(lcb,
                                 cc_state_name(CC_STATE_FAR_END_ALERTING),
                                 media);
                    rcv_port_started = TRUE;
                }

                if (media->direction == SDP_DIRECTION_SENDRECV ||
                    media->direction == SDP_DIRECTION_SENDONLY) {
                    lsm_tx_start(lcb,
                             cc_state_name(CC_STATE_FAR_END_ALERTING),
                             media);
                }
            }
        }

        if (!rcv_port_started) {
            /*
             * Since we had SDP we thought inband ringback was in order but
             * media attributes indicate the receive port is to remain
             * closed. In this case, go ahead and apply local ringback tone
             * or user will hear silence. We do not depend on ringback delay
             * timer to start the local ringback tone so we have to start it
             * here.
             */
            status = platform_get_phrase_index_str(CALL_ALERTING_LOCAL);
            lsm_util_start_tone(VCM_ALERTING_TONE, FALSE, lsm_get_ms_ui_call_handle(line, call_id, CC_NO_CALL_ID), dcb->group_id,
                           ((audio_media != NULL) ? audio_media->refid :
                                                   CC_NO_MEDIA_REF_ID),
                           VCM_PLAY_TONE_TO_EAR);
        } else {
            lsm_set_ringer(lcb, call_id, line, YES);
        }
    }

    ccb = fsmcnf_get_ccb_by_call_id(call_id);

    /* Update call information */
    lsm_internal_update_call_info(lcb, dcb);

    /* This is the case where remote end of the call has been early trasnfered
     * to another endpoint.
     */

    ccb = fsmcnf_get_ccb_by_call_id(lcb->call_id);

    if ((ccb != NULL) && (ccb->active == TRUE) &&
        (ccb->flags & LCL_CNF)) {
        call_state = evConference;
    } else {
        call_state = evRingOut;
    }

    /* If an invalid DN is dialed during CFA then CCM sends 183/Session Progress
     * (a.k.a. far end alerting) so it can play the invalid DN announcement. In
     * this case CCM sends 404 Not Found after playing the announcement. If we
     * are here due to that situation then don't propagate call info or status
     * to UI side as it will display "Session Progress" on the status line and
     * will log the DN in Placed calls; and we don't want either. Just skip the
     * update and following 404 Not Found will take care of playing/displaying
     * Reorder. Note that this condition may occur only in TNP/CCM mode.
     */
    if (dcb->active_feature != CC_FEATURE_CFWD_ALL) {
    	if(!is_session_progress) {//CSCtc18750
    		/*
    		 * update placed call info in call history with dialed digits
    		 */
    		if (dcb->placed_call_update_required) {
    		    lsm_update_placed_callinfo(dcb);
    		    dcb->placed_call_update_required = FALSE;
    		}

    		if (status) {
    		    ui_set_call_status(status, line, lcb->ui_id);
    		}
    	}

        lsm_ui_call_state(call_state, line, lcb, CC_CAUSE_NORMAL);

    }
    /* For roundtable phones, UI will be in dial state, which is different from TNP UI,
     * TNP UI does not have different dialing layer. In this case offhook dialing screen
     * does not vanish untill GSM provides procced call status, hence all the softkeys are
     * available during CFWD, which is not correct
     */
    if (dcb->active_feature == CC_FEATURE_CFWD_ALL) {
        lsm_ui_call_state(evReorder, line, lcb, CC_CAUSE_NORMAL);
    }

    return (CC_RC_SUCCESS);
}


static cc_rcs_t
lsm_call_received (lsm_lcb_t *lcb, cc_state_data_call_received_t *data)
{
    return (CC_RC_SUCCESS);
}


static cc_rcs_t
lsm_alerting (lsm_lcb_t *lcb, cc_state_data_alerting_t *data)
{
    callid_t        call_id = lcb->call_id;
    line_t          line = lcb->line;
    fsmdef_dcb_t   *dcb;

    dcb = lcb->dcb;
    if (dcb == NULL) {
        return (CC_RC_ERROR);
    }

    lsm_change_state(lcb, __LINE__, LSM_S_RINGIN);

    dcb->ui_update_required = TRUE;
    lsm_internal_update_call_info(lcb, dcb);

    ui_new_call(evRingIn, line, lcb->ui_id, NORMAL_CALL,
                dcb->caller_id.call_instance_id, FALSE);

    fsmutil_set_shown_calls_ci_element(dcb->caller_id.call_instance_id, line);
    lsm_ui_call_state(evRingIn, line, lcb, CC_CAUSE_NORMAL);
    lsm_update_inalert_status(line, lcb->ui_id, data, TRUE);


    lsm_set_ringer(lcb, call_id, line, YES);

    return (CC_RC_SUCCESS);
}


static cc_rcs_t
lsm_answered (lsm_lcb_t *lcb, cc_state_data_answered_t *data)
{
    line_t          line = lcb->line;
    fsmdef_dcb_t   *dcb;

    dcb = lcb->dcb;
    if (dcb == NULL) {
        return (CC_RC_ERROR);
    }

    lsm_change_state(lcb, __LINE__, LSM_S_OFFHOOK);


    lsm_internal_update_call_info(lcb, dcb);

    vcmControlRinger(VCM_RING_OFF, NO, NO, line, dcb->call_id);

    lsm_ui_call_state(evOffHook, line, lcb, CC_CAUSE_NORMAL);

    //vcmActivateWlan(TRUE);

    (void) lsm_stop_tone(lcb, NULL);

    return (CC_RC_SUCCESS);
}

/**
 *
 * Function updates media paths based on the negotated parameters.
 *
 * @param lcb          line control block
 * @param caller_fname caller function name
 *
 * @return  none
 *
 * @pre     (dcb not_eq NULL)
 * @pre     (fname not_eq NULL)
 */
static void
lsm_update_media (lsm_lcb_t *lcb, const char *caller_fname)
{
    static const char fname[] = "lsm_update_media";
    fsmdef_dcb_t   *dcb;
    fsmdef_media_t *media;
    boolean        rx_refresh;
    boolean        tx_refresh;
    char           addr_str[MAX_IPADDR_STR_LEN];
    int            i;

    dcb = lcb->dcb;
    if (dcb == NULL) {
        LSM_ERR_MSG(get_debug_string(DEBUG_INPUT_NULL),
                    fname);
        return;
    }

    addr_str[0] = '\0';

    /*
     * Close rx and tx port for media change. Check media direction
     * to see if port should be closed or remain open. If the port
     * needs to be kept open, lsm_close_* functions will check to
     * see if any media attributes have changed. If anything has
     * changed, the port is closed, otherwise the port remains
     * open. If media direction is not set, treat as if set to inactive.
     * Also, if multicast leave rx_refresh and tx_refresh to FALSE to
     * force a socket close.
     */
    GSMSDP_FOR_ALL_MEDIA(media, dcb) {
        if (!GSMSDP_MEDIA_ENABLED(media) ||
            FSM_CHK_FLAGS(media->hold, FSM_HOLD_LCL)) {
            /* this entry is not active or locally held */
            continue;
        }

        rx_refresh = FALSE;
        tx_refresh = FALSE;

        if ((media->direction_set) && (media->is_multicast == FALSE)) {
            if (media->direction == SDP_DIRECTION_SENDRECV ||
                media->direction == SDP_DIRECTION_RECVONLY) {
                rx_refresh = TRUE;
            }
            if (media->direction == SDP_DIRECTION_SENDRECV ||
                media->direction == SDP_DIRECTION_SENDONLY) {
                tx_refresh = TRUE;
            }
        }

        lsm_close_rx(lcb, rx_refresh, media);
        lsm_close_tx(lcb, tx_refresh, media);

        if (LSMDebug) {
            /* debug is enabled, format the dest addr into string */
            ipaddr2dotted(addr_str, &media->dest_addr);
            for (i = 0; i < media->num_payloads; i++)
            {
                LSM_DEBUG(DEB_L_C_F_PREFIX"%d rx, tx refresh's are %d %d"
                          ", dir=%d, payload=%p addr=%s, multicast=%d\n",
                          DEB_L_C_F_PREFIX_ARGS(LSM, dcb->line,
                          dcb->call_id, fname), media->refid, rx_refresh,
                          tx_refresh, media->direction,
                          &media->payloads[i], addr_str, media->is_multicast );
            }
        }
        if (rx_refresh ||
            (media->is_multicast &&
             media->direction_set &&
             media->direction == SDP_DIRECTION_RECVONLY)) {
            lsm_rx_start(lcb, caller_fname, media);
        }
        if (tx_refresh) {
            lsm_tx_start(lcb, caller_fname, media);
        }
        if ( rx_refresh &&
              (media->cap_index == CC_VIDEO_1)) {
             // force an additional update so UI can refresh the remote view
             ui_update_video_avail(dcb->line, lcb->ui_id, dcb->cur_video_avail);
             LSM_DEBUG(DEB_L_C_F_PREFIX"Video Avail Called %d",
                  DEB_L_C_F_PREFIX_ARGS(LSM, dcb->line, lcb->ui_id, fname), dcb->cur_video_avail);
         }
    }
}

/**
 *
 * Function to set media attributes and set the ui state.
 *
 * @param lcb        line control block
 * @param line       line
 * @param fname      caller function name
 *
 * @return  none
 *
 * @pre     (dcb not_eq NULL)
 * @pre     (fname not_eq NULL)
 */
static void
lsm_call_state_media (lsm_lcb_t *lcb, line_t line, const char *fname)
{
    fsmcnf_ccb_t   *ccb;
    fsmdef_dcb_t   *dcb;
    call_events     call_state;
    callid_t        call_id = lcb->call_id;

    dcb = lcb->dcb;
    if (dcb == NULL) {
        LSM_ERR_MSG(get_debug_string(DEBUG_INPUT_NULL),
                    "lsm_call_state_media");
        return;
    }

    ccb = fsmcnf_get_ccb_by_call_id(call_id);

    /* Update media parametes to the platform */
    lsm_update_media(lcb, fname);

    if ((ccb != NULL) && (ccb->active == TRUE)) {
        /* For joined call leg, do not change UI state to conf. */
        if ((ccb->flags & JOINED) ||
            (fname == cc_state_name(CC_STATE_RESUME))) {
            call_state = evConnected;
        } else {
            call_state = evConference;
        }
    } else {
        call_state = evConnected;
    }

    /*
     * Possible media changes, update call information and followed
     * by the state update. This is important sequence for 7940/60
     * SIP to force the BTXML update.
     */
    // Commenting out original code for CSCsv72370. Leaving here for reference.
    // lsm_internal_update_call_info(lcb, dcb);

    lsm_ui_call_state(call_state, line, lcb, CC_CAUSE_NORMAL);
    // CSCsv72370 - Important sequence for TNP this follows state change
    lsm_internal_update_call_info(lcb, dcb);
}


static cc_rcs_t
lsm_connected (lsm_lcb_t *lcb, cc_state_data_connected_t *data)
{
    callid_t        call_id = lcb->call_id;
    line_t          line = lcb->line;
    fsmdef_dcb_t   *dcb;
    int             alerting = YES;
    call_events     original_call_event;
    int ringSettingBusyStationPolicy;
    boolean tone_stop_bool = TRUE;
    int             sdpmode = 0;
    boolean         start_ice = FALSE;

    config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));

    dcb = lcb->dcb;
    if (dcb == NULL) {
        return (CC_RC_ERROR);
    }

    original_call_event = lcb->previous_call_event;
    /*
     * If a held call is being resumed, check the
     * policy to see if the phone should resume alerting.
     */
    if (lcb->state == LSM_S_HOLDING) {
        config_get_value(CFGID_RING_SETTING_BUSY_POLICY,
                     &ringSettingBusyStationPolicy,
                     sizeof(ringSettingBusyStationPolicy));
        if (0 == ringSettingBusyStationPolicy) {
            alerting = NO;
        }

		/*
		 * CSCtd31671: When agent phone resumes from a held call with
	     * monitor warning tone, the tone should not be stopped.
	     */
		if(lcb->dcb->active_tone == VCM_MONITORWARNING_TONE || lcb->dcb->active_tone == VCM_RECORDERWARNING_TONE)
			tone_stop_bool = FALSE;
    }

    /* Don't try to start ICE unless this is the first time connecting.
     *  TODO(ekr@rtfm.com): Is this the right ICE start logic? What about restarts
    */
    if (strlen(dcb->peerconnection) && lcb->state != LSM_S_CONNECTED)
      start_ice = TRUE;

    lsm_change_state(lcb, __LINE__, LSM_S_CONNECTED);

    if (!sdpmode) {
        if (tone_stop_bool == TRUE)
            (void) lsm_stop_tone(lcb, NULL);
    }

    /* Start ICE */
    if (start_ice) {
      short res = vcmStartIceChecks(dcb->peerconnection, !dcb->inbound);

      /* TODO(emannion): Set state to dead here. */
      if (res)
        return CC_RC_SUCCESS;
    }

    /*
     * Open the RTP receive channel.
     */
    lsm_call_state_media(lcb, line, cc_state_name(CC_STATE_CONNECTED));


    if (!sdpmode) {
        vcmEnableSidetone(YES);

        lsm_set_ringer(lcb, call_id, line, alerting);
    }

    FSM_RESET_FLAGS(lcb->flags, LSM_FLAGS_ANSWER_PENDING);
    FSM_RESET_FLAGS(lcb->flags, LSM_FLAGS_DUSTING);

    /*
     * update placed call info in call history with dialed digits
     */
    if (dcb->placed_call_update_required) {
        lsm_update_placed_callinfo(dcb);
        dcb->placed_call_update_required = FALSE;
    }

    /*
     * If UI state was changed, update status line.
     */
    if (lcb->previous_call_event != original_call_event) {
        if (lcb->previous_call_event == evConference) {
        } else {

             ui_set_call_status(platform_get_phrase_index_str(CALL_CONNECTED),
                               line, lcb->ui_id);
        }
    }
    ui_update_video_avail(line, lcb->ui_id, dcb->cur_video_avail);
    return (CC_RC_SUCCESS);
}

/**
 * Function: lsm_hold_reversion
 * Perform Hold Reversion on the given call
 * any other call pending then it should play call waiting tone.
 *
 * @param lsm_lcb_t     lcb for this call
 *
 * @return  cc_rcs_t   SUCCESS or FAILURE of the operation
 *
 * @pre     (lcb not_eq NULL)
 */

static cc_rcs_t
lsm_hold_reversion (lsm_lcb_t *lcb)
{
    callid_t        call_id = lcb->call_id;
    line_t          line = lcb->line;

    // Update call state on the JAVA side
    lsm_ui_call_state(evHoldRevert, line, lcb, CC_CAUSE_NORMAL);

    if (lsm_find_state(LSM_S_RINGIN) > CC_NO_CALL_ID) {
        // No Reversion ringing if we have calls in ringing state
        return CC_RC_SUCCESS;
    }
    ui_set_notification(line, call_id,
                        (char *)INDEX_STR_HOLD_REVERSION, CALL_ALERT_TIMEOUT,
                        FALSE, HR_NOTIFY_PRI);
    lsm_reversion_ringer(lcb, call_id, line);

    return (CC_RC_SUCCESS);
}

/*
 * lsm_hold_local
 *
 * Move the phone into the Hold state.
 *
 * Function is used when the local side initiated the hold.
 */
static cc_rcs_t
lsm_hold_local (lsm_lcb_t *lcb, cc_state_data_hold_t *data)
{
    callid_t        call_id = lcb->call_id;
    line_t          line = lcb->line;
    fsmdef_dcb_t   *dcb;
    cc_causes_t    cause;
    int ringSettingBusyStationPolicy;

    dcb = lcb->dcb;
    if (dcb == NULL) {
        return (CC_RC_ERROR);
    }

    /*
     * Stop ringer if spoofing ringout for CCM
     */
    if (dcb->spoof_ringout_applied) {
        (void) lsm_stop_tone(lcb, NULL);
    }

    /* hard close receive and transmit channels for all media entries */
    lsm_close_rx(lcb, FALSE, NULL);
    lsm_close_tx(lcb, FALSE, NULL);
    /*
     * Note that local hold does not have any newer UI information from the
     * network. Note need to update the call information and the UI will
     * be collapsed with "blocked" icon to indicate hold.
     */

    lsm_change_state(lcb, __LINE__, LSM_S_HOLDING);
    /* Round table phones need cause for the transfer or conference
     Do not set the cause if the conference or transfer is created by
     remote-cc
     */
    cause = CC_CAUSE_NORMAL;
    if (data->reason == CC_REASON_XFER) {
            cause = CC_CAUSE_XFER_LOCAL;
    } else if (data->reason == CC_REASON_CONF) {
        cause = CC_CAUSE_CONF;
    }

    lsm_ui_call_state(evHold, line, lcb, cause);

    ui_set_call_status(platform_get_phrase_index_str(CALL_INITIATE_HOLD),
                       line, lcb->ui_id);

    config_get_value(CFGID_RING_SETTING_BUSY_POLICY,
                     &ringSettingBusyStationPolicy,
                     sizeof(ringSettingBusyStationPolicy));
    if (ringSettingBusyStationPolicy) {
        lsm_set_ringer(lcb, call_id, line, YES);
    } else {
        /*
         * If the hold reason is internal this means the phone logic is placing
         * a call on hold, not the user. Thus don't update the alerting for the
         * hold state as the user should not hear the alerting pattern change
         * as they did not place the call on hold. The phone places calls on hold
         * in cases such as the phone has an active call, another call comes in
         * for that line and the new call is answered. Therefore the phone places the
         * active call on hold before answering the incoming call.
         *
         */
        if (data->reason == CC_REASON_INTERNAL) {
            lsm_set_ringer(lcb, call_id, line, NO);
        } else {
            lsm_set_ringer(lcb, call_id, line, YES);
        }
    }

    vcmActivateWlan(FALSE);

    return (CC_RC_SUCCESS);
}


/*
 * lsm_hold_remote
 *
 * Move the phone into the Hold state.
 *
 * Function is used when the remote side initiated the hold.
 */
static cc_rcs_t
lsm_hold_remote (lsm_lcb_t *lcb, cc_state_data_hold_t *data)
{
    static const char fname[] = "lsm_hold_remote";
    callid_t        call_id = lcb->call_id;
    line_t          line = lcb->line;
    const char     *prompt_status;
    fsmdef_dcb_t   *dcb;
    fsmdef_media_t *media;

    dcb = lcb->dcb;
    if (dcb == NULL) {
        return (CC_RC_ERROR);
    }

    /* close and re-open receive channel for all media entries */
    GSMSDP_FOR_ALL_MEDIA(media, dcb) {
        if (!GSMSDP_MEDIA_ENABLED(media)) {
            /* this entry is not active */
            continue;
        }
        if (media->direction_set &&
            media->direction == SDP_DIRECTION_INACTIVE) {
            lsm_close_rx(lcb, FALSE, media);
        } else {
            lsm_close_rx(lcb, TRUE, media);
        }

        /* reopen the receive channel if the direction is RECVONLY */
        if (media->direction_set &&
            media->direction == SDP_DIRECTION_RECVONLY) {
            lsm_rx_start(lcb, fname, media);
        }
        /* close tx if media is not inactive or receive only */
        if ((media->direction == SDP_DIRECTION_INACTIVE) ||
            (media->direction == SDP_DIRECTION_RECVONLY)) {
            lsm_close_tx(lcb, FALSE, media);
        }
    }

    lsm_internal_update_call_info(lcb, dcb);

    lsm_ui_call_state(evRemHold, line, lcb, CC_CAUSE_NORMAL);

    prompt_status = ((lcb->state == LSM_S_CONNECTED) ?
                     platform_get_phrase_index_str(CALL_CONNECTED) :
                     platform_get_phrase_index_str(CALL_INITIATE_HOLD));
    ui_set_call_status(prompt_status, line, lcb->ui_id);

    lsm_set_ringer(lcb, call_id, line, YES);


    return (CC_RC_SUCCESS);
}


static cc_rcs_t
lsm_hold (lsm_lcb_t *lcb, cc_state_data_hold_t *data)
{
    cc_rcs_t cc_rc;

    if (data == NULL) {
        return (CC_RC_ERROR);
    }

    LSM_DEBUG(get_debug_string(LSM_DBG_INT1), lcb->call_id, lcb->line,
              "lsm_hold", "local", data->local);

    switch (data->local) {
    case (TRUE):
        cc_rc = lsm_hold_local(lcb, data);
        break;

    case (FALSE):
        cc_rc = lsm_hold_remote(lcb, data);
        break;

    default:
        cc_rc = CC_RC_ERROR;
        break;
    }
    vcmEnableSidetone(NO);
    return (cc_rc);
}


static cc_rcs_t
lsm_resume_local (lsm_lcb_t *lcb, cc_state_data_resume_t *data)
{
    line_t        line = lcb->line;
    fsmdef_dcb_t *dcb;

    lsm_change_state(lcb, __LINE__, LSM_S_HOLDING);

    dcb = lcb->dcb;
    if (dcb == NULL) {
        return (CC_RC_ERROR);
    }

    ui_set_call_status(platform_get_phrase_index_str(CALL_CONNECTED),
                       line, lcb->ui_id);

    return (CC_RC_SUCCESS);
}


static cc_rcs_t
lsm_resume_remote (lsm_lcb_t *lcb, cc_state_data_resume_t *data)
{
    callid_t      call_id = lcb->call_id;
    line_t        line = lcb->line;
    const char   *prompt_status;

    if (lcb->dcb == NULL) {
        return (CC_RC_ERROR);
    }

    lsm_update_media(lcb, cc_state_name(CC_STATE_RESUME));

    prompt_status = ((lcb->state == LSM_S_CONNECTED) ?
                     platform_get_phrase_index_str(CALL_CONNECTED) :
                     platform_get_phrase_index_str(CALL_INITIATE_HOLD));
    ui_set_call_status(prompt_status, line, lcb->ui_id);

    lsm_set_ringer(lcb, call_id, line, YES);

    return (CC_RC_SUCCESS);
}


static cc_rcs_t
lsm_resume (lsm_lcb_t *lcb, cc_state_data_resume_t *data)
{
    cc_rcs_t cc_rc;

    if (data == NULL) {
        return (CC_RC_ERROR);
    }

    LSM_DEBUG(get_debug_string(LSM_DBG_INT1), lcb->call_id, lcb->line,
              "lsm_resume", "local", data->local);

    switch (data->local) {
    case (TRUE):
        cc_rc = lsm_resume_local(lcb, data);
        break;

    case (FALSE):
        cc_rc = lsm_resume_remote(lcb, data);
        break;

    default:
        cc_rc = CC_RC_ERROR;
        break;
    }

    vcmActivateWlan(TRUE);

    vcmEnableSidetone(YES);
    return (cc_rc);
}


static cc_rcs_t
lsm_onhook (lsm_lcb_t *lcb, cc_state_data_onhook_t *data)
{
    callid_t        call_id = lcb->call_id;
    line_t          line = lcb->line;
    fsmdef_dcb_t   *dcb;
    cc_causes_t     cause;
    int             sdpmode = 0;

	config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));


    dcb = lcb->dcb;
    if (dcb == NULL) {
        return (CC_RC_ERROR);
    }

    dp_int_onhook(line, call_id);

    /* hard close receive and transmit channels for all media entries */
    lsm_close_rx(lcb, FALSE, NULL);
    lsm_close_tx(lcb, FALSE, NULL);

    lsm_change_state(lcb, __LINE__, LSM_S_IDLE);

    if (lsm_is_phone_inactive()) {
        vcmEnableSidetone(NO);
    }

    ui_set_call_status(ui_get_idle_prompt_string(), line, lcb->ui_id);


    (void) lsm_stop_tone(lcb, NULL);

    if (!sdpmode) {
        vcmControlRinger(VCM_RING_OFF, NO, NO, line, dcb->call_id);
    }

    lsm_set_ringer(lcb, call_id, line, YES);

    cause = data->cause;
    if (FSM_CHK_FLAGS(dcb->flags, FSMDEF_F_XFER_COMPLETE)) {
        DEF_DEBUG(DEB_F_PREFIX"Transfer complete.", DEB_F_PREFIX_ARGS(LSM, "lsm_onhook"));
        cause = CC_CAUSE_XFER_COMPLETE;
    }
    lsm_ui_call_state(evOnHook, line, lcb, cause);


    lsm_free_lcb(lcb);

    vcmActivateWlan(FALSE);

    vcmRemoveBandwidth(lsm_get_ms_ui_call_handle(line, call_id, CC_NO_CALL_ID));

    return (CC_RC_SUCCESS);
}

static cc_rcs_t
lsm_call_failed (lsm_lcb_t *lcb, cc_state_data_call_failed_t *data)
{
    callid_t        call_id = lcb->call_id;
    line_t          line = lcb->line;
    vcm_tones_t     tone;
    lsm_states_t    line_state;
    const char     *status = NULL;
    call_events     state;
    boolean         send_call_info = TRUE;
	fsmdef_dcb_t   *dcb;
    boolean         must_log = FALSE;

    dcb = lcb->dcb;
    if (dcb == NULL) {
        return (CC_RC_ERROR);
    }

    /* For busy generated by UI-STATE in 183, do not manipulate the
     * media port
     */
   if (data->cause != CC_CAUSE_UI_STATE_BUSY) {
       /* hard close receive and transmit channels for all media entries */
       lsm_close_rx(lcb, FALSE, NULL);
       lsm_close_tx(lcb, FALSE, NULL);
   }

    switch (data->cause) {
    case (CC_CAUSE_BUSY):
        line_state = LSM_S_BUSY;
        state = evBusy;
        tone = VCM_LINE_BUSY_TONE;
        status = platform_get_phrase_index_str(LINE_BUSY);
        dp_int_update(line, call_id, data->caller_id.called_number);
        send_call_info = FALSE;
        break;

    case (CC_CAUSE_UI_STATE_BUSY):
        line_state = LSM_S_BUSY;
        state = evBusy;
        tone = VCM_LINE_BUSY_TONE;
        dp_int_update(line, call_id, data->caller_id.called_number);
        break;

    case (CC_CAUSE_INVALID_NUMBER):
        line_state = LSM_S_INVALID_NUMBER;
        state = evReorder;
        tone = VCM_REORDER_TONE;
        send_call_info = FALSE;
        break;

    case (CC_CAUSE_CONGESTION):
    case (CC_CAUSE_PAYLOAD_MISMATCH):
        dp_int_update(line, call_id, data->caller_id.called_number);

        /* FALLTHROUGH */
        /*sa_ignore FALL_THROUGH*/
    default:
        send_call_info = FALSE;
        line_state = LSM_S_CONGESTION;
        state = evReorder;
        tone = VCM_REORDER_TONE;
        if ( (data->cause == CC_CAUSE_NO_USER_ANS)||
             (data->cause == CC_TEMP_NOT_AVAILABLE) ) {
           must_log = TRUE;
        }
        break;
    }

    lsm_change_state(lcb, __LINE__, line_state);

    if (status) {
        ui_set_call_status(status, line, lcb->ui_id);
    }

    if (state == evReorder && !must_log) {
        ui_log_disposition(dcb->call_id, CC_CALL_LOG_DISP_IGNORE);
    }

    /* Send call info only if not error */
    if (send_call_info == TRUE) {
        ui_call_info(data->caller_id.calling_name,
                 data->caller_id.calling_number,
                 data->caller_id.alt_calling_number,
                 data->caller_id.display_calling_number,
                 data->caller_id.called_name,
                 data->caller_id.called_number,
                 data->caller_id.display_called_number,
                 data->caller_id.orig_called_name,
                 data->caller_id.orig_called_number,
                 data->caller_id.last_redirect_name,
                 data->caller_id.last_redirect_number,
                 (calltype_t)dcb->call_type,
                 line, lcb->ui_id,
                 dcb->caller_id.call_instance_id,
                 FSM_GET_SECURITY_STATUS(dcb),
                 FSM_GET_POLICY(dcb));
    }

    lsm_ui_call_state(state, line, lcb, CC_CAUSE_NORMAL);

    /* Tone played in remote-cc play tone request, so don't start tone again
     */
    if ((data->cause != CC_CAUSE_UI_STATE_BUSY) && (data->cause != CC_CAUSE_REMOTE_DISCONN_REQ_PLAYTONE)) {
        fsmdef_media_t *audio_media = gsmsdp_find_audio_media(dcb);

        lsm_util_start_tone(tone, FALSE, lsm_get_ms_ui_call_handle(line, call_id, CC_NO_CALL_ID), dcb->group_id,
                       ((audio_media != NULL) ? audio_media->refid :
                                                CC_NO_MEDIA_REF_ID),
                       VCM_PLAY_TONE_TO_EAR);
    }

    return (CC_RC_SUCCESS);
}

static void
lsm_ringer (lsm_lcb_t *lcb, cc_action_data_ringer_t *data)
{
    vcm_ring_mode_t ringer;
    line_t line = lcb->line;

    ringer = (data->on == FALSE) ? (VCM_RING_OFF) : (VCM_FEATURE_RING);

    LSM_DEBUG(DEB_F_PREFIX"CTI RING SETTING: line = %d, ringer Mode = %s,"
              "Ring once = NO, alertInfo = NO\n", DEB_F_PREFIX_ARGS(LSM, "lsm_ringer"),
              line, vm_alert_names[ringer]);

    vcmControlRinger(ringer, NO, NO, line, lcb->call_id);
}

static cc_rcs_t
lsm_dial_mode (lsm_lcb_t *lcb, cc_action_data_dial_mode_t *data)
{
    return (CC_RC_SUCCESS);
}


static cc_rcs_t
lsm_mwi (lsm_lcb_t *lcb, callid_t call_id, line_t line,
         cc_action_data_mwi_t *data)
{
    ui_set_mwi(line, data->on, data->type, data->newCount, data->oldCount, data->hpNewCount, data->hpOldCount);

    return (CC_RC_SUCCESS);
}


/*
 *  Function: lsm_update_ui
 *
 *  Parameters:
 *      call_id:
 *      line:
 *      data:
 *
 *  Description: This function is used to hide the UI platform details from
 *               the FSMs. This function is provided to allow the FSMs
 *               to update the UI in certain cases.
 *
 *  Returns: rc
 *
 */
cc_rcs_t
lsm_update_ui (lsm_lcb_t *lcb, cc_action_data_update_ui_t *data)
{
    callid_t        call_id = lcb->call_id;
    line_t          line = lcb->line;
    lsm_states_t    instance_state;
    call_events     call_state = evMaxEvent;
    fsmcnf_ccb_t   *ccb;
    fsmdef_dcb_t   *dcb;
    boolean         update = FALSE;
    boolean         inbound;
    cc_feature_data_call_info_t *call_info;
    call_events     original_call_event;
    lsm_lcb_t       *lcb_tmp;
    const char      *conf_str;//[] = {(char)0x80, (char)0x34, (char)0x00};

    instance_state = lcb->state;

    switch (data->action) {
    case CC_UPDATE_CONF_ACTIVE:

        switch (instance_state) {
        case LSM_S_RINGOUT:
            call_state = evRingOut;
            break;

        case LSM_S_CONNECTED:
        default:
            ccb = fsmcnf_get_ccb_by_call_id(call_id);
            if ((ccb != NULL) && (ccb->active == TRUE)) {

                conf_str = platform_get_phrase_index_str(UI_CONFERENCE);
                lcb_tmp = lsm_get_lcb_by_call_id(ccb->cnf_call_id);
                dcb = lcb_tmp->dcb;
                ui_call_info(CALL_INFO_NONE,
                              CALL_INFO_NONE,
                              CALL_INFO_NONE,
                              0,
                              conf_str,
                              CALL_INFO_NONE,
                              0,
                              CALL_INFO_NONE,
                              CALL_INFO_NONE,
                              CALL_INFO_NONE,
                              CALL_INFO_NONE,
                              FSMDEF_CALL_TYPE_OUTGOING,
                              dcb->line, lcb_tmp->ui_id,
                              dcb->caller_id.call_instance_id,
                              FSM_GET_SECURITY_STATUS(dcb),
                              FSM_GET_POLICY(dcb));

                call_state = evConference;

            } else if (instance_state == LSM_S_CONNECTED) {

                call_state = evConnected;

            } else {

                call_state = evRingOut;
            }
            break;
        }                       /* switch (instance_state) { */

        break;

    case CC_UPDATE_CALLER_INFO:

        /* For local conference, do not update the primary
         * call bubbles call-info. Primary call is already
         * displaying To conference in this case
         * But dcb-> caller_id should be updated to
         * refresh the UI when the call is dropped
         */
        ccb = fsmcnf_get_ccb_by_call_id(call_id);
        if (ccb && (ccb->flags & LCL_CNF) &&
            (ccb->cnf_call_id == call_id)) {
            break;
        }

        call_info = &data->data.caller_info;
        dcb = lcb->dcb;
        if (dcb == NULL || call_info == NULL) {
            return (CC_RC_ERROR);
        }

        inbound = dcb->inbound;
        if (call_info->feature_flag & CC_ORIENTATION) {
            inbound =
                (call_info->orientation == CC_ORIENTATION_FROM) ? TRUE : FALSE;
            update = TRUE;
        }

        if (call_info->feature_flag & CC_CALLER_ID) {
            update = TRUE;
            /*
             * This "if" block, without the "&& inbound" condition, was put in by Serhad
             * to fix CSCsm58054 and it results in CSCso98110. The "inbound" condition
             * is added to narrow the scope of CSCsm58054's fix. Note that "inbound" here
             * refers to the perceived orientation set in call info. So for example, in case
             * of a 3-way conf, and phone is the last party to receive the call, is ringing
             * and then be joined into a conference, direction would be outbound. The display
             * would say "To Conference".
             */
            if ( (instance_state == LSM_S_RINGIN) && inbound ) {
                cc_state_data_alerting_t alerting_data;

                alerting_data.caller_id = dcb->caller_id;
                lsm_update_inalert_status(line, lcb->ui_id, &alerting_data, TRUE);
            }
        }

        if (call_info->feature_flag & CC_CALL_INSTANCE) {
            update = TRUE;
        }

        if (call_info->feature_flag & CC_SECURITY) {
            update = TRUE;
        }

	if (call_info->feature_flag & CC_POLICY) {
	    update = TRUE;
	}

        /*
         * If we are going to spoof ring out, skip the explicit UI update.
         * the far end alerting handling will update the UI. Do not
         * update UI twice.
         */
        if (dcb->spoof_ringout_requested &&
            !dcb->spoof_ringout_applied &&
            lcb->state == LSM_S_CONNECTED) {
            cc_state_data_far_end_alerting_t alerting_data;

            alerting_data.caller_id = dcb->caller_id;
            (void) lsm_far_end_alerting(lcb, &alerting_data);
            dcb->spoof_ringout_applied = TRUE;
        } else if (update && dcb->ui_update_required) {

            calltype_t call_type;

            if (dcb->call_type == FSMDEF_CALL_TYPE_FORWARD) {
                call_type = (inbound) ? (calltype_t)dcb->call_type:FSMDEF_CALL_TYPE_OUTGOING;
            } else {
                if (inbound) {
                    call_type = FSMDEF_CALL_TYPE_INCOMING;
                } else {
                    call_type = FSMDEF_CALL_TYPE_OUTGOING;
                }
            }

             ui_call_info(dcb->caller_id.calling_name,
                          dcb->caller_id.calling_number,
                          dcb->caller_id.alt_calling_number,
                          dcb->caller_id.display_calling_number,
                          dcb->caller_id.called_name,
                          dcb->caller_id.called_number,
                          dcb->caller_id.display_called_number,
                          dcb->caller_id.orig_called_name,
                          dcb->caller_id.orig_called_number,
                          dcb->caller_id.last_redirect_name,
                          dcb->caller_id.last_redirect_number,
                          call_type,
                          line,
                          lcb->ui_id,
                          dcb->caller_id.call_instance_id,
                          FSM_GET_SECURITY_STATUS(dcb),
                          FSM_GET_POLICY(dcb));

            dcb->ui_update_required = FALSE;

            conf_str = platform_get_phrase_index_str(UI_CONFERENCE);
 	    if(cpr_strncasecmp(dcb->caller_id.called_name, conf_str, strlen(conf_str)) == 0){
 		    dcb->is_conf_call = TRUE;
 	    } else {
 		    dcb->is_conf_call = FALSE;
 	    }
        }

        break;

    case CC_UPDATE_SET_CALL_STATUS:
        {
            /* set call status line */
            cc_set_call_status_data_t *call_status_p =
                &data->data.set_call_status_parms;
            ui_set_call_status(call_status_p->phrase_str_p, call_status_p->line,
                               lcb->ui_id);
            break;
        }
    case CC_UPDATE_SET_NOTIFICATION:
        {
            /* set status line notification */
            cc_set_notification_data_t *call_notification_p =
                &data->data.set_notification_parms;
            ui_set_notification(line, lcb->ui_id,
                                call_notification_p->phrase_str_p,
                                call_notification_p->timeout, FALSE,
                                (char)call_notification_p->priority);
            break;
        }
    case CC_UPDATE_CLEAR_NOTIFICATION:
        /* clear status line notification */
        ui_clear_notification();
        break;

    case CC_UPDATE_SECURITY_STATUS:
        /* update security status */
        break;

    case CC_UPDATE_XFER_PRIMARY:
        call_state = evConnected;
        break;

    case CC_UPDATE_CALL_PRESERVATION:

        /* Call is in preservation mode. Update UI so that only endcall softkey is available */
        ui_call_in_preservation(line, lcb->ui_id);
        break;

    case CC_UPDATE_CALL_CONNECTED:
        if (instance_state == LSM_S_CONNECTED) {
            call_state = evConnected;
        }
        break;

    case CC_UPDATE_CONF_RELEASE:
        dcb = lcb->dcb;

        if (instance_state == LSM_S_CONNECTED) {
            call_state = evConnected;

        } else if (instance_state == LSM_S_RINGOUT) {
            call_state = evRingOut;
        }

        /*
         * If we are going to spoof ring out, skip the explicit UI update.
         * the far end alerting handling will update the UI. Do not
         * update UI twice.
         */
        if (dcb->spoof_ringout_requested &&
            !dcb->spoof_ringout_applied &&
            lcb->state == LSM_S_CONNECTED) {
            cc_state_data_far_end_alerting_t alerting_data;

            alerting_data.caller_id = dcb->caller_id;
            (void) lsm_far_end_alerting(lcb, &alerting_data);
            dcb->spoof_ringout_applied = TRUE;

            call_state = evRingOut;

        } else {
            calltype_t call_type;
            if (dcb->orientation == CC_ORIENTATION_FROM) {
                call_type = FSMDEF_CALL_TYPE_INCOMING;
            } else if (dcb->orientation == CC_ORIENTATION_TO) {
                call_type = FSMDEF_CALL_TYPE_OUTGOING;
            } else {
                call_type = (calltype_t)(dcb->call_type);
            }
            ui_call_info(dcb->caller_id.calling_name,
                          dcb->caller_id.calling_number,
                          dcb->caller_id.alt_calling_number,
                          dcb->caller_id.display_calling_number,
                          dcb->caller_id.called_name,
                          dcb->caller_id.called_number,
                          dcb->caller_id.display_called_number,
                          dcb->caller_id.orig_called_name,
                          dcb->caller_id.orig_called_number,
                          dcb->caller_id.last_redirect_name,
                          dcb->caller_id.last_redirect_number,
                          call_type,
                          line,
                          lcb->ui_id,
                          dcb->caller_id.call_instance_id,
                          FSM_GET_SECURITY_STATUS(dcb),
                          FSM_GET_POLICY(dcb));
        }


        break;

    default:
        break;
    }

    if (call_state != evMaxEvent) {
        original_call_event = lcb->previous_call_event;

        lsm_ui_call_state(call_state, line, lcb, CC_CAUSE_NORMAL);
        if (original_call_event != call_state) {
            /* Call state changed, take care of special event */
            switch (call_state) {
            case evConference:
                break;

            case evConnected:
            case evWhisper:
                ui_set_call_status(
                           platform_get_phrase_index_str(CALL_CONNECTED),
                           line, lcb->ui_id);
                break;

            default:
                break;
            }
        }
    }

    return (CC_RC_SUCCESS);
}


/*
 *  Function: lsm_update_placed_callinfo
 *
 *  Description: this helps log dialed digits (as opposed to RPID provided
 *               value) into placed calls.  This also decides whether to
 *               log called party name received in RPID.
 *
 *  Parameters: dcb - pointer to default SM control block
 *
 *  Returns: none
 *
 */
#define CISCO_PLAR_STRING  "x-cisco-serviceuri-offhook"
void
lsm_update_placed_callinfo (void *data)
{
    const char     *tmp_called_number = NULL;
    const char     *called_name = NULL;
    fsmdef_dcb_t   *dcb = NULL;
    lsm_lcb_t      *lcb;
    static const char fname[] = "lsm_update_placed_callinfo";
    boolean has_called_number = FALSE;

    LSM_DEBUG(DEB_F_PREFIX"Entering ...", DEB_F_PREFIX_ARGS(LSM, fname));
    dcb = (fsmdef_dcb_t *) data;
    lcb = lsm_get_lcb_by_call_id(dcb->call_id);
    if (lcb == NULL) {
        LSM_DEBUG(DEB_F_PREFIX"Exiting: lcb not found", DEB_F_PREFIX_ARGS(LSM, fname));
        return;
    }

    if (dcb->caller_id.called_number != NULL &&
        dcb->caller_id.called_number[0] != NUL) {
        has_called_number = TRUE;
    }


    tmp_called_number = lsm_get_gdialed_digits();


    /* if tmp_called_number is NULL or empty, return */
    if (tmp_called_number == NULL || (*tmp_called_number) == NUL) {
        LSM_DEBUG(DEB_L_C_F_PREFIX"Exiting : dialed digits is empty",
                  DEB_L_C_F_PREFIX_ARGS(LSM, lcb->line, lcb->call_id, fname));
        return;
    }

    /*
     * if tmp_called_number is same as what we receive in RPID,
     * then get the called name from RPID if provided.
     */
    if (has_called_number) {
        if (strcmp(tmp_called_number, CISCO_PLAR_STRING) == 0) {
            tmp_called_number = dcb->caller_id.called_number;
        }
        /* if RPID number matches, dialed digits, use RPID name */
        if (strcmp(dcb->caller_id.called_number, tmp_called_number) == 0) {
            called_name = dcb->caller_id.called_name;
        } else {
        	char tmp_str[STATUS_LINE_MAX_LEN];
        	platGetPhraseText(STR_INDEX_ANONYMOUS_SPACE, (char *)tmp_str, STATUS_LINE_MAX_LEN - 1);
            if(strcmp(dcb->caller_id.called_number,tmp_str) == 0
               && strcmp(dcb->caller_id.orig_rpid_number, tmp_called_number) == 0
               && strcmp(dcb->caller_id.called_name, platform_get_phrase_index_str(UI_UNKNOWN)) != 0) {
        	  called_name = dcb->caller_id.called_name;
            }
        }
    }
    ui_update_placed_call_info(lcb->line, lcb->call_id, called_name,
                               tmp_called_number);
    LSM_DEBUG(DEB_L_C_F_PREFIX"Exiting: invoked ui_update_placed_call_info()",
              DEB_L_C_F_PREFIX_ARGS(LSM, lcb->line, lcb->call_id, fname));
}

cc_int32_t
lsm_show_cmd (cc_int32_t argc, const char *arv[])
{
    int             i = 0;
    lsm_lcb_t      *lcb;

    debugif_printf("\n------------------ LSM lcbs -------------------");
    debugif_printf("\ni   call_id  line  state             lcb");
    debugif_printf("\n-----------------------------------------------\n");

    FSM_FOR_ALL_CBS(lcb, lsm_lcbs, LSM_MAX_LCBS) {
        debugif_printf("%-2d  %-7d  %-4d  %-16s  0x%8p\n",
                       i++, lcb->call_id, lcb->line,
                       lsm_state_name(lcb->state), lcb);

    }

    return (0);
}

void
lsm_init_config (void)
{
    /*
     * The silent period between call waiting bursts is now configurable
     * for TNP phones. Store away the value for the callwaiting code to use.
     * The config is in seconds, but CPR expects the duration in milliseconds
     * thus multiply the config value by 1000. Non-TNP phones default to
     * 10 seconds.
     */
    config_get_value(CFGID_CALL_WAITING_SILENT_PERIOD, &callWaitingDelay,
                     sizeof(callWaitingDelay));
    callWaitingDelay = callWaitingDelay * 1000;
}

void
lsm_init (void)
{
    static const char fname[] = "lsm_init";
    lsm_lcb_t *lcb;
    int i;

    /*
     * Init the lcbs.
     */
    lsm_lcbs = (lsm_lcb_t *) cpr_calloc(LSM_MAX_LCBS, sizeof(lsm_lcb_t));
    if (lsm_lcbs == NULL) {
        LSM_ERR_MSG(LSM_F_PREFIX"lsm_lcbs cpr_calloc returned NULL", fname);
        return;
    }

    FSM_FOR_ALL_CBS(lcb, lsm_lcbs, LSM_MAX_LCBS) {
        lsm_init_lcb(lcb);
    }

    /*
     * Create tones and continous tone timer. The same call back function
     * is utilized for each of these timers.
     */
    lsm_tmr_tones = cprCreateTimer("lsm_tmr_tones",
                                   GSM_MULTIPART_TONES_TIMER,
                                   TIMER_EXPIRATION, gsm_msgq);
    lsm_continuous_tmr_tones = cprCreateTimer("lsm_continuous_tmr_tones",
                                              GSM_CONTINUOUS_TONES_TIMER,
                                              TIMER_EXPIRATION,
                                              gsm_msgq);
    lsm_tone_duration_tmr = cprCreateTimer("lsm_tone_duration_tmr",
                                   		   GSM_TONE_DURATION_TIMER,
                                   		   TIMER_EXPIRATION, gsm_msgq);
    lsm_init_config();

    for (i=0 ; i<MAX_REG_LINES; i++) {
        lsm_call_perline[i] = 0;
    }

    memset(cfwdall_state_in_ccm_mode, 0, sizeof(cfwdall_state_in_ccm_mode));
}

void
lsm_shutdown (void)
{
    (void) cprDestroyTimer(lsm_tmr_tones);

    (void) cprDestroyTimer(lsm_continuous_tmr_tones);

    cpr_free(lsm_lcbs);
}

/**
 *
 * Peform reset for lsm, include all the variables that has to be reset
 *
 * @param none
 *
 * @return  none
 *
 */
void
lsm_reset (void)
{
    line_t line;
    int    i;
    lsm_lcb_t *lcb;

    lsm_init_config();

    FSM_FOR_ALL_CBS(lcb, lsm_lcbs, LSM_MAX_LCBS) {
        lsm_init_lcb(lcb);
    }

    for (i=0 ; i<MAX_REG_LINES; i++) {
        lsm_call_perline[i] = 0;
    }

    for (line=0; line < MAX_REG_LINES+1; line++) {

        cc_line_ringer_mode[line] = CC_RING_DEFAULT;
    }
}

/*
 * cc_call_attribute
 * This sets call attribute. During the conf or xfer consultation phase,
 * far end of the 1st call may disconnect the call. In this case phone has to
 * display the regular call softkey set instead of consultation softkey set.
 * For this reason GSM will call cc_call_attribute to the remaining call,
 * when original call is disconnected.
 */
void
cc_call_attribute (callid_t call_id, line_t line, call_attr_t attribute)
{
    static const char fname[] = "cc_call_attribute";


    LSM_DEBUG(DEB_L_C_F_PREFIX"attribute=%d",
			  DEB_L_C_F_PREFIX_ARGS(LSM, line, call_id, fname), attribute);

    ui_set_call_attr(line, call_id, attribute);
}


/*
 * lsm_call_state
 * This routine is responsible for responding to requests from the CSM and
 * doing whatever is required via platform specific routines.
 */
void
cc_call_state (callid_t call_id, line_t line, cc_states_t state,
               cc_state_data_t *data)
{
    static const char fname[] = "cc_call_state";
    cc_rcs_t   result = CC_RC_SUCCESS;
    lsm_lcb_t *lcb;

    LSM_DEBUG(get_debug_string(LSM_DBG_ENTRY), call_id, line,
              cc_state_name(state));

    lcb = lsm_get_lcb_by_call_id(call_id);

    if (lcb == NULL) {
        LSM_DEBUG(get_debug_string(DEBUG_INPUT_NULL), fname);
        return;
    }

    switch (state) {
    case CC_STATE_OFFHOOK:
        result = lsm_offhook(lcb, &(data->offhook));
#ifdef TEST
        test_dial_calls(line, call_id, 500, "10011234");
#endif
        break;

    case CC_STATE_DIALING:
        result = lsm_dialing(lcb, &(data->dialing));
        break;

    case CC_STATE_DIALING_COMPLETED:
        result = lsm_dialing_completed(lcb, &(data->dialing_completed));
        break;

    case CC_STATE_CALL_SENT:
        result = lsm_call_sent(lcb, &(data->call_sent));
        break;

    case CC_STATE_FAR_END_PROCEEDING:
        result = lsm_far_end_proceeding(lcb, &(data->far_end_proceeding));
        break;

    case CC_STATE_FAR_END_ALERTING:
        result = lsm_far_end_alerting(lcb, &(data->far_end_alerting));
        break;

    case CC_STATE_CALL_RECEIVED:
        result = lsm_call_received(lcb, &(data->call_received));
        break;

    case CC_STATE_ALERTING:
        result = lsm_alerting(lcb, &(data->alerting));
        break;

    case CC_STATE_ANSWERED:
        result = lsm_answered(lcb, &(data->answered));
        break;

    case CC_STATE_CONNECTED:
        result = lsm_connected(lcb, &(data->connected));
#ifdef TEST
        test_disc_call(line, call_id);
        test_line_offhook(line, cc_get_new_call_id());
#endif
        break;

    case CC_STATE_HOLD:
        result = lsm_hold(lcb, &(data->hold));
        break;

    case CC_STATE_HOLD_REVERT:
        result = lsm_hold_reversion(lcb);
        break;

    case CC_STATE_RESUME:
        result = lsm_resume(lcb, &(data->resume));
        break;

    case CC_STATE_ONHOOK:
        result = lsm_onhook(lcb, &(data->onhook));
        break;

    case CC_STATE_CALL_FAILED:
        result = lsm_call_failed(lcb, &(data->call_failed));
        break;

    default:
        break;
    }

    if (result == CC_RC_ERROR) {
        LSM_DEBUG(get_debug_string(LSM_DBG_CC_ERROR), call_id, line, fname,
                  state, data);
    }

    return;
}

static cc_rcs_t
lsm_media (lsm_lcb_t *lcb, callid_t call_id, line_t line)
{
    fsmdef_dcb_t *dcb;

    dcb = lcb->dcb;
    if (dcb == NULL) {
        return (CC_RC_ERROR);
    }

    if (!dcb->spoof_ringout_requested) {
        lsm_update_media(lcb, "MEDIA");
        vcmEnableSidetone(YES);
    } else if (!dcb->spoof_ringout_applied &&
               (lcb->state == LSM_S_CONNECTED)) {
        cc_state_data_far_end_alerting_t alerting_data;

        alerting_data.caller_id = dcb->caller_id;
        (void) lsm_far_end_alerting(lcb, &alerting_data);
        dcb->spoof_ringout_applied = TRUE;
    }

    return (CC_RC_SUCCESS);
}

/*
 *  Function: lsm_stop_media
 *
 *  Parameters:
 *     lcb     - pointer to lsm_lcb_t,
 *     call_id - gsm call id for the call in used.
 *     line    - line_t for the line number (dn line).
 *     data    - action data.
 *
 *  Description:
 *     The function simply stops media (close Rx and Tx) and set the
 *  proper ringer.
 *
 *  Returns:   None.
 */
static void
lsm_stop_media (lsm_lcb_t *lcb, callid_t call_id, line_t line,
                cc_action_data_t *data)
{
    static const char fname[] = "lsm_stop_media";
    fsmdef_dcb_t *dcb;
    fsmdef_media_t *media;

    dcb = lcb->dcb;
    if (dcb == NULL) {
        LSM_DEBUG(get_debug_string(DEBUG_INPUT_NULL), fname);
        return;
    }

    /* hard close receive and transmit channels */
    if ((data == NULL) ||
        (data->stop_media.media_refid == CC_NO_MEDIA_REF_ID)) {
        /* no data provided or no specific ref ID, defaul to all entries  */
        lsm_close_rx(lcb, FALSE, NULL);
        lsm_close_tx(lcb, FALSE, NULL);
    } else {
        /* look up the media entry for the given reference ID */
        media = gsmsdp_find_media_by_refid(dcb,
                                           data->stop_media.media_refid);
        if (media != NULL) {
            lsm_close_rx(lcb, FALSE, media);
            lsm_close_tx(lcb, FALSE, media);
        } else {
            /* no entry found */
            LSM_DEBUG(DEB_L_C_F_PREFIX"no media with reference ID %d found",
                      DEB_L_C_F_PREFIX_ARGS(LSM, dcb->line, dcb->call_id, fname),
					  data->stop_media.media_refid);
            return;
        }
    }
    lsm_set_ringer(lcb, call_id, line, YES);
}



/*
 * lsm_add_remote_stream
 *
 * Description:
 *    The function adds a remote stream to the media subsystem
 *
 * Parameters:
 *   [in]  line - line
 *   [in]  call_id - GSM call ID
 *   [in]  media - media line to add as remote stream
 *   [out] pc_stream_id
 *
 * Returns: CC_RC_SUCCESS or CC_RC_ERROR if unable to create or add stream
 */
cc_rcs_t
lsm_add_remote_stream (line_t line, callid_t call_id, fsmdef_media_t *media, int *pc_stream_id)
{
    lsm_lcb_t *lcb;
    fsmdef_dcb_t *dcb;
    int vcm_ret;

    lcb = lsm_get_lcb_by_call_id(call_id);
    if (!lcb) {
        CSFLogError(logTag, "%s: lcb is null", __FUNCTION__);
        return CC_RC_ERROR;
    }

    dcb = lcb->dcb;
    if (!dcb) {
        CSFLogError(logTag, "%s: dcb is null", __FUNCTION__);
        return CC_RC_ERROR;
    }

    vcm_ret = vcmCreateRemoteStream(media->cap_index, dcb->peerconnection,
            pc_stream_id);

    if (vcm_ret) {
        CSFLogError(logTag, "%s: vcmCreateRemoteStream returned error: %d",
            __FUNCTION__, vcm_ret);
        return CC_RC_ERROR;
    }

    return CC_RC_SUCCESS;
}

/*
 * lsm_initialize_datachannel
 *
 * Description:
 *    The function initializes the datachannel with port and
 *    protocol info.
 *
 * Parameters:
 *   [in]  dcb - pointer to get the peerconnection id
 *   [in]  media - pointer to get the datachannel info
 *   [in]  track_id - track ID (aka m-line number)
 * Returns: None
 */
void lsm_initialize_datachannel (fsmdef_dcb_t *dcb, fsmdef_media_t *media,
                                 int track_id)
{
    if (!dcb) {
        CSFLogError(logTag, "%s DCB is NULL", __FUNCTION__);
        return;
    }

    if (!media) {
        CSFLogError(logTag, "%s media is NULL", __FUNCTION__);
        return;
    }

    /*
     * have access to media->cap_index, media->streams, media->protocol,
     * media->local/remote_datachannel_port
     */
    vcmInitializeDataChannel(dcb->peerconnection,
        track_id, media->datachannel_streams,
        media->local_datachannel_port, media->remote_datachannel_port,
        media->datachannel_protocol);
}

/**
 *
 * Peform non call related action
 *
 * @param line_t     line
 * @param callid_t      gsm_id
 * @param action        type of action
 * @param cc_action_data_t        line
 *
 * @return  true if the action has been peformed, else false
 *
 * @pre     (action == CC_ACTION_MWI_LAMP_ONLY || CC_ACTION_SET_LINE_RINGER ||
                       CC_ACTION_PLAY_BLF_ALERTING_TONE)
 */
static boolean
cc_call_non_call_action (callid_t call_id, line_t line,
                         cc_actions_t action, cc_action_data_t *data)
{
    /* Certain requests are device based and does not contain any
     * line number and call_id associated with it. So handle thoese
     * requests here
     */
    switch (action) {

    case CC_ACTION_MWI_LAMP_ONLY:
        if (data != NULL) {
            ui_change_mwi_lamp(data->mwi.on);
            return(TRUE);
        }
        break;

    case CC_ACTION_SET_LINE_RINGER:

        if (data != NULL) {
            return(TRUE);
        }
        break;

    case CC_ACTION_PLAY_BLF_ALERTING_TONE:
        lsm_play_tone(CC_FEATURE_BLF_ALERT_TONE);
        return TRUE;

    default:
        break;
    }

    return(FALSE);
}

/*
 * LSM API supports various actions such as play tone, stop tone,
 * direct media operation etc.
 *
 * @param[in] call_id   GSM call ID of an active call.
 * @param[in] line      line number of the line_t type.
 * @param[in] action    cc_actions_t for the desired action.
 * @param[in] data      cc_action_data_t data or parameters that may be
 *                      required for certain action.
 *
 * @return              cc_rcs_t status.
 *
 * @pre                 line not_eqs CC_NO_LINE
 * @pre                 ((action equals CC_ACTION_PLAY_TONE) or
 *                       (action equals CC_ACTION_STOP_TONE) or
 *                       (action equals CC_ACTION_DIAL_MODE) or
 *                       (action equals CC_ACTION_MWI) or
 *                       (action equals CC_ACTION_OPEN_RCV) or
 *                       (action equals CC_ACTION_UPDATE_UI) or
 *                       (action equals CC_ACTION_RINGER))
 */
cc_rcs_t
cc_call_action (callid_t call_id, line_t line, cc_actions_t action,
               cc_action_data_t *data)
{
    static const char fname[] = "cc_call_action";
    cc_rcs_t   result = CC_RC_SUCCESS;
    lsm_lcb_t *lcb;
    fsmdef_dcb_t *dcb;
    fsmdef_media_t *media;

    LSM_DEBUG(get_debug_string(LSM_DBG_ENTRY), call_id, line,
              cc_action_name(action));

    /* perform non call related actions. lcb is not required
     * for these actions
     */
    if (cc_call_non_call_action(call_id, line, action, data)) {
        return (result);
    }

    lcb = lsm_get_lcb_by_call_id(call_id);

    if ((lcb == NULL) && (action != CC_ACTION_MWI)) {
        LSM_DEBUG(get_debug_string(DEBUG_INPUT_NULL), fname);
        return (CC_RC_ERROR);
    }

    switch (action) {
    case CC_ACTION_PLAY_TONE:
        if (data != NULL) {
            result = lsm_start_tone(lcb, &(data->tone));
        } else {
            result = CC_RC_ERROR;
        }
        break;

    case CC_ACTION_STOP_TONE:
        if (data != NULL) {
            result = lsm_stop_tone(lcb, &(data->tone));
        } else {
            result = CC_RC_ERROR;
        }
        break;

    case CC_ACTION_SPEAKER:
        break;

    case CC_ACTION_DIAL_MODE:
        if (data != NULL) {
            result = lsm_dial_mode(lcb, &(data->dial_mode));
        } else {
            result = CC_RC_ERROR;
        }
        break;

    case CC_ACTION_MWI:
        if (data != NULL) {
            result = lsm_mwi(NULL, call_id, line, &(data->mwi));
        } else {
            result = CC_RC_ERROR;
        }
        break;

    case CC_ACTION_OPEN_RCV:
        if (data != NULL) {
            result = lsm_open_rx(lcb, &(data->open_rcv), NULL);
        } else {
            result = CC_RC_ERROR;
        }
        break;

    case CC_ACTION_UPDATE_UI:
        if (data != NULL) {
            result = lsm_update_ui(lcb, &(data->update_ui));
        } else {
            result = CC_RC_ERROR;
        }
        break;

    case CC_ACTION_MEDIA:
        result = lsm_media(lcb, call_id, line);
        break;

    case CC_ACTION_RINGER:
        if (data != NULL) {
            lsm_ringer(lcb, &(data->ringer));
        }
        break;

    case CC_ACTION_STOP_MEDIA:
        lsm_stop_media(lcb, call_id, line, data);
        break;

    case CC_ACTION_START_RCV:
        /* start receiving */
        dcb = lcb->dcb;
        if (dcb == NULL) {
            /* No call ID */
            result = CC_RC_ERROR;
            break;
        }

        GSMSDP_FOR_ALL_MEDIA(media, dcb) {
            if (!GSMSDP_MEDIA_ENABLED(media)) {
                /* this entry is not active */
                continue;
            }

            /* only support starting all receive channels for now */
            lsm_rx_start(lcb, fname, media);
        }
        break;

    case CC_ACTION_ANSWER_PENDING:
        FSM_SET_FLAGS(lcb->flags, LSM_FLAGS_ANSWER_PENDING);
        break;

    default:
        break;
    }


    if (result == CC_RC_ERROR) {
        LSM_DEBUG(get_debug_string(LSM_DBG_CC_ERROR), call_id, line, fname,
                  action, data);
    }

    return (result);
}


void
lsm_ui_display_notify (const char *notify_str, unsigned long timeout)
{
    /*
     * add 0 as (default) priority; it is don't care in legacy mode
     */
    ui_set_notification(CC_NO_LINE, CC_NO_CALL_ID,
                        (char *)notify_str, (int)timeout, FALSE,
                        DEF_NOTIFY_PRI);
}

void
lsm_ui_display_status (const char *status_str, line_t line, callid_t call_id)
{
    lsm_lcb_t *lcb;

    if (call_id == CC_NO_CALL_ID) {
        /* Invalid call id */
        return;
    }
    lcb = lsm_get_lcb_by_call_id(call_id);
    if (lcb == NULL) {
        return;
    }

    ui_set_call_status((char *) status_str, line, lcb->ui_id);
}

/**
 * This function will display notification status line.
 *
 * @param[in] str_index - index into phrase dictionary
 *
 * @return none
 */
void lsm_ui_display_notify_str_index (int str_index)
{
    char tmp_str[STATUS_LINE_MAX_LEN];

    if ((platGetPhraseText(str_index,
                                 (char *)tmp_str,
                                 (STATUS_LINE_MAX_LEN - 1))) == CPR_SUCCESS) {
        lsm_ui_display_notify(tmp_str, NO_FREE_LINES_TIMEOUT);
    }
}

/*
 *  Function: lsm_parse_displaystr
 *
 *  Parameters:string to be parsed
 *
 *  Description:Wrapper function for parsing string to be displayed
 *
 *  Returns: Pointer to parsed number
 *
 */
string_t
lsm_parse_displaystr (string_t displaystr)
{
    return (sippmh_parse_displaystr(displaystr));
}

void
lsm_speaker_mode (short mode)
{
    ui_set_speaker_mode((boolean)mode);
}

/*
 *  Function:lsm_update_active_tone
 *
 *  Parameters:
 *          tone       - tone type
 *          call_id    - call identifier
 *
 *  Description: Update dcb->active_tone if starting infinite duration tone.
 *
 *  Returns:none
 *
 */
void
lsm_update_active_tone (vcm_tones_t tone, callid_t call_id)
{
    static const char fname[] = "lsm_update_active_tone";
    fsmdef_dcb_t *dcb;

    /* if tone is any of following then set active_tone in dcb b/c these
     * tones have infinite duration and need to be stopped. Other tones
     * only play for a finite/short duration so no need to stop them as
     * they will stop automatically.
     */
    switch (tone) {
    /* for all tones with infinite playing duration */
    case VCM_INSIDE_DIAL_TONE:
    case VCM_LINE_BUSY_TONE:
    case VCM_ALERTING_TONE:
    case VCM_STUTTER_TONE:
    case VCM_REORDER_TONE:
    case VCM_OUTSIDE_DIAL_TONE:
    case VCM_PERMANENT_SIGNAL_TONE:
    case VCM_RECORDERWARNING_TONE:
    case VCM_MONITORWARNING_TONE:
        dcb = fsmdef_get_dcb_by_call_id(call_id);

        if (dcb == NULL) {
            /* Possibibly the ui_id was passed in and the dcb is no longer existed.
             * Try to retrieve the corresponding dcb.
             */
            dcb = fsmdef_get_dcb_by_call_id(lsm_get_callid_from_ui_id(call_id));
        }

        if (dcb != NULL) {
            /* Ideally a call should not make a infinite tone start request
             * (without making a stop request) while there is already one playing.
             * However, DSP will start playing the new request tone by overriding
             * the current one. Technically its okay. So, just printing a log msg.
             */
            if (dcb->active_tone != VCM_NO_TONE) {
                LSM_DEBUG(DEB_L_C_F_PREFIX"Active Tone current = %d  new = %d",
                          DEB_L_C_F_PREFIX_ARGS(LSM, dcb->line, call_id, fname),
						  dcb->active_tone, tone);
            }
            dcb->active_tone = tone;
        }
        break;

    default:
        /* do nothing */
        break;
    }
}

/*
 *  Function: lsm_is_tx_channel_opened
 *
 *  Parameters: call_id
 *
 *  Description: check to see tx channel is openned
 *
 *  Returns:    TRUE or FALSE
 *
 */
boolean
lsm_is_tx_channel_opened(callid_t call_id)
{
    fsmdef_dcb_t *dcb_p = fsmdef_get_dcb_by_call_id(call_id);
    fsmdef_media_t *media = NULL;

    if (dcb_p == NULL) {
        return (FALSE);
    }

    /*
     * search the all entries that has a valid media and matches
     * SDP_MEDIA_AUDIO type.
     */
    GSMSDP_FOR_ALL_MEDIA(media, dcb_p) {
        if (media->type == SDP_MEDIA_AUDIO) {
            /* found a match */
            if (media->xmit_chan)
               return (TRUE);
        }
    }
    return (FALSE);
}

/*
 *  Function:lsm_update_monrec_tone_action
 *
 *  Parameters:
 *          tone       - tone type
 *          call_id    - call identifier
 *
 *  Description: Update dcb->monrec_tone_action.
 *
 *  Returns:none
 *
 */
void
lsm_update_monrec_tone_action (vcm_tones_t tone, callid_t call_id, uint16_t direction)
{
    static const char fname[] = "lsm_update_monrec_tone_action";
    fsmdef_dcb_t *dcb;
    boolean tx_opened = lsm_is_tx_channel_opened(call_id);

    dcb = fsmdef_get_dcb_by_call_id(call_id);

    if (dcb != NULL) {
            switch(tone) {
                case VCM_MONITORWARNING_TONE:
                    switch (dcb->monrec_tone_action) {
                        case FSMDEF_MRTONE_NO_ACTION:
                            if (!tx_opened) {
                                dcb->monrec_tone_action = FSMDEF_MRTONE_RESUME_MONITOR_TONE;
                            } else {
                                dcb->monrec_tone_action = FSMDEF_MRTONE_PLAYED_MONITOR_TONE;
                            }
                            break;

                        case FSMDEF_MRTONE_PLAYED_RECORDER_TONE:
                            dcb->monrec_tone_action = FSMDEF_MRTONE_PLAYED_BOTH_TONES;
                            break;

                         case FSMDEF_MRTONE_RESUME_RECORDER_TONE:
                            dcb->monrec_tone_action = FSMDEF_MRTONE_RESUME_BOTH_TONES;
                            break;

                        case FSMDEF_MRTONE_PLAYED_MONITOR_TONE:
                        case FSMDEF_MRTONE_PLAYED_BOTH_TONES:
                        case FSMDEF_MRTONE_RESUME_MONITOR_TONE:
                        case FSMDEF_MRTONE_RESUME_BOTH_TONES:
                        default:
                            DEF_DEBUG(DEB_F_PREFIX"Invalid action request... tone:%d monrec_tone_action:%d",
                                      DEB_F_PREFIX_ARGS("RCC", fname), tone, dcb->monrec_tone_action);
                            break;
                    }
                    dcb->monitor_tone_direction = direction;
                    break;

                case VCM_RECORDERWARNING_TONE:
                    switch (dcb->monrec_tone_action) {
                        case FSMDEF_MRTONE_NO_ACTION:
                            if (!tx_opened) {
                                dcb->monrec_tone_action = FSMDEF_MRTONE_RESUME_RECORDER_TONE;
                            } else {
                                dcb->monrec_tone_action = FSMDEF_MRTONE_PLAYED_RECORDER_TONE;
                            }
                            break;

                        case FSMDEF_MRTONE_PLAYED_MONITOR_TONE:
                            dcb->monrec_tone_action = FSMDEF_MRTONE_PLAYED_BOTH_TONES;
                            break;

                        case FSMDEF_MRTONE_RESUME_MONITOR_TONE:
                            dcb->monrec_tone_action = FSMDEF_MRTONE_RESUME_BOTH_TONES;
                            break;

                        case FSMDEF_MRTONE_PLAYED_RECORDER_TONE:
                        case FSMDEF_MRTONE_PLAYED_BOTH_TONES:
                        case FSMDEF_MRTONE_RESUME_RECORDER_TONE:
                        case FSMDEF_MRTONE_RESUME_BOTH_TONES:
                        default:
                            DEF_DEBUG(DEB_F_PREFIX"Invalid action request... tone:%d monrec_tone_action:%d",
                                      DEB_F_PREFIX_ARGS("RCC", fname), tone, dcb->monrec_tone_action);
                            break;
                    }
                    dcb->recorder_tone_direction = direction;
                    break;

                default:
                    break;
        } /* end of switch */

        LSM_DEBUG(DEB_L_C_F_PREFIX"Start request for tone: %d. Set monrec_tone_action: %d",
                  DEB_L_C_F_PREFIX_ARGS(LSM, dcb->line, call_id, fname),
			      tone, dcb->monrec_tone_action);

    } /* end of if */
}

/*
 *  Function:lsm_downgrade_monrec_tone_action
 *
 *  Parameters:
 *          tone       - tone type
 *          call_id    - call identifier
 *
 *  Description: Update dcb->monrec_tone_action.
 *
 *  Returns:none
 *
 */
void
lsm_downgrade_monrec_tone_action (vcm_tones_t tone, callid_t call_id)
{
    static const char fname[] = "lsm_downgrade_monrec_tone_action";
    fsmdef_dcb_t *dcb;

    dcb = fsmdef_get_dcb_by_call_id(call_id);

    /* Need to downgrade the monrec_tone_action */

    if (dcb != NULL) {
        switch (tone){
            case VCM_MONITORWARNING_TONE:
                switch (dcb->monrec_tone_action) {
                    case FSMDEF_MRTONE_PLAYED_MONITOR_TONE:
                    case FSMDEF_MRTONE_RESUME_MONITOR_TONE:
                        dcb->monrec_tone_action = FSMDEF_MRTONE_NO_ACTION;
                        break;

                    case FSMDEF_MRTONE_RESUME_BOTH_TONES:
                        dcb->monrec_tone_action = FSMDEF_MRTONE_RESUME_RECORDER_TONE;
                        break;

                    case FSMDEF_MRTONE_PLAYED_BOTH_TONES:
                        dcb->monrec_tone_action = FSMDEF_MRTONE_PLAYED_RECORDER_TONE;
                        break;

                    case FSMDEF_MRTONE_NO_ACTION:
                    case FSMDEF_MRTONE_PLAYED_RECORDER_TONE:
                    case FSMDEF_MRTONE_RESUME_RECORDER_TONE:
                    default:
                        DEF_DEBUG(DEB_F_PREFIX"Invalid action request... tone:%d monrec_tone_action:%d",
                                  DEB_F_PREFIX_ARGS("RCC", fname), tone, dcb->monrec_tone_action);
                        break;
                }
                dcb->monitor_tone_direction = VCM_PLAY_TONE_TO_EAR;
                break;

            case VCM_RECORDERWARNING_TONE:
                switch (dcb->monrec_tone_action) {
                    case FSMDEF_MRTONE_PLAYED_RECORDER_TONE:
                    case FSMDEF_MRTONE_RESUME_RECORDER_TONE:
                        dcb->monrec_tone_action = FSMDEF_MRTONE_NO_ACTION;
                        break;

                    case FSMDEF_MRTONE_RESUME_BOTH_TONES:
                        dcb->monrec_tone_action = FSMDEF_MRTONE_RESUME_MONITOR_TONE;
                        break;

                    case FSMDEF_MRTONE_PLAYED_BOTH_TONES:
                        dcb->monrec_tone_action = FSMDEF_MRTONE_PLAYED_MONITOR_TONE;
                        break;

                    case FSMDEF_MRTONE_NO_ACTION:
                    case FSMDEF_MRTONE_PLAYED_MONITOR_TONE:
                    case FSMDEF_MRTONE_RESUME_MONITOR_TONE:
                    default:
                        DEF_DEBUG(DEB_F_PREFIX"Invalid action request... tone:%d monrec_tone_action:%d",
                                  DEB_F_PREFIX_ARGS("RCC", fname), tone, dcb->monrec_tone_action);
                        break;
                }
                dcb->recorder_tone_direction = VCM_PLAY_TONE_TO_EAR;
                break;

            default:
                break;
        } /* end of switch */

        LSM_DEBUG(DEB_L_C_F_PREFIX"Stop request for tone: %d Downgrade monrec_tone_action: %d",
                  DEB_L_C_F_PREFIX_ARGS(LSM, dcb->line, call_id, fname),
			      tone, dcb->monrec_tone_action);
    } /* end of if */
}

/*
 *  Function: lsm_set_hold_ringback_status
 *
 *  Parameters:
 *         callid_t - callid of the lcb
 *         ringback_status - status of call hold ringback
 *
 *  Description: Function used to set the ringback status
 *
 *  Returns:None
 *
 */
void
lsm_set_hold_ringback_status(callid_t call_id, boolean ringback_status)
{
    lsm_lcb_t      *lcb;

    FSM_FOR_ALL_CBS(lcb, lsm_lcbs, LSM_MAX_LCBS) {
        if (lcb->call_id == call_id) {
            LSM_DEBUG(DEB_F_PREFIX"Setting ringback to %d for lcb %d",
                      DEB_F_PREFIX_ARGS(LSM, "lsm_set_hold_ringback_status"),  ringback_status, call_id);
            lcb->enable_ringback = ringback_status;
            break;
        }
    }
}

void lsm_play_tone (cc_features_t feature_id)
{
    int play_tone;

    switch (feature_id) {
    case CC_FEATURE_BLF_ALERT_TONE:
        if (lsm_find_state(LSM_S_RINGIN) > CC_NO_CALL_ID) {
            // No tone if we have calls in ringing state
            return;
        }

        if (!lsm_callwaiting()) {
            config_get_value(CFGID_BLF_ALERT_TONE_IDLE, &play_tone, sizeof(play_tone));
            if (play_tone == 0) {
                return;
            }
            lsm_util_tone_start_with_speaker_as_backup(VCM_CALL_WAITING_TONE, VCM_ALERT_INFO_OFF,
                                                  CC_NO_CALL_ID, CC_NO_GROUP_ID,
                                                  CC_NO_MEDIA_REF_ID, VCM_PLAY_TONE_TO_EAR);
        } else {
            config_get_value(CFGID_BLF_ALERT_TONE_BUSY, &play_tone, sizeof(play_tone));
            if (play_tone == 0) {
                return;
            }
            lsm_util_tone_start_with_speaker_as_backup(VCM_CALL_WAITING_TONE, VCM_ALERT_INFO_OFF,
                                                  CC_NO_CALL_ID, CC_NO_GROUP_ID,
                                                  CC_NO_MEDIA_REF_ID, VCM_PLAY_TONE_TO_EAR);
        }
        break;

    default:
        break;
    }
}

/*
 * lsm_update_inalert_status
 *
 * Description:
 *
 * TNP specific implementation of status line update for inalert state.
 *
 * Parameters:
 *
 * line_t line - Line facility of the call
 * callid_t call_id - Call id of call whose state is being reported
 * cc_state_data_alerting_t * data - alerting callinfo.
 * boolean notify - whether the msg be displayed at notify level.
 *
 * Returns: None
 */
static void
lsm_update_inalert_status (line_t line, callid_t call_id,
                           cc_state_data_alerting_t * data,
                           boolean notify)
{
    static const char fname[] = "lsm_update_inalert_status";
    char disp_str[LSM_DISPLAY_STR_LEN];

    // get localized tag index for From and append one space character
    sstrncpy(disp_str, platform_get_phrase_index_str(UI_FROM),
             sizeof(disp_str));

    LSM_DEBUG(DEB_L_C_F_PREFIX"+++ calling number = %s",
			  DEB_L_C_F_PREFIX_ARGS(LSM, line, call_id, fname),
              data->caller_id.calling_number);

    // append calling number if present or localized tag for Unknown Number
    // otherwise
    if ((data->caller_id.calling_number) &&
        (data->caller_id.calling_number[0] != '\0') &&
        data->caller_id.display_calling_number) {

        sstrncat(disp_str, data->caller_id.calling_number,
                sizeof(disp_str) - strlen(disp_str));
    } else {
        sstrncat(disp_str, platform_get_phrase_index_str(UI_UNKNOWN),
                sizeof(disp_str) - strlen(disp_str));
    }

    // we display (via notification) the "From ..." info for 10 seconds.
    // Note that this will remain displayed for 10 sec even if the user
    // answers the call or switches to another call. This happens because
    // notification has higher priority than call status (e.g. connected).
    // This is done to have parity with SCCP phone behavior.
    if (notify == TRUE) {
        ui_set_notification(line, call_id,
                            (char *)disp_str, (unsigned long)CALL_ALERT_TIMEOUT,
                            FALSE, FROM_NOTIFY_PRI);
    }
    // After the notification we wish to set the call status to From XXXX. Same as SCCP phone behavior
    lsm_ui_display_status((char *)disp_str, line, call_id);

    return;
}



/*
 * lsm_set_cfwd_all_nonccm
 * This function calls JNI API to set the CFA state and DN in non-ccm mode.
 *
 * @param[in] line - line on which to set the CFA
 * @param[in] callfwd_dialstring: CFA DN (will be stored in flash)
 *
 * @return: None
 */
void
lsm_set_cfwd_all_nonccm (line_t line, char *callfwd_dialstring)
{
    // call Java API
    ui_cfwd_status(line, TRUE, callfwd_dialstring, TRUE);
}

/*
 * lsm_set_cfwd_all_ccm
 *
 * Description:
 * This function calls JNI API to set the CFA state and DN in ccm mode.
 *
 * Parameters:
 * char * callfwd_dialstring: CFA DN (will NOT be stored in flash)
 *
 * Returns: None
 */
void
lsm_set_cfwd_all_ccm (line_t line, char *callfwd_dialstring)
{
    // set locally maintained variable
    cfwdall_state_in_ccm_mode[line] = TRUE;

    // call Java API
    ui_cfwd_status((line_t)line, TRUE, callfwd_dialstring, FALSE);
}

/*
 * lsm_clear_cfwd_all_nonccm
 * This function calls JNI API to clear the CFA state and DN in non-ccm mode.
 *
 * @param[in] line - line on which to clear the CFA
 *
 * @return: None
 */
void
lsm_clear_cfwd_all_nonccm (line_t line)
{
    // call Java API
    ui_cfwd_status(line, FALSE, "", TRUE);
}


/*
 * lsm_clear_cfwd_all_ccm
 *
 * Description:
 * This function calls JNI API to clear the CFA state and DN in ccm mode.
 *
 * Parameters: None
 *
 * Returns: None
 */
void
lsm_clear_cfwd_all_ccm (line_t line)
{
    // clear locally maintained variable
    cfwdall_state_in_ccm_mode[line] = FALSE;

    // call Java API
    ui_cfwd_status((line_t)line, FALSE, "", FALSE);
}

/*
 * lsm_check_cfwd_all_nonccm
 *
 * Description:
 * This function returns the CFA state in non-ccm mode.
 *
 * @param[in] line - line on which to check the CFA
 *
 * @return: TRUE (if CFA set) or FALSE (if CFA clear)
 */
int
lsm_check_cfwd_all_nonccm (line_t line)
{
    char cfg_cfwd_url[MAX_URL_LENGTH];

    cfg_cfwd_url[0] = '\0';

    // get the callfwdall url value from the config/flash table
    config_get_string(CFGID_LINE_CFWDALL+line-1, cfg_cfwd_url, MAX_URL_LENGTH);

    // return appropriate value: TRUE if non-NULL and FALSE otherwise
    if (cfg_cfwd_url[0]) {
        return ((int) TRUE);
    } else {
        return ((int) FALSE);
    }
}

/*
 * lsm_check_cfwd_all_ccm
 *
 * Description:
 * This function returns the CFA state in ccm mode.
 *
 * Parameters: None
 *
 * Returns: TRUE or FALSE
 */
int
lsm_check_cfwd_all_ccm (line_t line)
{
    return ((int) cfwdall_state_in_ccm_mode[line]);
}

/*
 * lsm_is_phone_forwarded
 *
 * Description:
 * This function is called from SIP stack to check if received INVITE
 * should be responded with 302 or not. In the CCM mode this function
 * will always return NULL... that is process the INVITE as normal and
 * DO NOT 302 it. In the non-CCM mode, if the cfwdall_url is non-NULL
 * then it will form a proper string to use in 302 response; otherwise
 * a NULL will be returned and the INVITE will be processed as normal.
 * NOTE: most all code is reused from the legacy phone code.
 *
 * Parameters: line - line for which to check the CFA status
 *
 * Returns: NULL if forwarding is not set;
 *          string to use in 302 response if forwarding is set (non-CCM only)
 */
char *
lsm_is_phone_forwarded (line_t line)
{
    static const char fname[] = "lsm_is_phone_forwarded";
    char     proxy_ipaddr_str[MAX_IPADDR_STR_LEN];
    int      port_number = 5060; // use this value only if none found
    char    *domain = NULL;
    char    *port = NULL;
    cpr_ip_addr_t proxy_ipaddr;


    LSM_DEBUG(DEB_F_PREFIX"called", DEB_F_PREFIX_ARGS(LSM, fname));

    // check if running in CCM mode. if so, return NULL that is cfwdall
    // not applicable
    if (sip_regmgr_get_cc_mode(TEL_CCB_START) == REG_MODE_CCM) {
        return (NULL);
    }
    // get stored callfwdall url value from the config/flash table

    config_get_string(CFGID_LINE_CFWDALL+line-1, cfwdall_url, sizeof(cfwdall_url));

    if (cfwdall_url[0]) {
        // find domain and port
        domain = strchr(cfwdall_url, '@');
        if (!domain) {
            (void) sipTransportGetServerAddress(&proxy_ipaddr,
                                        1, TEL_CCB_START);
            if (proxy_ipaddr.type != CPR_IP_ADDR_INVALID) {
                ipaddr2dotted(proxy_ipaddr_str, &proxy_ipaddr);
                port_number = sipTransportGetServerPort(1, TEL_CCB_START);
            }
        } else {
            port = strchr(domain + 1, ':');
        }

        // handle 3 cases
        if (domain == NULL) {
            /* case (1): no domain or port present
             * We have proxy's dotted ip address format. So, not FQDN check.
             * Append domain/ip-addr and port.
             */
            snprintf(cfwdall_url + strlen(cfwdall_url),
                     MAX_URL_LENGTH - strlen(cfwdall_url),
                     "@%s:%d", proxy_ipaddr_str, port_number);
        } else if (port == NULL) {
            /* case (2): domain present but no port
             * Check if the domain is dotted IP address and add port
             * only if dotted IP address is used
             */
            if (!str2ip((const char *) domain + 1, &proxy_ipaddr)) {
                port_number = sipTransportGetServerPort(1, TEL_CCB_START);
                snprintf(cfwdall_url + strlen(cfwdall_url),
                         MAX_URL_LENGTH - strlen(cfwdall_url),
                         ":%d", port_number);
            }
        } else {
            /* case (3): both domain and port present
             * Both domain and port exists, but strip the  port if the
             * domain is FQDN
             */
            memcpy(proxy_ipaddr_str, domain + 1, (port - domain - 1));
            *(proxy_ipaddr_str + (port - domain - 1)) = '\0';
            if (str2ip((const char *) proxy_ipaddr_str, &proxy_ipaddr) != 0) {
                *port = '\0';
            }
        }
        return ((char *)cfwdall_url);
    } else {
        return ((char *)NULL);
    }
}

/*
 * lsm_get_callid_from_ui_id()
 *
 * Description:
 *    The function gets the UI id from LSM's LCB for a given GSM call ID.
 *
 * Parameters:
 *    ui_id   - UI ID.
 *
 * Returns: callid_t
 */
callid_t
lsm_get_callid_from_ui_id (callid_t uid)
{
    lsm_lcb_t *lcb;
    FSM_FOR_ALL_CBS(lcb, lsm_lcbs, LSM_MAX_LCBS) {
        if (lcb->ui_id == uid) {
            return lcb->call_id;
        }
    }
    return (CC_NO_CALL_ID);
}

/*
 * lsm_get_ui_id
 *
 * Description:
 *    The function gets the UI id from LSM's LCB for a given GSM call ID.
 *
 * Parameters:
 *    call_id - GSM call ID
 *    ui_id   - UI ID.
 *
 * Returns: None
 */
callid_t
lsm_get_ui_id (callid_t call_id)
{
    lsm_lcb_t *lcb;

    lcb = lsm_get_lcb_by_call_id(call_id);
    if (lcb != NULL) {
        return (lcb->ui_id);
    }
    return (CC_NO_CALL_ID);
}

/*
 * lsm_get_ms_ui_id
 *
 * Description:
 *    The function gets the UI id from LSM's LCB for a given GSM call ID. During
 * certain features like barge ui_id is set to CC_NO_CALL_ID.
 *
 * Parameters:
 *    call_id - GSM call ID
 *    ui_id   - UI ID.
 *
 * Returns: None
 */
cc_call_handle_t
lsm_get_ms_ui_call_handle (line_t line, callid_t call_id, callid_t ui_id)
{
    callid_t lsm_ui_id;

    if (ui_id != CC_NO_CALL_ID) {
        return CREATE_CALL_HANDLE(line, ui_id);
    }

    /* If ui_id present use that */
    lsm_ui_id = lsm_get_ui_id(call_id);

    if (lsm_ui_id != CC_NO_CALL_ID) {
        return CREATE_CALL_HANDLE(line, lsm_ui_id);
    }

    return CREATE_CALL_HANDLE(line, call_id);
}
/*
 * lsm_set_ui_id
 *
 * Description:
 *    The function sets the UI id to LSM's LCB for a given GSM call ID.
 *
 * Parameters:
 *    call_id - GSM call ID
 *    ui_id   - UI ID.
 *
 * Returns: None
 */
void
lsm_set_ui_id (callid_t call_id, callid_t ui_id)
{
    lsm_lcb_t *lcb;

    lcb = lsm_get_lcb_by_call_id(call_id);
    if (lcb != NULL) {
        lcb->ui_id = ui_id;
    }
}

char *
lsm_get_gdialed_digits (void)
{
    return (dp_get_gdialed_digits());
}

/*
 * lsm_update_video_avail
 *
 * Description:
 *    The function updates session about the video availability
 *
 * Parameters:
 *    line - line
 *    call_id - GSM call ID
 *    dir - video avail dir
 *
 * Returns: None
 */
void lsm_update_video_avail (line_t line, callid_t call_id, int dir)
{
    static const char fname[] = "lsm_update_video_avail";
    fsmdef_dcb_t   *dcb;
    lsm_lcb_t *lcb;

    lcb = lsm_get_lcb_by_call_id(call_id);
    if (lcb != NULL) {
        dcb = lcb->dcb;
        if (dcb == NULL) {
            LSM_ERR_MSG(get_debug_string(DEBUG_INPUT_NULL), fname);
            return;
        }

        dir &= ~CC_ATTRIB_CAST;


        ui_update_video_avail (line, lcb->ui_id, dir);

        lsm_update_dscp_value(dcb);
    }
}

/*
 * lsm_update_video_offered
 *
 * Description:
 *    The function updates session about the video availability
 *
 * Parameters:
 *    line - line
 *    call_id - GSM call ID
 *    dir - video avail dir
 *
 * Returns: None
 */
void lsm_update_video_offered (line_t line, callid_t call_id, int dir)
{
    lsm_lcb_t *lcb;

    lcb = lsm_get_lcb_by_call_id(call_id);
    if (lcb != NULL) {
       ui_update_video_offered (line, lcb->ui_id, dir);
    }
}

/*
 * lsm_set_video_mute
 *
 * Description:
 *    The function sets the video mute state for the call
 *
 * Parameters:
 *    line - line
 *    call_id - This is the UI_ID coming from UI
 *    mute - mute state
 *
 * Returns: None
 */
void lsm_set_video_mute (callid_t call_id, int mute)
{
    lsm_lcb_t *lcb;
    callid_t cid = lsm_get_callid_from_ui_id(call_id); // get GSM_ID from UI_ID

    lcb = lsm_get_lcb_by_call_id(cid);
    if (lcb != NULL) {
       lcb->vid_mute = mute;
    }
}

/*
 * lsm_get_video_mute
 *
 * Description:
 *    The function gets the video mute state for the call
 *
 * Parameters:
 *    line - line
 *    call_id - GSM call ID
 *
 * Returns: t_video_mute
 */
int lsm_get_video_mute (callid_t call_id)
{
    lsm_lcb_t *lcb;

    lcb = lsm_get_lcb_by_call_id(call_id);
    if (lcb != NULL) {
       return lcb->vid_mute;
    }
    return (-1);
}


/*
 * lsm_set_video_window
 *
 * Description:
 *    The function sets the video window state for the call
 *
 * Parameters:
 *    call_id - This is the UI_ID coming from UI
 *    flags - video window flags
 *    x - video window x coordinate
 *    y - video window y coordinate
 *    h - video window height
 *    w - video window width
 *
 * Returns: None
 */
void lsm_set_video_window (callid_t call_id, int flags, int x, int y, int h, int w)
{
    lsm_lcb_t *lcb;
    callid_t cid = lsm_get_callid_from_ui_id(call_id); // get GSM_ID from UI_ID

    lcb = lsm_get_lcb_by_call_id(cid);
    if (lcb != NULL) {
       lcb->vid_flags = flags;
       lcb->vid_x = x;
       lcb->vid_y = y;
       lcb->vid_h = h;
       lcb->vid_w = w;
    }
}

/*
 * lsm_get_video_window
 *
 * Description:
 *    The function gets the video window for the call
 *
 * Parameters:
 *    call_id - GSM call ID
 *    *flags - video window flag
 *    *x - video window x coordinate
 *    *y - video window y coordinate
 *    *h - video window height
 *    *w - video window width
 *
 * Returns: void
 */
void lsm_get_video_window (callid_t call_id, int *flags, int *x, int *y, int *h, int *w)
{
    lsm_lcb_t *lcb;

    lcb = lsm_get_lcb_by_call_id(call_id);
    if (lcb != NULL) {
        *flags = lcb->vid_flags;
        *x = lcb->vid_x;
        *y = lcb->vid_y;
        *h = lcb->vid_h;
        *w = lcb->vid_w;
    }
}

/*
 * lsm_is_kpml_subscribed
 *
 * Description:
 *    check if kpml is subscribed for this call
 *
 * Parameters:
 *    call_id - GSM call ID
 *
 * Returns: true/false
 */
boolean lsm_is_kpml_subscribed (callid_t call_id)
{
    lsm_lcb_t *lcb;

    lcb = lsm_get_lcb_by_call_id(call_id);
    if (lcb == NULL) {
        return FALSE;
    }
    return kpml_is_subscribed(call_id, lcb->line);
}

/**
 * A helper method to start the tone.
 */
static void lsm_util_start_tone(vcm_tones_t tone, short alert_info,
        cc_call_handle_t call_handle, groupid_t group_id,
        streamid_t stream_id, uint16_t direction) {

	int               sdpmode = 0;
    static const char fname[] = "lsm_util_start_tone";
    line_t line = GET_LINE_ID(call_handle);
    callid_t call_id = GET_CALL_ID(call_handle);
    DEF_DEBUG(DEB_F_PREFIX"Enter, line=%d, call_id=%d.",
              DEB_F_PREFIX_ARGS(MED_API, fname), line, call_id);

    sdpmode = 0;
    config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));
    if (!sdpmode) {
        vcmToneStart(tone, alert_info, call_handle, group_id, stream_id, direction);
	}
    /*
     * Set delay value for multi-part tones and repeated tones.
     * Currently the only multi-part tones are stutter and message
     * waiting tones. The only repeated tones are call waiting and
     * tone on hold tones.  If the DSP ever supports stutter and
     * message waiting tones, these tones can be removed from this
     * switch statement.
     */
    switch (tone) {
    case VCM_MSG_WAITING_TONE:
        lsm_start_multipart_tone_timer(tone, MSG_WAITING_DELAY, call_id);
        break;

    case VCM_HOLD_TONE:
        lsm_start_continuous_tone_timer(tone, TOH_DELAY, call_id);
        break;

    default:
        break;
    }

    /*
     * Update dcb->active_tone if start request
     * is for an infinite duration tone.
     */
    lsm_update_active_tone(tone, call_id);
}

/*
 * Plays a short tone. uses the open audio path.
 * If no audio path is open, plays on speaker.
 *
 * @param[in] tone       - tone type
 * @param[in] alert_info - alertinfo header
 * @param[in] call_id    - call identifier
 * @param[in] direction  - network, speaker, both
 *
 * @return none
 */
void
lsm_util_tone_start_with_speaker_as_backup (vcm_tones_t tone, short alert_info,
                                    cc_call_handle_t call_handle, groupid_t group_id,
                                    streamid_t stream_id, uint16_t direction) {
    static const char *fname = "lsm_util_tone_start_with_speaker_as_backup";
    line_t line = GET_LINE_ID(call_handle);
    callid_t call_id = GET_CALL_ID(call_handle);
    DEF_DEBUG(DEB_L_C_F_PREFIX"tone=%-2d: direction=%-2d",
              DEB_L_C_F_PREFIX_ARGS(MED_API, line, call_id, fname),
              tone, direction);

    //vcmToneStart
    vcmToneStart(tone, alert_info, call_handle, group_id, stream_id, direction);

    /*
     * Set delay value for multi-part tones and repeated tones.
     * Currently the only multi-part tones are stutter and message
     * waiting tones. The only repeated tones are call waiting and
     * tone on hold tones.  If the DSP ever supports stutter and
     * message waiting tones, these tones can be removed from this
     * switch statement.
     */
    switch (tone) {
    case VCM_MSG_WAITING_TONE:
        lsm_start_multipart_tone_timer(tone, MSG_WAITING_DELAY, call_id);
        break;

    case VCM_HOLD_TONE:
        lsm_start_continuous_tone_timer(tone, TOH_DELAY, call_id);
        break;

    default:
        break;
    }

    /*
     * Update dcb->active_tone if start request
     * is for an infinite duration tone.
     */
    lsm_update_active_tone(tone, call_id);

}
