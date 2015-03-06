/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sdp_os_defs.h"
#include "sdp.h"
#include "sdp_private.h"

#include "CSFLog.h"

static const char* logTag = "sdp_config";

/* Function:    void *sdp_init_config()
 * Description: Initialize SDP configuration structure with the
 *              following defaults:
 *              All debug levels turned OFF.
 *              All token lines required per RFC2327.
 *              No media types supported.
 *              No network types supported.
 *              No address types supported.
 *              No transport types supported.
 * Parameters:	None.
 * Returns:	A handle for the configuration as a void ptr.
 */
sdp_conf_options_t *sdp_init_config ()
{
    int i;
    sdp_conf_options_t *conf_p;

    conf_p = SDP_MALLOC(sizeof(sdp_conf_options_t));

    if (!conf_p) {
        CSFLogError(logTag, "SDP: could not allocate configuration object.");
        return NULL;
    }

    /* Set default debug flags. */
    conf_p->debug_flag[SDP_DEBUG_TRACE]    = FALSE;
    conf_p->debug_flag[SDP_DEBUG_WARNINGS] = FALSE;
    conf_p->debug_flag[SDP_DEBUG_ERRORS]   = FALSE;

    /* Set required lines flags.  Note: Only need to set those that */
    /* are questionable.  Most lines aren't required by default.    */
    conf_p->version_reqd       = TRUE;
    conf_p->owner_reqd         = TRUE;
    conf_p->session_name_reqd  = TRUE;
    conf_p->timespec_reqd      = TRUE;

    /* No media types supported by default. */
    for (i=0; i < SDP_MAX_MEDIA_TYPES; i++) {
        conf_p->media_supported[i] = FALSE;
    }

    /* No network types supported by default. */
    for (i=0; i < SDP_MAX_NETWORK_TYPES; i++) {
        conf_p->nettype_supported[i] = FALSE;
    }

    /* No address types supported by default. */
    for (i=0; i < SDP_MAX_ADDR_TYPES; i++) {
        conf_p->addrtype_supported[i] = FALSE;
    }

    /* No transport types supported by default. */
    for (i=0; i < SDP_MAX_TRANSPORT_TYPES; i++) {
        conf_p->transport_supported[i] = FALSE;
    }

    /* No choose parameters allowed by default. */
    for (i=0; i < SDP_MAX_CHOOSE_PARAMS; i++) {
        conf_p->allow_choose[i] = FALSE;
    }

    /* Initialize statistics counts */
    conf_p->num_parses              = 0;
    conf_p->num_builds              = 0;
    conf_p->num_not_sdp_desc        = 0;
    conf_p->num_invalid_token_order = 0;
    conf_p->num_invalid_param       = 0;
    conf_p->num_no_resource         = 0;

    /* Parse error handler stuff */
    conf_p->error_handler           = NULL;
    conf_p->error_handler_context   = NULL;

    CSFLogInfo(logTag, "SDP: Initialized config pointer: %p", conf_p);

    return (conf_p);
}

void sdp_free_config(sdp_conf_options_t* conf_p) {
  if (conf_p) {
    SDP_FREE(conf_p);
  }
}

/* Function:    void sdp_appl_debug(sdp_conf_options_t *conf_p, sdp_debug_e debug_type,
 *                                  tinybool my_bool);
 * Description:	Define the default type of debug for the application.
 *              Valid debug types are ERRORS, WARNINGS, and TRACE.  Each
 *              debug type can be turned on/off individually.  The
 *              default debug level can be redefined at any time.
 * Parameters:	conf_p     The config handle returned by sdp_init_config.
 *              debug_type Specifies the debug type being enabled/disabled.
 *              debug_flag  Defines whether the debug should be enabled or not.
 * Returns:	Nothing.
 */
void sdp_appl_debug (sdp_conf_options_t *conf_p, sdp_debug_e debug_type,
                     tinybool debug_flag)
{
    if (debug_type < SDP_MAX_DEBUG_TYPES)  {
        conf_p->debug_flag[debug_type] = debug_flag;
    }
}


/* Functions:	void sdp_require_version
 *              void sdp_require_owner
 *              void sdp_require_session_name
 *              void sdp_require_timespec
 * Description:	These functions allow the application to not require several
 *              of the tokens that are specifically required by RFC 2327.
 * Parameters:	conf_p    The config handle returned by sdp_init_config.
 *              version_required   TRUE or FALSE whether the token should
 *              be required.
 * Returns:	Nothing.
 */
void sdp_require_version (sdp_conf_options_t *conf_p, tinybool version_required)
{
    conf_p->version_reqd = version_required;
}

void sdp_require_owner (sdp_conf_options_t *conf_p, tinybool owner_required)
{
    conf_p->owner_reqd = owner_required;
}

void sdp_require_session_name (sdp_conf_options_t *conf_p, tinybool sess_name_required)
{
    conf_p->session_name_reqd = sess_name_required;
}

void sdp_require_timespec (sdp_conf_options_t *conf_p, tinybool timespec_required)
{
    conf_p->timespec_reqd = timespec_required;
}


/* Function:    sdp_media_supported
 * Description: These functions allow the application to specify which
 *              media types it supports. The application must set any/all
 *              as required.  No media types are supported by default.
 * Parameters:  conf_p    The config handle returned by sdp_init_config.
 *              nettype     The network type for which support is being set.
 *              media_supported TRUE or FALSE whether the support is provided.
 * Returns:     Nothing.
 */
void sdp_media_supported (sdp_conf_options_t *conf_p, sdp_media_e media_type,
                         tinybool media_supported)
{
    conf_p->media_supported[media_type] = media_supported;
}


/* Function:    sdp_nettype_supported
 * Description: This function allows the application to specify which
 *              network types it supports.  The application must set
 *              any/all as required.  No network types are supported by
 *              default.
 * Parameters:  conf_p    The config handle returned by sdp_init_config.
 *              nettype     The network type for which support is being set.
 *              nettype_supported TRUE or FALSE whether the support is
 *                          provided.
 * Returns:     Nothing.
 */
void sdp_nettype_supported (sdp_conf_options_t *conf_p, sdp_nettype_e nettype,
                            tinybool nettype_supported)
{
    conf_p->nettype_supported[nettype] = nettype_supported;
}


/* Function:    sdp_addrtype_supported
 * Description: This function allows the application to specify which
 *              address types it supports.  The application must set
 *              any/all as required.  No address types are supported by
 *              default.
 * Parameters:  conf_p    The config handle returned by sdp_init_config.
 *              addrtype    The address type for which support is being set.
 *              addrtype_supported TRUE or FALSE whether the support is
 *                          provided.
 * Returns:     Nothing.
 */
void sdp_addrtype_supported (sdp_conf_options_t *conf_p, sdp_addrtype_e addrtype,
                             tinybool addrtype_supported)
{
    conf_p->addrtype_supported[addrtype] = addrtype_supported;
}


/* Function:    sdp_transport_supported
 * Description: This function allows the application to specify which
 *              transport types it supports.  The application must set
 *              any/all as required.  No transport types are supported
 *              by default.
 * Parameters:  conf_p    The config handle returned by sdp_init_config.
 *              transport   The transport type for which support is being set.
 *              transport_supported  TRUE or FALSE whether the support is
 *                          provided.
 * Returns:     Nothing.
 */
void sdp_transport_supported (sdp_conf_options_t *conf_p, sdp_transport_e transport,
                              tinybool transport_supported)
{
    conf_p->transport_supported[transport] = transport_supported;
}


/* Function:    sdp_allow_choose
 * Description: These functions allow the CHOOSE parameter `$' to be
 *              specified in place of certain parameters.
 * Parameters:  conf_p        The config handle returned by sdp_init_config.
 *              param           The param that may or may not be CHOOSE.
 *              choose_allowed  TRUE or FALSE whether the CHOOSE parameter
 *                              should be allowed.
 * Returns:     Nothing.
 */
void sdp_allow_choose (sdp_conf_options_t *conf_p, sdp_choose_param_e param, tinybool choose_allowed)
{
    if (param < SDP_MAX_CHOOSE_PARAMS) {
        conf_p->allow_choose[param] = choose_allowed;
    }
}

void sdp_config_set_error_handler(sdp_conf_options_t *conf_p,
                                  sdp_parse_error_handler handler,
                                  void *context)
{
    conf_p->error_handler = handler;
    conf_p->error_handler_context = context;
}
