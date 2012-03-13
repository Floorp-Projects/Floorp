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

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#include "cpr.h"
#include "cpr_socket.h"
#include "cpr_in.h"
#include "cpr_stdlib.h"
#include "phntask.h"
#include <stdarg.h>
#include "configmgr.h"
#include "ci.h"
#include "debug.h"
#include "config.h"
#include "vcm.h"
#include "direct.h"
#include "dialplan.h"
#include "dns_utils.h"
#include "time2.h"
#include "debug.h"
#include "phone_debug.h"
#include "util_string.h"
#include "sip_common_transport.h"


uint32_t gtick;

//====================================================================================================
//includes\dns_utils.h
//====================================================================================================
#define DNS_ERR_HOST_UNAVAIL   5
typedef void *srv_handle_t;


/*--------------------------------------------------------------------------
 * Global data New for CPR
 *--------------------------------------------------------------------------
 */


/*--------------------------------------------------------------------------
 * External function prototypes New for CPR
 *--------------------------------------------------------------------------
 */


// got from emuiface.h
enum AUDIOFLAGS {
    NO_AUDIOFLAGS                  = 0x0000,
    HANDSET_AUDIOFLAG              = 0x0001<<0,
    SPEAKERPHONE_AUDIOFLAG         = 0x0001<<1,
    HEADSET_AUDIOFLAG              = 0x0001<<2,
    SPEAKER_ON_REQUEST_AUDIOFLAG   = 0x0001<<3,
    SPEAKER_OFF_REQUEST_AUDIOFLAG  = 0x0001<<4,
    ANY_AUDIOFLAGS                 = (HANDSET_AUDIOFLAG      | 
                                      SPEAKERPHONE_AUDIOFLAG |
                                      HEADSET_AUDIOFLAG)
};


static boolean b_Locked = TRUE;
boolean CFGIsLocked (void)
{
    return (b_Locked);
}


/*
 * cfg_sanity_check_config_settings()
 *
 * Checks the sanity of the media range config settings
 * and sets them to defaults if they are incorrect.
 */
void cfg_sanity_check_media_range (void)
{
    int32_t start_port = 0;
    int32_t end_port = 0;
    boolean changed = FALSE;  

    config_get_value(CFGID_MEDIA_PORT_RANGE_START, 
                     &start_port, sizeof(start_port));
    config_get_value(CFGID_MEDIA_PORT_RANGE_END, 
                     &end_port, sizeof(end_port));
                     
    // Ensure that the ports are on an even port boundary
    if (start_port & 0x1) {                  
        start_port =  start_port & ~0x1;
        changed = TRUE;
    }
    if (end_port & 0x1) { 
        end_port = end_port & ~0x1;
        changed = TRUE;
    }
                  
    /*
     * If the ranges are swapped, swap them for convenience here
     */
    if (end_port < start_port) {
        unsigned int temp = end_port;
        end_port = start_port;
        start_port = temp;
        changed = TRUE; 
    }
    
    if ((end_port - start_port) < 4) {
        start_port  = RTP_START_PORT;
        end_port = RTP_END_PORT;
        changed = TRUE; 
    }
    
    /*
     * We are trying to ensure that the start_port and the end_port
     * are in the range of 0x4000 through and including 0x7ffe.
     * We also need to guarantee that there are at least 3 available
     * port pairs (Even port for RTP and Odd port for RTCP)
     * between the start_port and end_port
     * By using two different ranges for the low and high port
     * Namely:
     * RTP_START_PORT <= start_port < RTP_END_PORT - 4
     * and
     * RTP_START_PORT + 4 < end_port <= RTP_END_PORT
     * we guarantee that there will always be 3 port pairs
     * between the low port and the high port since the two if
     * statements above guarantee that there will be a two port
     * pair spread to begin with and start_port < end_port
     */
    if ((start_port < RTP_START_PORT) || (start_port > (RTP_END_PORT - 4))) {
        start_port = RTP_START_PORT;
        changed = TRUE; 
    }

    if ((end_port < (RTP_START_PORT + 4)) || (end_port > RTP_END_PORT)) {
        end_port = RTP_END_PORT;
        changed = TRUE; 
    } 
    
    if (changed) {
        config_set_value(CFGID_MEDIA_PORT_RANGE_START, 
                         &start_port, sizeof(start_port));
        config_set_value(CFGID_MEDIA_PORT_RANGE_END, 
                         &end_port, sizeof(end_port));
    }      
}


//====================================================================================================
//source\debug.c
//====================================================================================================
#define MAX_DEBUG_CMDS 40
static debug_entry_t debug_table[MAX_DEBUG_CMDS];


/*================================================================
 * TRansient CPR stuff
 *===============================================================*/

/* required in various btxml files */

/*================================================================
 * From timer_platform.c which used to be included and needs to be FIXED
 *===============================================================*/	


extern struct tm *gmtime_r(const time_t *timer, struct tm *pts)
{
    errno_t err;

    if (pts == NULL)
    {
        return NULL;
    }

    memset(pts, 0, sizeof(struct tm));

    if (timer == NULL)
    {
        return NULL;
    }

    // In Visual C++ 2005, gmtime_s is an inline function which evaluates
    // to _gmtime64_s and time_t is equivalent to __time64_t.
    err = gmtime_s(pts, timer);
    if (err)
    {
        memset(pts, 0, sizeof(struct tm));
        return NULL;
    }

    return pts;
}


//seconds_to_gmt_string               ccsip_messaging.o
unsigned long seconds_to_gmt_string (unsigned long seconds, char *gmt_string)
{
    sprintf(gmt_string, "%s, %02d %s %04d %02d:%02d:%02d GMT",
        "Mon",
        1,
        "Jan",
        1,
        1,
        1,
        1);
    return 0;
}


//====================================================================================================
// Global Data added to support CPR
//====================================================================================================

/* used by lsm_7960.o should be wrapped as platform_get_audio_mode ??*/
int PhoneAudioFlags = NO_AUDIOFLAGS;

#include "lsm.h"
void OnTerminateCall() 
{
	terminate_active_calls();
}

//====================================================================================================
//network\source\dns_utils.c
//====================================================================================================

int32_t
cprShowMessageQueueStats (int32_t argc, const char *argv[])
{
    debugif_printf("CPR Message Queues\n");
    return 0;
}


int32_t
cpr_debug_memory_cli (int32_t argc, const char *argv[])
{
        return 1;
}


int debugif_printf_response(int responseType,const char *_format, ...)
{
    return 0;
}


cc_int32_t
cpr_clear_memory (cc_int32_t argc, const char *argv[])
{
    return 0;
}


cc_int32_t
cpr_show_memory (cc_int32_t argc, const char *argv[])
{
    return 0;
}
