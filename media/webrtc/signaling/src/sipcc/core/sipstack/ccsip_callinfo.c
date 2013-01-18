/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <limits.h>
#include <errno.h>

#include "ccsip_callinfo.h"
#include "ccsip_protocol.h"
#include "ccsip_core.h"
#include "cpr_string.h"
#include "cpr_strings.h"
#include "cpr_types.h"
#include "cpr_stdio.h"
#include "cpr_memory.h"
#include "cpr_stdlib.h"
#include "phone_debug.h"


#define FEAT_STRING_SIZE 80

/*
 * which_feature
 *
 * Description:
 *
 * A quick determination of the feature based on the string.
 */
static cc_call_info_e
which_feature (char *feat_string_p)
{
    if (cpr_strcasecmp(feat_string_p, SIP_CI_HOLD_STR) == 0)
        return CC_FEAT_HOLD;

    if (cpr_strcasecmp(feat_string_p, SIP_CI_RESUME_STR) == 0)
        return CC_FEAT_RESUME;

    if (cpr_strcasecmp(feat_string_p, SIP_CI_BARGE_STR) == 0)
        return CC_FEAT_BARGE;

    if (cpr_strcasecmp(feat_string_p, SIP_CI_CBARGE_STR) == 0)
        return CC_FEAT_CBARGE;

    if (cpr_strcasecmp(feat_string_p, SIP_CI_CALL_INFO_STR) == 0)
        return CC_FEAT_CALLINFO;

    return CC_FEAT_NONE;
}

/*
 * parse_call_info_parm
 *
 * Description:
 *
 * Parse potential callinfo feature parms.
 */
static void
parse_call_info_parm (char *parm_p, cc_call_info_data_t * feature_data_p)
{
    static const char fname[] = "parse_call_info_parm";
    char *temp_p;
    uint16_t instance_id;
    unsigned long strtoul_result;
    char *strtoul_end;

    if (!parm_p)
        return;

    while (parm_p) {
        parm_p++;
        SKIP_LWS(parm_p);

        if (!cpr_strncasecmp(parm_p, SIP_CI_SECURITY,
                             sizeof(SIP_CI_SECURITY) - 1)) {
            parm_p = parm_p + sizeof(SIP_CI_SECURITY) - 1;
            SKIP_LWS(parm_p);

            if (*parm_p) {
                feature_data_p->call_info_feat_data.feature_flag |= CC_SECURITY;
                if (!cpr_strncasecmp(parm_p, SIP_CI_SECURITY_UNKNOWN,
                                     sizeof(SIP_CI_SECURITY_UNKNOWN) - 1)) {
                    feature_data_p->call_info_feat_data.security = CC_SECURITY_UNKNOWN;
                } else if (!cpr_strncasecmp(parm_p, SIP_CI_SECURITY_AUTH,
                                         sizeof(SIP_CI_SECURITY_AUTH) - 1)) {
                    feature_data_p->call_info_feat_data.security = CC_SECURITY_AUTHENTICATED;
                } else if (!cpr_strncasecmp(parm_p, SIP_CI_SECURITY_ENCRYPTED,
                                         sizeof(SIP_CI_SECURITY_ENCRYPTED) - 1)) {
                    feature_data_p->call_info_feat_data.security = CC_SECURITY_ENCRYPTED;
                } else if (!cpr_strncasecmp(parm_p, SIP_CI_SECURITY_NOT_AUTH,
                                             sizeof(SIP_CI_SECURITY_NOT_AUTH) - 1)) {
                    feature_data_p->call_info_feat_data.security = CC_SECURITY_NOT_AUTHENTICATED;
                } else {
                            CCSIP_DEBUG_ERROR(SIP_F_PREFIX "Unknown security"
                                              " value %s\n", fname, parm_p);
                    feature_data_p->call_info_feat_data.security = CC_SECURITY_UNKNOWN;
                }
            } else {
                break;
            }
        } else if (!cpr_strncasecmp(parm_p, SIP_CI_POLICY,
			sizeof(SIP_CI_POLICY) - 1)) {
	    parm_p = parm_p + sizeof(SIP_CI_POLICY) - 1;
	    SKIP_LWS(parm_p);

	    if (*parm_p) {
		feature_data_p->call_info_feat_data.feature_flag |= CC_POLICY;
		if (!cpr_strncasecmp(parm_p, SIP_CI_POLICY_CHAPERONE ,
				sizeof(SIP_CI_POLICY_CHAPERONE) - 1)) {
		    feature_data_p->call_info_feat_data.policy = CC_POLICY_CHAPERONE;
		} else if (!cpr_strncasecmp(parm_p, SIP_CI_POLICY_UNKNOWN,
				sizeof(SIP_CI_POLICY_UNKNOWN) - 1)) {
		    feature_data_p->call_info_feat_data.policy = CC_POLICY_UNKNOWN;
		} else {
		    CCSIP_DEBUG_ERROR("%s ERROR: Unknown policy"
			" value %s\n", fname, parm_p) ;
		    feature_data_p->call_info_feat_data.policy = CC_POLICY_UNKNOWN;
		}
	    } else {
		break;
	    }
       	} else if (!cpr_strncasecmp(parm_p, SIP_CI_ORIENTATION,
                                    sizeof(SIP_CI_ORIENTATION) - 1)) {
            parm_p = parm_p + sizeof(SIP_CI_ORIENTATION) - 1;
            SKIP_LWS(parm_p);

            if (*parm_p) {
                feature_data_p->call_info_feat_data.feature_flag |= CC_ORIENTATION;
                if (!cpr_strncasecmp(parm_p, SIP_CI_ORIENTATION_FROM,
                                     sizeof(SIP_CI_ORIENTATION_FROM) - 1)) {
                    feature_data_p->call_info_feat_data.orientation = CC_ORIENTATION_FROM;
                } else if (!cpr_strncasecmp(parm_p, SIP_CI_ORIENTATION_TO,
                                         sizeof(SIP_CI_ORIENTATION_TO) - 1)) {
                    feature_data_p->call_info_feat_data.orientation = CC_ORIENTATION_TO;
                } else {
                    CCSIP_DEBUG_ERROR(SIP_F_PREFIX  "Unknown orientation info"
                                      " value %s\n", fname, parm_p);
                    feature_data_p->call_info_feat_data.orientation = CC_ORIENTATION_NONE;
                }
            } else {
                break;
            }
        } else if (!cpr_strncasecmp(parm_p, SIP_CI_UI_STATE,
                                    sizeof(SIP_CI_UI_STATE) - 1)) {
            parm_p = parm_p + sizeof(SIP_CI_UI_STATE) - 1;
            SKIP_LWS(parm_p);

            if (*parm_p) {
                feature_data_p->call_info_feat_data.feature_flag |= CC_UI_STATE;
                if (!cpr_strncasecmp(parm_p, SIP_CI_UI_STATE_RINGOUT,
                                     sizeof(SIP_CI_UI_STATE_RINGOUT) - 1)) {
                    feature_data_p->call_info_feat_data.ui_state = CC_UI_STATE_RINGOUT;
                } else if (!cpr_strncasecmp(parm_p, SIP_CI_UI_STATE_CONNECTED,
                                         sizeof(SIP_CI_UI_STATE_CONNECTED) - 1)) {
                    feature_data_p->call_info_feat_data.ui_state = CC_UI_STATE_CONNECTED;
                } else if (!cpr_strncasecmp(parm_p, SIP_CI_UI_STATE_BUSY,
                                         sizeof(SIP_CI_UI_STATE_BUSY) - 1)) {
                    feature_data_p->call_info_feat_data.ui_state = CC_UI_STATE_BUSY;
                } else {
                    CCSIP_DEBUG_ERROR(SIP_F_PREFIX  "Unknown call state"
                                      " value %s\n", fname, parm_p);
                    /* Unknown value, ignore the call state */
                    feature_data_p->call_info_feat_data.feature_flag &=
                         ~(CC_UI_STATE);
                    feature_data_p->call_info_feat_data.ui_state =
                         CC_UI_STATE_NONE;
                }
            } else {
                break;
            }
        } else if (!cpr_strncasecmp(parm_p, SIP_CI_CALL_INSTANCE,
                                    sizeof(SIP_CI_CALL_INSTANCE) - 1)) {
            parm_p = parm_p + sizeof(SIP_CI_CALL_INSTANCE) - 1;
            SKIP_LWS(parm_p);

            if (*parm_p) {
                int idx=0;
                char tempbuf[4];

                feature_data_p->call_info_feat_data.feature_flag |= CC_CALL_INSTANCE;
                /* Initialized the call instance id, just in case */
                feature_data_p->call_info_feat_data.caller_id.call_instance_id
                    = 0;
                /* Parse instance id from line */
                temp_p = parm_p;
                while (isdigit((int) *parm_p)&&idx<3) {
					tempbuf[idx++] = *parm_p++;
                }
                tempbuf[idx] = 0;
                if (idx == 0) {
                    /* Did not find any digit after "call_instance=" */
                    CCSIP_DEBUG_ERROR(SIP_F_PREFIX  "no digits found for"
                                      " call_instance parameter.\n", fname);
                    feature_data_p->call_info_feat_data.feature_flag &=
                             ~(CC_CALL_INSTANCE);
                    break;
                } else {
                    errno = 0;
                    strtoul_result = strtoul(tempbuf, &strtoul_end, 10);

                    if (errno || tempbuf == strtoul_end || strtoul_result > USHRT_MAX) {
                      CCSIP_DEBUG_ERROR(SIP_F_PREFIX  "parse error for call_instance_id: %s",
                                        __FUNCTION__, tempbuf);
                      strtoul_result = 0;
                    }

                    feature_data_p->call_info_feat_data.caller_id.call_instance_id =
                        (uint16_t) strtoul_result;
                }
            } else {
                break;
            }
        } else if (!cpr_strncasecmp(parm_p, SIP_CI_PRIORITY,
                                    sizeof(SIP_CI_PRIORITY) - 1)) {
            parm_p = parm_p + sizeof(SIP_CI_PRIORITY) - 1;
            SKIP_LWS(parm_p);

            if (*parm_p) {
                temp_p = parm_p;
                if ((!cpr_strncasecmp(parm_p, SIP_CI_PRIORITY_URGENT,
                                      sizeof(SIP_CI_PRIORITY_URGENT) - 1)) ||
                    (!cpr_strncasecmp(parm_p, SIP_CI_PRIORITY_EMERGENCY,
                                      sizeof(SIP_CI_PRIORITY_EMERGENCY) - 1))) {
                    feature_data_p->call_info_feat_data.priority = CC_CALL_PRIORITY_URGENT;
                } // otherwise, it will be defaulted to normal priority
                else {
                    errno = 0;
                    strtoul_result = strtoul(temp_p, &strtoul_end, 10);

                    if (errno || temp_p == strtoul_end || strtoul_result > (MAX_CALLS - 1)) {
                        /*
                         * Call instance ID should not exceed max instances
                         * or calls.
                         */
                        CCSIP_DEBUG_ERROR(SIP_F_PREFIX  "invalid call_instance"
                                          " value %u\n", fname, (unsigned) strtoul_result);
                        feature_data_p->call_info_feat_data.feature_flag &=
                                 ~(CC_CALL_INSTANCE);
                    } else {
                        instance_id = (uint16_t) strtoul_result;
                        feature_data_p->call_info_feat_data.caller_id.call_instance_id = instance_id;
                    }
                }
            }
        } else if (!cpr_strncasecmp(parm_p, SIP_CI_GCID,
                                    sizeof(SIP_CI_GCID) - 1)) {
            parm_p = parm_p + sizeof(SIP_CI_GCID) - 1;
            SKIP_LWS(parm_p);
            memset(feature_data_p->call_info_feat_data.global_call_id, 0, CC_GCID_LEN);
            if (*parm_p) {
              temp_p = strchr(parm_p, SEMI_COLON);
              if (temp_p) {
                unsigned int length = ((temp_p - parm_p)<CC_GCID_LEN) ?
                                          (temp_p - parm_p):(CC_GCID_LEN);
                sstrncpy(feature_data_p->call_info_feat_data.global_call_id, parm_p, length);
              } else {
                // No Semicolon found this could be the last parameter
                sstrncpy(feature_data_p->call_info_feat_data.global_call_id, parm_p, CC_GCID_LEN);
              }
              feature_data_p->call_info_feat_data.global_call_id[CC_GCID_LEN-1] = 0;
            }
        } else if (!cpr_strncasecmp(parm_p, SIP_CI_DUSTINGCALL,
                                    sizeof(SIP_CI_DUSTINGCALL) - 1)) {
            parm_p = parm_p + sizeof(SIP_CI_DUSTINGCALL) - 1;
            SKIP_LWS(parm_p);
            feature_data_p->call_info_feat_data.dusting = TRUE;
        }

        parm_p = strchr(parm_p, SEMI_COLON);
    }
}

/*
 * parse_gen_parm
 *
 * Description:
 *
 * Parse feature parms where the only expected parm is the purpose.
 */
static void
parse_gen_parm (char *parm_p, cc_call_info_data_t * feature_data_p)
{
    if (!parm_p)
        return;

    while (parm_p) {
        parm_p++;
        SKIP_LWS(parm_p);

        if (!cpr_strncasecmp(parm_p, SIP_CI_GENERIC,
                             sizeof(SIP_CI_GENERIC) - 1)) {
            parm_p = parm_p + sizeof(SIP_CI_GENERIC) - 1;
            SKIP_LWS(parm_p);

            if (*parm_p) {
                if (!cpr_strncasecmp(parm_p, SIP_CI_GENERIC_ICON,
                                     sizeof(SIP_CI_GENERIC_ICON) - 1)) {
                    feature_data_p->purpose = CC_PURPOSE_ICON;
                } else {
                    if (!cpr_strncasecmp(parm_p, SIP_CI_GENERIC_INFO,
                                         sizeof(SIP_CI_GENERIC_INFO) - 1)) {
                        feature_data_p->purpose = CC_PURPOSE_INFO;
                    } else {
                        if (!cpr_strncasecmp(parm_p, SIP_CI_GENERIC_CARD,
                                             sizeof(SIP_CI_GENERIC_CARD) - 1)) {
                            feature_data_p->purpose = CC_PURPOSE_CARD;
                        }
                    }
                }
            }
        } else {
            break;
        }
        parm_p = strchr(parm_p, SEMI_COLON);
    }
}

/*
 * set_parm_defaults
 *
 * Description:
 *
 * A quick determination of the feature based on the string.
 */
static void
set_parm_defaults (cc_call_info_t *call_info_p)
{
    switch (call_info_p->type) {
    case CC_FEAT_HOLD:
    case CC_FEAT_RESUME:
    case CC_FEAT_NONE:
        call_info_p->data.hold_resume_reason = CC_REASON_NONE;
        break;

    case CC_FEAT_BARGE:
    case CC_FEAT_CBARGE:
        call_info_p->data.purpose = CC_PURPOSE_NONE;
        break;

    case CC_FEAT_CALLINFO:
        call_info_p->data.call_info_feat_data.policy = CC_POLICY_NONE;
        call_info_p->data.call_info_feat_data.security = CC_SECURITY_NONE;
        call_info_p->data.call_info_feat_data.orientation = CC_ORIENTATION_NONE;
        call_info_p->data.call_info_feat_data.ui_state = CC_UI_STATE_NONE;
        call_info_p->data.call_info_feat_data.priority = CC_CALL_PRIORITY_NORMAL;
        call_info_p->data.call_info_feat_data.global_call_id[0] = 0;
        call_info_p->data.call_info_feat_data.dusting = FALSE;
        break;

    default:
        break;
    }

}

/*
 * ccsip_decode_call_info_hdr
 *
 * Description:
 *
 * Main method which decodes a single call info header and stores the
 * related parms.
 *
 * Example Input:
 * ---------------
 * <urn:x-cisco-remotecc:hold>; reason= conference
 * <urn:x-cisco-remotecc:callinfo>; seCuRity=unsecure; orienTation= to
 */
static void
ccsip_decode_call_info_hdr (const char *call_info_hdr_p,
                            cc_call_info_t *call_info_p)
{
    char           *ptr = NULL;
    char           *laq_ptr = NULL;
    char           *raq_ptr = NULL;
    boolean         ret_val = FALSE;
    char            feat_string[FEAT_STRING_SIZE];

    memset(feat_string, '\0', sizeof(feat_string));

    /*
     * call_info_hdr_p and call_info_p are verified by caller so they
     * are not checked here.
     */

    ptr = laq_ptr = strchr(call_info_hdr_p, LAQUOT);
    raq_ptr = strchr(call_info_hdr_p, RAQUOT);

    // Parse out the remotecc string and the feature string.
    if (laq_ptr && raq_ptr) {
        ptr++;

        // Verify the remotecc string.
        if (!cpr_strncasecmp(ptr, URN_REMOTECC, sizeof(URN_REMOTECC) - 1)) {
            ptr += sizeof(URN_REMOTECC) - 1;
            sstrncpy(feat_string, ptr, raq_ptr - ptr + 1);

            // Which feature do we have in this header?
            call_info_p->type = which_feature(feat_string);

            if (call_info_p->type != CC_FEAT_NONE) {
                ret_val = TRUE;
                set_parm_defaults(call_info_p);
            }
        }
    }

    if (!ret_val) {
        return;
    }

    if (!(ptr = strchr(raq_ptr, SEMI_COLON))) {
        return;
    }

    switch (call_info_p->type) {
    case CC_FEAT_CALLINFO:
        parse_call_info_parm(ptr, &call_info_p->data);
        break;
    default:
        parse_gen_parm(ptr, &call_info_p->data);
    }
}

/*
 * ccsip_encode_call_info_hdr
 *
 * Description:
 *
 * Encode the call info header using the passed in feature id and
 * feature specific data.
 *
 * The miscParms parameter will usually be null.  It exists in case
 * you want to toss in an additional string parm without using the
 * encoding mechanism.  An example would be "extraParm= text".
 *
 * Remember to delete the store in the return parm.  It is the
 * caller's responsibility.
 */
char *
ccsip_encode_call_info_hdr (cc_call_info_t *call_info_p,
                            const char *misc_parms_p)
{
    static const char *fname = "ccsip_encode_call_info_hdr";
    char *header;

    header = (char *) cpr_malloc(MAX_SIP_HEADER_LENGTH);
    if (!header) {
        return NULL;
    }

    if (!call_info_p) {
        cpr_free(header);
        return NULL;
    }

    snprintf(header, MAX_SIP_HEADER_LENGTH, "<%s", URN_REMOTECC);

    switch (call_info_p->type) {
    case CC_FEAT_HOLD:
    case CC_FEAT_RESUME:
        if (call_info_p->type == CC_FEAT_HOLD) {
            sstrncat(header, SIP_CI_HOLD_STR,
                    MAX_SIP_HEADER_LENGTH - strlen(header));
        } else {
            sstrncat(header, SIP_CI_RESUME_STR,
                    MAX_SIP_HEADER_LENGTH - strlen(header));
        }
        sstrncat(header, ">", MAX_SIP_HEADER_LENGTH - strlen(header));

        switch (call_info_p->data.hold_resume_reason) {
        case CC_REASON_NONE:
        case CC_REASON_INTERNAL:
        case CC_REASON_SWAP:
            break;
        case CC_REASON_XFER:
            sstrncat(header, "; reason= ",
                    MAX_SIP_HEADER_LENGTH - strlen(header));
            sstrncat(header, SIP_CI_HOLD_REASON_XFER,
                    MAX_SIP_HEADER_LENGTH - strlen(header));
            break;
        case CC_REASON_CONF:
            sstrncat(header, "; reason= ",
                    MAX_SIP_HEADER_LENGTH - strlen(header));
            sstrncat(header, SIP_CI_HOLD_REASON_CONF,
                    MAX_SIP_HEADER_LENGTH - strlen(header));
            break;
        default:
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX  "unsupported hold_resume_reason\n",
                              fname);
            cpr_free(header);
            return NULL;
        }

        /* Add swap information */
        if (call_info_p->data.call_info_feat_data.swap == TRUE) {
            sstrncat(header, "; operation= swap",
                    MAX_SIP_HEADER_LENGTH - strlen(header));
        }

        if (call_info_p->data.call_info_feat_data.protect == TRUE) {
            sstrncat(header, "; protect= true; noholdreversion",
                    MAX_SIP_HEADER_LENGTH - strlen(header));
        }

        break;

    case CC_FEAT_INIT_CALL:
        /* Add global call id here */
        if (call_info_p->data.initcall.gcid[0] != '\0') {
            sstrncat(header, "callinfo>; gci= ",
                    MAX_SIP_HEADER_LENGTH - strlen(header));
            sstrncat(header, call_info_p->data.initcall.gcid,
                    MAX_SIP_HEADER_LENGTH - strlen(header));
        } else {
            cpr_free(header);
            return NULL;
        }
        /* Add the monitor mode here if it exists */
        if (call_info_p->data.initcall.monitor_mode != CC_MONITOR_NONE) {
            sstrncat(header, "; mode=",
                    MAX_SIP_HEADER_LENGTH - strlen(header));

            switch (call_info_p->data.initcall.monitor_mode) {

            case CC_MONITOR_SILENT :
                sstrncat(header, SIP_CI_SILENT_STR,
                        MAX_SIP_HEADER_LENGTH - strlen(header));
                break;

            case CC_MONITOR_COACHING :
                sstrncat(header, SIP_CI_COACHING_STR,
                        MAX_SIP_HEADER_LENGTH - strlen(header));
                break;

            default:
                break;
            }
        }
        break;

    case CC_FEAT_TOGGLE_TO_WHISPER_COACHING:
        sstrncat(header, "callinfo>",
        	MAX_SIP_HEADER_LENGTH - strlen(header));
        sstrncat(header, "; mode=",
            MAX_SIP_HEADER_LENGTH - strlen(header));
        sstrncat(header, SIP_CI_COACHING_STR,
            MAX_SIP_HEADER_LENGTH - strlen(header));

        break;

    case CC_FEAT_TOGGLE_TO_SILENT_MONITORING:
        sstrncat(header, "callinfo>",
            MAX_SIP_HEADER_LENGTH - strlen(header));
        sstrncat(header, "; mode=",
            MAX_SIP_HEADER_LENGTH - strlen(header));
        sstrncat(header, SIP_CI_SILENT_STR,
            MAX_SIP_HEADER_LENGTH - strlen(header));

        break;

    default:
        cpr_free(header);
        return NULL;
    }


    if (misc_parms_p) {
        sstrncat(header, misc_parms_p,
                MAX_SIP_HEADER_LENGTH - strlen(header));
    }
    sstrncat(header, "\0", MAX_SIP_HEADER_LENGTH - strlen(header));
    return (header);
}

/*
 * ccsip_free_call_info_header
 *
 * Description:
 *
 * Frees the memory allocated to a call info structure.
 */
void
ccsip_free_call_info_header (cc_call_info_t *call_info_p)
{
	if(call_info_p->type == CC_FEAT_CALLINFO) {

	}
    cpr_free(call_info_p);
}

/*
 * ccsip_process_call_info_header
 *
 * Description:
 *
 * Checks if there is a call info header in the provided SIP message. If there is,
 * the call info in the CCB is cleared and the new call info is parsed into the
 * CCB call info structure.
 */
void
ccsip_process_call_info_header (sipMessage_t *request_p, ccsipCCB_t *ccb)
{
    char       *call_info_hdrs[MAX_CALL_INFO_HEADERS];
    uint16_t    num_call_info_headers;
    int         i = 0;

    if (!ccb) {
        return;
    }

    if (ccb->in_call_info) {
        ccsip_free_call_info_header(ccb->in_call_info);
        ccb->in_call_info = NULL;
    }

    if (!request_p) {
        return;
    }

    memset(call_info_hdrs, 0, MAX_CALL_INFO_HEADERS * sizeof(char *));

    num_call_info_headers = sippmh_get_num_particular_headers(request_p,
                                                              SIP_HEADER_CALL_INFO,
                                                              SIP_HEADER_CALL_INFO,
                                                              call_info_hdrs,
                                                              MAX_CALL_INFO_HEADERS);

    if (num_call_info_headers > 0) {
        ccb->in_call_info = (cc_call_info_t *)
            cpr_calloc(1, sizeof(cc_call_info_t));
        if (ccb->in_call_info) {

            ccb->in_call_info->data.call_info_feat_data.feature_flag = 0;

            // Parse each Call-Info header
            for (i = 0; i < MAX_CALL_INFO_HEADERS; i++) {
                if (call_info_hdrs[i]) {
                    ccsip_decode_call_info_hdr(call_info_hdrs[i], ccb->in_call_info);
                }
            }

        } else {
            ccb->in_call_info = NULL;
        }
    }

}

/*
 * ccsip_store_call_info
 *
 * Description: Used for storing call_info received from GSM
 *
 * Store specified call info structure in ccb.
 */
void
ccsip_store_call_info (cc_call_info_t *call_info_p, ccsipCCB_t *ccb)
{
    if (!ccb) {
        return;
    }

    if (ccb->out_call_info) {
        ccsip_free_call_info_header(ccb->out_call_info);
        ccb->out_call_info = NULL;
    }

    if (call_info_p->type != CC_FEAT_NONE) {
        ccb->out_call_info = (cc_call_info_t *)
            cpr_malloc(sizeof(cc_call_info_t));
        if (ccb->out_call_info) {
            memcpy(ccb->out_call_info, call_info_p, sizeof(cc_call_info_t));
        } else {
            ccb->out_call_info = NULL;
        }
    }
}
