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
 
#include "capability_set.h"
#include "CCProvider.h"

// --------------------------------------------------------------------------------
// Data Structures Used to Store the Capability Set Information
//    Settings on kept on a per call state, per feature basis.
//    Includes all general endpoint capabilities, plus fcp-controlled ones
// --------------------------------------------------------------------------------
static cc_boolean  capability_set[MAX_CALL_STATES][CCAPI_CALL_CAP_MAX];
static cc_boolean  capability_idleset[CCAPI_CALL_CAP_MAX];


// --------------------------------------------------------------------------------
// Data Structures Used to Store the Parsed FCP information from UCM/FCP file
//    (after parse will be used to selectively set capabilities in the above also)
// --------------------------------------------------------------------------------

// maximum number of features
#define FCP_FEATURE_MAX      9

char                                    g_fp_version_stamp[MAX_FP_VERSION_STAMP_LEN];
static cc_feature_control_policy_info_t cc_feat_control_policy[FCP_FEATURE_MAX];
static ccapi_call_capability_e          cc_fcp_id_to_capability_map[FCP_FEATURE_MAX+1];    // 0th entry of map is not used/null entry


static const unsigned int               CALL_FORWARD_ALL_FCP_INDEX  = 1;
static const unsigned int               CONFERENCE_LIST_FCP_INDEX   = 4;
static const unsigned int               SPEED_DIAL_FCP_INDEX        = 5;
static const unsigned int               CALL_BACK_FCP_INDEX         = 6;
static const unsigned int               REDIAL_FCP_INDEX            = 7;
  
static int                              fcp_index = -1;

// --------------------------------------------------------------------------------


/*
 * capset_set_fcp_forwardall - sets the fcp-controlled call forward all feature
 *
 */
static void capset_set_fcp_forwardall (cc_boolean state)
{
   CONFIG_DEBUG(DEB_F_PREFIX"FCP Setting CALLFWD Capability to [%d]\n", DEB_F_PREFIX_ARGS(JNI, "capset_set_fcp_forwardall"), (unsigned int)state);

   capability_idleset[CCAPI_CALL_CAP_CALLFWD]       = state;
   capability_set[OFFHOOK][CCAPI_CALL_CAP_CALLFWD]  = state;
}

/*
 * capset_set_fcp_redial - sets the fcp-controlled redial feature
 *
 */
static void capset_set_fcp_redial (cc_boolean state)
{
   CONFIG_DEBUG(DEB_F_PREFIX"FCP Setting REDIAL capability to [%d]\n", DEB_F_PREFIX_ARGS(JNI, "capset_set_fcp_redial"), (unsigned int)state);

   capability_idleset[CCAPI_CALL_CAP_REDIAL]        = state;
   capability_set[OFFHOOK][CCAPI_CALL_CAP_REDIAL]   = state;
   capability_set[ONHOOK][CCAPI_CALL_CAP_REDIAL]    = state;
}

// --------------------------------------------------------------------------------


/*
 * fcp_set_index - sets a given feture index number (from fcp xml file), maps it to the internal
 * call capability, and sets the appropriate state-based capability for the feature index
 *
 */
static void fcp_set_index (unsigned int fcpCapabilityId, cc_boolean state)
{
   ccapi_call_capability_e capabilityId = CCAPI_CALL_CAP_MAX;

   // range check the capability index
   if ((fcpCapabilityId <= 0) || (fcpCapabilityId > FCP_FEATURE_MAX))
   {
        CONFIG_ERROR(CFG_F_PREFIX "Unable to set capability of unknown feature [%d] in FCP \n", "fcp_set_index", fcpCapabilityId);                    
        return;   
   }

   // convert the fcp index to an fcp capability id      
   capabilityId = cc_fcp_id_to_capability_map[fcpCapabilityId];
   
       
   // based on the capability id, invoke the appropate method specific to that capability      
   switch (capabilityId)
   {
       case CCAPI_CALL_CAP_CALLFWD  :  capset_set_fcp_forwardall (state);      break;
       case CCAPI_CALL_CAP_REDIAL   :  capset_set_fcp_redial (state);          break;
       default :
       {
           CONFIG_ERROR(CFG_F_PREFIX "Unable to update settings for capability [%d]\n", "fcp_set_index", (int)capabilityId);                    
           break;
       }
    }
}

/*
 * capset_init - initialize the internal capability data structures to defaults
 *
 */
static void capset_init () 
{
   // initialize the 4 tables related to capability set to false
   memset(capability_idleset, 0, sizeof(capability_idleset));
   memset(capability_set, 0, sizeof(capability_set));

   // ----------------------------------------------------------------------
   // FCP based capabilities
   // ----------------------------------------------------------------------
   
   CONFIG_DEBUG(DEB_F_PREFIX"FCP Initializing Capabilities to default\n", DEB_F_PREFIX_ARGS(JNI, "capset_init"));
      
   // ----------------------------------------------------------------------
   // Non-FCP-based Capabilities
   // ----------------------------------------------------------------------
   
   // Now, set all the non-FCP based capabilities to appropriate default settings
   // (some of which may be enabled by default)
   
   capability_idleset[CCAPI_CALL_CAP_NEWCALL]                    = TRUE;

   // call-state based settings   
   // offhook
   capability_set[OFFHOOK][CCAPI_CALL_CAP_ENDCALL]               = TRUE;

   // onhook
   capability_set[ONHOOK][CCAPI_CALL_CAP_NEWCALL] = TRUE;

   // ringout
   capability_set[RINGOUT][CCAPI_CALL_CAP_ENDCALL] = TRUE;

   // ringing
   capability_set[RINGIN][CCAPI_CALL_CAP_ANSWER] = TRUE;

   // proceed
   capability_set[PROCEED][CCAPI_CALL_CAP_ENDCALL] = TRUE;

   // connected
   capability_set[CONNECTED][CCAPI_CALL_CAP_ENDCALL] = TRUE;
   capability_set[CONNECTED][CCAPI_CALL_CAP_HOLD] = TRUE;
   capability_set[CONNECTED][CCAPI_CALL_CAP_TRANSFER] = TRUE;
   capability_set[CONNECTED][CCAPI_CALL_CAP_CONFERENCE] = TRUE;
   capability_set[CONNECTED][CCAPI_CALL_CAP_SELECT] = TRUE;

   // hold
   capability_set[HOLD][CCAPI_CALL_CAP_RESUME] = TRUE;
   capability_set[REMHOLD][CCAPI_CALL_CAP_RESUME] = TRUE;

   // busy
   capability_set[BUSY][CCAPI_CALL_CAP_ENDCALL] = TRUE;

   // reorder
   capability_set[REORDER][CCAPI_CALL_CAP_ENDCALL] = TRUE;

   // dialing
   capability_set[DIALING][CCAPI_CALL_CAP_ENDCALL] = TRUE;
   capability_set[DIALING][CCAPI_CALL_CAP_DIAL] = TRUE;
   capability_set[DIALING][CCAPI_CALL_CAP_SENDDIGIT] = TRUE;
   capability_set[DIALING][CCAPI_CALL_CAP_BACKSPACE] = TRUE;

   // holdrevert 
   capability_set[HOLDREVERT][CCAPI_CALL_CAP_ANSWER] = TRUE;

   // preservation
   capability_set[PRESERVATION][CCAPI_CALL_CAP_ENDCALL] = TRUE;

   // waiting for digits
   capability_set[WAITINGFORDIGITS][CCAPI_CALL_CAP_SENDDIGIT] = TRUE;
   capability_set[WAITINGFORDIGITS][CCAPI_CALL_CAP_BACKSPACE] = TRUE;
}


/*
 * capset_get_idleset - get the set of features associated with idle state
 *
 */
void capset_get_idleset ( cc_cucm_mode_t mode, cc_boolean features[])
{
  static const char fname[] = "capset_get_idleset";
  int i;

  CCAPP_DEBUG(DEB_F_PREFIX"updating idleset",
            DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

     for (i=0;i < CCAPI_CALL_CAP_MAX; i++) {
  CCAPP_DEBUG(DEB_F_PREFIX"updating line features %d=%d",
            DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), i, capability_idleset[i]);
       features[i] = capability_idleset[i];
     }

}

/*
 * capset_get_allowed_features - get the set of features
 *
 */
void capset_get_allowed_features ( cc_cucm_mode_t mode, cc_call_state_t state, cc_boolean features[]) 
{
  static const char fname[] = "capset_get_allowed_features";
  int i;

  CCAPP_DEBUG(DEB_F_PREFIX"updating idleset",
            DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  for (i=0;i < CCAPI_CALL_CAP_MAX; i++) {
      features[i] = capability_set[state][i];
  }

}

// --------------------------------------------------------------------------------------------------------
// ---------------------------- XML Parse Logic for Feature Control Policy --------------------------------
// --------------------------------------------------------------------------------------------------------

/*
 * fcp_set_capabilities - updates the capabilities structure, based on the now parsed information 
 * from fcp xml file
 *
 */

static void fcp_set_capabilities()
{
   int my_fcp_index = 0;

    if ( (fcp_index+1) >= FCP_FEATURE_MAX) {
        fcp_index = (FCP_FEATURE_MAX -1);
        CONFIG_ERROR(CFG_F_PREFIX "Received more than the maximum supported features [%d] in FCP \n", "fcp_set_capabilities", FCP_FEATURE_MAX);                    
        
    }
   // loop over all the FCP features parsed, and for each one, based on ID, and enabled settings, 
   // update the corresponding call capability flags
   for (my_fcp_index = 0; my_fcp_index <= fcp_index; my_fcp_index++)
   {   // set the capability if fcp file has it marked as 'enabled'
       fcp_set_index(cc_feat_control_policy[my_fcp_index].featureId, (cc_feat_control_policy[my_fcp_index].featureEnabled == TRUE));       
   }
}   

/*
 * fcp_init - initialize the data structure used to store the fcp parse info
 *
 */
static void fcp_init()
{
   // master index; set to null
   fcp_index = -1;
      
   // initialize the map of fcp xml feature indexes to internal call capabilities
   cc_fcp_id_to_capability_map[CALL_FORWARD_ALL_FCP_INDEX]   = CCAPI_CALL_CAP_CALLFWD;
   cc_fcp_id_to_capability_map[REDIAL_FCP_INDEX]             = CCAPI_CALL_CAP_REDIAL;

   // initialize the capability set data structures
   capset_init();
   
   // initialize the version
   strcpy (g_fp_version_stamp, "");
}


/*
 * config_parse_fcp_feature - parse info associated with a given feature
 *
 */
static void config_parse_fcp_feature (char* featureName, xmlNode* fcp_node, xmlDocPtr doc)
{
    xmlChar*     xmlValue;
    xmlNode*     cur_node;
    
    // obtain a new feature index
    if ((fcp_index+1) >= FCP_FEATURE_MAX) 
    {
        CONFIG_ERROR(CFG_F_PREFIX "Received more than the maximum supported features [%d] in FCP \n", "config_parse_fcp_feature", FCP_FEATURE_MAX);                    
        return;
    }
    fcp_index++;
    
    // initialize the element
    strcpy (cc_feat_control_policy[fcp_index].featureName, "");
    cc_feat_control_policy[fcp_index].featureEnabled = FALSE;
    cc_feat_control_policy[fcp_index].featureId       = 0;
    
    // set the name of the feature
    strncpy (cc_feat_control_policy[fcp_index].featureName, featureName, FCP_FEATURE_NAME_MAX);

    // parse the id and enabled state of the feature, and save in structure
    for (cur_node = fcp_node; cur_node; cur_node = cur_node->next) 
    {
        if (cur_node->type == XML_ELEMENT_NODE) 
        {
            xmlValue = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
            if (xmlValue != NULL)
            {
            
                if (!xmlStrcmp(cur_node->name, (const xmlChar *) "id")) 
                {        
                    unsigned int featureId = atoi((char*)xmlValue);
                    if (featureId > FCP_FEATURE_MAX)
                    {
                        CONFIG_ERROR(CFG_F_PREFIX "Feature Id [%d] in FCP file Out Of Range [%d] \n", "config_parse_fcp_feature", featureId, FCP_FEATURE_MAX);                    
                        featureId = 0;
                    }
            
                    cc_feat_control_policy[fcp_index].featureId = featureId;
                                                            
                } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "enable")) 
                {
                    cc_feat_control_policy[fcp_index].featureEnabled = (cc_boolean)(strcmp((char*)xmlValue, "true") == 0);                
                }
                
                xmlFree(xmlValue); 
            }        
        }
    }
}

/*
 * config_parse_fcp_element
 *
 * Parse the elements in the XML document and call the 
 * corressponding set functions
 */
static void config_parse_fcp_element (xmlNode* fcp_node, char* value, xmlDocPtr doc )
{    
   int versionStampSize = 0;
    
    if (!xmlStrcmp(fcp_node->name, (const xmlChar *) "versionStamp")) 
    {
        versionStampSize = strlen(value);
    
        // ensure version stamp size is not unexpectedly large...
        if (versionStampSize > MAX_FP_VERSION_STAMP_LEN) {
            CONFIG_ERROR(CFG_F_PREFIX "FCP Version Length [%d] is crazy large!\n", "config_parse_fcp_element", versionStampSize);
            return;
        }

        // trusting nothing, ensure we don't copy more bytes than we can handle... 
        sstrncpy(g_fp_version_stamp, value, MAX_FP_VERSION_STAMP_LEN);        

    } else if (!xmlStrcmp(fcp_node->name, (const xmlChar *) "featureDef")) 
    {
        xmlChar* featureName = NULL;
            
        featureName = xmlGetProp(fcp_node, (const xmlChar *) "name");
        if (featureName == NULL)
        {
            CONFIG_ERROR(CFG_F_PREFIX "FCP Couldnt get feature name element!\n", "config_parse_fcp_element");
            return;
        }
        
        // pass the name below to store
        CONFIG_DEBUG(CFG_F_PREFIX "FCP Parsing FeatureName=[%s] \n", "config_parse_fcp_element", (char*)featureName);
        config_parse_fcp_feature ((char*)featureName, fcp_node->children, doc);        
        
        xmlFree(featureName); 
    }     
}


/**
 * fcp_parse_and_set:
 * @a_node: the initial xml node to consider.
 * @doc: The DOM tree of the xml file
 *
 * Get the names of the all the xml elements
 * that are siblings or children of a given xml node.
 *
 * Recursive call to traverse all the elements of the tree.
 */
static int fcp_parse_and_set (xmlNode* a_node, xmlDocPtr doc)
{    
    xmlNode*  cur_node = NULL;
    xmlChar*  data;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next) 
    {
        if (cur_node->type == XML_ELEMENT_NODE) 
        {
            data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
            if (data != NULL) 
            {
                config_parse_fcp_element(cur_node, (char *)data, doc);
                xmlFree(data);
            }
        }
        
        fcp_parse_and_set(cur_node->children, doc);
    }

    return (0);
}

/*
 * capset_set_fcp_forwardall - sets the fcp-controlled call forward all feature
 *
 */
int fcp_init_template (const char* fcp_plan_string)
{
    static const char fname[] = "fcp_init_template";
 
    xmlNode*  fcp_xml_root_element;
    xmlDocPtr fcp_xml_doc;
        
    fcp_init();    
        
    if (fcp_plan_string == NULL)
    {   // set up the default fcp
       return (0);  
    }

    fcp_xml_doc = xmlReadFile(fcp_plan_string, NULL, 0);
    if (fcp_xml_doc == NULL)
    {    
        CONFIG_ERROR(CFG_F_PREFIX "FCP - Unable to xmlReadFile = [%s]", fname, fcp_plan_string);
        return (-1);    
    }

    // get the root element
    fcp_xml_root_element = xmlDocGetRootElement(fcp_xml_doc);
    if (fcp_xml_root_element == NULL)
    {    
        CONFIG_ERROR(CFG_F_PREFIX "FCP - Unable to get root element for FCP file = [%s]", fname, fcp_plan_string);
        xmlFreeDoc(fcp_xml_doc);

        return (-1);   
    }

    fcp_parse_and_set (fcp_xml_root_element, fcp_xml_doc);

    // free the document */
    xmlFreeDoc(fcp_xml_doc);

    // Now set the capability bools, based on the parsed FCP values
    CONFIG_DEBUG(DEB_F_PREFIX"FCP Post FCP Parse Setting Capabilities\n", DEB_F_PREFIX_ARGS(JNI, "fcp_display"));

    // update the fcp capabilities structure, based on the parsed feature information    
    fcp_set_capabilities();

    return (0);
}
// ---------------------------- End Of XML Parse Logic for Feature Control Policy --------------------------
