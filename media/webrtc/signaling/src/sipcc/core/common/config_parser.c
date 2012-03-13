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
#include "cc_constants.h"
#include "cc_types.h"
#include "cc_config.h"
#include "phone_debug.h"
#include "debug.h"
#include "ccapi.h"
#include "prot_configmgr.h"
#include <string.h> /* for strncpy */
#include "call_logger.h"
#include "sip_common_transport.h"
#include "sip_ccm_transport.h"
#include "config_parser.h"
#include "cc_device_feature.h"
#include "ccapi_snapshot.h"
#include "config_api.h"
#include "dialplan.h"
#include "capability_set.h"
#include "util_string.h"

#define MAC_ADDR_SIZE   6
#define FILE_PATH 256
#define MAX_MULTI_LEVEL_CONFIG 2

#define MLCFG_VIDEO_CAPABILITY    0
#define MLCFG_CISCO_CAMERA        1
#define MLCFG_CAPABILITY_MAX      2
#define MLCFG_NOT_SET -1

#define VERSION_LENGTH_MAX 100

#define ID_BLOCK_PREF1   1
#define ID_BLOCK_PREF3   3
/*
 * File location is hardcoded for getting mac and IP addr
 */ 
#define IP_ADDR_FILE  "/sdcard/myip.txt"
static char autoreg_name[MAX_LINE_NAME_SIZE];

static char dialTemplateFile[FILE_PATH] = "";
static char fcpTemplateFile[FILE_PATH] = "";
char g_cfg_version_stamp[MAX_CFG_VERSION_STAMP_LEN + 1] = {0};
char g_applyConfigFcpVersion[VERSION_LENGTH_MAX] = "";
char g_applyConfigDialPlanVersion[VERSION_LENGTH_MAX] = "";

int line = -1;  //initialize line to -1, as 0 is valid line
boolean apply_config = FALSE;
cc_apply_config_result_t apply_config_result = APPLY_CONFIG_NONE;
extern var_t prot_cfg_table[];
void print_config_value (int id, char *get_set, const char *entry_name, void *buffer, int length);
void config_set_sipline_properties (xmlNode *sipline_node, xmlDocPtr doc, boolean button_configured[] );
void config_minimum_check(xmlNode * a_node, xmlDocPtr doc);

static int sip_port[MAX_CCM];
static int secured_sip_port[MAX_CCM];
static int security_mode = 1; /*NONSECURE*/
extern accessory_cfg_info_t g_accessoryCfgInfo;

// Configurable settings
static int gTransportLayerProtocol = 4;   //  4 = tcp, 2 = udp
static boolean gP2PSIP = FALSE;
static int gVoipControlPort = 5060;
static int gCcm1_sip_port = 5060;
static boolean gROAPPROXY = FALSE;
static boolean gROAPCLIENT = FALSE;

typedef struct _multiLevel {
    int cfgId;           /* config id */
    xmlChar *name;          /* tag name */
    char *vendorVal;     /* vendor config value or NULL */
    char *commonVal;     /* common config value or NULL */
    char *enterpriseVal; /* enterprise config value or NULL */
} multiLevelConfig;

static multiLevelConfig mlConfigs[MAX_MULTI_LEVEL_CONFIG];

void initMultiLevelConfigs() {
    int i;

    for (i = 0; i < MAX_MULTI_LEVEL_CONFIG; ++i) {
        mlConfigs[i].name = NULL;
        mlConfigs[i].vendorVal = NULL;
        mlConfigs[i].commonVal = NULL;
        mlConfigs[i].enterpriseVal = NULL;
    }
    i = 0;
    mlConfigs[i++].cfgId = MLCFG_VIDEO_CAPABILITY;
    mlConfigs[i++].cfgId = MLCFG_CISCO_CAMERA;
    if (i != MAX_MULTI_LEVEL_CONFIG) {
        CONFIG_ERROR(CFG_F_PREFIX " !!!init error i=%d !!!\n", 
                     "initMultiLevelConfigs", i);
    }
}

boolean updateMultiLevelConfig(int cfgId, const xmlChar *name, char *vendor, char *common, char *enterprise) {
    int i;    
    boolean found = FALSE;

    for (i = 0; (found == FALSE) && (i < MAX_MULTI_LEVEL_CONFIG); ++i) {
        if (cfgId == mlConfigs[i].cfgId) {
            mlConfigs[i].name = (xmlChar *) name;
            if (vendor != NULL) {
                mlConfigs[i].vendorVal = vendor;
            }
            if (common != NULL) {
                mlConfigs[i].commonVal = common;
            }
            if (enterprise != NULL) {
                mlConfigs[i].enterpriseVal = enterprise;
            }
            found = TRUE;
        }
    }
    if (found == FALSE) {
        CONFIG_ERROR(CFG_F_PREFIX " !!! cfgId %d not found !!!\n", 
                     "updateMultiLevelConfig", cfgId);
    }
    return found;
}

boolean updateVendorConfig(int cfgId, const xmlChar *name, char *value) {
    return updateMultiLevelConfig(cfgId, name, value, NULL, NULL);
}

boolean updateCommonConfig(int cfgId, const xmlChar *name, char *value) {
    return updateMultiLevelConfig(cfgId, name, NULL, value, NULL);
}

boolean updateEnterpriseConfig(int cfgId, const xmlChar *name, char *value) {
    return updateMultiLevelConfig(cfgId, name, NULL, NULL, value);
}

/*
 * This function determine whether the passed config parameter should be used
 * in comparing the new and old config value for apply-config purpose.  Only
 * those config ids on whose change phone needs to restart are part of this
 * function. For remaining parameters, it is assumed that any change can be
 * applied dynamically.
 *
 */
boolean is_cfgid_in_restart_list(int cfgid) {

    if ((cfgid >= CFGID_LINE_FEATURE && cfgid < (CFGID_LINE_FEATURE + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_INDEX && cfgid < (CFGID_LINE_INDEX + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_PROXY_ADDRESS && cfgid < (CFGID_PROXY_ADDRESS + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_PROXY_PORT && cfgid < (CFGID_PROXY_PORT + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_NAME && cfgid < (CFGID_LINE_NAME + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_DISPLAYNAME && cfgid < (CFGID_LINE_DISPLAYNAME + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_SPEEDDIAL_NUMBER && cfgid < (CFGID_LINE_SPEEDDIAL_NUMBER + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_MESSAGES_NUMBER && cfgid < (CFGID_LINE_MESSAGES_NUMBER + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_FWD_CALLER_NAME_DIPLAY && cfgid < (CFGID_LINE_FWD_CALLER_NAME_DIPLAY + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_FWD_CALLER_NUMBER_DIPLAY && cfgid < (CFGID_LINE_FWD_CALLER_NUMBER_DIPLAY + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_FWD_REDIRECTED_NUMBER_DIPLAY && cfgid < (CFGID_LINE_FWD_REDIRECTED_NUMBER_DIPLAY + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_FWD_DIALED_NUMBER_DIPLAY && cfgid < (CFGID_LINE_FWD_DIALED_NUMBER_DIPLAY + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_CALL_WAITING && cfgid < (CFGID_LINE_CALL_WAITING + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_AUTHNAME && cfgid < (CFGID_LINE_AUTHNAME + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_PASSWORD && cfgid < (CFGID_LINE_PASSWORD + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_FEATURE_OPTION_MASK && cfgid < (CFGID_LINE_FEATURE_OPTION_MASK + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_MSG_WAITING_LAMP && cfgid < (CFGID_LINE_MSG_WAITING_LAMP + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_MESSAGE_WAITING_AMWI && cfgid < (CFGID_LINE_MESSAGE_WAITING_AMWI + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_RING_SETTING_IDLE && cfgid < (CFGID_LINE_RING_SETTING_IDLE + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_RING_SETTING_ACTIVE && cfgid < (CFGID_LINE_RING_SETTING_ACTIVE + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_CONTACT && cfgid < (CFGID_LINE_CONTACT + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_MAXNUMCALLS && cfgid < (CFGID_LINE_MAXNUMCALLS + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_BUSY_TRIGGER && cfgid < (CFGID_LINE_BUSY_TRIGGER + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_AUTOANSWER_ENABLED && cfgid < (CFGID_LINE_AUTOANSWER_ENABLED + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_AUTOANSWER_MODE && cfgid < (CFGID_LINE_AUTOANSWER_MODE + MAX_CONFIG_LINES))
       )
    {
        return TRUE;
    }
    switch (cfgid) {
        case CFGID_CCM1_ADDRESS:
        case CFGID_CCM2_ADDRESS:
        case CFGID_CCM3_ADDRESS:
        case CFGID_CCM1_SIP_PORT:
        case CFGID_CCM2_SIP_PORT:
        case CFGID_CCM3_SIP_PORT:

        case CFGID_PROXY_BACKUP:
        case CFGID_PROXY_BACKUP_PORT:
        case CFGID_PROXY_EMERGENCY:
        case CFGID_PROXY_EMERGENCY_PORT:
        case CFGID_OUTBOUND_PROXY:
        case CFGID_OUTBOUND_PROXY_PORT:

        case CFGID_PROXY_REGISTER:
        case CFGID_REMOTE_CC_ENABLED:

        case CFGID_SIP_INVITE_RETX:
        case CFGID_SIP_RETX:
        case CFGID_TIMER_INVITE_EXPIRES:
        case CFGID_TIMER_KEEPALIVE_EXPIRES:
        case CFGID_TIMER_SUBSCRIBE_EXPIRES:
        case CFGID_TIMER_SUBSCRIBE_DELTA:
        case CFGID_TIMER_T1:
        case CFGID_TIMER_T2:

        case CFGID_SIP_MAX_FORWARDS:
        case CFGID_REMOTE_PARTY_ID:
        case CFGID_REG_USER_INFO:

        case CFGID_PREFERRED_CODEC:
        case CFGID_VOIP_CONTROL_PORT:
        case CFGID_NAT_ENABLE:
        case CFGID_NAT_ADDRESS:
        case CFGID_NAT_RECEIVED_PROCESSING:

        case CFGID_DTMF_AVT_PAYLOAD:
        case CFGID_DTMF_DB_LEVEL:
        case CFGID_DTMF_OUTOFBAND:

        case CFGID_KPML_ENABLED:
        case CFGID_MEDIA_PORT_RANGE_START:
        case CFGID_TRANSPORT_LAYER_PROT:

        case CFGID_TIMER_REGISTER_EXPIRES:
        case CFGID_TIMER_REGISTER_DELTA:
        case CFGID_DSCP_FOR_CALL_CONTROL:
            return TRUE;
        default:
            return FALSE;
    }
}


/*
 * This function either compare the new and old config value or set the value
 * for the config_id passed depending upon whether apply-config is true or not.
 */
void compare_or_set_byte_value(int cfgid, unsigned char value, const unsigned char * config_name) {
    int temp_value ;
    const var_t *entry;
    if (apply_config == TRUE) { 
        if (is_cfgid_in_restart_list(cfgid) == TRUE) {
            config_get_value(cfgid, &temp_value, sizeof(temp_value));
            if (((int)value) !=  temp_value) {
                apply_config_result = RESTART_NEEDED;
                entry = &prot_cfg_table[cfgid];
                print_config_value(cfgid, "changed Get Val", entry->name, &temp_value, sizeof(temp_value));
                DEF_DEBUG(CFG_F_PREFIX "config %s[%d] changed. Old value=%d new value=%d\n", "compare_or_set_byte_value", config_name, cfgid, temp_value, value);
            }
        }
    } else {
        CC_Config_setByteValue(cfgid, value);
    }
}

/*
 * This function either compare the new and old config value or set the value
 * for the config_id passed depending upon whether apply-config is true or not.
 */
void compare_or_set_boolean_value(int cfgid, cc_boolean value, const unsigned char * config_name) {
    int temp_value ;
    const var_t *entry;
    if (apply_config == TRUE) { 
        if (is_cfgid_in_restart_list(cfgid) == TRUE) {
            config_get_value(cfgid, &temp_value, sizeof(temp_value));
            if (((int)value) !=  temp_value) {
                apply_config_result = RESTART_NEEDED;
                entry = &prot_cfg_table[cfgid];
                print_config_value(cfgid, "changed Get Val", entry->name, &temp_value, sizeof(temp_value));
                DEF_DEBUG(CFG_F_PREFIX "config %s[%d] changed. Old value=%d new value=%d\n", "compare_or_set_boolean_value", config_name, cfgid, temp_value, value);
            }
        }
    } else {
        CC_Config_setBooleanValue(cfgid, value);
    }
}

/*
 * This function either compare the new and old config value or set the value
 * for the config_id passed depending upon whether apply-config is true or not.
 */
void compare_or_set_int_value(int cfgid, int value, const unsigned char * config_name) {
    int temp_value;
    const var_t *entry;
    if (apply_config == TRUE) { 
        if (is_cfgid_in_restart_list(cfgid) == TRUE) {
            config_get_value(cfgid, &temp_value, sizeof(temp_value));
            if (value !=  temp_value) {
                apply_config_result = RESTART_NEEDED;
                entry = &prot_cfg_table[cfgid];
                print_config_value(cfgid, "changed Get Val", entry->name, &temp_value, sizeof(temp_value));

                DEF_DEBUG(CFG_F_PREFIX "config %s[%d] changed. new value=%d Old value=%d\n", "compare_or_set_int_value", config_name, cfgid, value, temp_value);
            }
        }
    } else {
        CC_Config_setIntValue(cfgid, value);
    }
}

/*
 * This function either compare the new and old config value or set the value
 * for the config_id passed depending upon whether apply-config is true or not.
 */
void compare_or_set_string_value (int cfgid, const char* value, const unsigned char * config_name) {
    static char temp_value[MAX_SIP_URL_LENGTH];
    const var_t *entry;
    if (apply_config == TRUE ) { 
        if (is_cfgid_in_restart_list(cfgid) == TRUE) {
            config_get_string(cfgid, temp_value, MAX_SIP_URL_LENGTH);
            if (strcmp(value, temp_value) != 0) {
                apply_config_result = RESTART_NEEDED;
                entry = &prot_cfg_table[cfgid];
                print_config_value(cfgid, "changed Get Val", entry->name, &temp_value, sizeof(temp_value));
                DEF_DEBUG(CFG_F_PREFIX "config %s[%d] changed. new value=%s Old value=%s\n", "compare_or_set_string_value", config_name, cfgid, value, temp_value);
            }
        }
    } else {
        CC_Config_setStringValue(cfgid, value);
    }
}

int lineConfig = 0;
int portConfig = 0;
int proxyConfig = 0;

void update_sipline_properties(xmlNode *sipline_node, xmlDocPtr doc) {
    int button_num, line_feature;
    boolean is_button_configured_in_new_config[MAX_CONFIG_LINES];

    memset(is_button_configured_in_new_config, 0, sizeof(is_button_configured_in_new_config));
    config_set_sipline_properties(sipline_node, doc, is_button_configured_in_new_config);
    /**
     * If we have reached here, then it means that the lines, set by config
     * file, are exactly same as that of in existing phone configuration. 
     * But we still need to find if any line has been removed by the new
     * config file. To do that, we go through each line in existing phone
     * config and see if any of these line is configured but the 
     * corresponding flag in isLineSetFromConfigFile is false. Then this 
     * line must have been removed in new config.
     **/ 
        for (button_num = 0; button_num < MAX_CONFIG_LINES; button_num++) {
            if (is_button_configured_in_new_config[button_num] == 1) {
                /* 
                 * since this button is configured by new config file, we have
                 * already checked it for any change. Move on to next button.
                 */
                continue; 
            }
            // get the curret or existing feature id for this line.
            config_get_line_value(CFGID_LINE_FEATURE, &line_feature, sizeof(line_feature), button_num + 1);
            if (line_feature != cfgLineFeatureNone) {
                // this line has been removed in new config
    		if (apply_config == TRUE) {
                	apply_config_result = RESTART_NEEDED; 
			DEF_DEBUG(CFG_F_PREFIX "line-feauture removed for line %d.  Old value=%d new value=%d\n", 
				"config_set_sipline_properties", button_num+1, line_feature, cfgLineFeatureNone);

            	} else {
                	compare_or_set_int_value(CFGID_LINE_FEATURE + button_num , cfgLineFeatureNone, 
				(const unsigned char * )"line");
                }
            }
    	}
}
/*
 * config_set_sipline_properties
 *
 */
void config_set_sipline_properties (xmlNode *sipline_node, xmlDocPtr doc, boolean button_configured[] )
{
    static char fname[] = "config_set_sipline_properties";
    xmlChar * parser_ret_value;
    char * value;
    xmlNode *cur_node;
    xmlChar *data;
    int lineIndex ;


    for (cur_node = sipline_node; cur_node; cur_node = cur_node->next) {

        if (cur_node->type == XML_ELEMENT_NODE) {

            parser_ret_value = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
            value =  parser_ret_value?(char *)parser_ret_value:"";
            if (!xmlStrcmp(cur_node->name, (const xmlChar *) "line")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n",
                             fname, cur_node->name, value);
                data = xmlGetProp(cur_node, (const xmlChar *) "button");
                line = atoi((char *)data);
                line = line - 1;

                /* this button is configured in new config */
                button_configured[line] = 1;
        
                CONFIG_DEBUG(CFG_F_PREFIX "line  %d\n", fname, line);
                xmlFree(data);

                data = xmlGetProp(cur_node, (const xmlChar *) "lineIndex");
                if(data != NULL){
                	lineIndex = atoi((char *)data);
					CONFIG_DEBUG(CFG_F_PREFIX "lineIndex  %d\n", fname, lineIndex);

					xmlFree(data);
					compare_or_set_int_value(CFGID_LINE_INDEX + line, lineIndex, (const unsigned char *)"lineIndex");
                }

            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "featureID")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n",
                            "config_set_sipline_properties", cur_node->name, value);
                compare_or_set_int_value(CFGID_LINE_FEATURE + line, atoi(value), cur_node->name);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "proxy")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n",
                             "config_set_sipline_properties", cur_node->name, value);
                compare_or_set_string_value(CFGID_PROXY_ADDRESS + line, value, cur_node->name);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "port")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", 
                             "config_set_sipline_properties", cur_node->name, value);
                compare_or_set_int_value(CFGID_PROXY_PORT + line, atoi(value), cur_node->name);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "featureLabel")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);
                if ( apply_config == FALSE )  {
			// don't set if just comparing props
                	ccsnap_set_line_label(line+1, value);
                }
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "name")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);
                compare_or_set_string_value(CFGID_LINE_NAME + line, value, cur_node->name);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "displayName")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);
                compare_or_set_string_value(CFGID_LINE_DISPLAYNAME + line, value, cur_node->name);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "speedDialNumber")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);
                compare_or_set_string_value(CFGID_LINE_SPEEDDIAL_NUMBER + line, value, cur_node->name);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "retrievalPrefix")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);
                compare_or_set_string_value(CFGID_LINE_RETRIEVAL_PREFIX + line, value, cur_node->name);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "messagesNumber")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);
                compare_or_set_string_value(CFGID_LINE_MESSAGES_NUMBER + line, value, cur_node->name);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "callerName")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);
                compare_or_set_boolean_value(CFGID_LINE_FWD_CALLER_NAME_DIPLAY + line,
                                          (!strcmp(value,"true")) ? TRUE : FALSE, cur_node->name);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "callerNumber")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);
                compare_or_set_boolean_value(CFGID_LINE_FWD_CALLER_NUMBER_DIPLAY + line,
                                          (!strcmp(value,"true")) ? TRUE : FALSE, cur_node->name);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "redirectedNumber")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);
                compare_or_set_boolean_value(CFGID_LINE_FWD_REDIRECTED_NUMBER_DIPLAY + line,
                                          (!strcmp(value,"true")) ? TRUE : FALSE, cur_node->name);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "dialedNumber")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);
                compare_or_set_boolean_value(CFGID_LINE_FWD_DIALED_NUMBER_DIPLAY + line,
                                          (!strcmp(value,"true")) ? TRUE : FALSE, cur_node->name);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "featureOptionMask")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);
                compare_or_set_int_value(CFGID_LINE_FEATURE_OPTION_MASK + line, atoi(value), cur_node->name);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "authName")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);
                compare_or_set_string_value(CFGID_LINE_AUTHNAME + line, value, cur_node->name);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "authPassword")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);
                compare_or_set_string_value(CFGID_LINE_PASSWORD + line, value, cur_node->name);
            } else if (!xmlStrcmp(cur_node->name,
                                 (const xmlChar *) "messageWaitingLampPolicy")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);
                compare_or_set_byte_value(CFGID_LINE_MSG_WAITING_LAMP + line, atoi(value), cur_node->name);

            } else if (!xmlStrcmp(cur_node->name,
                                 (const xmlChar *) "messageWaitingAMWI")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);
                compare_or_set_byte_value(CFGID_LINE_MESSAGE_WAITING_AMWI + line, atoi(value), cur_node->name);

            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "ringSettingIdle")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);
                compare_or_set_byte_value(CFGID_LINE_RING_SETTING_IDLE + line, atoi(value), cur_node->name);
            } else if (!xmlStrcmp(cur_node->name,
                                 (const xmlChar *) "ringSettingActive")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);
                compare_or_set_byte_value(CFGID_LINE_RING_SETTING_ACTIVE + line, atoi(value), cur_node->name);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "contact")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);
                compare_or_set_string_value(CFGID_LINE_CONTACT + line, value, cur_node->name);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "maxNumCalls")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);
                compare_or_set_int_value(CFGID_LINE_MAXNUMCALLS + line, atoi(value), cur_node->name);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "busyTrigger")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);
                compare_or_set_int_value(CFGID_LINE_BUSY_TRIGGER + line, atoi(value), cur_node->name);

            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "autoAnswer")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);

            } else if (!xmlStrcmp(cur_node->name,
                                 (const xmlChar *) "autoAnswerEnabled")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);
                compare_or_set_byte_value(CFGID_LINE_AUTOANSWER_ENABLED + line, atoi(value) & 0x1, cur_node->name);
                
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "autoAnswerMode")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);
                compare_or_set_string_value(CFGID_LINE_AUTOANSWER_MODE + line, value, cur_node->name);
                   
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "callWaiting")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);
                compare_or_set_byte_value(CFGID_LINE_CALL_WAITING + line, atoi(value) & 0x1, cur_node->name);

            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "callFwdAll")) {
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_set_sipline_properties",
                             cur_node->name, value);
                compare_or_set_string_value(CFGID_LINE_CFWDALL + line, value, cur_node->name);
            }

        }
        config_set_sipline_properties(cur_node->children, doc, button_configured);
    }
}


/*
 * config_set_autoreg_properties
 *
 */
void config_set_autoreg_properties ()
{
    CC_Config_setIntValue(CFGID_LINE_INDEX + 0, 1);
	CC_Config_setIntValue(CFGID_LINE_FEATURE + 0, 9);
    CC_Config_setStringValue(CFGID_PROXY_ADDRESS + 0, "USECALLMANAGER");
    CC_Config_setIntValue(CFGID_PROXY_PORT + 0, 5060);
    CC_Config_setStringValue(CFGID_LINE_NAME + 0, autoreg_name);
    CC_Config_setBooleanValue(CFGID_PROXY_REGISTER, 1);
    CC_Config_setIntValue(CFGID_TRANSPORT_LAYER_PROT, 2);

    /* timerRegisterExpires = 3600 */
    CC_Config_setIntValue(CFGID_TIMER_REGISTER_EXPIRES, 3600);
    /* sipRetx = 10 */
   CC_Config_setIntValue(CFGID_SIP_RETX, 10);
    /* sipInviteRetx = 6 */
    CC_Config_setIntValue(CFGID_SIP_INVITE_RETX, 6);
    /* timerRegisterDelta = 5 */
    CC_Config_setIntValue(CFGID_TIMER_REGISTER_DELTA, 5);
    /* MaxRedirects = 70 */
    CC_Config_setIntValue(CFGID_SIP_MAX_FORWARDS, 70);
    /* timerInviteExpires = 180 */
    CC_Config_setIntValue(CFGID_TIMER_INVITE_EXPIRES, 180);
	/* timerSubscribeDelta = 5 */
    CC_Config_setIntValue(CFGID_TIMER_SUBSCRIBE_DELTA, 5);
	/* timerSubscribeExpires = 120 */
    CC_Config_setIntValue(CFGID_TIMER_SUBSCRIBE_EXPIRES, 120);

    CC_Config_setIntValue(CFGID_REMOTE_CC_ENABLED, 1);
    CC_Config_setIntValue(CFGID_VOIP_CONTROL_PORT, 5060);
}

/*
 * config_get_default_gw
 *
 * Get the default gw
 */
void config_get_default_gw(char *buf)
{
    platGetDefaultGW(buf);
}

/*
 * update_security_mode_and_ports
 *
 */
void update_security_mode_and_ports(void) {
        sec_level_t sec_level = NON_SECURE;

    // convert security mode (from UCM xml) into internal enum        
    switch (security_mode)
    {
       case 1:  sec_level = NON_SECURE; break;
       case 2:  sec_level = AUTHENTICATED; break;
       case 3:  sec_level = ENCRYPTED; break;
       default:
          CONFIG_ERROR(CFG_F_PREFIX "unable to translate securite mode [%d]\n", "update_security_mode_and_ports", (int)security_mode);
          break;
        }
                                
	compare_or_set_int_value(CFGID_CCM1_SEC_LEVEL, sec_level,
							 (const unsigned char *)"deviceSecurityMode");
	compare_or_set_int_value(CFGID_CCM2_SEC_LEVEL, sec_level,
							 (const unsigned char *)"deviceSecurityMode");
	compare_or_set_int_value(CFGID_CCM3_SEC_LEVEL, sec_level,
							 (const unsigned char *)"deviceSecurityMode");
                                                                                
	if (sec_level == NON_SECURE) {
		compare_or_set_int_value(CFGID_CCM1_SIP_PORT, sip_port[0],
								 (const unsigned char *)"ccm1_sip_port");
		compare_or_set_int_value(CFGID_CCM2_SIP_PORT, sip_port[1],
								 (const unsigned char *)"ccm2_sip_port");
		compare_or_set_int_value(CFGID_CCM3_SIP_PORT, sip_port[2],
								 (const unsigned char *)"ccm3_sip_port");
	} else {
		compare_or_set_int_value(CFGID_CCM1_SIP_PORT, secured_sip_port[0],
								 (const unsigned char *)"ccm1_secured_sip_port");
		compare_or_set_int_value(CFGID_CCM2_SIP_PORT, secured_sip_port[1],
								 (const unsigned char *)"ccm2_secured_sip_port");
		compare_or_set_int_value(CFGID_CCM3_SIP_PORT, secured_sip_port[2],
								 (const unsigned char *)"ccm3_secured_sip_port");
	}
}

/*
 * config_process_multi_level_config
 */
void config_process_multi_level_config(xmlNode *ml_node, xmlDocPtr doc, const xmlChar *fname, boolean (*updater)(int, const xmlChar *, char *))
{
    char * value;
    xmlNode *cur_node;
    int cfgId;

    for (cur_node = ml_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            value = (char *)xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
            CONFIG_ERROR(CFG_F_PREFIX "%s tag %s, XML node type %d, value %s", "config_process_multi_level_config",
                                      fname, cur_node->name, cur_node->type, value);
            if (value != NULL) {

                if (!xmlStrcmp(cur_node->name, (const xmlChar *) "videoCapability")) {
                    cfgId = MLCFG_VIDEO_CAPABILITY;
                } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "ciscoCamera")) {
                    cfgId = MLCFG_CISCO_CAMERA;
                } else {
                    cfgId = MLCFG_NOT_SET;
                }

                if (cfgId != MLCFG_NOT_SET) {
                    CONFIG_ERROR(CFG_F_PREFIX "%s setting %s for %s config id %d", "config_process_multi_level_config",
                                              fname, value, cur_node->name, cfgId);
                    (*updater)(cfgId, cur_node->name, value);
                }
            }
        }
    }  
}

void compare_or_set_multi_level_config_value() {
    int i;
    int intVal;
    char * value;

    CONFIG_ERROR(CFG_F_PREFIX " apply_config %d\n", "compare_or_set_multi_level_config_value",
                                apply_config);
    if (apply_config == FALSE) {
        for (i = 0; i < MAX_MULTI_LEVEL_CONFIG; ++i) {

            if (mlConfigs[i].vendorVal != NULL) {
                value = mlConfigs[i].vendorVal;
            } else if (mlConfigs[i].commonVal != NULL) {
                value = mlConfigs[i].commonVal;
            } else {
                value = mlConfigs[i].enterpriseVal;
            }

            if (value != NULL) {
                switch (mlConfigs[i].cfgId) {
                    case MLCFG_VIDEO_CAPABILITY:
                        //first check whether config can modify it. If config last 
                        //modified it, change it from config, otherwise ignore.
                        if (g_accessoryCfgInfo.video == ACCSRY_CFGD_CFG) {
                            intVal = atoi(value);
                            CONFIG_ERROR(CFG_F_PREFIX " setting %d for %s or config id %d\n", "compare_or_set_multi_level_config_value",
                                                      intVal, mlConfigs[i].name, mlConfigs[i].cfgId);
                            cc_media_update_video_cap((intVal == 1) ? TRUE : FALSE);
                        }
                        break;
                    case MLCFG_CISCO_CAMERA:
                        //first check whether config can modify it. If config last 
                        //modified it, change it from config, otherwise ignore.
                        if (g_accessoryCfgInfo.video == ACCSRY_CFGD_CFG) {
                            intVal = atoi(value);
                            CONFIG_ERROR(CFG_F_PREFIX " setting %d for %s or config id %d\n", "compare_or_set_multi_level_config_value",
                                                      intVal, mlConfigs[i].name, mlConfigs[i].cfgId);
                            cc_media_update_native_video_txcap((intVal == 1) ? TRUE : FALSE);
                        }
                        break; 
                    default:
                        CONFIG_ERROR(CFG_F_PREFIX " config id %d at index %d not found in table\n", "compare_or_set_multi_level_config_value",
                                                  mlConfigs[i].cfgId, i);
                        break; 
                }
            } 
        }
    }
}

#define MISSEDCALLS "Application:Cisco/MissedCalls"
#define PLACEDCALLS "Application:Cisco/PlacedCalls"
#define RECEIVEDCALLS "Application:Cisco/ReceivedCalls"
/*
 * config_set_phone_services
 *
 */
void config_process_phone_services (xmlNode *services, xmlDocPtr doc)
{
    xmlNode *cur_node, *cnode;
    char *data;
    boolean missed=FALSE, plcd=FALSE, rcvd=FALSE; 

    for (cur_node = services->xmlChildrenNode; cur_node; cur_node = cur_node->next) {
        data = (char*)xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
        if (cur_node->type == XML_ELEMENT_NODE) {
            if (!xmlStrcmp(cur_node->name, (const xmlChar *) "provisioning")) {
                CONFIG_DEBUG(CFG_F_PREFIX " provisioning value=%s\n", "config_process_phone_services", data);
                if ( data && !strcmp((char*)data, "1")  ) {
                    // we are done. External provisioning is true
                    ccsnap_set_phone_services_provisioning( FALSE, FALSE, FALSE );
                    return;
                }
	    }
            if (!xmlStrcmp(cur_node->name, (const xmlChar *) "phoneService")) {
                for (cnode = cur_node->xmlChildrenNode; cnode; cnode = cnode->next) {
                    data = (char*)xmlNodeListGetString(doc, cnode->xmlChildrenNode, 1);
                    if (!xmlStrcmp(cnode->name, (const xmlChar *) "url")) {
                        CONFIG_DEBUG(CFG_F_PREFIX " processing node %s value=%s\n", 
					      "config_process_phone_services", cnode->name, data);
                        if ( data && !strcmp(data, PLACEDCALLS)){
                            plcd = TRUE;
                        } else if ( data && !strcmp(data, RECEIVEDCALLS)){
                            rcvd = TRUE;
                        } else if ( data && !strcmp(data, MISSEDCALLS )){
                            missed = TRUE;
                        }
		    }
                }
            }
        }
    }
    ccsnap_set_phone_services_provisioning(missed, plcd, rcvd);
}


/*
 * config_set_ccm_properties
 *
 */
void config_process_ccm_properties  (xmlNode *ccm_node, xmlDocPtr doc)
{
    char ccm_name[MAX_CCM][MAX_SIP_URL_LENGTH];
    int ccm_index = 0;
    unsigned int i;
    cpr_ip_addr_t ip_addr;
    boolean valid_ccm_found = FALSE;

    memset(ccm_name, 0, sizeof(ccm_name));
        for (i = 0; i < sizeof(sip_port)/sizeof(sip_port[0]); i++) {
                sip_port[i] = 5060;
                secured_sip_port[i] = 5061;
        }
        
            
    process_ccm_config(ccm_node, doc, ccm_name, sip_port,
                                           secured_sip_port, &ccm_index);

    DEF_DEBUG("ccm0=%s ccm1=%s  ccm2=%s sip_port_0=%d sip_port_1=%d sip_port_2=%d length=%d",
            ccm_name[0], ccm_name[1], ccm_name[2], sip_port[0], sip_port[1], sip_port[2], strlen(ccm_name[2]));
    DEF_DEBUG("ccm0=%s ccm1=%s  ccm2=%s sec_sip_port_0=%d sec_sip_port_1=%d sec_sip_port_2=%d length=%d",
                          ccm_name[0], ccm_name[1], ccm_name[2],
                          secured_sip_port[0], secured_sip_port[1], secured_sip_port[2], strlen(ccm_name[2]));

    if (strlen(ccm_name[2]) > 0 && !dnsGetHostByName(ccm_name[2], &ip_addr, 100, 1)) {
        compare_or_set_string_value(CFGID_CCM3_ADDRESS+0, ccm_name[2], (const unsigned char *) "ccm3_addr");
        compare_or_set_boolean_value(CFGID_CCM3_IS_VALID + 0, 1, (const unsigned char *)"ccm3_isvalid");
        valid_ccm_found = TRUE;
    }

    if (strlen(ccm_name[1]) > 0 && !dnsGetHostByName(ccm_name[1], &ip_addr, 100, 1)) {
        compare_or_set_string_value(CFGID_CCM2_ADDRESS+0, ccm_name[1], (const unsigned char *) "ccm2_addr");
        compare_or_set_boolean_value(CFGID_CCM2_IS_VALID + 0, 1, (const unsigned char * )"ccm2_isvalid");
        valid_ccm_found = TRUE;
    } 

    if (strlen(ccm_name[0]) > 0 && !dnsGetHostByName(ccm_name[0], &ip_addr, 100, 1)) {
        compare_or_set_string_value(CFGID_CCM1_ADDRESS+0, ccm_name[0], (const unsigned char *) "ccm1_addr");
        compare_or_set_boolean_value(CFGID_CCM1_IS_VALID + 0, 1, (const unsigned char *)"ccm1_isvalid");
    }
    (void) valid_ccm_found; // XXX set but not used
}

void process_ccm_config(xmlNode *ccm_node, xmlDocPtr doc,
						char ccm_name[][MAX_SIP_URL_LENGTH],
						int * sip_port, int * secured_sip_port,
						int *iteration) {
    int ccm_index = -1; 
    char * value;
    xmlNode *cur_node;
    for (cur_node = ccm_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            value = (char *)xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
            value = value?value:"";
            if (!xmlStrcmp(cur_node->name, (const xmlChar *) "processNodeName")) {
                /* 
                 * processNodeName is the last element of this set of cucm info. So we 
                 * increment iteraton here for next set of cucm info.
                 */
                ccm_index = *iteration;
                ++(*iteration);
                //strcpy(*(ccm_name + ccm_index), value); 
                strcpy(ccm_name[ccm_index], value); 
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s: ccm_name[%d]=%s\n", "process_ccm_config", 
                        cur_node->name, value, ccm_index, ccm_name[ccm_index]);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "sipPort")) {
                /* 
                 * we process sipPort before processNodeName. So we set
                 * ccm_index here.
                 */
                ccm_index = *iteration;
                *(sip_port + ccm_index) =  atoi(value);
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s port[%d]=%d\n", "process_ccm_config", 
                    cur_node->name, value, ccm_index, *(sip_port + ccm_index));
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "securedSipPort")) {
                /* 
                 * we process securedSipPort before processNodeName. So we set
                 * ccm_index here.
                 */
                ccm_index = *iteration;
                *(secured_sip_port + ccm_index) =  atoi(value);
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s secure_port[%d]=%d\n", "process_ccm_config", 
                    cur_node->name, value, ccm_index, *(secured_sip_port + ccm_index));
            }
        }
        process_ccm_config(cur_node->children, doc, ccm_name, sip_port,
						   secured_sip_port, iteration);
    }
}

/*
 * config_get_mac_addr
 *
 * Get the filename that has the mac address and parse the string 
 * convert it into an mac address stored in the bytearray maddr
*/
void config_get_mac_addr (char *maddr)
{
    platGetMacAddr(maddr);

}

/*
 * Set the MAC address in the config table
 */
void config_set_ccm_ip_mac ()
{

    char macaddr[MAC_ADDR_SIZE];

    compare_or_set_int_value(CFGID_DSCP_FOR_CALL_CONTROL , 1, (const unsigned char *) "DscpCallControl");
    compare_or_set_int_value(CFGID_SPEAKER_ENABLED, 1, (const unsigned char *) "speakerEnabled");

    if (apply_config == FALSE) {
        config_get_mac_addr(macaddr);     

        CONFIG_DEBUG(CFG_F_PREFIX ": MAC Address IS:  %x:%x:%x:%x:%x:%x  \n",
                          "config_get_mac_addr", macaddr[0], macaddr[1],
                           macaddr[2], macaddr[3], macaddr[4], macaddr[5]);

        CC_Config_setArrayValue(CFGID_MY_MAC_ADDR, macaddr, MAC_ADDR_SIZE);
        CC_Config_setArrayValue(CFGID_MY_ACTIVE_MAC_ADDR, macaddr, MAC_ADDR_SIZE);
    }
}

/*
 * config_parse_element
 *
 * Parse the elements in the XML document and call the 
 * corresponding set functions
 */
void config_parse_element (xmlNode *cur_node, char *  value, xmlDocPtr doc )
{
    char temp_string[MAX_SIP_URL_LENGTH];
    unsigned int i;
    int callidblock;
    int anonblock;

    value = value?value:"";
    if (!xmlStrcmp(cur_node->name, (const xmlChar *) "autoRegistrationName")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        //Set auto-reg name
        strncpy(autoreg_name, value, MAX_LINE_NAME_SIZE);

    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "autoRegistration")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        if (apply_config == FALSE) {
            config_set_autoreg_properties();
        }

    } else if(!xmlStrcmp(cur_node->name, (const xmlChar *) "MissedCallLoggingOption")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        if (apply_config == FALSE) {
            calllogger_setMissedCallLoggingConfig(value);
        }
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "startMediaPort")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_MEDIA_PORT_RANGE_START, atoi(value), cur_node->name);
        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "stopMediaPort")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_MEDIA_PORT_RANGE_END, atoi(value), cur_node->name);
        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "callerIdBlocking")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
                        
        callidblock = atoi(value);
        if ((callidblock == ID_BLOCK_PREF1) || (callidblock == ID_BLOCK_PREF3)) {
            callidblock = TRUE;
        } else {
            callidblock = FALSE;
        }
        compare_or_set_boolean_value(CFGID_CALLERID_BLOCKING,
                                   callidblock, cur_node->name);
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "anonymousCallBlock")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        anonblock = atoi(value);
        if ((anonblock == ID_BLOCK_PREF1) || (anonblock == ID_BLOCK_PREF3)) {
            anonblock = TRUE;
        } else {
            anonblock = FALSE;
        }
        compare_or_set_boolean_value(CFGID_ANONYMOUS_CALL_BLOCK,
                                  anonblock, cur_node->name);
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "preferredCodec")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_string_value(CFGID_PREFERRED_CODEC, value, cur_node->name);

    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "dtmfOutofBand")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_string_value(CFGID_DTMF_OUTOFBAND, value, cur_node->name);
                        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "dtmfAvtPayload")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_DTMF_AVT_PAYLOAD, atoi(value), cur_node->name);
                        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "dialTemplate")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        strncpy(dialTemplateFile, value, FILE_PATH);
        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "featurePolicyFile")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        strncpy(fcpTemplateFile, value, FILE_PATH);        
                        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "dtmfDbLevel")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_DTMF_DB_LEVEL, atoi(value), cur_node->name);
                        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "sipRetx")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_SIP_RETX, atoi(value), cur_node->name);

    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "sipInviteRetx")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_SIP_INVITE_RETX, atoi(value), cur_node->name);

    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "timerT1")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_TIMER_T1, atoi(value), cur_node->name);
                        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "timerT2")) {
        compare_or_set_int_value(CFGID_TIMER_T2, atoi(value), cur_node->name);
                        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "timerInviteExpires")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_TIMER_INVITE_EXPIRES, atoi(value), cur_node->name);
                        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "timerRegisterExpires")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_TIMER_REGISTER_EXPIRES, atoi(value), cur_node->name);
                        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "registerWithProxy")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_boolean_value(CFGID_PROXY_REGISTER,
                                  (!strcmp(value,"true")) ? TRUE : FALSE, cur_node->name);
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "backupProxy")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_string_value(CFGID_PROXY_BACKUP, value, cur_node->name);
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "backupProxyPort")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_PROXY_BACKUP_PORT, atoi(value), cur_node->name);
                        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "emergencyProxy")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
            compare_or_set_string_value(CFGID_PROXY_EMERGENCY, value, cur_node->name);
            
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "emergencyProxyPort")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_PROXY_EMERGENCY_PORT, atoi(value), cur_node->name);
        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "outboundProxy")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
            compare_or_set_string_value(CFGID_OUTBOUND_PROXY, value, cur_node->name);
            
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "outboundProxyPort")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_OUTBOUND_PROXY_PORT, atoi(value), cur_node->name);
                        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "natRecievedProcessing")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_boolean_value(CFGID_NAT_RECEIVED_PROCESSING,
                                          (!strcmp(value,"true")) ? TRUE : FALSE, cur_node->name);
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "userInfo")) {
        for (i=0; i < strlen(value); i++) {
            temp_string[i] = tolower(value[i]); 
        }
        temp_string[i] = '\0';
        compare_or_set_string_value(CFGID_REG_USER_INFO, temp_string, cur_node->name);
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            temp_string);
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "remotePartyID")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_boolean_value(CFGID_REMOTE_PARTY_ID,
                                  (!strcmp(value,"true")) ? TRUE : FALSE, cur_node->name);
                        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "semiAttendedTransfer")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_boolean_value (CFGID_SEMI_XFER,
                                      (!strcmp(value,"true")) ? TRUE : FALSE, cur_node->name);                        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "callHoldRingback")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_CALL_HOLD_RINGBACK, atoi(value), cur_node->name);            
                        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "stutterMsgWaiting")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_boolean_value(CFGID_STUTTER_MSG_WAITING,
                                  (!strcmp(value,"true")) ? TRUE : FALSE, cur_node->name);
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "callForwardURI")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
            compare_or_set_string_value(CFGID_CALL_FORWARD_URI, value, cur_node->name);
    
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "callStats")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_boolean_value(CFGID_CALL_STATS,
                                  (!strcmp(value,"true")) ? TRUE : FALSE, cur_node->name);
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "timerRegisterDelta")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_TIMER_REGISTER_DELTA, atoi(value), cur_node->name);
        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "maxRedirects")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_SIP_MAX_FORWARDS, atoi(value), cur_node->name);
        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "rfc2543Hold")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_boolean_value(CFGID_2543_HOLD,
                                  (!strcmp(value,"true")) ? TRUE : FALSE, cur_node->name);
            
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "localCfwdEnable")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_boolean_value(CFGID_LOCAL_CFWD_ENABLE,
                                  (!strcmp(value,"true")) ? TRUE : FALSE, cur_node->name);

    } else if (!xmlStrcmp(cur_node->name,(const xmlChar *) "connectionMonitorDuration")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_CONN_MONITOR_DURATION, atoi(value), cur_node->name);
            
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "callLogBlfEnabled")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_CALL_LOG_BLF_ENABLED, atoi(value) & 0x1, cur_node->name);
            
    } else if (!xmlStrcmp(cur_node->name,
                          (const xmlChar *) "retainForwardInformation")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_boolean_value(CFGID_RETAIN_FORWARD_INFORMATION,
                                  (!strcmp(value,"true")) ? TRUE : FALSE, cur_node->name);
 
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "remoteCcEnable")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        if (!strncasecmp(value, "true", 4)) {
                compare_or_set_int_value(CFGID_REMOTE_CC_ENABLED, 1, cur_node->name);
        } else {
                compare_or_set_int_value(CFGID_REMOTE_CC_ENABLED, 0, cur_node->name);
        }
                        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "timerKeepAliveExpires")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_TIMER_KEEPALIVE_EXPIRES, atoi(value), cur_node->name);
        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "timerSubscribeExpires")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_TIMER_SUBSCRIBE_EXPIRES, atoi(value), cur_node->name);
        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "timerSubscribeDelta")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_TIMER_SUBSCRIBE_DELTA, atoi(value), cur_node->name);
        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "deviceSecurityMode")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
		security_mode = atoi(value);
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "transportLayerProtocol")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_TRANSPORT_LAYER_PROT, atoi(value), cur_node->name);
        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "kpml")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_KPML_ENABLED, atoi(value), cur_node->name);
        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "natEnabled")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_boolean_value(CFGID_NAT_ENABLE,
                                          (!strcmp(value,"true")) ? TRUE : FALSE, cur_node->name);
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "natAddress")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
            compare_or_set_string_value(CFGID_NAT_ADDRESS, value, cur_node->name);
            
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "voipControlPort")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_VOIP_CONTROL_PORT, atoi(value), cur_node->name);
        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "enableVad")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
                        value);
        compare_or_set_boolean_value(CFGID_ENABLE_VAD,
                                          (!strcmp(value,"true")) ? TRUE : FALSE, cur_node->name);
                                          
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "autoAnswerAltBehavior")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element",
                     cur_node->name, value);
        compare_or_set_boolean_value(CFGID_AUTOANSWER_IDLE_ALTERNATE,
                                          (!strcmp(value,"true")) ? TRUE : FALSE, cur_node->name);
                                          
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "autoAnswerTimer")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_AUTOANSWER_TIMER, atoi(value), cur_node->name);

    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "autoAnswerOverride")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n",
                             "config_parser_element", cur_node->name, value);
        compare_or_set_boolean_value(CFGID_AUTOANSWER_OVERRIDE,
                                          (!strcmp(value,"true")) ? TRUE : FALSE, cur_node->name);
    } else if (!xmlStrcmp(cur_node->name,
                        (const xmlChar *) "offhookToFirstDigitTimer")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_OFFHOOK_TO_FIRST_DIGIT_TIMER, atoi(value), cur_node->name);
    } else if (!xmlStrcmp(cur_node->name,
                         (const xmlChar *) "silentPeriodBetweenCallWaitingBursts")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_CALL_WAITING_SILENT_PERIOD, atoi(value), cur_node->name);
        
    } else if (!xmlStrcmp(cur_node->name,
                          (const xmlChar *) "ringSettingBusyStationPolicy")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
                          value);
        compare_or_set_int_value(CFGID_RING_SETTING_BUSY_POLICY, atoi(value), cur_node->name);
        
    } else if (!xmlStrcmp(cur_node->name,
                          (const xmlChar *) "blfAudibleAlertSettingOfIdleStation")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value (CFGID_BLF_ALERT_TONE_IDLE, atoi(value), cur_node->name);
        
    } else if (!xmlStrcmp(cur_node->name,
                         (const xmlChar *) "blfAudibleAlertSettingOfBusyStation")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value (CFGID_BLF_ALERT_TONE_BUSY, atoi(value), cur_node->name);
        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "joinAcrossLines")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value (CFGID_JOIN_ACROSS_LINES, atoi(value), cur_node->name);
        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "cnfJoinEnabled")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_boolean_value(CFGID_CNF_JOIN_ENABLE,
                                          (!strcmp(value,"true")) ? TRUE : FALSE, cur_node->name);
                                          
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "rollover")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value (CFGID_ROLLOVER, atoi(value), cur_node->name);
        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "transferOnhookEnabled")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_boolean_value(CFGID_XFR_ONHOOK_ENABLED,
                                          (!strcmp(value,"true")) ? TRUE : FALSE, cur_node->name);   
                                               
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "dscpForAudio")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_DSCP_AUDIO, atoi(value), cur_node->name);

    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "dscpVideo")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_DSCP_VIDEO, atoi(value), cur_node->name);

    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "T302Timer")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);
        compare_or_set_int_value(CFGID_INTER_DIGIT_TIMER, atoi(value), cur_node->name);

    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "sipLines")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_parser_element", cur_node->name,
            value);

        update_sipline_properties(cur_node,  doc);
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "callManagerGroup")) {
      
        CONFIG_DEBUG(CFG_F_PREFIX "%s  \n", "config_parser_element", cur_node->name);
        config_process_ccm_properties(cur_node,  doc);

        config_set_ccm_ip_mac();
        
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "vendorConfig")) {

        CONFIG_DEBUG(CFG_F_PREFIX "%s  \n", "config_parse_element", cur_node->name);
        config_process_multi_level_config(cur_node->children, doc, cur_node->name, updateVendorConfig);

    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "commonConfig")) {

        CONFIG_DEBUG(CFG_F_PREFIX "%s  \n", "config_parse_element", cur_node->name);
        config_process_multi_level_config(cur_node->children, doc, cur_node->name, updateCommonConfig);

    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "enterpriseConfig")) {

        CONFIG_DEBUG(CFG_F_PREFIX "%s  \n", "config_parse_element", cur_node->name);
        config_process_multi_level_config(cur_node->children, doc, cur_node->name, updateEnterpriseConfig);
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "phoneServices")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s \n", "config_parse_element", cur_node->name);
        config_process_phone_services(cur_node,  doc);
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "versionStamp")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s \n", "config_parse_element", cur_node->name);
        CONFIG_ERROR(CFG_F_PREFIX "%s new=%s old=%s \n", "config_parser_element", cur_node->name, value, g_cfg_version_stamp);
        if (apply_config == FALSE) {
            memset(g_cfg_version_stamp, 0, sizeof(g_cfg_version_stamp));
            i = strlen(value);
            if (i > MAX_CFG_VERSION_STAMP_LEN) {
                CONFIG_ERROR(CFG_F_PREFIX "config version %d, bigger than allocated space %d\n", "config_parser_element", i, MAX_CFG_VERSION_STAMP_LEN);
                i = MAX_CFG_VERSION_STAMP_LEN;
            }
            strncpy(g_cfg_version_stamp, value, i);
        }
        else {
            CONFIG_ERROR(CFG_F_PREFIX "got NULL value for %s\n", "config_parser_element", cur_node->name);
        } 
    } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "externalNumberMask")) {
        CONFIG_DEBUG(CFG_F_PREFIX "%s \n", "config_parser_element", cur_node->name);
            compare_or_set_string_value(CFGID_CCM_EXTERNAL_NUMBER_MASK, value, cur_node->name);
    }                        
}


/*
 * config_setup_element
 * Setup elements that once were downloaded from CUCM in an XML file.
 * Settings are stored in config.h
 */
void config_setup_elements (const char *sipUser, const char *sipPassword, const char *sipDomain)
{
    unsigned int i;
    char buf[MAX_SIP_URL_LENGTH] = {'\0'};
    char ip[MAX_SIP_URL_LENGTH] = {'\0'};
    char option[MAX_SIP_URL_LENGTH] = {'\0'};
    int line = 0;
    cc_boolean isSecure = FALSE, isValid = TRUE;
    char macaddr[MAC_ADDR_SIZE];

    compare_or_set_int_value(CFGID_MEDIA_PORT_RANGE_START, gStartMediaPort, (const unsigned char *) "startMediaPort");
    compare_or_set_int_value(CFGID_MEDIA_PORT_RANGE_END, gStopMediaPort, (const unsigned char *) "stopMediaPort");
    compare_or_set_boolean_value(CFGID_CALLERID_BLOCKING, gCallerIdBlocking, (const unsigned char *) "callerIdBlocking");
    compare_or_set_boolean_value(CFGID_ANONYMOUS_CALL_BLOCK, gAnonblock, (const unsigned char *) "anonymousCallBlock");
    compare_or_set_string_value(CFGID_PREFERRED_CODEC, gPreferredCodec, (const unsigned char *) "preferredCodec");
    compare_or_set_string_value(CFGID_DTMF_OUTOFBAND, gDtmfOutOfBand, (const unsigned char *) "dtmfOutofBand");
    compare_or_set_int_value(CFGID_DTMF_AVT_PAYLOAD, gDtmfAvtPayload, (const unsigned char *) "dtmfAvtPayload");
    compare_or_set_int_value(CFGID_DTMF_DB_LEVEL, gDtmfDbLevel, (const unsigned char *) "dtmfDbLevel");
    compare_or_set_int_value(CFGID_SIP_RETX, gSipRetx, (const unsigned char *) "sipRetx");
    compare_or_set_int_value(CFGID_SIP_INVITE_RETX, gSipInviteRetx, (const unsigned char *) "sipInviteRetx");
    compare_or_set_int_value(CFGID_TIMER_T1, gTimerT1, (const unsigned char *) "timerT1");
    compare_or_set_int_value(CFGID_TIMER_T2, gTimerT2, (const unsigned char *) "timerT2");
    compare_or_set_int_value(CFGID_TIMER_INVITE_EXPIRES, gTimerInviteExpires, (const unsigned char *) "timerInviteExpires");
    compare_or_set_int_value(CFGID_TIMER_REGISTER_EXPIRES, gTimerRegisterExpires, (const unsigned char *) "timerRegisterExpires");
    compare_or_set_boolean_value(CFGID_PROXY_REGISTER, gRegisterWithProxy, (const unsigned char *) "registerWithProxy");
    compare_or_set_string_value(CFGID_PROXY_BACKUP, gBackupProxy, (const unsigned char *) "backupProxy");
    compare_or_set_int_value(CFGID_PROXY_BACKUP_PORT, gBackupProxyPort, (const unsigned char *) "backupProxyPort");
    compare_or_set_string_value(CFGID_PROXY_EMERGENCY, gEmergencyProxy, (const unsigned char *) "emergencyProxy");
    compare_or_set_int_value(CFGID_PROXY_EMERGENCY_PORT, gEmergencyProxyPort, (const unsigned char *) "emergencyProxyPort");
    compare_or_set_string_value(CFGID_OUTBOUND_PROXY, gOutboundProxy, (const unsigned char *) "outboundProxy");
    compare_or_set_int_value(CFGID_OUTBOUND_PROXY_PORT, gOutboundProxyPort, (const unsigned char *) "outboundProxyPort");
    compare_or_set_boolean_value(CFGID_NAT_RECEIVED_PROCESSING, gNatRecievedProcessing, (const unsigned char *) "natRecievedProcessing");
    compare_or_set_string_value(CFGID_REG_USER_INFO, gUserInfo, (const unsigned char *) "userInfo");
    compare_or_set_boolean_value(CFGID_REMOTE_PARTY_ID, gRemotePartyID, (const unsigned char *) "remotePartyID");
    compare_or_set_boolean_value (CFGID_SEMI_XFER, gSemiAttendedTransfer, (const unsigned char *) "semiAttendedTransfer");
    compare_or_set_int_value(CFGID_CALL_HOLD_RINGBACK, gCallHoldRingback, (const unsigned char *) "callHoldRingback");
    compare_or_set_boolean_value(CFGID_STUTTER_MSG_WAITING, gStutterMsgWaiting, (const unsigned char *) "stutterMsgWaiting");
    compare_or_set_string_value(CFGID_CALL_FORWARD_URI, gCallForwardURI, (const unsigned char *) "callForwardURI");
    compare_or_set_boolean_value(CFGID_CALL_STATS, gCallStats, (const unsigned char *) "callStats");
    compare_or_set_int_value(CFGID_TIMER_REGISTER_DELTA, gTimerRegisterDelta, (const unsigned char *) "timerRegisterDelta");
    compare_or_set_int_value(CFGID_SIP_MAX_FORWARDS, gMaxRedirects, (const unsigned char *) "maxRedirects");
    compare_or_set_boolean_value(CFGID_2543_HOLD, gRfc2543Hold, (const unsigned char *) "rfc2543Hold");
    compare_or_set_boolean_value(CFGID_LOCAL_CFWD_ENABLE, gLocalCfwdEnable, (const unsigned char *) "localCfwdEnable");
    compare_or_set_int_value(CFGID_CONN_MONITOR_DURATION, gConnectionMonitorDuration, (const unsigned char *) "connectionMonitorDuration");
    compare_or_set_int_value(CFGID_CALL_LOG_BLF_ENABLED, gCallLogBlfEnabled, (const unsigned char *) "callLogBlfEnabled");
    compare_or_set_boolean_value(CFGID_RETAIN_FORWARD_INFORMATION, gRetainForwardInformation, (const unsigned char *) "retainForwardInformation");
    compare_or_set_int_value(CFGID_REMOTE_CC_ENABLED, gRemoteCcEnable, (const unsigned char *) "remoteCcEnable");
    compare_or_set_int_value(CFGID_TIMER_KEEPALIVE_EXPIRES, gTimerKeepAliveExpires, (const unsigned char *) "timerKeepAliveExpires");
    compare_or_set_int_value(CFGID_TIMER_SUBSCRIBE_EXPIRES, gTimerSubscribeExpires, (const unsigned char *) "timerSubscribeExpires");
    compare_or_set_int_value(CFGID_TIMER_SUBSCRIBE_DELTA, gTimerSubscribeDelta, (const unsigned char *) "timerSubscribeDelta");
    compare_or_set_int_value(CFGID_TRANSPORT_LAYER_PROT, gTransportLayerProtocol, (const unsigned char *) "transportLayerProtocol");
    compare_or_set_int_value(CFGID_KPML_ENABLED, gKpml, (const unsigned char *) "kpml");
    compare_or_set_boolean_value(CFGID_NAT_ENABLE, gNatEnabled, (const unsigned char *) "natEnabled");
    compare_or_set_string_value(CFGID_NAT_ADDRESS, gNatAddress, (const unsigned char *) "natAddress");
    compare_or_set_int_value(CFGID_VOIP_CONTROL_PORT, gVoipControlPort, (const unsigned char *) "voipControlPort");
    compare_or_set_boolean_value(CFGID_ENABLE_VAD, gAnableVad, (const unsigned char *) "enableVad");
    compare_or_set_boolean_value(CFGID_AUTOANSWER_IDLE_ALTERNATE, gAutoAnswerAltBehavior, (const unsigned char *) "autoAnswerAltBehavior");
    compare_or_set_int_value(CFGID_AUTOANSWER_TIMER, gAutoAnswerTimer, (const unsigned char *) "autoAnswerTimer");
    compare_or_set_boolean_value(CFGID_AUTOANSWER_OVERRIDE, gAutoAnswerOverride, (const unsigned char *) "autoAnswerOverride");
    compare_or_set_int_value(CFGID_OFFHOOK_TO_FIRST_DIGIT_TIMER, gOffhookToFirstDigitTimer, (const unsigned char *) "offhookToFirstDigitTimer");
    compare_or_set_int_value(CFGID_CALL_WAITING_SILENT_PERIOD, gSilentPeriodBetweenCallWaitingBursts, (const unsigned char *) "silentPeriodBetweenCallWaitingBursts");
    compare_or_set_int_value(CFGID_RING_SETTING_BUSY_POLICY, gRingSettingBusyStationPolicy, (const unsigned char *) "ringSettingBusyStationPolicy");
    compare_or_set_int_value (CFGID_BLF_ALERT_TONE_IDLE, gBlfAudibleAlertSettingOfIdleStation, (const unsigned char *) "blfAudibleAlertSettingOfIdleStation");
    compare_or_set_int_value (CFGID_BLF_ALERT_TONE_BUSY, gBlfAudibleAlertSettingOfBusyStation, (const unsigned char *) "blfAudibleAlertSettingOfBusyStation");
    compare_or_set_int_value (CFGID_JOIN_ACROSS_LINES, gJoinAcrossLines, (const unsigned char *) "joinAcrossLines");
    compare_or_set_boolean_value(CFGID_CNF_JOIN_ENABLE, gCnfJoinEnabled, (const unsigned char *) "cnfJoinEnabled");
    compare_or_set_int_value (CFGID_ROLLOVER, gRollover, (const unsigned char *) "rollover");
    compare_or_set_boolean_value(CFGID_XFR_ONHOOK_ENABLED, gTransferOnhookEnabled, (const unsigned char *) "transferOnhookEnabled");
    compare_or_set_int_value(CFGID_DSCP_AUDIO, gDscpForAudio, (const unsigned char *) "dscpForAudio");
    compare_or_set_int_value(CFGID_DSCP_VIDEO, gDscpVideo, (const unsigned char *) "dscpVideo");
    compare_or_set_int_value(CFGID_INTER_DIGIT_TIMER, gT302Timer, (const unsigned char *) "T302Timer");
 	compare_or_set_int_value(CFGID_LINE_INDEX + line, gLineIndex, (const unsigned char *)"lineIndex");
    compare_or_set_int_value(CFGID_LINE_FEATURE + line, gFeatureID, (const unsigned char *) "featureID");
    compare_or_set_string_value(CFGID_PROXY_ADDRESS + line, gProxy, (const unsigned char *) "proxy");
    compare_or_set_int_value(CFGID_PROXY_PORT + line, gPort, (const unsigned char *) "port");

    if ( apply_config == FALSE )  {
         ccsnap_set_line_label(line+1, "LINELABEL");
    }

    compare_or_set_string_value(CFGID_LINE_NAME + line, sipUser, (const unsigned char *) "name");
    compare_or_set_string_value(CFGID_LINE_DISPLAYNAME + line, gDisplayName, (const unsigned char *) "displayName");
    compare_or_set_string_value(CFGID_LINE_MESSAGES_NUMBER + line, gMessagesNumber, (const unsigned char *) "messagesNumber");
    compare_or_set_boolean_value(CFGID_LINE_FWD_CALLER_NAME_DIPLAY + line, gCallerName, (const unsigned char *) "callerName");
    compare_or_set_boolean_value(CFGID_LINE_FWD_CALLER_NUMBER_DIPLAY + line, gCallerNumber, (const unsigned char *) "callerNumber");
    compare_or_set_boolean_value(CFGID_LINE_FWD_REDIRECTED_NUMBER_DIPLAY + line, gRedirectedNumber, (const unsigned char *) "redirectedNumber");
    compare_or_set_boolean_value(CFGID_LINE_FWD_DIALED_NUMBER_DIPLAY + line, gDialedNumber, (const unsigned char *) "dialedNumber");
    compare_or_set_byte_value(CFGID_LINE_MSG_WAITING_LAMP + line, gMessageWaitingLampPolicy, (const unsigned char *) "messageWaitingLampPolicy");
    compare_or_set_byte_value(CFGID_LINE_MESSAGE_WAITING_AMWI + line, gMessageWaitingAMWI, (const unsigned char *) "messageWaitingAMWI");
    compare_or_set_byte_value(CFGID_LINE_RING_SETTING_IDLE + line, gRingSettingIdle, (const unsigned char *) "ringSettingIdle");
    compare_or_set_byte_value(CFGID_LINE_RING_SETTING_ACTIVE + line, gRingSettingActive, (const unsigned char *) "ringSettingActive");
    compare_or_set_string_value(CFGID_LINE_CONTACT + line, sipUser, (const unsigned char *) "contact");
    compare_or_set_int_value(CFGID_LINE_MAXNUMCALLS + line, gMaxNumCalls, (const unsigned char *) "maxNumCalls");
    compare_or_set_int_value(CFGID_LINE_BUSY_TRIGGER + line, gBusyTrigger, (const unsigned char *) "busyTrigger");
    compare_or_set_byte_value(CFGID_LINE_AUTOANSWER_ENABLED + line, gAutoAnswerEnabled, (const unsigned char *) "autoAnswerEnabled");
    compare_or_set_byte_value(CFGID_LINE_CALL_WAITING + line, gCallWaiting, (const unsigned char *) "callWaiting");


    compare_or_set_string_value(CFGID_LINE_AUTHNAME + line, sipUser, (const unsigned char *)"authName");
    compare_or_set_string_value(CFGID_LINE_PASSWORD + line, sipPassword, (const unsigned char *)"authPassword");


    compare_or_set_int_value(CFGID_CCM1_SEC_LEVEL, gDeviceSecurityMode,(const unsigned char *)"deviceSecurityMode");
    compare_or_set_int_value(CFGID_CCM1_SIP_PORT, gCcm1_sip_port,(const unsigned char *)"ccm1_sip_port");
    compare_or_set_int_value(CFGID_CCM2_SIP_PORT, gCcm2_sip_port,(const unsigned char *)"ccm2_sip_port");
    compare_or_set_int_value(CFGID_CCM3_SIP_PORT, gCcm3_sip_port, (const unsigned char *)"ccm3_sip_port");


    isSecure = FALSE;
    sstrncpy(ip, "", MAX_SIP_URL_LENGTH);
    sstrncpy(option, "User Specific", MAX_SIP_URL_LENGTH);

    if (!strncmp( option, "Use Default Gateway", MAX_SIP_URL_LENGTH)) {
        config_get_default_gw(buf);
        if (buf[0] == '\0') {
        	isValid = FALSE;
        } else {
        	sstrncpy(ip, buf, MAX_SIP_URL_LENGTH);
        }
    } else if (!strncmp( option, "Disable", MAX_SIP_URL_LENGTH)) {
        isValid= FALSE;
        isSecure= FALSE;
        ip[0] = '\0';
    }

    compare_or_set_string_value(CFGID_CCM1_ADDRESS+0, sipDomain, (const unsigned char *) "ccm1_addr");
    compare_or_set_boolean_value(CFGID_CCM1_IS_VALID + 0, gCcm1_isvalid, (const unsigned char *)"ccm1_isvalid");
    compare_or_set_int_value(CFGID_DSCP_FOR_CALL_CONTROL , gDscpCallControl, (const unsigned char *) "DscpCallControl");
    compare_or_set_int_value(CFGID_SPEAKER_ENABLED, gSpeakerEnabled, (const unsigned char *) "speakerEnabled");

    if (apply_config == FALSE) {
        config_get_mac_addr(macaddr);

        CONFIG_DEBUG(CFG_F_PREFIX ": MAC Address IS:  %x:%x:%x:%x:%x:%x  \n",
                                  "config_get_mac_addr", macaddr[0], macaddr[1],
                                   macaddr[2], macaddr[3], macaddr[4], macaddr[5]);

        CC_Config_setArrayValue(CFGID_MY_MAC_ADDR, macaddr, MAC_ADDR_SIZE);
        CC_Config_setArrayValue(CFGID_MY_ACTIVE_MAC_ADDR, macaddr, MAC_ADDR_SIZE);
    }

    CONFIG_DEBUG(CFG_F_PREFIX "%s \n", "config_parse_element", "phoneServices");
    CONFIG_DEBUG(CFG_F_PREFIX "%s \n", "config_parse_element", "versionStamp");
    CONFIG_ERROR(CFG_F_PREFIX "%s new=%s old=%s \n", "config_parser_element", "versionStamp",
        		"1284570837-bbc096ed-7392-427d-9694-5ce49d5c3acb", g_cfg_version_stamp);

    if (apply_config == FALSE) {
        memset(g_cfg_version_stamp, 0, sizeof(g_cfg_version_stamp));
        i = strlen("1284570837-bbc096ed-7392-427d-9694-5ce49d5c3acb");
        if (i > MAX_CFG_VERSION_STAMP_LEN) {
            CONFIG_ERROR(CFG_F_PREFIX "config version %d, bigger than allocated space %d\n", "config_parser_element", i, MAX_CFG_VERSION_STAMP_LEN);
            i = MAX_CFG_VERSION_STAMP_LEN;
        }
        strncpy(g_cfg_version_stamp, "1284570837-bbc096ed-7392-427d-9694-5ce49d5c3acb", i);
    }
    else {
        CONFIG_ERROR(CFG_F_PREFIX "got NULL value for %s\n", "config_parser_element", "versionStamp");
    }

    CONFIG_DEBUG(CFG_F_PREFIX "%s \n", "config_parser_element", "externalNumberMask");
    compare_or_set_string_value(CFGID_CCM_EXTERNAL_NUMBER_MASK, gExternalNumberMask, (const unsigned char *) "externalNumberMask");

    // Set SIP P2P boolean
    compare_or_set_boolean_value(CFGID_P2PSIP, gP2PSIP, (const unsigned char *) "p2psip");

    // Set ROAP Proxy Mode boolean
    compare_or_set_boolean_value(CFGID_ROAPPROXY, gROAPPROXY, (const unsigned char *) "roapproxy");

    // Set ROAP Client Mode boolean
    compare_or_set_boolean_value(CFGID_ROAPCLIENT, gROAPCLIENT, (const unsigned char *) "roapclient");

    // Set product version
    compare_or_set_string_value(CFGID_VERSION, gVersion, (const unsigned char *) "version");

    (void) isSecure; // XXX set but not used
    (void) isValid; // XXX set but not used
}

void config_setup_server_address (const char *sipDomain) {
	compare_or_set_string_value(CFGID_CCM1_ADDRESS+0, sipDomain, (const unsigned char *) "ccm1_addr");
}

void config_setup_transport_udp(const cc_boolean is_udp) {
	gTransportLayerProtocol = is_udp ? 2 : 4;
	compare_or_set_int_value(CFGID_TRANSPORT_LAYER_PROT, gTransportLayerProtocol, (const unsigned char *) "transportLayerProtocol");
}

void config_setup_local_voip_control_port(const int voipControlPort) {
	gVoipControlPort = voipControlPort;
	compare_or_set_int_value(CFGID_VOIP_CONTROL_PORT, voipControlPort, (const unsigned char *) "voipControlPort");
}

void config_setup_remote_voip_control_port(const int voipControlPort) {
	gCcm1_sip_port = voipControlPort;
	compare_or_set_int_value(CFGID_CCM1_SIP_PORT, voipControlPort,(const unsigned char *)"ccm1_sip_port");
}

int config_get_local_voip_control_port() {
	return gVoipControlPort;
}

int config_get_remote_voip_control_port() {
	return gCcm1_sip_port;
}

const char* config_get_version() {
	return gVersion;
}

void config_setup_p2p_mode(const cc_boolean is_p2p) {
	gP2PSIP = is_p2p;
	compare_or_set_boolean_value(CFGID_P2PSIP, is_p2p, (const unsigned char *) "p2psip");
}

void config_setup_roap_proxy_mode(const cc_boolean is_roap_proxy) {
	gROAPPROXY = is_roap_proxy;
	compare_or_set_boolean_value(CFGID_ROAPPROXY, is_roap_proxy, (const unsigned char *) "roapproxy");
}

void config_setup_roap_client_mode(const cc_boolean is_roap_client) {
	gROAPCLIENT = is_roap_client;
	compare_or_set_boolean_value(CFGID_ROAPCLIENT, is_roap_client, (const unsigned char *) "roapclient");
}

/**
 * config_get_elements:
 * @a_node: the initial xml node to consider.
 * @doc: The DOM tree of the xml file
 *
 * Get the names of the all the xml elements
 * that are siblings or children of a given xml node.
 *
 * Recursive call to traverse all the elements of the tree.
 */
static void
config_get_elements(xmlNode * a_node, xmlDocPtr doc)
{
    xmlNode *cur_node = NULL;
    xmlChar *data;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
            if (data != NULL) {
                config_parse_element(cur_node, (char *)data, doc);
                xmlFree(data);
            }
        }
        config_get_elements(cur_node->children, doc);
    }
}


/**
* config_minimum_check:
*
* @a_node: the initial xml node to consider.
* @doc: The DOM tree of the xml file
*
* Check if minimum set of elements are present in the config file and
* have values that can be used by sipstack
*
*/
void
config_minimum_check_siplines(xmlNode * a_node, xmlDocPtr doc)
{
    xmlNode *cur_node = NULL;
    xmlChar *data;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            if (!xmlStrcmp(cur_node->name, (const xmlChar *) "name")) {
                data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_minimum_check",
                             cur_node->name, data);
                /*
                 * If we have not found a valid line, see if there is valid data
                 */
                if (lineConfig == 0 ) {
                    if ((strcmp((char *)data, UNPROVISIONED) == 0) || (data[0] == '\0')) {
                        lineConfig = 0 ;
                    } else {
                        lineConfig = 1 ;
                    }
                }
                xmlFree(data);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "proxy")) {
                data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_minimum_check",
                             cur_node->name, data);
                /*
                 * If we have not found a valid proxy, see if there is valid data
                 */
                if (proxyConfig == 0 ) {
                    if ((strcmp((char *)data, UNPROVISIONED) == 0) || (data[0] == '\0')) {
                        proxyConfig = 0 ;
                    } else {
                        proxyConfig = 1 ;
                    }
                }
                xmlFree(data);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *) "port")) {
                data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n",
                             "config_minimum_check", cur_node->name, data);
                /*
                 * If we have not found a valid port, see if there is valid data
                 */
                if (portConfig == 0 ) {
                    if (data == 0) {
                        portConfig = 0 ;
                    } else {
                        portConfig = 1 ;
                    }
                }
                xmlFree(data);
            }
        }
        config_minimum_check_siplines(cur_node->children, doc);
    }
}

boolean is_config_valid(char * config, int from_memory) {
    xmlDoc *doc = NULL;
    xmlNode *root_element = NULL;
    int size = 0;

    /*parse the config and get the DOM */
    if (from_memory) {
        size = strlen(config); 
        doc = xmlReadMemory(config, size, "noname.xml",  NULL, 0);
    } else {
        doc = xmlReadFile(config, NULL, 0);
    }

    if (doc == NULL) {
        CONFIG_ERROR(CFG_F_PREFIX "Could not open %s\n", "is_config_valid", config);
        return FALSE;
    }

    /*Get the root element node */
    root_element = xmlDocGetRootElement(doc);

    line = -1;

    lineConfig = 0;
    portConfig = 0;
    proxyConfig = 0;

    /*Check if minimum config is present to proceed */
    config_minimum_check(root_element, doc);
    /*free the document */
    xmlFreeDoc(doc);
    /*
     *Free the global variables that may
     *have been allocated by the parser.
     */
    xmlCleanupParser();

    if (!(portConfig) || !(proxyConfig) || !(lineConfig)) {
        CONFIG_ERROR(CFG_F_PREFIX "Minimum Config check Failed!\n", "is_config_valid" );
        return FALSE;
    }
    return TRUE; 
}

void
config_minimum_check(xmlNode * a_node, xmlDocPtr doc)
{
    xmlNode *cur_node = NULL;
    xmlChar *data;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            if (!xmlStrcmp(cur_node->name, (const xmlChar *) "sipLines")) {
                data = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                CONFIG_DEBUG(CFG_F_PREFIX "%s  %s\n", "config_minimum_check", cur_node->name, data);
                config_minimum_check_siplines(cur_node,  doc);
                xmlFree(data);
            }
        }
        config_minimum_check(cur_node->children, doc);
    }
}

/**
 * Process/Parse the FCP file if specified in the master config file. 
*/
void config_parser_handle_fcp_file (char* fcpTemplateFile)
{
    static char fname[] = "config_parser_handle_fcp_file";

    // if no fcp file specified in master config file, then set the default dialplan
    if (strcmp (fcpTemplateFile, "") == 0)
    {
        CC_Config_setFcp(NULL, 0);
        ccsnap_gen_deviceEvent(CCAPI_LINE_EV_CAPSET_CHANGED, CC_DEVICE_ID);

        return;
    }
    
    if  ((strcmp (g_applyConfigFcpVersion, "") != 0) &&
         (strcmp (g_fp_version_stamp, g_applyConfigFcpVersion) == 0))
    {
        CONFIG_DEBUG(CFG_F_PREFIX "No need to refetch FCP.  Proposed Version Matches With Current [%s]\n", fname, g_fp_version_stamp);    
    
        // Apply Config (proposed version) is not blank (so this is apply-config operation)
        // If fcp version matches running version, then no reload of file is necessary
        return;
    }
        
    // fetch and apply the dialplan (will save running version in g_dp_version_stamp)
    if (!fcpFetchReq(0, fcpTemplateFile))
    {   // fcp fetch failed - apply the default
        CC_Config_setFcp(NULL, 0);
        
        ccsnap_gen_deviceEvent(CCAPI_LINE_EV_CAPSET_CHANGED, CC_DEVICE_ID);
        CONFIG_ERROR(CFG_F_PREFIX "Unable to fetch FCP file=[%s].  Setting to default.\n", fname, fcpTemplateFile);       
        return;   
    }        
        
    ccsnap_gen_deviceEvent(CCAPI_LINE_EV_CAPSET_CHANGED, CC_DEVICE_ID);
    CONFIG_DEBUG(CFG_F_PREFIX "Applied FCP with version=[%s]\n", fname, g_fp_version_stamp);    
}

/**
 * Process/Parse the dialplan file if specified in the master config file. 
*/
void config_parser_handle_dialplan_file (char* dialTemplateFile)
{
    static char fname[] = "config_parser_handle_dialplan_file";

    // if no dialplan file specified in master config file, then set the default dialplan
    if (strcmp (dialTemplateFile, "") == 0)
    {
        CC_Config_setDialPlan(NULL, 0);
        return;
    }
    
    if  ((strcmp (g_applyConfigDialPlanVersion, "") != 0) &&
         (strcmp (g_dp_version_stamp, g_applyConfigDialPlanVersion) == 0))
    {
        CONFIG_DEBUG(CFG_F_PREFIX "No need to refetch DP.  Proposed Version Matches With Current [%s]\n", fname, g_fp_version_stamp);    
    
        // Apply Config (proposed version) is not blank (so this is apply-config operation)
        // If dialplan version matches running version, then no reload of file is necessary
        return;
    }
        
    // fetch and apply the dialplan (will save running version in g_dp_version_stamp)
    if (!dialPlanFetchReq(0, dialTemplateFile))
    {   // dialplan fetch failed - apply the default
        CC_Config_setDialPlan(NULL, 0);
        CONFIG_ERROR(CFG_F_PREFIX "Unable to fetch dialplan file=[%s].  Setting to default.\n", fname, dialTemplateFile);       
        return;
   
    }            
        
    CONFIG_DEBUG(CFG_F_PREFIX "Applied DIALPLAN with version=[%s]\n", fname, g_dp_version_stamp);    
}

/**
 * Parse the file that is passed in, 
 * walk down the DOM that is created , and get the
 * xml elements nodes.
 */
int
config_parser_main( char *config, int complete_config)
{
    xmlDoc*  doc          = NULL;
    xmlNode* root_element = NULL;
    int size = 0;

/*  
    CONFIG_DEBUG(CFG_F_PREFIX "Config [%s] ; complete_config [%d]\n", \
                 "config_parser_main", config, complete_config);
*/
  
    if (is_empty_str(config)) {
        CONFIG_ERROR(CFG_F_PREFIX "Received null/empty configuration to parse.\n", "config_parser_main");    
        return (-1);
    }

    // initialize the (global) values for dial template and fcp files
    strcpy (dialTemplateFile, "");
    strcpy (fcpTemplateFile, "");

    /*parse the config and get the DOM */
    if (complete_config) {
        size = strlen(config); 
        doc = xmlReadMemory(config, size, "noname.xml",  NULL, 0);
    } else {
        doc = xmlReadFile(config, NULL, 0);
    }

    if (doc == NULL) {
        CONFIG_ERROR(CFG_F_PREFIX "Could not open %s\n", "config_parser_main", config);
        return -1;
    }

    /*Get the root element node */
    root_element = xmlDocGetRootElement(doc);

    /* initialize multi level config table */
    initMultiLevelConfigs();
    /*Get the elements in the xml doc */
    config_get_elements(root_element, doc);

    /* update multi level config values */ 
    compare_or_set_multi_level_config_value();
	update_security_mode_and_ports();

    /*free the document */
    xmlFreeDoc(doc);

    /*
     *Free the global variables that may
     *have been allocated by the parser.
     */
    xmlCleanupParser();

    // Take care of Fetch and apply of FCP and DialPlan if configured and necessary    
    if (apply_config == FALSE) {                
        // parse and apply dialplan
        config_parser_handle_dialplan_file (dialTemplateFile);
        config_parser_handle_fcp_file (fcpTemplateFile); 
        
        // initialize proposed fcp and dialplan versions to blanks (will be updated next time apply config is done)
        strcpy (g_applyConfigFcpVersion, "");        
        strcpy (g_applyConfigDialPlanVersion, "");               
        }

    return 0;
}


/**
 * Function called as part of registration without using cnf device file download.
 */
int config_setup_main( const char *sipUser, const char *sipPassword, const char *sipDomain)
{
    // initialize the (global) values for dial template and fcp files
    strcpy (dialTemplateFile, "");
    strcpy (fcpTemplateFile, "");

    /* initialize multi level config table */
    initMultiLevelConfigs();
    /*Get the elements in the xml doc */
    config_setup_elements(sipUser, sipPassword, sipDomain);

    /* update multi level config values */
    compare_or_set_multi_level_config_value();
	update_security_mode_and_ports();


    // Take care of Fetch and apply of FCP and DialPlan if configured and necessary
    if (apply_config == FALSE) {
        // parse and apply dialplan
        config_parser_handle_dialplan_file (dialTemplateFile);
        config_parser_handle_fcp_file (fcpTemplateFile);

        // initialize proposed fcp and dialplan versions to blanks (will be updated next time apply config is done)
        strcpy (g_applyConfigFcpVersion, "");
        strcpy (g_applyConfigDialPlanVersion, "");
    }

    return 0;
}
