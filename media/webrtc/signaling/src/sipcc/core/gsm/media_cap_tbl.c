/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cpr_types.h>
#include "ccapi.h"
#include "phone_debug.h"
#include "cc_debug.h"
#include "CCProvider.h"
#include "ccapi_snapshot.h"

/**
 * global media cap table for interaction between what platform
 * can do and the GSM-SDP module
 * AUDIO_1 is used for audio cannot be turned off
 * VIDEO_1 is used for video
 */
cc_media_cap_table_t g_media_table = {
      1,
      {
        {CC_AUDIO_1,SDP_MEDIA_AUDIO,TRUE,TRUE,SDP_DIRECTION_RECVONLY},
        {CC_VIDEO_1,SDP_MEDIA_VIDEO,TRUE,TRUE,SDP_DIRECTION_RECVONLY},
        {CC_DATACHANNEL_1,SDP_MEDIA_APPLICATION,FALSE,TRUE,SDP_DIRECTION_SENDRECV},
      }
};

static boolean g_nativeVidSupported = FALSE;
static boolean g_vidCapEnabled = FALSE;
static boolean g_natve_txCap_enabled = FALSE;

/**
 * Simple method to trigger escalation and deescalation
 * based on media capability changes
 */
void escalateDeescalate() {
    g_media_table.id++;
    if ( ccapp_get_state() != CC_INSERVICE ) {
        VCM_DEBUG(MED_F_PREFIX"Ignoring video cap update", "escalateDeescalate");
        return;
    }

    //post the event
    cc_int_feature(CC_SRC_UI, CC_SRC_GSM, CC_NO_CALL_ID,
         CC_NO_LINE, CC_FEATURE_UPD_MEDIA_CAP, NULL);
}

cc_boolean cc_media_isTxCapEnabled() {
   return g_natve_txCap_enabled;
}

cc_boolean cc_media_isVideoCapEnabled() {
    if ( g_nativeVidSupported ) {
       return g_vidCapEnabled;
    }
    return FALSE;
}

/**
 * API to update the local video cap in the table
 * called when native video support or vidCap cfg changes
 *
 * This method looks at video cap in cfg & native vid support on platform
 */
static void updateVidCapTbl(){

    if ( g_vidCapEnabled  ) {
        if ( g_media_table.cap[CC_VIDEO_1].enabled == FALSE ) {
            // cfg is enabled but cap tbl is not
            if ( g_nativeVidSupported ) {
                // we can do native now enable cap
                g_media_table.cap[CC_VIDEO_1].enabled = TRUE;
                g_media_table.cap[CC_VIDEO_1].support_direction =
                   g_natve_txCap_enabled?SDP_DIRECTION_SENDRECV:SDP_DIRECTION_RECVONLY;
                if ( g_natve_txCap_enabled == FALSE ) {

                }
                escalateDeescalate();
            } else {

            }
        }
    }  else {
        // disable vid cap
        DEF_DEBUG(MED_F_PREFIX"video capability disabled", "updateVidCapTbl");

        if ( g_media_table.cap[CC_VIDEO_1].enabled ) {
            g_media_table.cap[CC_VIDEO_1].enabled = FALSE;
            escalateDeescalate();
        }
    }
}


/**
 * API to update video capability on the device
 * expected to be called once in the beginning only
 */
void cc_media_update_native_video_support(boolean val) {
    DEF_DEBUG(MED_F_PREFIX"Setting native video support val=%d", "cc_media_update_native_video_support", val);
    g_nativeVidSupported = val;
    updateVidCapTbl();
}

/**
 *
 * API to update video capability on the device based on config
 */
void cc_media_update_video_cap(boolean val) {
    DEF_DEBUG(MED_F_PREFIX"Setting video cap val=%d", "cc_media_update_video_cap", val);
    g_vidCapEnabled = val;
    updateVidCapTbl();
    if ( g_nativeVidSupported ) {
        ccsnap_gen_deviceEvent(CCAPI_DEVICE_EV_VIDEO_CAP_ADMIN_CONFIG_CHANGED, CC_DEVICE_ID);
    }
}

/**
 *
 * API to update video tx capability based on camera plugin events
 */

void cc_media_update_native_video_txcap(boolean enable) {

    VCM_DEBUG(MED_F_PREFIX"Setting txcap val=%d", "cc_media_update_video_txcap", enable);

    if ( g_natve_txCap_enabled == enable ) {
        // nothing to do
        return;
    }

    g_natve_txCap_enabled = enable;
    ccsnap_gen_deviceEvent(CCAPI_DEVICE_EV_CAMERA_ADMIN_CONFIG_CHANGED, CC_DEVICE_ID);

    if ( g_nativeVidSupported && g_vidCapEnabled ) {
    // act on camera events only iof native video is enabled
        if ( g_natve_txCap_enabled ) {

        } else if (g_media_table.cap[CC_VIDEO_1].enabled) {

        }

        g_media_table.cap[CC_VIDEO_1].support_direction  =
            g_natve_txCap_enabled?SDP_DIRECTION_SENDRECV:SDP_DIRECTION_RECVONLY;

        escalateDeescalate();
    }
}

static cc_boolean vidAutoTxPref=FALSE;
void cc_media_setVideoAutoTxPref(cc_boolean txPref){
	vidAutoTxPref = txPref;
}

cc_boolean cc_media_getVideoAutoTxPref(){
	return vidAutoTxPref;
}


