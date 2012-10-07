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

#include "cpr_types.h"
#include "cpr_stdio.h"

#include "ccsip_core.h"
#include "ccsip_callinfo.h"
#include "ccsip_messaging.h"
#include "uiapi.h"
#include "lsm.h"
#include "fsm.h"
#include "vcm.h"
#include "phone_debug.h"
#include "singly_link_list.h"
#include "ccapi.h"

extern int httpish_strncasecmp(const char *s1, const char *s2, size_t len);

typedef unsigned int info_index_t;
typedef unsigned int type_index_t;

typedef struct {
    info_package_handler_t handler;
    info_index_t info_index;    // the index of the info_package in g_registered_info array
                                // i.e., the value returned by find_info_index()
    type_index_t type_index;    // the index of the content_type in g_registered_type array
                                // i.e., the value returned by find_type_index()
} handler_record_t;

/* Info Package handler registry */
static sll_handle_t s_handler_registry = NULL;

#define INDEX_NOT_FOUND     ((unsigned int)-1)

/*
 * g_registered_info[] contains the Info Package strings (such as
 * "conference") for the registered handlers.
 */
char *g_registered_info[MAX_INFO_HANDLER];

static char *s_registered_type[MAX_INFO_HANDLER];

static sll_match_e is_matching_type(void *find_by_p, void *data_p);
static info_index_t find_info_index(const char *info_package);
static info_index_t find_next_available_info_index(void);
static type_index_t find_type_index(const char *type_package);
static type_index_t find_next_available_type_index(void);
static handler_record_t *find_handler_record(info_index_t info_index,
                                             type_index_t type_index);
static boolean is_info_package_registered(info_index_t info_index);
static boolean is_content_type_registered(type_index_t type_index);
static void update_recv_info_list(const char *header_field_value,
                                  string_t *info_packages);
static void media_control_info_package_handler(line_t line, callid_t call_id,
                                               const char *info_package,
                                               const char *content_type,
                                               const char *message_body);
#ifdef _CONF_ROSTER_
static void conf_info_package_handler(line_t line, callid_t call_id,
                                           const char *info_package,
                                           const char *content_type,
                                           const char *message_body);
#endif



/*
 *  Function: find_info_index
 *
 *  Parameters:
 *      info_package - the Info Package to find
 *
 *  Description:
 *      Finds the Info Package in g_registered_info array.
 *
 *  Return:
 *      info_index_t - the index of the Info Package in the array
 *      INDEX_NOT_FOUND - the Info Package was not found in the array
 */
static info_index_t
find_info_index(const char *info_package)
{
    info_index_t info_index;

    for (info_index = 0; info_index < MAX_INFO_HANDLER; info_index++) {
        if (g_registered_info[info_index] &&
            httpish_strncasecmp(info_package,
                                g_registered_info[info_index],
                                strlen(g_registered_info[info_index])) == 0) {
            return info_index;
        }
    }

    return INDEX_NOT_FOUND;
}

/*
 *  Function: find_next_available_info_index
 *
 *  Parameters:
 *      None
 *
 *  Description:
 *      Finds an empty slot in g_registered_info array.
 *
 *  Return:
 *      info_index_t - the index of the empty slot in the array
 *      INDEX_NOT_FOUND - the array is full
 */
static info_index_t
find_next_available_info_index(void)
{
    info_index_t info_index;

    for (info_index = 0; info_index < MAX_INFO_HANDLER; info_index++) {
        if (g_registered_info[info_index] == NULL) {
            return info_index;
        }
    }

    return INDEX_NOT_FOUND;
}

/*
 *  Function: find_type_index
 *
 *  Parameters:
 *      content_type - the Content Type to find
 *
 *  Description:
 *      Finds the Content Type in s_registered_type array.
 *
 *  Return:
 *      type_index_t - the index of the Content Type in the array
 *      INDEX_NOT_FOUND - the Content Type was not found in the array
 */
static type_index_t
find_type_index(const char *content_type)
{
    type_index_t type_index;

    for (type_index = 0; type_index < MAX_INFO_HANDLER; type_index++) {
        if (s_registered_type[type_index] &&
            httpish_strncasecmp(content_type,
                                s_registered_type[type_index],
                                strlen(s_registered_type[type_index])) == 0) {
            return type_index;
        }
    }

    return INDEX_NOT_FOUND;
}

/*
 *  Function: find_next_available_type_index
 *
 *  Parameters:
 *      None
 *
 *  Description:
 *      Finds an empty slot in s_registered_type array.
 *
 *  Return:
 *      type_index_t - the index of the empty slot in the array
 *      INDEX_NOT_FOUND - the array is full
 */
static type_index_t
find_next_available_type_index(void)
{
    type_index_t type_index;

    for (type_index = 0; type_index < MAX_INFO_HANDLER; type_index++) {
        if (s_registered_type[type_index] == NULL) {
            return type_index;
        }
    }

    return INDEX_NOT_FOUND;
}

/*
 *  Function: is_matching_type
 *
 *  Parameters:
 *      find_by_p - searching criteria
 *      data_p - a node
 *
 *  Description:
 *      Checks to see if the node matches the searching criteria.
 *
 *  Return:
 *      SLL_MATCH_FOUND - the node is a match
 *      SLL_MATCH_NOT_FOUND - the node is not a match
 */
static sll_match_e
is_matching_type(void *find_by_p, void *data_p)
{
    handler_record_t *tuple = (handler_record_t *)find_by_p;
    handler_record_t *node = (handler_record_t *)data_p;

    if ((node->info_index == tuple->info_index) &&
        (node->type_index == tuple->type_index)) {
        return SLL_MATCH_FOUND;
    }
    return SLL_MATCH_NOT_FOUND;
}

/*
 *  Function: find_handler_record
 *
 *  Parameters:
 *      info_index - the index of the Info Package in g_registered_info array
 *      type_index - the index of the Content Type in s_registered_type array
 *
 *  Description:
 *      Finds the Info Package handler registered for the Info Package/Content
 *      Type pair.
 *
 *  Return:
 *      handler_record_t * - the registered handler record
 *      NULL - otherwise
 */
static handler_record_t *
find_handler_record(info_index_t info_index, type_index_t type_index)
{
    handler_record_t tuple;

    tuple.info_index = info_index;
    tuple.type_index = type_index;

    return (handler_record_t *)sll_find(s_handler_registry, &tuple);
}

/*
 *  Function: is_info_package_registered
 *
 *  Parameters:
 *      info_index - the index of the Info Package in g_registered_info array
 *
 *  Description:
 *      Checks to see if a handler was registered for the Info Package.
 *
 *  Return:
 *      TRUE - a handler was registered for the Info Package
 *      FALSE - otherwise
 */
static boolean
is_info_package_registered(info_index_t info_index)
{
    handler_record_t *record;

    for (record = (handler_record_t *)sll_next(s_handler_registry, NULL);
         record != NULL;
         record = (handler_record_t *)sll_next(s_handler_registry, record)) {
        if (record->info_index == info_index) {
            return TRUE;
        }
    }

    return FALSE;
}

/*
 *  Function: is_content_type_registered
 *
 *  Parameters:
 *      type_index - the index of the Content Type in s_registered_type array
 *
 *  Description:
 *      Checks to see if a handler was registered for the Content Type.
 *
 *  Return:
 *      TRUE - a handler was registered for the Content Type
 *      FALSE - otherwise
 */
static boolean
is_content_type_registered(type_index_t type_index)
{
    handler_record_t *record;

    for (record = (handler_record_t *)sll_next(s_handler_registry, NULL);
         record != NULL;
         record = (handler_record_t *)sll_next(s_handler_registry, record)) {
        if (record->type_index == type_index) {
            return TRUE;
        }
    }

    return FALSE;
}

/*
 *  Function: ccsip_register_info_package_handler
 *
 *  Parameters:
 *      info_package - the Info Package for the handler
 *      content_type - the Content Type for the handler
 *      handler - the handler
 *
 *  Description:
 *      Registers the handler for the Info Package/Content Type pair.
 *
 *  Return:
 *      SIP_OK - the handler was registered successfully
 *      SIP_ERROR - otherwise
 */
int
ccsip_register_info_package_handler(const char *info_package,
                                    const char *content_type,
                                    info_package_handler_t handler)
{
    static const char *fname = "ccsip_register_info_package_handler";
    info_index_t info_index;
    type_index_t type_index;
    char *tmp_info = NULL;
    char *tmp_type = NULL;
    handler_record_t *record;

    if (s_handler_registry == NULL) {
        CCSIP_DEBUG_TASK("%s: Info Package handler was not initialized\n", fname);
        return SIP_ERROR;
    }

    if ((info_package == NULL) || (content_type == NULL) || (handler == NULL)) {
        CCSIP_DEBUG_ERROR("%s: invalid parameter\n", fname);
        return SIP_ERROR;
    }

    /* Find the info_index for the info_package */
    info_index = find_info_index(info_package);

    if (info_index == INDEX_NOT_FOUND) {
        /* Find the first available slot */
        info_index = find_next_available_info_index();

        if (info_index == INDEX_NOT_FOUND) {
            CCSIP_DEBUG_ERROR("%s: maximum reached\n", fname);
            return SIP_ERROR;
        }

        tmp_info = cpr_strdup(info_package);
        if (tmp_info == NULL) {
            CCSIP_DEBUG_ERROR("%s: failed to duplicate info_package string\n", fname);
            return SIP_ERROR;
        }
    }

    /* Find the type_index for the content_type */
    type_index = find_type_index(content_type);

    if (type_index == INDEX_NOT_FOUND) {
        /* Find the first available slot */
        type_index = find_next_available_type_index();

        if (type_index == INDEX_NOT_FOUND) {
            CCSIP_DEBUG_ERROR("%s: maximum reached\n", fname);
            if (tmp_info != NULL) {
                cpr_free(tmp_info);
            }
            return SIP_ERROR;
        }

        tmp_type = cpr_strdup(content_type);
        if (tmp_type == NULL) {
            CCSIP_DEBUG_ERROR("%s: failed to duplicate info_package string\n", fname);
            if (tmp_info != NULL) {
                cpr_free(tmp_info);
            }
            return SIP_ERROR;
        }
    }

    /* Check to see if the info/type tuple has been registered before */
    if (find_handler_record(info_index, type_index) != NULL) {
        CCSIP_DEBUG_ERROR("%s: Info Package handler already registered\n", fname);
        return SIP_ERROR;
    }

    /*
     * At this point, info_index points to the slot in g_registered_info where
     * either:
     *
     * 1) the info_package is residing, or
     * 2) the info_package (a copy of which is pointed to by *tmp_info) will be
     *    copied to before the function returns
     *
     * type_index is similar.
     */

    record = (handler_record_t *)cpr_malloc(sizeof(handler_record_t));
    if (record == NULL) {
        if (tmp_type != NULL) {
            cpr_free(tmp_type);
        }
        if (tmp_info != NULL) {
            cpr_free(tmp_info);
        }
        CCSIP_DEBUG_ERROR("%s: failed to allocate info handler record\n", fname);
        return SIP_ERROR;
    }

    record->handler = handler;
    record->info_index = info_index;
    record->type_index = type_index;

    if (sll_append(s_handler_registry, record) != SLL_RET_SUCCESS) {
        cpr_free(record);
        if (tmp_type != NULL) {
            cpr_free(tmp_type);
        }
        if (tmp_info != NULL) {
            cpr_free(tmp_info);
        }
        CCSIP_DEBUG_ERROR("%s: failed to insert to the registry\n", fname);
        return SIP_ERROR;
    }

    if (tmp_info != NULL) {
        g_registered_info[info_index] = tmp_info;
    }
    if (tmp_type != NULL) {
        s_registered_type[type_index] = tmp_type;
    }

    return SIP_OK;
}

/*
 *  Function: ccsip_deregister_info_package_handler
 *
 *  Parameters:
 *      info_package - the Info Package for the handler
 *      content_type - the Content Type for the handler
 *      handler - the handler
 *
 *  Description:
 *      Deregisters the handler for the Info Package/Content Type pair.
 *
 *  Return:
 *      SIP_OK - the handler was registered successfully
 *      SIP_ERROR - otherwise
 */
int
ccsip_deregister_info_package_handler(const char *info_package,
                                      const char *content_type,
                                      info_package_handler_t handler)
{
    static const char *fname = "ccsip_deregister_info_package_handler";
    info_index_t info_index;
    type_index_t type_index;
    handler_record_t *record;

    if (s_handler_registry == NULL) {
        CCSIP_DEBUG_TASK("%s: Info Package handler was not initialized\n", fname);
        return SIP_ERROR;
    }

    /* Find the info_index for the info_package */
    info_index = find_info_index(info_package);
    if (info_index == INDEX_NOT_FOUND) {
        CCSIP_DEBUG_ERROR("%s: handler was not registered (%s)\n",
                          fname, info_package);
        return SIP_ERROR;
    }

    /* Find the type_index for the content_type */
    type_index = find_type_index(content_type);
    if (type_index == INDEX_NOT_FOUND) {
        CCSIP_DEBUG_ERROR("%s: handler was not registered (%s)\n",
                          fname, content_type);
        return SIP_ERROR;
    }

    /* Find the handler record */
    record = find_handler_record(info_index, type_index);
    if ((record == NULL) || (record->handler != handler)) {
        CCSIP_DEBUG_ERROR("%s: handler was not registered (%p)\n",
                          fname, handler);
        return SIP_ERROR;
    }

    (void)sll_remove(s_handler_registry, record);

    cpr_free(record);

    if (!is_info_package_registered(info_index)) {
        /* The info_package was not found in the registry, meaning we're
         * the last one who registered for this particular info_package */
        cpr_free(g_registered_info[info_index]);
        g_registered_info[info_index] = NULL;
    }

    if (!is_content_type_registered(type_index)) {
        /* The content_type was not found in the registry, meaning we're
         * the last one who registered for this particular content_type */
        cpr_free(s_registered_type[type_index]);
        s_registered_type[type_index] = NULL;
    }

    return SIP_OK;
}

/*
 *  Function:  update_recv_info_list
 *
 *  Parameters:
 *      header_field_value - the header field value to match (e.g.,
 *      "conference")
 *      info_packages - the Info Packages string to append the header
 *      field value to
 *
 *  Description:
 *      Checks to see if a handler is registered for the header field value
 *      (e.g., "conference"), if so, append the header field value to
 *      the end of info_packages.
 *
 *  Returns:
 *      None
 */
static void
update_recv_info_list(const char *header_field_value, string_t *info_packages)
{
    static const char *fname = "update_recv_info_list";
    info_index_t info_index;

    if ((header_field_value == NULL) || (info_packages == NULL) ||
        (*info_packages == NULL)) {
        CCSIP_DEBUG_ERROR("%s: invalid parameter\n", fname);
        return;
    }

    info_index = find_info_index(header_field_value);
    if (info_index != INDEX_NOT_FOUND) {
        /* Info-Package is supported */
        if (**info_packages == '\0') {
            *info_packages = strlib_update(*info_packages,
                                           g_registered_info[info_index]);
        } else {
            *info_packages = strlib_append(*info_packages, ", ");
            *info_packages = strlib_append(*info_packages,
                                           g_registered_info[info_index]);
        }
    }
}

/*
 *  Function:  ccsip_parse_send_info_header
 *
 *  Parameters:
 *      ccb         - the SIP CCB
 *      pSipMessage - the SIP message
 *
 *  Description:
 *      Checks the Send-Info header (if exists) to see if a handler was
 *      registered for the Info Package.  If so, append the Info Package
 *      to the end of recv_info_list.
 *
 *  Returns:
 *      None
 */
void
ccsip_parse_send_info_header(sipMessage_t *pSipMessage, string_t *recv_info_list)
{
    char *send_info[MAX_INFO_HANDLER];
    int count;
    int i;
    char *header_field_values;
    char *header_field_value;
    char *separator;

    // leading white spaces are trimmed, but not trailing ones
    count = sippmh_get_num_particular_headers(pSipMessage,
                                              SIP_HEADER_SEND_INFO,
                                              NULL,
                                              send_info,
                                              MAX_INFO_HANDLER);

    if (count == 0) {
        return;
    }

    for (i = 0; (i < count) && (i < MAX_INFO_HANDLER); i++) {
        header_field_values = cpr_strdup(send_info[i]);
        if (header_field_values == NULL) {
            return;
        }
        header_field_value = header_field_values;

        while ((separator = strchr(header_field_value, COMMA)) != NULL) {
            *separator++ = '\0';
            update_recv_info_list(header_field_value, recv_info_list);
            header_field_value = separator;
            SKIP_WHITE_SPACE(header_field_value);
        }
        update_recv_info_list(header_field_value, recv_info_list);

        cpr_free(header_field_values);
    }
}

/*
 *  Function: ccsip_handle_info_package
 *
 *  Parameters:
 *      ccb         - the SIP CCB
 *      pSipMessage - the SIP message
 *
 *  Description:
 *      Handles an incoming unsolicited Info Package message.
 *      XXX Currently this function only handles the first part in a
 *          multi-part Info Package message.
 *
 *  Return:
 *      SIP_OK - if request processed.
 *      SIP_ERROR - if there is error
 */
int
ccsip_handle_info_package(ccsipCCB_t *ccb, sipMessage_t *pSipMessage)
{
    static const char *fname = "ccsip_handle_info_package";
    const char  *info_package;
    const char  *content_type;
    info_index_t info_index;
    type_index_t type_index;
    handler_record_t *record;
    uint16_t     status_code;
    const char  *reason_phrase;
    int          return_code = SIP_ERROR;

    /* FIXME Media Control currently does not follow the IETF draft
             draft-ietf-sip-info-events-01, so short-circuit here and
             bypass all the Info Package related stuff below. */
    // leading white spaces are trimmed, but not trailing ones
    content_type = sippmh_get_cached_header_val(pSipMessage,
                                                CONTENT_TYPE);
    if (content_type &&
        httpish_strncasecmp(content_type,
                            SIP_CONTENT_TYPE_MEDIA_CONTROL,
                            strlen(SIP_CONTENT_TYPE_MEDIA_CONTROL)) == 0) {

        media_control_info_package_handler(ccb->dn_line, ccb->gsm_id,
                                           "",  // legacy mode, no Info Package
                                           SIP_CONTENT_TYPE_MEDIA_CONTROL,
                                           pSipMessage->mesg_body[0].msgBody);

        if (sipSPISendErrorResponse(pSipMessage, 200, SIP_SUCCESS_SETUP_PHRASE,
                                    0, NULL, NULL) != TRUE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                              fname, SIP_SUCCESS_SETUP_PHRASE);
            return SIP_ERROR;
        }

        return SIP_OK;
    }

    /*
     * Parse the Info-Package header
     */
    // leading white spaces are trimmed, but not trailing ones
    info_package = sippmh_get_header_val(pSipMessage,
                                         SIP_HEADER_INFO_PACKAGE,
                                         NULL);

    if (info_package == NULL) {
        /* No Info-Package header */
        CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Missing Info-Package header\n",
                            DEB_F_PREFIX_ARGS(SIP_INFO_PACKAGE, fname));

        if (pSipMessage->num_body_parts == 0) {
            /* No Info-Package header, and no body poarts */
            CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Missing message body\n",
                                DEB_F_PREFIX_ARGS(SIP_INFO_PACKAGE, fname));
            /* Send 200 OK for legacy UA support */
            status_code = 200;
            reason_phrase = SIP_SUCCESS_SETUP_PHRASE;
            return_code = SIP_OK;
        } else {
            /* No Info-Package header, but with body part(s) */
            if (pSipMessage->num_body_parts > 1) {
                CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Multipart Info Package"
                                    DEB_F_PREFIX_ARGS(SIP_INFO_PACKAGE, fname));
            }

            type_index = find_type_index(pSipMessage->mesg_body[0].msgContentType);
            if (type_index == INDEX_NOT_FOUND) {
                /* No Info-Package header, and Content-Type is not supported */
                CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Unsupported Content Type\n",
                                    DEB_F_PREFIX_ARGS(SIP_INFO_PACKAGE, fname));
                /* Send 415 Unsupported Media Type */
                status_code = SIP_CLI_ERR_MEDIA;
                reason_phrase = SIP_CLI_ERR_MEDIA_PHRASE;
            } else {
                /* No Info-Package header, but Conent-Type is supported */
                /* Send 200 OK for legacy UA support */
                status_code = 200;
                reason_phrase = SIP_SUCCESS_SETUP_PHRASE;
                return_code = SIP_OK;
            }
        }
    } else {
        /* With Info-Package header */
        if (pSipMessage->num_body_parts == 0) {
            /* With Info-Package header, but no body parts */
            CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Missing message body\n",
                                DEB_F_PREFIX_ARGS(SIP_INFO_PACKAGE, fname));

            /* ? */
            /* Send 489 Bad Event */
            status_code = SIP_CLI_ERR_BAD_EVENT;
            reason_phrase = SIP_CLI_ERR_BAD_EVENT_PHRASE;

        } else {
            /* With Info-Package header and body part(s) */
            if (pSipMessage->num_body_parts > 1) {
                CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Multipart Info Package "
                                    "(only the first part is processed)\n",
                                    DEB_F_PREFIX_ARGS(SIP_INFO_PACKAGE, fname));
            }

            info_index = find_info_index(info_package);
            if (info_index == INDEX_NOT_FOUND) {
                /* Info-Package is not supported */
                CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Unsupported Info Package\n",
                                    DEB_F_PREFIX_ARGS(SIP_INFO_PACKAGE, fname));

                /* Send 489 Bad Event */
                status_code = SIP_CLI_ERR_BAD_EVENT;
                reason_phrase = SIP_CLI_ERR_BAD_EVENT_PHRASE;

            } else {
                /* Info-Package is supported */
                type_index = find_type_index(pSipMessage->mesg_body[0].msgContentType);
                record = find_handler_record(info_index, type_index);
                if (record == NULL) {
                    /* Info-Package header is supported, but Content-Type is not */
                    CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Unsupported Content Type\n",
                                        DEB_F_PREFIX_ARGS(SIP_INFO_PACKAGE, fname));
                    /* Send 415 Unsupported Media Type */
                    status_code = SIP_CLI_ERR_MEDIA;
                    reason_phrase = SIP_CLI_ERR_MEDIA_PHRASE;
                } else {
                    /* a handler is registered */
                    (*record->handler)(ccb->dn_line, ccb->gsm_id,
                                       g_registered_info[record->info_index],
                                       s_registered_type[record->type_index],
                                       pSipMessage->mesg_body[0].msgBody);
                    /* Send 200 OK */
                    status_code = 200;
                    reason_phrase = SIP_SUCCESS_SETUP_PHRASE;
                    return_code = SIP_OK;
                }
            }
        }
    }

    if (sipSPISendErrorResponse(pSipMessage, status_code, reason_phrase,
                                0, NULL, NULL) != TRUE) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                          fname, reason_phrase);
        return SIP_ERROR;
    }

    return return_code;
}

static void
media_control_info_package_handler(line_t line, callid_t call_id,
                                   const char *info_package,
                                   const char *content_type,
                                   const char *message_body)
{
}

#ifdef _CONF_ROSTER_
static void
conf_info_package_handler(line_t line, callid_t call_id,
                               const char *info_package,
                               const char *content_type,
                               const char *message_body)
{
    static const char *fname = "conf_info_package_handler";

    CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"info_package: %s content_type: %s\n",
                        DEB_F_PREFIX_ARGS(SIP_INFO_PACKAGE, fname),
                        info_package, content_type);

    ui_info_received(line, lsm_get_ui_id(call_id), info_package, content_type,
                     message_body);
}
#endif

/*
 *  Function: ccsip_info_package_handler_init
 *
 *  Parameters:
 *      None
 *
 *  Description:
 *      Initializes the Info Package handler framework.
 *
 *  Return:
 *      SIP_OK - Info Package handler framework initialized successfully
 *      SIP_ERROR - otherwise
 */
int
ccsip_info_package_handler_init(void)
{
    static const char *fname = "ccsip_info_package_handler_init";
    info_index_t info_index;
    type_index_t type_index;

    if (s_handler_registry != NULL) {
        // Is this considered an error?
        CCSIP_DEBUG_TASK("%s: Info Package handler already initialized\n", fname);
        return SIP_OK;
    }

    /* Create the SLL */
    s_handler_registry = sll_create(is_matching_type);
    if (s_handler_registry == NULL) {
        CCSIP_DEBUG_ERROR("%s: failed to create the registry\n", fname);
        return SIP_ERROR;
    }

    for (info_index = 0; info_index < MAX_INFO_HANDLER; info_index++) {
        g_registered_info[info_index] = NULL;
    }

    for (type_index = 0; type_index < MAX_INFO_HANDLER; type_index++) {
        s_registered_type[type_index] = NULL;
    }

    // XXX Where is the best place to register the application-specific handler?
#ifdef _CONF_ROSTER_
    /* Register the handler for conference & x-cisco-conference Info Packages */
    ccsip_register_info_package_handler(INFO_PACKAGE_CONFERENCE,
                                        CONTENT_TYPE_CONFERENCE_INFO,
                                        conf_info_package_handler);
    ccsip_register_info_package_handler(INFO_PACKAGE_CISCO_CONFERENCE,
                                        CONTENT_TYPE_CONFERENCE_INFO,
                                        conf_info_package_handler);
#endif
    return SIP_OK;
}

/*
 *  Function: ccsip_info_package_handler_shutdown
 *
 *  Parameters:
 *      None
 *
 *  Description:
 *      Shuts down the Info Package handler framework.
 *
 *  Return:
 *      None
 */
void
ccsip_info_package_handler_shutdown(void)
{
    static const char *fname = "ccsip_info_package_handler_shutdown";
    info_index_t info_index;
    type_index_t type_index;
    handler_record_t *record;

    if (s_handler_registry == NULL) {
        // Is this considered an error?
        CCSIP_DEBUG_TASK("%s: Info Package handler was not initialized\n", fname);
        return;
    }

    for (type_index = 0; type_index < MAX_INFO_HANDLER; type_index++) {
        if (s_registered_type[type_index] != NULL) {
            cpr_free(s_registered_type[type_index]);
            s_registered_type[type_index] = NULL;
        }
    }

    for (info_index = 0; info_index < MAX_INFO_HANDLER; info_index++) {
        if (g_registered_info[info_index] != NULL) {
            cpr_free(g_registered_info[info_index]);
            g_registered_info[info_index] = NULL;
        }
    }

    /* Deregister each Info Package handler */
    for (record = (handler_record_t *)sll_next(s_handler_registry, NULL);
         record != NULL;
         record = (handler_record_t *)sll_next(s_handler_registry, record)) {
        cpr_free(record);
    }

    /* Destroy the SLL */
    sll_destroy(s_handler_registry);
    s_handler_registry = NULL;
}
