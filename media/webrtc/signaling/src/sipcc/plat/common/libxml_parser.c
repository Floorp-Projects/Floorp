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

#include <libxml/parser.h>
#include <libxml/tree.h>
#include "xml_parser.h"
#include "ccsip_subsmanager.h"
#include "ccsip_common_cb.h"

#define MAX_BODY_SIZE 2047
/*KPML REQUEST DEFAULT VALUES*/
#define DEFAULT_KPML_INTERDIGITTIMER_VALUE 4000
#define DEFAULT_KPML_CRITICALDIGITTIMER_VALUE 1000
#define DEFAULT_KPML_EXTRADIGITTIMER_VALUE 500
#define DEFAULT_KPML_LONGREPEAT_VALUE 0
#define DEFAULT_KPML_NOPARTIAL_VALUE 0
#define DEFAULT_KPML_PERSIST_VALUE 0 //one-shot

#define DEFAULT_LINE_NUMBER 1
#define DEFAULT_LINEKEY_EVENT 0

#define XML_SUCCESS 1
#define XML_FAILURE 0

unsigned char offenderText[128] = {0};

int persistType (char *str) {

    if (strcmp(str, "one-shot") == 0 ) {
        return XML_PERSIST_TYPE_ONE_SHOT;
    }
    if (strcmp(str, "persist") == 0 ) {
        return XML_PERSIST_TYPE_PERSIST;
    }
    if (strcmp(str, "single-notify") == 0 ) {
        return XML_PERSIST_TYPE_SINGLE_NOTIFY;
    }
    return XML_PERSIST_TYPE_ONE_SHOT;
}

int stateValues(char *str) {

    if (strcmp(str, "partial") == 0 ) {
        return XML_STATE_PARTIAL;
    }
    if (strcmp(str, "full") == 0 ) {
        return XML_STATE_FULL;
    }
    return XML_STATE_FULL;
}
int eventValues(char *str) {

    if (strcmp(str, "cancelled") == 0 ) {
        return XML_EVENT_CANCELLED;
    }
    if (strcmp(str, "rejected") == 0 ) {
        return XML_EVENT_REJECTED;
    }
    if (strcmp(str, "replaced") == 0 ) {
        return XML_EVENT_REPLACED;
    }
    if (strcmp(str, "local-bye") == 0 ) {
        return XML_EVENT_LOCAL_BYE;
    }
    if (strcmp(str, "remote-bye") == 0 ) {
        return XML_EVENT_REMOTE_BYE;
    }
    if (strcmp(str, "error") == 0 ) {
        return XML_EVENT_ERROR;
    }
    if (strcmp(str, "timeout") == 0 ) {
        return XML_EVENT_TIMEOUT;
    }
    return XML_EVENT_CANCELLED;
}

int callOrientation(char *str) {

    if (strcmp(str, "Unspecified") == 0 ) {
        return XML_CALL_ORIENTATION_UNSPECIFIED;
    }
    if (strcmp(str, "To") == 0 ) {
        return XML_CALL_ORIENTATION_TO;
    }
    if (strcmp(str, "From") == 0 ) {
        return XML_CALL_ORIENTATION_FROM;
    }
    return XML_CALL_ORIENTATION_UNSPECIFIED;
}

int callLock(char *str) {

    if (strcmp(str, "unlocked") == 0 ) {
        return XML_CALLLOCK_UNLOCKED;
    }
    if (strcmp(str, "locked") == 0 ) {
        return XML_CALLLOCK_LOCKED;
    }
    if (strcmp(str, "remote-locked") == 0 ) {
        return XML_CALLLOCK_REMOTE_LOCKED;
    }
    return XML_CALLLOCK_UNLOCKED;
}

int holdReason(char *str) {

    if (strcmp(str, "none") == 0 ) {
        return XML_HOLD_REASON_NONE;
    }
    if (strcmp(str, "transfer") == 0 ) {
        return XML_HOLD_REASON_TRANSFER;
    }
    if (strcmp(str, "conference") == 0 ) {
        return XML_HOLD_REASON_CONFERENCE;
    }
    if (strcmp(str, "internal") == 0 ) {
        return XML_HOLD_REASON_INTERNAL;
    }
    return XML_HOLD_REASON_NONE;
}

int directionValues(char *str) {

    if (strcmp(str, "initiator") == 0 ) {
        return XML_DIRECTION_INITIATOR;
    }
    if (strcmp(str, "recipient") == 0 ) {
        return XML_DIRECTION_RECIPIENT;
    }
    return XML_DIRECTION_INITIATOR;
}

int trueOrfalse(char *str) {

    if (strcmp(str, "false") == 0 ) {
        return XML_FALSE;
    }
    if (strcmp(str, "true") == 0 ) {
        return XML_TRUE;
    }
    return XML_FALSE;
}

int onOroff(char *str) {

    if (strcmp(str, "off") == 0 ) {
        return XML_OFF;
    }
    if (strcmp(str, "on") == 0 ) {
        return XML_ON;
    }
    return XML_OFF;
}

int yesOrno(char *str) {

    if (strcmp(str, "no") == 0 ) {
        return XML_NO;
    }
    if (strcmp(str, "yes") == 0 ) {
        return XML_YES;
    }
    return XML_NONEAPPLICABLE;
}

int softkeyEventCode(char *str) {
    if (strcmp(str, "Undefined") == 0 ) {
        return XML_SKEY_EVENT_UNDEFINED;
    }
    if (strcmp(str, "Redial") == 0 ) {
        return XML_SKEY_EVENT_REDIAL;
    }
    if (strcmp(str, "NewCall") == 0 ) {
        return XML_SKEY_EVENT_NEWCALL;
    }
    if (strcmp(str, "Hold") == 0 ) {
        return XML_SKEY_EVENT_HOLD;
    }
    if (strcmp(str, "Transfer") == 0 ) {
        return XML_SKEY_EVENT_TRANSFER;
    }
    if (strcmp(str, "CFwdAll") == 0 ) {
        return XML_SKEY_EVENT_CFWDALL;
    }
    if (strcmp(str, "CFwdBusy") == 0 ) {
        return XML_SKEY_EVENT_CFWDBUSY;
    }
    if (strcmp(str, "CFwdNoAns") == 0 ) {
        return XML_SKEY_EVENT_CFWDNOANSWER;
    }
    if (strcmp(str, "BackSpace") == 0 ) {
        return XML_SKEY_EVENT_BACKSPACE;
    }
    if (strcmp(str, "EndCall") == 0 ) {
        return XML_SKEY_EVENT_ENDCALL;
    }
    if (strcmp(str, "Resume") == 0 ) {
        return XML_SKEY_EVENT_RESUME;
    }
    if (strcmp(str, "Answer") == 0 ) {
        return XML_SKEY_EVENT_ANSWER;
    }
    if (strcmp(str, "Info") == 0 ) {
        return XML_SKEY_EVENT_INFO;
    }
    if (strcmp(str, "Conference") == 0 ) {
        return XML_SKEY_EVENT_CONFERENCE;
    }
    if (strcmp(str, "Join") == 0 ) {
        return XML_SKEY_EVENT_JION;
    }
    if (strcmp(str, "RmLastConf") == 0 ) {
        return XML_SKEY_EVENT_REMVOVE_LAST_CONF_PARTICIPANT;
    }
    if (strcmp(str, "DirectXfer") == 0 ) {
        return XML_SKEY_EVENT_DIRECT_XFER;
    }
    if (strcmp(str, "Select") == 0 ) {
        return XML_SKEY_EVENT_SELECT;
    }
    if (strcmp(str, "TransferToVM") == 0 ) {
        return XML_SKEY_EVENT_TRANSFER_TO_VOICE_MAIL;
    }
    if (strcmp(str, "SAC") == 0 ) {
        return XML_SKEY_EVENT_SAC;
    }
    if (strcmp(str, "Unselect") == 0 ) {
        return XML_SKEY_EVENT_UNSELECT;
    }
    if (strcmp(str, "Cancel") == 0 ) {
        return XML_SKEY_EVENT_CANCEL;
    }
    if (strcmp(str, "ConfDetails") == 0 ) {
        return XML_SKEY_EVENT_COPNFERENCE_DETAILS;
    }
    if (strcmp(str, "TrnsfMg") == 0 ) {
        return XML_SKEY_EVENT_TRASFMG;
    }
    if (strcmp(str, "Intrcpt") == 0 ) {
        return XML_SKEY_EVENT_INTRCPT;
    }
    if (strcmp(str, "SetWtch") == 0 ) {
        return XML_SKEY_EVENT_SETWTCH;
    }
    if (strcmp(str, "TrnsfVM") == 0 ) {
        return XML_SKEY_EVENT_TRNSFVM;
    }
    if (strcmp(str, "TrnsfAs") == 0 ) {
        return XML_SKEY_EVENT_TRNSFAS;
    }
    return XML_SKEY_EVENT_UNDEFINED;
}

char *getSoftketEventMsg(int event_code) {

    char *event_str = NULL;
    switch(event_code) {
        case XML_SKEY_EVENT_UNDEFINED : return(event_str = "Undefined");
        case XML_SKEY_EVENT_REDIAL : return(event_str = "Redial");
        case XML_SKEY_EVENT_NEWCALL : return(event_str = "NewCall");
        case XML_SKEY_EVENT_HOLD : return(event_str = "Hold");
        case XML_SKEY_EVENT_TRANSFER : return(event_str = "Transfer");
        case XML_SKEY_EVENT_CFWDALL : return(event_str = "CFwdAll");
        case XML_SKEY_EVENT_CFWDBUSY : return(event_str = "CFwdBusy");
        case XML_SKEY_EVENT_CFWDNOANSWER : return(event_str = "CFwdNoAns");
        case XML_SKEY_EVENT_BACKSPACE : return(event_str = "BackSpace");
        case XML_SKEY_EVENT_ENDCALL : return(event_str = "EndCall");
        case XML_SKEY_EVENT_RESUME : return(event_str = "Resume");
        case XML_SKEY_EVENT_ANSWER : return(event_str = "Answer");
        case XML_SKEY_EVENT_INFO : return(event_str = "Info");
        case XML_SKEY_EVENT_CONFERENCE : return(event_str = "Conference");
        case XML_SKEY_EVENT_JION : return(event_str = "Join");
        case XML_SKEY_EVENT_REMVOVE_LAST_CONF_PARTICIPANT : return(event_str = "RmLastConf");
        case XML_SKEY_EVENT_DIRECT_XFER : return(event_str = "DirectXfer");
        case XML_SKEY_EVENT_SELECT : return(event_str = "Select");
        case XML_SKEY_EVENT_TRANSFER_TO_VOICE_MAIL : return(event_str = "TransferToVM");
        case XML_SKEY_EVENT_SAC : return(event_str = "SAC");
        case XML_SKEY_EVENT_UNSELECT : return(event_str = "Unselect");
        case XML_SKEY_EVENT_CANCEL : return(event_str = "Cancel");
        case XML_SKEY_EVENT_COPNFERENCE_DETAILS : return(event_str = "ConfDetails");
        case XML_SKEY_EVENT_TRASFMG : return(event_str = "TrnsfMg");
        case XML_SKEY_EVENT_INTRCPT : return(event_str = "Intrcpt");
        case XML_SKEY_EVENT_SETWTCH : return(event_str = "SetWtch");
        case XML_SKEY_EVENT_TRNSFVM : return(event_str = "TrnsfVM");
        case XML_SKEY_EVENT_TRNSFAS : return(event_str = "TrnsfAs");
        default : return "Undefined";
    }
}

int linekeyEvent(char *str) {
    if(strcmp(str, "line") == 0){
        return XML_LINE_KEY_EVENT_LINE;
    }
    if(strcmp(str, "speeddial") == 0){
        return XML_LINE_KEY_EVENT_SPEEDDIAL;
    }
    return XML_LINE_KEY_EVENT_LINE;
}

int softkeyInvokeType(char *str) {
    if (strcmp(str, "explicit") == 0 ) {
        return XML_SKEY_INVOKE_EXPLICIT;
    }
    if (strcmp(str, "implicit") == 0 ) {
        return XML_SKEY_NVOKE_IMPLICIT;
    }
    return XML_SKEY_NVOKE_IMPLICIT;
}

int stationSequence(char *str) {
    if (strcmp(str, "StationSequenceFirst") == 0 ) {
        return XML_STATION_SEQ_FIRST;
    }
    if (strcmp(str, "StationSequenceMore") == 0 ) {
        return XML_STATION_SEQ_MORE;
    }
    if (strcmp(str, "StationSequenceLast") == 0 ) {
        return XML_STATION_SEQ_LAST;
    }
    return XML_STATION_SEQ_FIRST;
}

void
parse_kpml_request (xmlNode * a_node, xmlDocPtr doc, KPMLRequest *kreq)
{
    xmlChar *data;
    xmlNode *cur_node;
    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
         if (cur_node->type == XML_ELEMENT_NODE) {
            if (!xmlStrcmp(cur_node->name, (const xmlChar *) "kpml-request")) {
                data = xmlGetProp(cur_node, (const xmlChar *) "version");
                if (data) {
                     strncpy(kreq->version, (char *)data, 16);
                     xmlFree(data);
                }
            }
            if (!xmlStrcmp(cur_node->name, (const xmlChar *) "reverse")) {
                data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                if (data) {
                     strncpy(kreq->stream.reverse, (char *)data, 16);
                     xmlFree(data);
                }

            }
            if (!xmlStrcmp(cur_node->name, (const xmlChar *) "pattern")) {
                data = xmlGetProp(cur_node, (const xmlChar *) "criticaldigittimer");
                if(data) {
                    kreq->pattern.criticaldigittimer = atoi((char *)data);
                    xmlFree(data);
                }else {
                    kreq->pattern.criticaldigittimer = DEFAULT_KPML_CRITICALDIGITTIMER_VALUE;
                }
                data = xmlGetProp(cur_node, (const xmlChar *) "extradigittimer");
                if(data) {
                    kreq->pattern.extradigittimer = atoi((char *)data);
                    xmlFree(data);
                }else {
                    kreq->pattern.extradigittimer =  DEFAULT_KPML_EXTRADIGITTIMER_VALUE;
                }
                data = xmlGetProp(cur_node, (const xmlChar *) "interdigittimer");
                if(data) {
                    kreq->pattern.interdigittimer = atoi((char *)data);
                    xmlFree(data);
                }else {
                    kreq->pattern.interdigittimer =  DEFAULT_KPML_INTERDIGITTIMER_VALUE;
                }
                data = xmlGetProp(cur_node, (const xmlChar *) "persist");
                if(data) {
                    kreq->pattern.persist = persistType((char *)data);
                    xmlFree(data);
                }else {
                    kreq->pattern.persist =  XML_PERSIST_TYPE_ONE_SHOT;
                }
                data = xmlGetProp(cur_node, (const xmlChar *) "long");
                if(data) {
                    kreq->pattern.longhold = atoi((char *)data);
                    xmlFree(data);
                }
                data = xmlGetProp(cur_node, (const xmlChar *) "longrepeat");
                if(data) {
                    kreq->pattern.longrepeat = trueOrfalse((char *)data);
                    xmlFree(data);
                }else {
                    kreq->pattern.longrepeat = DEFAULT_KPML_LONGREPEAT_VALUE;
                }
                data = xmlGetProp(cur_node, (const xmlChar *) "nopartial");
                if(data) {
                    kreq->pattern.nopartial = trueOrfalse((char *)data);
                    xmlFree(data);
                }else {
                    kreq->pattern.nopartial = DEFAULT_KPML_NOPARTIAL_VALUE;
                }
                data = xmlGetProp(cur_node, (const xmlChar *) "enterkey");
                if(data) {
                    strncpy(kreq->pattern.enterkey, (char *)data, 8);
                    xmlFree(data);
                }
            }
            if (!xmlStrcmp(cur_node->name, (const xmlChar *) "flush")) {
                data = xmlNodeGetContent(cur_node);
                if (data) {
                    kreq->pattern.flush = atoi((char *)data);
                    xmlFree(data);
                }
            }
            if (!xmlStrcmp(cur_node->name, (const xmlChar *) "regex")) {
                data = xmlGetProp(cur_node, (const xmlChar *) "tag");
                if (data) {
                    strncpy(kreq->pattern.regex.tag, (char *)data, 32);
                    xmlFree(data);
                }
                data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                if (data) {
                    strncpy(kreq->pattern.regex.regexData, (char *)data, 32);
                    xmlFree(data);
                }
            }
            if (!xmlStrcmp(cur_node->name, (const xmlChar *) "pre")) {
                data = xmlNodeGetContent(cur_node);
                if (data) {
                    strncpy(kreq->pattern.regex.pre, (char *)data, 32);
                    xmlFree(data);
                }
            }
        }
        parse_kpml_request(cur_node->xmlChildrenNode, doc, kreq);
    }
}

void
parse_presence_body(xmlNode *node, xmlDocPtr doc, Presence_ext_t *presenceRpid) {
    xmlChar *data;
    xmlNode *cur_node;

    for (cur_node = node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            if (!xmlStrcmp(cur_node->name, (const xmlChar *) "presence")) {
                data = xmlGetProp(cur_node, (const xmlChar *) "entity");
                if (data) {
                    strncpy(presenceRpid->presence_body.entity, (char *)data, 256);
                    xmlFree(data);
                }
            }
            if (!xmlStrcmp(cur_node->name, (const xmlChar *) "note")) {
                if (!xmlStrcmp(cur_node->parent->name, (const xmlChar *) "presence")){
                    data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                    if (data) {
                        strncpy(presenceRpid->presence_body.note[0], (char *)data, 1024);
                        strncpy(presenceRpid->presence_body.note[1], (char *)data+1024, 1024);
                        strncpy(presenceRpid->presence_body.note[2], (char *)data+2048, 1024);
                        strncpy(presenceRpid->presence_body.note[3], (char *)data+3072, 1024);
                        strncpy(presenceRpid->presence_body.note[4], (char *)data+4096, 1024);
                        xmlFree(data);
                    }
                }
                if (!xmlStrcmp(cur_node->parent->name, (const xmlChar *) "tuple")){
                    data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                    if (data) {
                        strncpy(presenceRpid->presence_body.tuple[0].note[0], (char *)data, 1024);
                        xmlFree(data);
                    }
                }
            }
            if(!xmlStrcmp(cur_node->name, (const xmlChar *) "person")){
                data = xmlGetProp(cur_node, (const xmlChar *) "id");
                if(data) {
                    strncpy(presenceRpid->presence_body.person.id, (char *)data, 256);
                    xmlFree(data);
                }
            }
	    if(!xmlStrcmp(cur_node->name, (const xmlChar *) "basic")){
                if (!xmlStrcmp(cur_node->parent->parent->name, (const xmlChar *) "person")){
                    data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                    if(data) {
                        strncpy(presenceRpid->presence_body.person.personStatus.basic, (char *)data, 32);
                        xmlFree(data);
                    }
                }
                if (!xmlStrcmp(cur_node->parent->parent->name, (const xmlChar *) "tuple")){
                    data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                    if(data) {
                        strncpy(presenceRpid->presence_body.tuple[0].status.basic, (char *)data, 32);
                        xmlFree(data);
                    }
                }
            }
            if(!xmlStrcmp(cur_node->name, (const xmlChar *) "alerting")){
                if (!xmlStrcmp(cur_node->parent->parent->name, (const xmlChar *) "person")){
                    data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                    if(data) {
                        strncpy(presenceRpid->presence_body.person.activities.alerting, (char *)data, 12);
                        xmlFree(data);
                    }
                }
                if (!xmlStrcmp(cur_node->parent->parent->parent->name, (const xmlChar *) "tuple")){
                    data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                    if(data) {
                        strncpy(presenceRpid->presence_body.tuple[0].status.activities.alerting, (char *)data, 12);
                        xmlFree(data);
                    }
                }
                presenceRpid->alerting = TRUE;
            }
            if(!xmlStrcmp(cur_node->name, (const xmlChar *) "on-the-phone")){
                if (!xmlStrcmp(cur_node->parent->parent->name, (const xmlChar *) "person")){
                    data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                    if(data) {
                        strncpy(presenceRpid->presence_body.person.activities.onThePhone, (char *)data, 12);
                        xmlFree(data);
                    }
                }
                if (!xmlStrcmp(cur_node->parent->parent->parent->name, (const xmlChar *) "tuple")){
                    data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                    if(data) {
                        strncpy(presenceRpid->presence_body.tuple[0].status.activities.onThePhone, (char *)data, 12);
                        xmlFree(data);
                    }
                }
                presenceRpid->onThePhone = TRUE;
            }
            if(!xmlStrcmp(cur_node->name, (const xmlChar *) "busy")){
                if (!xmlStrcmp(cur_node->parent->parent->name, (const xmlChar *) "person")){
                    data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                    if(data) {
                        strncpy(presenceRpid->presence_body.person.activities.busy, (char *)data, 12);
                        xmlFree(data);
                    }
                }
                if (!xmlStrcmp(cur_node->parent->parent->parent->name, (const xmlChar *) "tuple")){
                    data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                    if(data) {
                        strncpy(presenceRpid->presence_body.tuple[0].status.activities.busy, (char *)data, 12);
                        xmlFree(data);
                    }
                }
                presenceRpid->busy = TRUE;
            }
            if(!xmlStrcmp(cur_node->name, (const xmlChar *) "away")){
                if (!xmlStrcmp(cur_node->parent->parent->name, (const xmlChar *) "person")){
                    data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                    if(data) {
                        strncpy(presenceRpid->presence_body.person.activities.away, (char *)data, 12);
                        xmlFree(data);
                    }
                }
                if (!xmlStrcmp(cur_node->parent->parent->parent->name, (const xmlChar *) "tuple")){
                    data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                    if(data) {
                        strncpy(presenceRpid->presence_body.tuple[0].status.activities.away, (char *)data, 12);
                        xmlFree(data);
                    }
                }
                presenceRpid->away = TRUE;
            }
            if(!xmlStrcmp(cur_node->name, (const xmlChar *) "meeting")){
                if (!xmlStrcmp(cur_node->parent->parent->name, (const xmlChar *) "person")){
                    data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                    if(data) {
                        strncpy(presenceRpid->presence_body.person.activities.meeting, (char *)data, 12);
                        xmlFree(data);
                    }
                }
                if (!xmlStrcmp(cur_node->parent->parent->parent->name, (const xmlChar *) "tuple")){
                    data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                    if(data) {
                        strncpy(presenceRpid->presence_body.tuple[0].status.activities.meeting, (char *)data, 12);
                        xmlFree(data);
                    }
                }
                presenceRpid->meeting = TRUE;
            }
            if(!xmlStrcmp(cur_node->name, (const xmlChar *) "tuple")){
                data = xmlGetProp(cur_node, (const xmlChar *) "id");
                if(data) {
                    strncpy(presenceRpid->presence_body.tuple[0].id, (char *)data, 256);
                    xmlFree(data);
                }
            }
            if(!xmlStrcmp(cur_node->name, (const xmlChar *) "contact")){
                data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                if(data) {
                    strncpy(presenceRpid->presence_body.tuple[0].contact[0], (char *)data, 256);
                    xmlFree(data);
                }
            }
        }
        parse_presence_body(cur_node->xmlChildrenNode, doc, presenceRpid);
    }
}

void
parse_configapp_request(xmlNode *node, xmlDocPtr doc, ConfigApp_req_data_t *configappData) {
    xmlChar *data;
    xmlNode *cur_node;

    for (cur_node = node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            if (!xmlStrcmp(cur_node->name, (const xmlChar *) "kpml")) {
                data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                if (data) {
                    configappData->sip_profile.kpml_val = atoi((char *)data);
                    xmlFree(data);
                }
            }
        }
        parse_configapp_request(cur_node->xmlChildrenNode, doc, configappData);
    }
}

void
parse_media_info(xmlNode *node, xmlDocPtr doc, media_control_ext_t *mediaControlData) {
    xmlChar *data;
    xmlNode *cur_node;

    for (cur_node = node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            if (!xmlStrcmp(cur_node->name, (const xmlChar *) "picture_fast_update")) {
                data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                if (data) {
                    mediaControlData->media_control.vc_primitive.to_encoder.picture_fast_update = atoi((char *)data);
                    xmlFree(data);
                }
                mediaControlData->picture_fast_update = 1;
            }
            if (!xmlStrcmp(cur_node->name, (const xmlChar *) "stream_id")) {
                data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                if (data) {
                    strncpy(mediaControlData->media_control.vc_primitive.stream_id, (char *)data, 128);
                    xmlFree(data);
                }
            }
            if (!xmlStrcmp(cur_node->name, (const xmlChar *) "general_error")) {
                data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                if (data) {
                    strncpy(mediaControlData->media_control.general_error, (char *)data, 128);
                    xmlFree(data);
                }
            }

        }
        parse_media_info(cur_node->xmlChildrenNode, doc, mediaControlData);
    }
}


int ccxmlInitialize(void ** xml_parser_handle) {
    return SIP_OK;
}

void ccxmlDeInitialize(void ** xml_parser_handle) {

}

/*
 * Takes care of all the XML framing requirements
 */
char *
ccxmlEncodeEventData (void * xml_parser_handle, ccsip_event_data_t *eventDatap)
{
    const char     *fname = "ccxmlEncodeEventData";
    char           *buffer = NULL;
    xmlDocPtr doc = NULL;       // document pointer 
    xmlNodePtr root_node = NULL, node1 = NULL, node2 = NULL, node3 = NULL, nodetext = NULL;
    xmlChar *xmlbuff = NULL;
    char buf[256];
    int buffersize = 0;
    
    if (!eventDatap) {
        return NULL;
    }

    /**
     * For event of EVENT_DATA_RAW, it will not come here.
     */

    // This should be converted to using the type that comes in eventData

    switch (eventDatap->type) {
    case EVENT_DATA_KPML_REQUEST:
	
        //Creating tree for kpml request
        doc = xmlNewDoc(BAD_CAST "1.0");
        root_node = xmlNewNode(NULL, BAD_CAST "kpml-request");
        xmlDocSetRootElement(doc, root_node);
        //stream
        node1 = xmlNewNode(NULL, BAD_CAST "stream");
        xmlAddChild(root_node, node1);
        //reverse
        node2 = xmlNewNode(NULL, BAD_CAST "reverse");
        nodetext = xmlNewText(BAD_CAST (eventDatap->u.kpml_request).stream.reverse);
        xmlAddChild(node1, node2);
        xmlAddChild(node2, nodetext);
        //pattern
        node1 = xmlNewNode(NULL, BAD_CAST "pattern");
        xmlAddChild(root_node, node1);
        node2 = xmlNewNode(NULL, BAD_CAST "flush");
        nodetext = xmlNewText(BAD_CAST (eventDatap->u.kpml_request).pattern.flush);
        xmlAddChild(node1, node2);
        xmlAddChild(node2, nodetext);
        //persist
        xmlNewProp(node1, BAD_CAST "persist", BAD_CAST (((eventDatap->u.kpml_request).pattern.persist == 0 ? "one-shot" :
                                       ((eventDatap->u.kpml_request).pattern.persist == 1 ? "persist" : "single-notify"))));
        //interdigittimer
        snprintf(buf, 256, "%u", (unsigned int)(eventDatap->u.kpml_request).pattern.interdigittimer);
        xmlNewProp(node1, BAD_CAST "interdigittimer", BAD_CAST buf);
        //criticaldigittimer
        snprintf(buf, 256, "%u", (unsigned int)(eventDatap->u.kpml_request).pattern.criticaldigittimer);
        xmlNewProp(node1, BAD_CAST "criticaldigittimer", BAD_CAST buf);
        //extradigittimer
        snprintf(buf, 256, "%u", (unsigned int)(eventDatap->u.kpml_request).pattern.extradigittimer);
        xmlNewProp(node1, BAD_CAST "extradigittimer", BAD_CAST buf);
        //long
        snprintf(buf, 256, "%d", (eventDatap->u.kpml_request).pattern.longhold);
        xmlNewProp(node1, BAD_CAST "long", BAD_CAST buf);
        //longrepeat
        xmlNewProp(node1, BAD_CAST "longrepeat", BAD_CAST ((eventDatap->u.kpml_request).pattern.longrepeat ? "true" : "false"));
        //nopartial
        xmlNewProp(node1, BAD_CAST "nopartial", BAD_CAST ((eventDatap->u.kpml_request).pattern.nopartial ? "true" : "false"));
        //enterkey
        xmlNewProp(node1, BAD_CAST "enterkey", BAD_CAST ((eventDatap->u.kpml_request).pattern.enterkey));
        //regex
        node2 = xmlNewNode(NULL, BAD_CAST "regex");
        xmlAddChild(node1, node2);
        node3 = xmlNewNode(NULL, BAD_CAST "regexData");
        nodetext = xmlNewText(BAD_CAST (eventDatap->u.kpml_request).pattern.regex.regexData);
        xmlAddChild(node2, node3);
        xmlAddChild(node3, nodetext);
        node3 = xmlNewNode(NULL, BAD_CAST "tag");
        nodetext = xmlNewText(BAD_CAST (eventDatap->u.kpml_request).pattern.regex.tag);
        xmlAddChild(node2, node3);
        xmlAddChild(node3, nodetext);
        node3 = xmlNewNode(NULL, BAD_CAST "pre");
        nodetext = xmlNewText(BAD_CAST (eventDatap->u.kpml_request).pattern.regex.pre);
        xmlAddChild(node2, node3);
        xmlAddChild(node3, nodetext);
        //version
        node1 = xmlNewNode(NULL, BAD_CAST "version");
        nodetext = xmlNewText(BAD_CAST (eventDatap->u.kpml_request).version);
        xmlAddChild(root_node, node1);
        xmlAddChild(node1, nodetext);
        //store the contents of the DOM tree into the buffer
        xmlDocDumpFormatMemoryEnc(doc, &xmlbuff, &buffersize, "UTF-8", 1);
        xmlbuff[buffersize - 1] = '\0';
        buffer = (char *) ccAllocXML(buffersize);
        snprintf(buffer, buffersize, "%s", xmlbuff);
        xmlFreeDoc(doc);
        xmlFree(xmlbuff);
        xmlCleanupParser();
        return(buffer);
	

    case EVENT_DATA_KPML_RESPONSE:
	
	doc = xmlNewDoc(BAD_CAST "1.0");
        root_node = xmlNewNode(NULL, BAD_CAST "kpml-response");
        xmlDocSetRootElement(doc, root_node);
        xmlSetNs(root_node, xmlNewNs(root_node, BAD_CAST "urn:ietf:params:xml:ns:kpml-response",NULL));

        xmlNewProp(root_node, BAD_CAST "version", BAD_CAST (eventDatap->u.kpml_response).version);
        xmlNewProp(root_node, BAD_CAST "code", BAD_CAST (eventDatap->u.kpml_response).code);
        xmlNewProp(root_node, BAD_CAST "text", BAD_CAST (eventDatap->u.kpml_response).text);
        xmlNewProp(root_node, BAD_CAST "suppressed", BAD_CAST ((eventDatap->u.kpml_response).suppressed ? "true" : "false"));
        xmlNewProp(root_node, BAD_CAST "forced_flush", BAD_CAST (eventDatap->u.kpml_response).forced_flush);
        xmlNewProp(root_node, BAD_CAST "digits", BAD_CAST (eventDatap->u.kpml_response).digits);
        xmlNewProp(root_node, BAD_CAST "tag", BAD_CAST (eventDatap->u.kpml_response).tag);

        xmlDocDumpFormatMemoryEnc(doc, &xmlbuff, &buffersize, "UTF-8", 1);
        xmlbuff[buffersize - 1] = '\0';
        buffer = (char *) ccAllocXML(buffersize);
        snprintf(buffer, buffersize, "%s", xmlbuff);
        xmlFreeDoc(doc);
        xmlFree(xmlbuff);
        xmlCleanupParser();
        return(buffer);
	
    case EVENT_DATA_PRESENCE:
        buffer = (char *) ccAllocXML(MAX_BODY_SIZE);
        if (buffer == NULL) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_MEMORY_OUT_OF_MEM), fname);
            return NULL;
        }
        if (snprintf(buffer, MAX_BODY_SIZE, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n"
                     "<presence xmlns=\"urn:ietf:params:xml:ns:pidf\" "
                     "xmlns:dm=\"urn:ietf:params:xml:ns:pidf:data-model\" "
                     "xmlns:e=\"urn:ietf:params:xml:ns:pidf:status:rpid\" "
                     "xmlns:ce=\"urn:cisco:params:xml:ns:pidf:rpid\" "
                     "xmlns:sc=\"urn:ietf:params:xml:ns:pidf:servcaps\" "
                     "entity=\"sip:%s\">\n"
                     "<tuple id=\"1\"> <status> <basic>open</basic> </status> </tuple>\n"
                     "<dm:person><e:activities><ce:%s/></e:activities> </dm:person>\n</presence>",
                     eventDatap->u.presence_rpid.presence_body.entity,
                     "available")
            >= MAX_BODY_SIZE) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX": failed to frame the xml: buffer len insufficient\n",
                              fname);
            ccFreeXML(buffer);
            buffer = NULL;
        }
        return buffer;

    default:
        CCSIP_DEBUG_TASK(DEB_F_PREFIX" Data type not supported!\n",DEB_F_PREFIX_ARGS(SIP_SUB, fname));
        return NULL;
    }
}

/*
 * Parse the msg body based on event type passed and store the parsed
 * information in the data structure passed by eventDatap. If function
 * returns error, then it is user responsibility to free the memory allocated
 * to eventDatap.
 * @param event_type type of event of information received that need parsing
 * @param msgBody storage containing the information to be parsed
 * @param msgLength  length of information stored in msgBody
 * @param eventDatap  data structure where information is stored after parsing
 */
int
ccxmlDecodeEventData (void * xml_parser_handle, cc_subscriptions_ext_t event_type,
const char *msgBody, int msgLength, ccsip_event_data_t ** event_data_ptr)
{
    const char     *fname = "ccxmlDecodeEventData";
    ccsip_event_data_type_e type = EVENT_DATA_INVALID;
    ccsip_event_data_t *eventDatap = NULL;
    xmlDocPtr doc = NULL;
    xmlNode *root_element = NULL;
    xmlParserCtxtPtr ctxt = NULL;

    *event_data_ptr = NULL;
    eventDatap = (ccsip_event_data_t *) ccAllocXML(sizeof(ccsip_event_data_t));
    if ( !eventDatap ) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX":malloc failed\n", fname);
        return SIP_ERROR;
    }
    memset(eventDatap, 0, sizeof(ccsip_event_data_t));

    switch (event_type) {
        case CC_SUBSCRIPTIONS_KPML:
            
	    type = EVENT_DATA_KPML_REQUEST;
            ctxt = xmlNewParserCtxt();
            eventDatap->type = type;
            doc = xmlCtxtReadMemory(ctxt, msgBody, msgLength, "noname.xml", NULL, 1);
            if (doc == NULL) {
                DEF_DEBUG(DEB_F_PREFIX"Failed to create DOM tree from the sip msgbody\n", DEB_F_PREFIX_ARGS(SIP_TAG, fname));
                xmlFreeParserCtxt(ctxt);
                free_event_data(eventDatap);
                return SIP_ERROR;
            }else{
                DEF_DEBUG(DEB_F_PREFIX"DOM tree created successfully\n", DEB_F_PREFIX_ARGS(SIP_TAG, fname));
            }
            // Get the root element node
            root_element = xmlDocGetRootElement(doc);

            if ((!xmlStrcmp(root_element->name, (const xmlChar *)"kpml-request"))) {
                parse_kpml_request(root_element, doc, &(eventDatap->u.kpml_request));
            }

            xmlFreeParserCtxt(ctxt);
            xmlFreeDoc(doc);
            *event_data_ptr = eventDatap;
            return SIP_OK;
	    
        case CC_SUBSCRIPTIONS_PRESENCE:
	    
	    ctxt = xmlNewParserCtxt();
            type = EVENT_DATA_PRESENCE;
            eventDatap->type = type;
            doc = xmlCtxtReadMemory(ctxt, msgBody, msgLength, "noname.xml", NULL, 1);
            if (doc == NULL) {
                DEF_DEBUG(DEB_F_PREFIX"Failed to create DOM tree from the sip msgbody\n", DEB_F_PREFIX_ARGS(SIP_TAG, fname));
                xmlFreeParserCtxt(ctxt);
                free_event_data(eventDatap);
                return SIP_ERROR;
            }else{
                DEF_DEBUG(DEB_F_PREFIX"DOM tree created successfully\n", DEB_F_PREFIX_ARGS(SIP_TAG, fname));
            }
            // Get the root element node
            root_element = xmlDocGetRootElement(doc);

            if ((!xmlStrcmp(root_element->name, (const xmlChar *)"presence"))) {
                parse_presence_body(root_element, doc, &(eventDatap->u.presence_rpid));
            }
            xmlFreeParserCtxt(ctxt);
            xmlFreeDoc(doc);
            *event_data_ptr = eventDatap;
            return SIP_OK;
	    
        case CC_SUBSCRIPTIONS_CONFIGAPP:
            
	    ctxt = xmlNewParserCtxt();
            type = EVENT_DATA_CONFIGAPP_REQUEST;
            eventDatap->type = type;

            doc = xmlCtxtReadMemory(ctxt, msgBody, msgLength, "noname.xml", NULL, 1);
            if (doc == NULL) {
                DEF_DEBUG(DEB_F_PREFIX"Failed to create DOM tree from the sip msgbody\n", DEB_F_PREFIX_ARGS(SIP_TAG, fname));
                xmlFreeParserCtxt(ctxt);
                free_event_data(eventDatap);
                return SIP_ERROR;
            }else{
                DEF_DEBUG(DEB_F_PREFIX"DOM tree created successfully\n", DEB_F_PREFIX_ARGS(SIP_TAG, fname));
            }
            // Get the root element node
            root_element = xmlDocGetRootElement(doc);

            if ((!xmlStrcmp(root_element->name, (const xmlChar *)"device"))) {
                parse_configapp_request(root_element, doc, &(eventDatap->u.configapp_data));
            }
            xmlFreeParserCtxt(ctxt);
            xmlFreeDoc(doc);
            *event_data_ptr = eventDatap;
            return SIP_OK;

        case CC_SUBSCRIPTIONS_MEDIA_INFO:
            
	    ctxt = xmlNewParserCtxt();
            type = EVENT_DATA_MEDIA_INFO;
            eventDatap->type = type;

            doc = xmlCtxtReadMemory(ctxt, msgBody, msgLength, "noname.xml", NULL, 1);
            if (doc == NULL) {
                DEF_DEBUG(DEB_F_PREFIX"Failed to create DOM tree from the sip msgbody\n", DEB_F_PREFIX_ARGS(SIP_TAG, fname));
                xmlFreeParserCtxt(ctxt);
                free_event_data(eventDatap);
                return SIP_ERROR;
            }else{
                DEF_DEBUG(DEB_F_PREFIX"DOM tree created successfully\n", DEB_F_PREFIX_ARGS(SIP_TAG, fname));
            }
            // Get the root element node
            root_element = xmlDocGetRootElement(doc);

            if ((!xmlStrcmp(root_element->name, (const xmlChar *)"media-control"))) {
                parse_media_info(root_element, doc, &(eventDatap->u.media_control_data));
            }
            xmlFreeParserCtxt(ctxt);
            xmlFreeDoc(doc);
            *event_data_ptr = eventDatap;
            return SIP_OK;

        default:
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"%s: unknown event type\n", fname, fname);
            free_event_data(eventDatap);
            return SIP_ERROR;
    }

}
