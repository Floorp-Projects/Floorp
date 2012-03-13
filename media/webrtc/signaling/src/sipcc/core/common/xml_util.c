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

#include "xml_parser.h"
#include "cpr_stdlib.h"
#include "phone_debug.h"
#include "xml_util.h"

static void *xml_parser_handle = NULL;

int xmlInit() {
    if (ccxmlInitialize(&xml_parser_handle) == CC_FAILURE) {
         return CC_FAILURE;
    }
    return CC_SUCCESS;
}

void xmlDeInit() {
    ccxmlDeInitialize(&xml_parser_handle);
}

char * xmlEncodeEventData(ccsip_event_data_t *event_datap) {
    const char     *fname = "xmlEncodeEventData";
    char           *buffer;
    uint32_t        nbytes = 0;

    CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Encode event data: entered,\n",
                                    DEB_F_PREFIX_ARGS(SIP_SUB, fname));
    //For RAW data, no encoding is needed, just copy back.
    if (event_datap->type == EVENT_DATA_RAW) {
        nbytes = event_datap->u.raw_data.length;
        buffer = (char *) ccAllocXML(nbytes + 1);
        if (buffer) {
            memcpy(buffer, event_datap->u.raw_data.data, nbytes);
            CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Framed raw buffer: length = %d,\n",
                                DEB_F_PREFIX_ARGS(SIP_SUB, fname), nbytes);
        }
        return (buffer);

    }
    buffer = ccxmlEncodeEventData(xml_parser_handle, event_datap);
    DEF_DEBUG(DEB_F_PREFIX"returned content after encoding:\n%s\n", DEB_F_PREFIX_ARGS(SIP_REG, fname), buffer);
    return (buffer);

    //return ccxmlEncodeEventData(xml_parser_handle, event_datap);
}

int xmlDecodeEventData (cc_subscriptions_ext_t msg_type, const char *msg_body, int msg_length, ccsip_event_data_t ** event_datap) {
    const char     *fname = "xmlDecodeEventData";
    CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Decode event data: entered,\n",
                                    DEB_F_PREFIX_ARGS(SIP_SUB, fname));

    return ccxmlDecodeEventData(xml_parser_handle, msg_type, msg_body, msg_length, event_datap );
}

void *ccAllocXML(cc_size_t size) {
    return cpr_calloc(1, size);
}

void ccFreeXML(void *mem) {
    cpr_free(mem);
}

