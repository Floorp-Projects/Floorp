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

#include <stdio.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "sll_lite.h"
#include "cc_constants.h"
#include "cc_types.h"
#include "cc_config.h"
#include "phone_debug.h"
#include "debug.h"
#include <string.h> /* for strncpy */
#include "CCProvider.h"
#include "ccapi_call_info.h"
#include "conf_roster.h"
#include "ccapi.h"
#include "ccapp_task.h"

cc_conf_participant_status_t
convertStringToParticipantStatus(const char *data)
{
    if (strcmp(data, "connected") == 0) {
        return CCAPI_CONFPARTICIPANT_CONNECTED;
    } else if (strcmp(data, "alerting") == 0) {
        return CCAPI_CONFPARTICIPANT_ALERTING;
    } else if (strcmp(data, "dialing-out") == 0) {
        return CCAPI_CONFPARTICIPANT_DIALING_OUT;
    } else if (strcmp(data, "on-hold") == 0) {
        return CCAPI_CONFPARTICIPANT_ON_HOLD;
    } else if (strcmp(data, "disconnected") == 0) {
        return CCAPI_CONFPARTICIPANT_DISCONNECTED;
    } else {
        return CCAPI_CONFPARTICIPANT_UNKNOWN;
    }
}

cc_call_security_t
convertStringToParticipantSecurity(const char *data)
{

    if (strcmp(data, "NotAuthenticated") == 0) {
        return CC_SECURITY_NOT_AUTHENTICATED;
    } else if (strcmp(data, "Authenticated") == 0) {
        return CC_SECURITY_AUTHENTICATED;
    } else if (strcmp(data, "Encrypted") == 0) {
        return CC_SECURITY_ENCRYPTED;
    } else if (strcmp(data, "Unknown") == 0) {
        return CC_SECURITY_UNKNOWN;
    } else {
        return CC_SECURITY_NONE;
    }
}
/*
 * parse_call_info_node
 *
 * Parse call-info node.
 *
 * Assumes a_node, participant are not null.
 */
static void 
parse_call_info_node (xmlNode * a_node, cc_call_conferenceParticipant_Info_t *participant) {
    xmlChar *data;
    xmlNode *cur_node;
    xmlNode *walker_node;

    for (cur_node = a_node->children; cur_node != NULL; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            if (xmlStrcmp(cur_node->name, (const xmlChar *) "security") == 0) {
                data = xmlNodeGetContent(cur_node); 
                if (data != NULL) {
                    participant->participantSecurity = convertStringToParticipantSecurity((const char*)data);
                    xmlFree(data);
                }
            } else if (xmlStrcmp(cur_node->name, (const xmlChar *) "sip") == 0) {
                for (walker_node = cur_node->children; walker_node != NULL; walker_node = walker_node->next) {
                    if (cur_node->type == XML_ELEMENT_NODE) {
                        if (xmlStrcmp(walker_node->name, (const xmlChar *) "call-id") == 0) {
                            data = xmlNodeGetContent(walker_node); 
                            if (data != NULL) {
                                participant->callid = strlib_update(participant->callid, (const char*)data);
                                xmlFree(data);
                            }
                        }
                    }
                }
            }
        }
    }
}

/*
 * parse_self_node
 *
 * Parse self node.
 *
 * Assumes a_node, participant are not null.
*/

static void 
parse_self_node (xmlNode * a_node, cc_call_conferenceParticipant_Info_t *participant) 
{
    xmlChar* data;
    xmlNode* cur_node;
    xmlNode* walker_node;

    for (cur_node = a_node->children; cur_node != NULL; cur_node = cur_node->next) 
    {
          if (xmlStrcmp(cur_node->name, (const xmlChar *) "user-capabilities") == 0) 
          {
                for (walker_node = cur_node->children; walker_node != NULL; walker_node = walker_node->next) 
                {
                   if (cur_node->type == XML_ELEMENT_NODE) 
                   {
                       if (xmlStrcmp(walker_node->name, (const xmlChar *) "remove-participant") == 0) 
                       {
                          data = xmlNodeGetContent(walker_node); 
                          if (data != NULL) 
                          {   // set field to indicate this participant can remove other participants                          
                              participant->canRemoveOtherParticipants = TRUE;
                              xmlFree(data);
                          }
                       } 
                   }
              }
          }
     }
}

/*
 * parse_user_endpoint_node
 *
 * Do endpoint specific parsing.
 *
 * Assumes a_node, participant is not null.
 */
static void 
parse_user_endpoint_node (xmlNode * a_node, cc_call_conferenceParticipant_Info_t *participant, cc_call_conference_Info_t *info) {
    xmlChar *data;
    xmlNode *cur_node;

    for (cur_node = a_node->children; cur_node != NULL; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            if (xmlStrcmp(cur_node->name, (const xmlChar *) "status") == 0) {
                data = xmlNodeGetContent(cur_node); 
                if (data != NULL) {
                    participant->participantStatus = convertStringToParticipantStatus((const char*)data);
                    xmlFree(data);
                } else {
                    //continue parsing other elements
                    CCAPP_ERROR(DEB_F_PREFIX" Error while getting the data for node: status\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONFPARSE"));
                }
            } else if (xmlStrcmp(cur_node->name, (const xmlChar *) "call-info") == 0) {
                parse_call_info_node(cur_node, participant);
            } else if (xmlStrcmp(cur_node->name, (const xmlChar *) "self") == 0) {
                parse_self_node(cur_node, participant);
                info->myParticipantId = strlib_update(info->myParticipantId, participant->callid);
            }
        }
    }
}

/*
 * parse_user_node
 *
 * Do user node specific parsing.
 *
 * Assumes a_node, info are not null.
 */
static void 
parse_user_node (xmlNode * a_node, cc_call_conference_Info_t *info) 
{
    xmlChar *data;
    xmlNode *cur_node;
    cc_call_conferenceParticipant_Info_t *participant;
    sll_lite_return_e sll_ret_val;

    //allocate a new participant
    participant = cpr_malloc(sizeof(cc_call_conferenceParticipant_Info_t)); 
    if (participant == NULL) {
        CCAPP_ERROR(DEB_F_PREFIX" Malloc failure for participant\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONFPARSE"));
        return;
    } else {
        participant->participantName            = strlib_empty();
        participant->endpointUri                = strlib_empty();
        participant->callid                     = strlib_empty();
        participant->participantNumber          = strlib_empty();
        participant->participantSecurity        = CC_SECURITY_NONE; 
        participant->participantStatus          = CCAPI_CONFPARTICIPANT_UNKNOWN; 
        participant->canRemoveOtherParticipants = FALSE;
    }

    sll_ret_val = sll_lite_link_tail(&info->currentParticipantsList, (sll_lite_node_t *)participant);
    if (sll_ret_val != SLL_LITE_RET_SUCCESS) {
        CCAPP_ERROR(DEB_F_PREFIX" Error while trying to insert in the linked list\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONFPARSE"));
        cpr_free(participant);
        return;
    }

    data = xmlGetProp(a_node, (const xmlChar *) "entity");
    if (data != NULL) {
    	char *tmp2;
    	char *tmp1;
        participant->endpointUri = strlib_update(participant->endpointUri, (const char*)data);

        // Parse the endpoint URI, to get the Participant number

        tmp1 = (char *) strstr((const char*)data, "sip:");
        if (tmp1) {
            tmp1 += 4;
            tmp2 = (char *) strchr(tmp1, '@');
            if (tmp2) {
                *tmp2 = 0;
                participant->participantNumber = strlib_update(participant->participantNumber, (const char*)tmp1);
            }
        }
        xmlFree(data);
    } else {
        //continue parsing other elements
        CCAPP_ERROR(DEB_F_PREFIX" Error while trying to find the endpoint URI\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONFPARSE"));
    }

    for (cur_node = a_node->children; cur_node != NULL; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            if (xmlStrcmp(cur_node->name, (const xmlChar *) "endpoint") == 0) {
                parse_user_endpoint_node(cur_node, participant, info);
            } else if (xmlStrcmp(cur_node->name, (const xmlChar *) "display-text") == 0) { 
                data = xmlNodeGetContent(cur_node); 
                if (data != NULL) {
                    participant->participantName = strlib_update(participant->participantName, (const char*)data);
                    xmlFree(data);
                } else {
                    //No display text - continue parsing other elements -
                    CCAPP_ERROR(DEB_F_PREFIX" Error while trying to get the display text\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONFPARSE"));
                }
            }
        }
    }
}

/*
 * parse_conference_state_node
 *
 * Do conference-state specific parsing.
 *
 * Assumes a_node, info are not null.
 */
static void 
parse_conference_state_node (xmlNode * a_node, cc_call_conference_Info_t *info) 
{
    xmlNode *cur_node;
    xmlChar *data;

    for (cur_node = a_node->children; cur_node != NULL; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            if (xmlStrcmp(cur_node->name, (const xmlChar *) "user-count") == 0) {
                data = xmlNodeGetContent(cur_node); 
                if (data != NULL) {
                    info->participantCount = atoi((const char*)data); 
                    xmlFree(data);
                } else {
                    //No user-count - continue parsing other elements -
                    CCAPP_ERROR(DEB_F_PREFIX" Error while trying to get the user-count\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONFPARSE"));
                }
            }
        }
    }
}

/*
 * parse_conference_description_node
 *
 * Do conference-description specific parsing.
 *
 * Assumes a_node, info are not null.
 */ 
static void 
parse_conference_description_node (xmlNode * a_node, cc_call_conference_Info_t *info) 
{
    xmlNode *cur_node;
    xmlChar *data;

    for (cur_node = a_node->children; cur_node != NULL; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            if (xmlStrcmp(cur_node->name, (const xmlChar *) "maximum-user-count") == 0) {
                data = xmlNodeGetContent(cur_node); 
                if (data != NULL) {
                    info->participantMax = atoi((const char*)data); 
                    xmlFree(data);
                } else {
                    //No max-user-count - continue parsing other elements -
                    CCAPP_ERROR(DEB_F_PREFIX" Error while trying to get the max-user-count\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONFPARSE"));
                } 
            }
        }
    }
}

/*
 * parse_conference_info_node
 *
 * Do conference-info specific parsing.
 *
 * Assumes a_node, doc, info are not null. 
 */
static void 
parse_conference_info_node (xmlNode * a_node, xmlDocPtr doc, cc_call_conference_Info_t *info)
{
    xmlNode *cur_node;
    xmlNode *nodeLevel1;
    xmlNode *nodeLevel2;
    
    for (cur_node = a_node; cur_node != NULL; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {

            for (nodeLevel1 = cur_node->children; nodeLevel1 != NULL; nodeLevel1 = nodeLevel1->next) {
                if (nodeLevel1->type == XML_ELEMENT_NODE) {
            
                    if (xmlStrcmp(nodeLevel1->name, (const xmlChar *) "conference-description") == 0) {
                        parse_conference_description_node(nodeLevel1, info);
                    }
                    if (xmlStrcmp(nodeLevel1->name, (const xmlChar *) "conference-state") == 0) {
                        parse_conference_state_node(nodeLevel1, info);
                    }

                    if (xmlStrcmp(nodeLevel1->name, (const xmlChar *) "users") == 0) {
                        for (nodeLevel2 = nodeLevel1->children; nodeLevel2 != NULL; nodeLevel2 = nodeLevel2->next) {
                            if (nodeLevel2->type == XML_ELEMENT_NODE) {
                                if (xmlStrcmp(nodeLevel2->name, (const xmlChar *) "user") == 0) {
                                    parse_user_node(nodeLevel2, info);
                                }
                            }
                        }
                    } 
                }
            }
        }
    }
}

/**
 * 
 * @content: the content of the document
 * @length: the length in bytes
 *
 * Parse the in memory document and free the resulting tree
 */
void
decodeInfoXML(const char *content, int length, cc_call_conference_Info_t *info) {
    xmlDocPtr doc; /* the resulting document tree */
    xmlNode *root_element = NULL;

    doc = xmlReadMemory(content, length, "noname.xml", NULL, 0);
    if (doc == NULL) {
        CCAPP_ERROR(DEB_F_PREFIX" Failed to parse document\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONFPARSE"));
        return;
    }

    /* Get the root element node */
    root_element = xmlDocGetRootElement(doc);
    if (root_element == NULL) {
        CCAPP_ERROR(DEB_F_PREFIX" Failed to get the root element\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONFPARSE"));
        return;
    }

    if (xmlStrcmp(root_element->name, (const xmlChar *)"conference-info") == 0) {
        parse_conference_info_node(root_element, doc, info);
    }                

    xmlFreeDoc(doc);
}


void conf_roster_init_call_conference (cc_call_conference_Info_t *info)
{
    CCAPP_DEBUG(DEB_F_PREFIX"in init_call_conference \n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONFPARSE"));
    
    info->participantMax = 0;
    info->participantCount = 0;
    info->myParticipantId = strlib_empty();
    
    sll_lite_init(&info->currentParticipantsList); 
}

void conf_roster_free_call_conference (cc_call_conference_Info_t *confInfo)
{
    cc_call_conferenceParticipant_Info_t *participant;

    CCAPP_DEBUG(DEB_F_PREFIX"in free_call_confrerence \n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONFPARSE"));

    while((participant=(cc_call_conferenceParticipant_Info_t *)
                sll_lite_unlink_head(&confInfo->currentParticipantsList)) != NULL) 
    {
        strlib_free(participant->participantName);
        strlib_free(participant->endpointUri);
        strlib_free(participant->callid);
        strlib_free(participant->participantNumber);
        
        participant->participantSecurity        = CC_SECURITY_NONE; 
        participant->participantStatus          = CCAPI_CONFPARTICIPANT_UNKNOWN;
        participant->canRemoveOtherParticipants = FALSE;
        
        cpr_free(participant);
        participant = NULL;
    }

    strlib_free(confInfo->myParticipantId);
    conf_roster_init_call_conference(confInfo);
}

void conf_roster_copy_call_conferance (cc_call_conference_Info_t *dest, cc_call_conference_Info_t * src)
{
    cc_call_conferenceParticipant_Info_t *destParticipant;
    cc_call_conferenceParticipant_Info_t *srcParticipant;
    sll_lite_node_t *iterator;
    sll_lite_return_e sll_ret_val;

    CCAPP_DEBUG(DEB_F_PREFIX"in copy_call_confrerence \n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONFPARSE"));

    iterator = src->currentParticipantsList.head_p;
    conf_roster_init_call_conference(dest);

    dest->participantMax = src->participantMax;
    dest->participantCount = src->participantCount;
    dest->myParticipantId = strlib_copy(src->myParticipantId);

    while (iterator) {
        srcParticipant = (cc_call_conferenceParticipant_Info_t *)iterator;

        destParticipant = cpr_malloc(sizeof(cc_call_conferenceParticipant_Info_t)); 
        if (destParticipant == NULL) {
            CCAPP_ERROR(DEB_F_PREFIX" Malloc failure for participant\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONFPARSE"));
            return;
        } else {
            destParticipant->participantName = strlib_copy(srcParticipant->participantName);
            destParticipant->endpointUri = strlib_copy(srcParticipant->endpointUri);
            destParticipant->callid = strlib_copy(srcParticipant->callid);
            
            destParticipant->participantNumber          = strlib_copy(srcParticipant->participantNumber);
            destParticipant->participantSecurity        = srcParticipant->participantSecurity; 
            destParticipant->participantStatus          = srcParticipant->participantStatus; 
            destParticipant->canRemoveOtherParticipants = srcParticipant->canRemoveOtherParticipants;
        }

        sll_ret_val = sll_lite_link_tail(&dest->currentParticipantsList, (sll_lite_node_t *)destParticipant);
        if (sll_ret_val != SLL_LITE_RET_SUCCESS) {
            CCAPP_ERROR(DEB_F_PREFIX" Error while trying to insert in the linked list\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONFPARSE"));
            cpr_free(destParticipant);
            return;
        }

        iterator = iterator->next_p;
    } 
}

// -------------------
// API Implementation
// -------------------

/**
* Get Conference Participants
* @param [in] handle - call handle
* @param [in/out] participantHandles - array of participant handles to be returned
* @param [in/out] count - in:  size of array provided in participantHandles; out:  number of entries populated (up to original value provided)
* @return void
*/
void CCAPI_CallInfo_getConfParticipants (cc_callinfo_ref_t handle, cc_participant_ref_t participantHandles[], int* count)
{  
   cc_call_conference_ref_t              callConference   = NULL;    // conference reference (from call info)
   cc_call_conference_participant_ref_t  participant      = NULL;    // participant reference
   cc_uint16_t                           participantIndex = 0;       // participant index
   cc_uint16_t                           nodeCount        = 0;       // linked list node count

   CCAPP_DEBUG(DEB_F_PREFIX"Entering:  CCAPI_CallInfo_getConfParticipants\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"));

   // get conference reference from the call info
   callConference = getCallConferenceRef(handle);
   if (callConference == NULL)
   {
      CCAPP_ERROR(DEB_F_PREFIX"Unable to get conference handle\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"));
      *count = 0;
      return;
   }

   nodeCount = SLL_LITE_NODE_COUNT(&(callConference->currentParticipantsList));
   CCAPP_DEBUG(DEB_F_PREFIX"SLL NODE COUNT = [%d]\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"), nodeCount);
   if (nodeCount <= 0)
   {      
      *count = 0;
      return;      
   }

   participant = (cc_call_conference_participant_ref_t)SLL_LITE_LINK_HEAD(&callConference->currentParticipantsList);
   while (participant != NULL)
   {
      if (participantIndex >= *count)
      {
          CCAPP_ERROR(DEB_F_PREFIX"Not Enough Room Provided To List All Participants.  Listed [%d] of [%d]\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"), count, nodeCount);
          return;            
      }
      
      // add this participant to our list of particpiants
      participantHandles[participantIndex] = (participant->callid);
                
      // step to the next stored participant in the list
      participant = (cc_call_conference_participant_ref_t)SLL_LITE_LINK_NEXT_NODE(participant);
      participantIndex++;
   }

   // sanity check
   if (participantIndex != nodeCount)
   {  // did not find the expected number of participants!
       CCAPP_ERROR(DEB_F_PREFIX"Detected mismatch between counted participants [%d] and SLL returned nodecount [%d]\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"),
                participantIndex, nodeCount);
       *count = 0;
       return;
   }
   
   // return number of participants
   *count = nodeCount;
   return;
}

/**
* Get Maximum Number of Conference Participants ( in case gui wants to show %full conference info )
* @param [in] handle - call handle
* @return maximum number of conference participants
*/
cc_uint16_t CCAPI_CallInfo_getConfParticipantMax (cc_callinfo_ref_t handle)
{  // 
   cc_call_conference_ref_t callConference;    // conference reference (from call info)

   CCAPP_DEBUG(DEB_F_PREFIX"Entering:  CCAPI_CallInfo_getConfParticipantMax\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"));

   // get conference reference from the call info
   callConference = getCallConferenceRef(handle);
   if (callConference == NULL)
   {
      // no conference reference available
      CCAPP_ERROR(DEB_F_PREFIX"Unable to get conference reference\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"));
      return (0);
   }

   // return the max
   return (callConference->participantMax);
}

/**
* Get Participant Name
* @param [in] handle - call info handle
* @param [in] participantHandle - specific handle for conference participant
* @return display name of the conference participant
*/
cc_string_t CCAPI_CallInfo_getConfParticipantName (cc_callinfo_ref_t handle, cc_participant_ref_t participantHandle)
{ 
    cc_call_conference_participant_ref_t participant = getConferenceParticipantRef (handle, participantHandle);
    if (participant == NULL)
    {
        return strlib_empty();    
    }

    return (participant->participantName);
}

/**
* Get Participant Number
* @param [in] handle - handle of call
* @param [in] participantHandle - handle of conference participant
* @return display number of the conference participant
*/
cc_string_t CCAPI_CallInfo_getConfParticipantNumber (cc_callinfo_ref_t handle, cc_participant_ref_t participantHandle)
{  
    cc_call_conference_participant_ref_t participant = getConferenceParticipantRef (handle, participantHandle);
    if (participant == NULL)
    {
        return strlib_empty();
    }

    return (participant->participantNumber);
}

/**
* Get Conference Participant Status
* @param [in] handle - call handle
* @param [in] participantHandle - handle of conference participant
* @return conference participant status 
*/
cc_conf_participant_status_t CCAPI_CallInfo_getConfParticipantStatus (cc_callinfo_ref_t handle, cc_participant_ref_t participantHandle)
{
    cc_call_conference_participant_ref_t participant = getConferenceParticipantRef (handle, participantHandle);
    if (participant == NULL)
    {
        return (CCAPI_CONFPARTICIPANT_UNKNOWN);
    }

    return (participant->participantStatus);
}

/**
* Get Participant Security
* @param [in] handle - call handle
* @param [in] participantHandle - handle of conference participant
* @return security setting of the specific conference participant
*/
cc_call_security_t CCAPI_CallInfo_getConfParticipantSecurity (cc_callinfo_ref_t handle, cc_participant_ref_t participantHandle)
{  
    cc_call_conference_participant_ref_t participant = getConferenceParticipantRef (handle, participantHandle);
    if (participant == NULL)
    {
        return (CC_SECURITY_NONE);
    }

    return (participant->participantSecurity);  
}

/**
*/
cc_boolean CCAPI_CallInfo_isConfSelfParticipant (cc_callinfo_ref_t handle, cc_participant_ref_t participantHandle)
{ 
   cc_call_conference_ref_t callConference;    // conference reference (from call info)

   // get conference reference from the call info
   callConference = getCallConferenceRef(handle);
   if (callConference == NULL)
   {
      // error - log 
      CCAPP_ERROR(DEB_F_PREFIX"Unable to get conference reference\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"));
      return (FALSE);
   }
   
   return (strcmp((callConference->myParticipantId), participantHandle) == 0);
}

/**
*/
cc_participant_ref_t CCAPI_CallInfo_getConfSelfParticipant (cc_callinfo_ref_t handle)
{  
   cc_call_conference_ref_t callConference;    // conference reference (from call info)

   // get conference reference from the call info
   callConference = getCallConferenceRef(handle);
   if (callConference == NULL)
   {
      // unexpected error
      CCAPP_ERROR(DEB_F_PREFIX"Unable to get conference reference\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"));
      return strlib_empty();
   }
   
   return (callConference->myParticipantId);
}

// -----
/**
 * Get the call conference reference
 * @param [in] handle - call info handle
 * @return cc_call_conference_Info_t
 */
cc_call_conference_ref_t  getCallConferenceRef(cc_callinfo_ref_t handle)
{
  session_data_t *data = (session_data_t *)handle;

  if (!CCAPI_CallInfo_getIsConference(handle))
  {
      CCAPP_ERROR(DEB_F_PREFIX"Conference API Invoked, but Not In Conference Call\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"));
      return (NULL);
  };

  if (data == NULL)
  {
      return (NULL);
  }
  
  return (&data->call_conference);
}

// ------------------------------------------------------------------------------------------------------------------
// getConferenceParticipantRef:  returns participant ref (pointer) to a specific participant handle
// ------------------------------------------------------------------------------------------------------------------
cc_call_conference_participant_ref_t getConferenceParticipantRef(cc_callinfo_ref_t handle, cc_participant_ref_t participantHandle)
{
   cc_call_conference_ref_t              callConference;    // conference reference (from call info)
   cc_call_conference_participant_ref_t  participant;  

   // get conference reference from the call info
   callConference = getCallConferenceRef(handle);
   if (callConference == NULL)
   {
      // no conference reference available
      CCAPP_ERROR(DEB_F_PREFIX"Unable to get conference reference\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"));
      return (NULL);
   }

   // see if participantHandle is legit...
   if (participantHandle == NULL)
   {
       CCAPP_DEBUG(DEB_F_PREFIX"Received query for null participant\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"));
       return (NULL);
   }

   if (SLL_LITE_NODE_COUNT(&(callConference->currentParticipantsList)) <= 0)
   {
      CCAPP_ERROR(DEB_F_PREFIX"Participant list node count is 0, returning NULL\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"));
      return (NULL);
   }

   participant = (cc_call_conference_participant_ref_t)SLL_LITE_LINK_HEAD(&callConference->currentParticipantsList);
   while (participant != NULL)
   {   
      // see if we've found the participant we're looking for
      if (strcmp(participant->callid, participantHandle) == 0)
      {
         return (participant);
      }

      // no match so far, so look at the next item in the list...      
      participant = (cc_call_conference_participant_ref_t)SLL_LITE_LINK_NEXT_NODE(participant);
   }

   CCAPP_ERROR(DEB_F_PREFIX"    Did Not Find participant!\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"));
   return (NULL);
}
