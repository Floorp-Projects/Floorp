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

/*
 * Implements functions to parse and create SDP(Session Description Protocol)
 * (RFC 2327) messages. Coded to be sufficient for SIP only.
 * Note: The SDP body fields as per the RFC are :
 * Optional items are marked with a `*'.
 *
 * Session description
 *       v=  (protocol version)
 *       o=  (owner/creator and session identifier).
 *       s=  (session name)
 *       i=* (session information)
 *       u=* (URI of description)
 *       e=* (email address)
 *       p=* (phone number)
 *       c=* (connection information - not required if included in all media)
 *       b=* (bandwidth information)
 *       One or more time descriptions (see below)
 *       z=* (time zone adjustments)
 *       k=* (encryption key)
 *       a=* (zero or more session attribute lines)
 *       Zero or more media descriptions (see below)
 *
 *Time description
 *       t=  (time the session is active)
 *       r=* (zero or more repeat times)
 *
 *Media description
 *       m=  (media name and transport address)
 *       i=* (media title)
 *       c=* (connection information - optional if included at session-level)
 *       b=* (bandwidth information)
 *       k=* (encryption key)
 *       a=* (zero or more media attribute lines)
 *
 * Note that the mandatory fields are :
 *  v= , o=, s=, t=, m=
 * Email address and phone number (e=, p=) fields would very soon be useful
 * to SDP in large scale SIP networks.
 *
 * t= 0 0
 * would indicate an unbound or a permanent session . This would be changed
 * to specific (or probably user-configured values) once the time duration
 * of SIP call sessions are recorded and are used for scheduling.
 */


#include "cpr_types.h"
#include "cpr_memory.h"
#include "sdp.h"
#include "text_strings.h"
#include "ccsip_sdp.h"
#include "ccsip_core.h"
#include "phone_debug.h"



static void *ccsip_sdp_config = NULL; /* SDP parser/builder configuration */

/*
 * sip_sdp_init
 *
 * This function initializes SDP parser with SIP-specific parameters.
 * This includes supported media, network types, address types,
 * transports and codecs.
 *
 * The function must be called once.
 *
 * Returns TRUE  - successful
 *         FALSE - failed
 */
boolean
sip_sdp_init (void)
{
    ccsip_sdp_config = sdp_init_config();
    if (!ccsip_sdp_config) {
        CCSIP_ERR_MSG("sdp_init_config() failure");
        return FALSE;
    }

    sdp_media_supported(ccsip_sdp_config, SDP_MEDIA_AUDIO, TRUE);
    sdp_media_supported(ccsip_sdp_config, SDP_MEDIA_VIDEO, TRUE);
    sdp_media_supported(ccsip_sdp_config, SDP_MEDIA_APPLICATION, TRUE);
    sdp_media_supported(ccsip_sdp_config, SDP_MEDIA_DATA, TRUE);
    sdp_media_supported(ccsip_sdp_config, SDP_MEDIA_CONTROL, TRUE);
    sdp_media_supported(ccsip_sdp_config, SDP_MEDIA_NAS_RADIUS, TRUE);
    sdp_media_supported(ccsip_sdp_config, SDP_MEDIA_NAS_TACACS, TRUE);
    sdp_media_supported(ccsip_sdp_config, SDP_MEDIA_NAS_DIAMETER, TRUE);
    sdp_media_supported(ccsip_sdp_config, SDP_MEDIA_NAS_L2TP, TRUE);
    sdp_media_supported(ccsip_sdp_config, SDP_MEDIA_NAS_LOGIN, TRUE);
    sdp_media_supported(ccsip_sdp_config, SDP_MEDIA_NAS_NONE, TRUE);
    sdp_media_supported(ccsip_sdp_config, SDP_MEDIA_IMAGE, TRUE);
    sdp_media_supported(ccsip_sdp_config, SDP_MEDIA_TEXT, TRUE);
    sdp_nettype_supported(ccsip_sdp_config, SDP_NT_INTERNET, TRUE);
    sdp_addrtype_supported(ccsip_sdp_config, SDP_AT_IP4, TRUE);
    sdp_addrtype_supported(ccsip_sdp_config, SDP_AT_IP6, TRUE);
    sdp_transport_supported(ccsip_sdp_config, SDP_TRANSPORT_RTPAVP, TRUE);
    sdp_transport_supported(ccsip_sdp_config, SDP_TRANSPORT_UDPTL, TRUE);
    sdp_require_session_name(ccsip_sdp_config, FALSE);

    return (TRUE);
}

/*
 * sip_sdp_create()
 *
 * Allocate a standard SDP with SIP config and set debug options
 * based on ccsip debug settings
 */
void *
sipsdp_create (void)
{
    const char *fname = "sipsdp_create :";
    void *sdp;
//    char debug_str[80];

    sdp = sdp_init_description(ccsip_sdp_config);
    if (!sdp) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"SDP allocation failure\n", fname);
        return (NULL);
    }

    /*
     * Map "ccsip debug events (or info)" to SDP warnings
     *     "ccsip debug errors to SDP errors
     */
    CCSIP_INFO_DEBUG {
        sdp_debug(sdp, SDP_DEBUG_WARNINGS, TRUE);
    }

    CCSIP_ERR_DEBUG {
        sdp_debug(sdp, SDP_DEBUG_ERRORS, TRUE);
    }


#ifdef DEBUG_SDP_LIB
    /*
     * Enabling this is redundant to the existing message printing
     * available for the entire SIP text messages, so it is not
     * enabled here. It is useful for debugging the SDP parser,
     * however.
     */
    CCSIP_MESSAGES_DEBUG {
        sdp_debug(sdp, SDP_DEBUG_TRACE, TRUE);
    }
#endif

    return (sdp);
}

/*
 * sipsdp_free()
 *
 * Frees the SIP SDP structure, the contained common SDP structure,
 * and each stream structure in the linked stream list.
 *
 * sip_sdp is set to NULL after being freed.
 */
void
sipsdp_free (cc_sdp_t **sip_sdp)
{
    const char *fname = "sipsdp_free: ";
    sdp_result_e sdp_ret;

    if (!*sip_sdp) {
        return;
    }

    if ((*sip_sdp)->src_sdp) {
        sdp_ret = sdp_free_description((*sip_sdp)->src_sdp);
        if (sdp_ret != SDP_SUCCESS) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"%d while freeing src_sdp\n",
                          fname, sdp_ret);
        }
    }
    if ((*sip_sdp)->dest_sdp) {
        sdp_ret = sdp_free_description((*sip_sdp)->dest_sdp);
        if (sdp_ret != SDP_SUCCESS) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"%d while freeing dest_sdp\n",
                          fname, sdp_ret);
        }
    }

    SDP_FREE(*sip_sdp);
}

/*
 * sipsdp_info_create()
 *
 * Allocates and initializes a SIP SDP structure.  This function does
 * not create the src_sdp, dest_sdp, or streams list.
 *
 * Returns: pointer to SIP SDP structure - if successful
 *          NULL                         - failure to allocate storage
 */
cc_sdp_t *
sipsdp_info_create (void)
{
    cc_sdp_t *sdp_info = (cc_sdp_t *) cpr_malloc(sizeof(cc_sdp_t));

    if (sdp_info) {
        sdp_info->src_sdp = NULL;
        sdp_info->dest_sdp = NULL;
    }

    return (sdp_info);
}


/*
 * sipsdp_src_dest_free()
 *
 * Frees the SRC and/or DEST SDP.  It will also free the sdp_info
 * structure if both SRC and DEST SDP are freed.
 *
 * Input:
 *   flags   - bitmask indicating if src and/or dest sdp should be
 *             freed
 *
 * Returns:
 *   sdp_info is set to NULL if freed.
 */
void
sipsdp_src_dest_free (uint16_t flags, cc_sdp_t **sdp_info)
{
    const char *fname = "sipsdp_src_dest_free: ";
    sdp_result_e sdp_ret;

    if ((sdp_info == NULL) || (*sdp_info == NULL)) {
        return;
    }

    /* Free the SRC and/or DEST SDP */
    if (flags & CCSIP_SRC_SDP_BIT) {
        if ((*sdp_info)->src_sdp) {
            sdp_ret = sdp_free_description((*sdp_info)->src_sdp);
            if (sdp_ret != SDP_SUCCESS) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"%d while freeing src_sdp\n",
                              fname, sdp_ret);
            }
            (*sdp_info)->src_sdp = NULL;
        }
    }

    if (flags & CCSIP_DEST_SDP_BIT) {
        if ((*sdp_info)->dest_sdp) {
            sdp_ret = sdp_free_description((*sdp_info)->dest_sdp);
            if (sdp_ret != SDP_SUCCESS) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"%d while freeing dest_sdp\n",
                              fname, sdp_ret);
            }
            (*sdp_info)->dest_sdp = NULL;
        }

    }

    /*
     * If both src and dest sdp are NULL, there is no need to keep the
     * sdp_info structure around.  Free it.
     */
    if (((*sdp_info)->src_sdp == NULL) && ((*sdp_info)->dest_sdp == NULL)) {
        sipsdp_free(sdp_info);
        *sdp_info = NULL;
    }
}


/*
 * sipsdp_src_dest_create()
 *
 * Allocates and initializes a SRC and/or DEST SDP.  If the sdp_info
 * structure has not yet been allocated, it is allocated here.
 *
 * Input:
 *   flags   - bitmask indicating if src and/or dest sdp should be
 *             created
 *
 * Returns: pointer to SIP SDP structure - if successful
 *          NULL                         - failure to allocate storage
 */
void
sipsdp_src_dest_create (uint16_t flags, cc_sdp_t **sdp_info)
{

    if (!(*sdp_info)) {
        *sdp_info = sipsdp_info_create();
        if (!(*sdp_info)) {
            return;
        }
    }

    /* Create the SRC and/or DEST SDP */
    if (flags & CCSIP_SRC_SDP_BIT) {
        (*sdp_info)->src_sdp = sipsdp_create();
        if (!((*sdp_info)->src_sdp)) {
            sipsdp_src_dest_free(flags, sdp_info);
            return;
        }
    }

    if (flags & CCSIP_DEST_SDP_BIT) {
        (*sdp_info)->dest_sdp = sipsdp_create();
        if (!((*sdp_info)->dest_sdp)) {
            sipsdp_src_dest_free(flags, sdp_info);
            return;
        }
    }
}

/*
 * sipsdp_create_from_buf()
 *
 * Parse buffer into common SDP structure
 *
 * Returns: NULL if non-recoverable error encountered
 *
 */
cc_sdp_t *
sipsdp_create_from_buf (char *buf, uint32_t nbytes, cc_sdp_t *sdp)
{
    const char *fname = "sipsdp_create_from_buf";
    cc_sdp_t *sip_info = NULL;

    if (!buf) {
        return (NULL);
    }

    if (sdp) {
        sip_info = sdp;
    } else {
        sipsdp_src_dest_create(CCSIP_DEST_SDP_BIT | CCSIP_SRC_SDP_BIT,
                               &sip_info);
    }

    if (!sip_info) {
        /*
         * make sure that the src_sdp and dest_sdp are
         * valid as well.
         */
        return (NULL);
    }


    if (sdp_parse(sip_info->dest_sdp, &buf, (uint16_t)nbytes) != SDP_SUCCESS) {
        sipsdp_src_dest_free(CCSIP_DEST_SDP_BIT | CCSIP_SRC_SDP_BIT,
                             &sip_info);
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error parsing SDP\n", fname);
        return (NULL);
    }

    return (sip_info);
}

/*
 * sipsdp_write_to_buf()
 *
 * This function builds the specified SDP in a text buffer and returns
 * a pointer to this buffer.
 *
 * Returns: pointer to buffer - no errors
 *          NULL              - errors were encountered while building
 *                              the SDP. The details of the build failure
 *                              can be determined by enabling SDP debugs
 *                              and by examining SDP library error counters.
 */
char *
sipsdp_write_to_buf (cc_sdp_t *sdp_info, uint32_t *retbytes)
{
    const char *fname = "sipsdp_write_to_buf";
    char *buf, *new_buf;
    char *sdp_buf;
    uint32_t sdp_len;
    sdp_result_e rc;

    if (!sdp_info || !sdp_info->src_sdp) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"NULL sdp_info or src_sdp\n", fname);
        return (NULL);
    }

    /*
     * Allocate storage for the SDP text
     */
    buf = (char *) cpr_malloc(CCSIP_SDP_BUF_SIZE);
    if (!buf) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"malloc failure\n", fname);
        return (NULL);
    }

    sdp_buf = buf;

    if ((rc = sdp_build(sdp_info->src_sdp, &sdp_buf, CCSIP_SDP_BUF_SIZE))
        != SDP_SUCCESS) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"sdp_build rc=%s\n", DEB_F_PREFIX_ARGS(SIP_SDP, fname),
                         sdp_get_result_name(rc));

        if (rc == SDP_POTENTIAL_SDP_OVERFLOW) {
            /* SDP may have been truncated. Issue a warning and continue. */
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Build SDP buffer overflow\n", fname);
        } else {
            cpr_free(buf);
            *retbytes = 0;
            return (NULL);
        }
    }

    /*
     * Compute length of SDP
     */
    sdp_len = sdp_buf - buf;
    /*
     * Minimize the memory impact on the SDP buffer by reallocating buffer
     * with a smaller size that fits the actual SDP body for the size.
     */
    if ((CCSIP_SDP_BUF_SIZE - sdp_len) > 64) {
        /*
         * Allocate space with NULL string character, the
         * output buffer content contains the NULL char but the
         * the length represents an actual length of SDP
         * without NULL char. The NULL char is added just in case,
         * there is code that uses strlen() on the SDP then
         * it will terminate properly.
         */
        new_buf = (char *) cpr_malloc(sdp_len + 1);
        if (new_buf != NULL) {
            memcpy(new_buf, buf, sdp_len);
            new_buf[sdp_len] = '\0';
            cpr_free(buf);
            buf = new_buf;
        }
    }
    *retbytes = sdp_len;

    return (buf);
}
