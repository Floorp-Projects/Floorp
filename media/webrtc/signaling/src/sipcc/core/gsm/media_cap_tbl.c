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
        {CC_AUDIO_1,SDP_MEDIA_AUDIO,TRUE,TRUE,SDP_DIRECTION_SENDRECV},
        {CC_VIDEO_1,SDP_MEDIA_VIDEO,FALSE,FALSE,SDP_DIRECTION_SENDRECV},
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
        VCM_DEBUG(MED_F_PREFIX"Ignoring video cap update\n", "escalateDeescalate");
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
        DEF_DEBUG(MED_F_PREFIX"video capability disabled \n", "updateVidCapTbl");

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
    DEF_DEBUG(MED_F_PREFIX"Setting native video support val=%d\n", "cc_media_update_native_video_support", val);
    g_nativeVidSupported = val;
    updateVidCapTbl();
}

/**
 *
 * API to update video capability on the device based on config 
 */
void cc_media_update_video_cap(boolean val) {
    DEF_DEBUG(MED_F_PREFIX"Setting video cap val=%d\n", "cc_media_update_video_cap", val);
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

    VCM_DEBUG(MED_F_PREFIX"Setting txcap val=%d\n", "cc_media_update_video_txcap", enable);

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


