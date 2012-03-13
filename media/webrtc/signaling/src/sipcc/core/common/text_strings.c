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

#include <cpr_string.h>
#include "text_strings.h"


#define dcl_str(x,p) char str_##x[] = p
#define use_str(x) str_##x


/*
 * Debug strings. No localization needed. Keep it separate.
 */

dcl_str(DEBUG_START, "SIPCC-START:\0");
dcl_str(DEBUG_SEPARATOR_BAR, "===============\n");
dcl_str(DEBUG_CONSOLE_PASSWORD, "CONSOLE-PWD:cisco");
dcl_str(DEBUG_CONSOLE_KEYWORD_CONSOLE_STALL, "CONSOLE-STALL");
dcl_str(DEBUG_CONSOLE_KEYWORD_MEMORYMAP, "CONSOLE-MEMORYMAP");
dcl_str(DEBUG_CONSOLE_KEYWORD_MALLOCTABLE, "CONSOLE-MALLOCTABLE");
dcl_str(DEBUG_CONSOLE_KEYWORD_MEMORYDUMP, "CONSOLE-DUMP");
dcl_str(DEBUG_CONSOLE_KEYWORD_DNS, "CONSOLE-DNS");
dcl_str(DEBUG_CONSOLE_KEYWORD_DSPSTATE, "CONSOLE-DSPSTATE");
dcl_str(DEBUG_CONSOLE_USAGE_MEMORYDUMP, "CONSOLE-MEMORYDUMP: addr bytes [cnt blk [1/0 char output]]\n");
dcl_str(DEBUG_CONSOLE_BREAK, "CONSOLE-BREAK\r\n");

dcl_str(DEBUG_FUNCTION_ENTRY, "SIPCC-FUNC_ENTRY: LINE %d/%d: %-35s: %s <- %s\n");
dcl_str(DEBUG_FUNCTION_ENTRY2, "SIPCC-FUNC_ENTRY: LINE %d/%d: %-35s: %s <- %s(%d)\n");
dcl_str(DEBUG_SIP_ENTRY, "SIPCC-ENTRY: LINE %d/%d: %-35s: %s\n");
dcl_str(DEBUG_SIP_URL_ERROR, "SIPCC-%s: Error: URL is not SIP.\n");
dcl_str(DEBUG_LINE_NUMBER_INVALID, "SIPCC-LINE_NUM: %s: Error: Line number (%d) is invalid\n");
dcl_str(DEBUG_SIP_SPI_SEND_ERROR, "SIPCC-SPI_SEND_ERR: %s: Error: sipSPISendErrorResponse(%d) failed.\n");
dcl_str(DEBUG_SIP_SDP_CREATE_BUF_ERROR, "SIPCC-SDP_BUF: %s: Error: sipsdp_src_dest_create() returned null\n");
dcl_str(DEBUG_SIP_PARSE_SDP_ERROR, "SIPCC-SDP_PARSE: %s: Error: sdp_parse()\n");
dcl_str(DEBUG_SIP_FEATURE_UNSUPPORTED, "SIPCC-FEATURE: LINE %d/%d: %-35s: This feature is unsupported in the current state.\n");
dcl_str(DEBUG_SIP_DEST_SDP, "SIPCC-SDP_DEST: LINE %d/%d: %-35s: Process SDP: Dest=<%s>:<%d>\n");
dcl_str(DEBUG_SIP_MSG_SENDING_REQUEST, "SIPCC-MSG_SEND_REQ: %s: Sending %s...\n");
dcl_str(DEBUG_SIP_MSG_SENDING_RESPONSE, "SIPCC-MSG_SEND_RESP: %s: Sending response %d...\n");
dcl_str(DEBUG_SIP_MSG_RECV, "SIPCC-MSG_RECV: %s: Received SIP message %s.\n");
dcl_str(DEBUG_SIP_STATE_UNCHANGED, "SIPCC-SIP_STATE: LINE %d/%d: %-35s: State unchanged -> %s\n");
dcl_str(DEBUG_SIP_FUNCTIONCALL_FAILED, "SIPCC-FUNC_CALL: LINE %d/%d: %-35s: Error: %s returned error.\n");
dcl_str(DEBUG_SIP_BUILDFLAG_ERROR, "SIPCC-BUILD_FLAG: %s: Error: Build flag is not successful. Will not send message.\n");
dcl_str(DEBUG_GENERAL_FUNCTIONCALL_FAILED, "SIPCC-FUNC_CALL: %s: Error: %s returned error.\n");
dcl_str(DEBUG_GENERAL_SYSTEMCALL_FAILED, "SIPCC-SYS_CALL: %s: Error: %s failed: errno = %d\n");
dcl_str(DEBUG_GENERAL_FUNCTIONCALL_BADARGUMENT, "SIPCC-FUNC_CALL: %s: Error: invalid argument: %s\n");
dcl_str(DEBUG_FUNCTIONNAME_SIPPMH_PARSE_FROM, "SIPCC-FUNC_NAME: sippmh_parse_from_or_to(FROM)");
dcl_str(DEBUG_FUNCTIONNAME_SIPPMH_PARSE_TO, "SIPCC-FUNC_NAME: sippmh_parse_from_or_to(TO)");
dcl_str(DEBUG_FUNCTIONNAME_SIP_SM_REQUEST_CHECK_AND_STORE, "SIPCC-SM_REQ: sip_sm_request_check_and_store()");
dcl_str(DEBUG_SNTP_LI_ERROR, "SIPCC-SNTP: Leap indicator == 3\n");
dcl_str(DEBUG_SNTP_MODE_ERROR, "SIPCC-SNTP: Mode is not server (4/5) in response\n");
dcl_str(DEBUG_SNTP_STRATUM_ERROR, "SIPCC-SNTP: Invalid stratum > 15\n");
dcl_str(DEBUG_SNTP_TIMESTAMP_ERROR, "SIPCC-SNTP: Server did not echo our transmit timestamp\n");
dcl_str(DEBUG_SNTP_TIMESTAMP1, "SIPCC-SNTP: %-15s: 0x%08x %08x, ");
dcl_str(DEBUG_SNTP_TIMESTAMP2, "SIPCC-SNTP: (%+03d:%02d) %s");
dcl_str(DEBUG_SNTP_TIME_UPDATE, "SIPCC-SNTP: Updating date and time to:\n%s %lu %02lu:%02lu:%02lu "
 "%04lu, %s, week %lu and day %lu.  "
 "Daylight saving is: %d\n\n");
dcl_str(DEBUG_SNTP_TS_HEADER, "SIPCC-SNTP: Settings:\nMode        : %lu\nTimezone    "
 ": %d\nServer Addr : %s\nTimeStruct  : TZ/Offset: %d/%lu,"
 " AutoAdjust: %d\nMo  Day DoW WoM Time\n");
dcl_str(DEBUG_SNTP_TS_PRINT, "SIPCC-SNTP: %3lu %3lu %3lu %3lu %4lu\n");
dcl_str(DEBUG_SNTP_SOCKET_REOPEN, "SIPCC-SNTP: Re-opening listening port due to IP change\n");
dcl_str(DEBUG_SNTP_DISABLED, "SIPCC-SNTP: Unicast w/server addr 0.0.0.0, SNTP disabled\n");
dcl_str(DEBUG_SNTP_REQUEST, "SIPCC-SNTP: Sending NTP request packet [%s]\n");
dcl_str(DEBUG_SNTP_RESPONSE, "SIPCC-SNTP: Receiving NTP response packet\n");
dcl_str(DEBUG_SNTP_RETRANSMIT, "SIPCC-SNTP: Waiting %d msec to retransmit\n");
dcl_str(DEBUG_SNTP_UNICAST_MODE, "SIPCC-SNTP: Unicast mode [%s]\n");
dcl_str(DEBUG_SNTP_MULTICAST_MODE, "SIPCC-SNTP: Multicast/Directed broadcast mode [%s]\n");
dcl_str(DEBUG_SNTP_ANYCAST_MODE, "SIPCC-SNTP: Anycast mode [%s]\n");
dcl_str(DEBUG_SNTP_VALIDATION, "SIPCC-SNTP: Dropping unauthorized SNTP response: %s\n");
dcl_str(DEBUG_SNTP_VALIDATION_PACKET, "SIPCC-SNTP: Semantic check failed for NTP packet\n");
dcl_str(DEBUG_SNTP_WRONG_SERVER, "SIPCC-SNTP: Unauthorized server");
dcl_str(DEBUG_SNTP_NO_REQUEST, "SIPCC-SNTP: No request sent");
dcl_str(DEBUG_SNTP_ANYCAST_RESET, "SIPCC-SNTP: Reset to Unicast to server: %s\n");
dcl_str(DEBUG_SOCKET_UDP_RTP, "SIPCC-SOC_TASK: UDP_RTP event received.\n");
dcl_str(DEBUG_MAC_PRINT, "SIPCC-MAC_PRINT: %04x:%04x:%04x");
dcl_str(DEBUG_IP_PRINT, "SIPCC-IP_PRINT: %u.%u.%u.%u");
dcl_str(DEBUG_SYSBUF_UNAVAILABLE, "SIPCC-SYS_BUF: %s: Error: IRXLstGet() failed\n");
dcl_str(DEBUG_MSG_BUFFER_TOO_BIG, "SIPCC-MSG_BUF: %s: Error: Args Check: message buffer length (%d) too big.\n");
dcl_str(DEBUG_UNKNOWN_TIMER_BLOCK, "SIPCC-TIMER: %s: Error: Unknown timer block\n");
dcl_str(DEBUG_CREDENTIALS_BAG_CORRUPTED, "SIPCC-CRED: %-35s: Error: credentials bags corrupted");
dcl_str(DEBUG_INPUT_NULL, "SIPCC-INPUT: %s: Error: Input is null\n");
dcl_str(DEBUG_INPUT_EMPTY, "SIPCC-INPUT: %s: Error: Input is empty\n");
dcl_str(DEBUG_STRING_DUP_FAILED, "SIPCC-STR_DUP: %s: Unable to duplicate string.\n");
dcl_str(DEBUG_PARSER_STRING_TOO_LARGE, "SIPCC-PARSE: Parse error: string too big (%d,%d)\n");
dcl_str(DEBUG_PARSER_NULL_KEY_TABLE, "SIPCC-PARSE: Parse error: NULL key table passed into parser\n");
dcl_str(DEBUG_PARSER_UNKNOWN_KEY, "SIPCC-PARSE: Parse error: Unknown key name: %s\n");
dcl_str(DEBUG_PARSER_UNKNOWN_KEY_ENUM, "SIPCC-PARSE: Print error: Unknown key enum: %d\n");
dcl_str(DEBUG_PARSER_INVALID_START_VAR, "SIPCC-PARSE: Parse error: Invalid start variable ch=0x%02x(%c)\n");
dcl_str(DEBUG_PARSER_INVALID_VAR_CHAR, "SIPCC-PARSE: Parse error: Invalid variable ch=0x%02x(%c)\n");
dcl_str(DEBUG_PARSER_MISSING_COLON, "SIPCC-PARSE: Parse error: Missing colon separator\n");
dcl_str(DEBUG_PARSER_NO_VALUE, "SIPCC-PARSE: Parse error: no value for variable\n");
dcl_str(DEBUG_PARSER_EARLY_EOL, "SIPCC-PARSE: Parse error: early EOL for value\n");
dcl_str(DEBUG_PARSER_INVALID_VAR_NAME, "SIPCC-PARSE: Parse error: parse_var_name failed: %d\n");
dcl_str(DEBUG_PARSER_INVALID_VAR_VALUE, "SIPCC-PARSE: Parse error: var: %s  parse_var_value failed: %d\n");
dcl_str(DEBUG_PARSER_UNKNOWN_VAR, "SIPCC-PARSE: Parse error: var: %s not found in table\n");
dcl_str(DEBUG_PARSER_NAME_VALUE, "SIPCC-PARSE: Name: [%s]  Value: [%s]\n");
dcl_str(DEBUG_PARSER_UNKNOWN_NAME_VALUE, "SIPCC-PARSE: Parse error: Name: [%s] Value: [%s] rc:%d\n");
dcl_str(DEBUG_PARSER_UNKNOWN_ERROR, "SIPCC-PARSE: Default error: Name: [%s] Value: [%s] rc:%d\n");
dcl_str(DEBUG_PARSER_NUM_ERRORS, "SIPCC-PARSE: Parse error: %d Errors found\n");
dcl_str(DEBUG_PARSER_SET_DEFAULT, "SIPCC-PARSE: Parser Info: Setting var: %s to default value: %s\n\n");
dcl_str(DEBUG_SDP_ERROR_BODY_FIELD, "SIPCC-SDP: \n%s: Error in one of the SDP body fields \n");
dcl_str(DEBUG_UDP_OPEN_FAIL, "SIPCC-UDP: %s: UdpOpen(R IP=%d, R Port=%d, L Port=%d) failed\n");
dcl_str(DEBUG_UDP_PAYLOAD_TOO_LARGE, "SIPCC-UDP: %s: Error: payload size=<%d> > allowed size=<%d>\n");
dcl_str(DEBUG_RTP_TRANSPORT, "SIPCC-RTP: %s: transport= %d\n");
dcl_str(DEBUG_RTP_INVALID_VOIP_TYPE, "SIPCC-RTP: %s: Error: Unexpected voipCodec_t type: <%d>\n");
dcl_str(DEBUG_RTP_INVALID_RTP_TYPE, "SIPCC-RTP: %s: Error: Unexpected rtp_ptype in SDP body: <%d>\n");
dcl_str(DEBUG_MEMORY_ALLOC, "SIPCC-MEM: Malloc Addr:0x%lx, Size:%d\n");
dcl_str(DEBUG_MEMORY_FREE, "SIPCC-MEM: Free Addr:0x%lx, Size:%d\n");
dcl_str(DEBUG_MEMORY_MALLOC_ERROR, "SIPCC-MEM: 0x%lx:Malloc error for size %d\n");
dcl_str(DEBUG_MEMORY_REALLOC_ERROR, "SIPCC-MEM: %s: Error: malloc_tagged() returned null.\n");
dcl_str(DEBUG_MEMORY_OUT_OF_MEM, "SIPCC-MEM: %s: Error: malloc failed\n");
dcl_str(DEBUG_MEMORY_ENTRY, "SIPCC-MEM: >> Used: %1d size: %6d addr:0x%08x\n");
dcl_str(DEBUG_MEMORY_SUMMARY, "===== MEMORY MAP START =====\n"
 "free blocks : %6d, free block space:%6d, largest free block: %6d\n"
 "used blocks : %6d, used block space:%6d, largest used block: %6d\n"
 "wasted block: %6d, str_lib space   :%6d\n"
 "used space excluding str_lib space :%6d\n \n"
 "=====  MEMORY MAP END  =====\n");
dcl_str(DEBUG_MEMORY_ADDRESS_HEADER, "SIPCC-MEM: 0x%08x: ");
dcl_str(DEBUG_MEMORY_DUMP, "SIPCC-MEM: DUMP: 0x%08x - 0x%08x\n");
dcl_str(DEBUG_DNS_GETHOSTBYNAME, "SIPCC-DNS: gethostbyname('%s',%08x,%d,%d)\n");
dcl_str(DEBUG_PMH_INCORRECT_SYNTAX, "SIPCC-PMH: INCORRECT SYNTAX");
dcl_str(DEBUG_PMH_INVALID_FIELD_VALUE, "SIPCC-PMH: INVALID FIELD VALUE");
dcl_str(DEBUG_PMH_INVALID_SCHEME, "SIPCC-PMH: INVALID SCHEME");
dcl_str(DEBUG_PMH_UNKNOWN_SCHEME, "SIPCC-PMH: UNKNOWN SCHEME");
dcl_str(DEBUG_PMH_NOT_ENOUGH_PARAMETERS, "SIPCC-PMH: NOT ENOUGH PARAMETERS");
dcl_str(DEBUG_REG_DISABLED, "SIPCC-REG: LINE %d/%d: %-35s: registration disabled\n");
dcl_str(DEBUG_REG_PROXY_EXPIRES, "SIPCC-REG: LINE %d/%d: %-35s: Using proxy expires value\n");
dcl_str(DEBUG_REG_SIP_DATE, "SIPCC-REG: LINE %d/%d: %-35s: SIP-date= %s\n");
dcl_str(DEBUG_REG_SIP_RESP_CODE, "SIPCC-REG: LINE %d/%d: %-35s: Error: SIP response code\n");
dcl_str(DEBUG_REG_SIP_RESP_FAILURE, "SIPCC-REG: LINE %d/%d: %-35s: SIP failure %d resp\n");
dcl_str(DEBUG_REG_INVALID_LINE, "SIPCC-REG: %-35s: Line %d: Invalid line\n");
dcl_str(CC_NO_MSG_BUFFER, "SIPCC-MSG_BUF: %s : no msg buffer available\n");
dcl_str(CC_SEND_FAILURE, "SIPCC-MSG_SEND: %s : unable to send msg\n");
dcl_str(GSM_UNDEFINED, "SIPCC-GSM: UNDEFINED");
dcl_str(GSM_DBG_PTR, "SIPCC-GSM_DBG_PTR: %s %-4d: %-35s: %s= %p\n");
dcl_str(GSM_FUNC_ENTER, "SIPCC-GSM_FUNC_ENT: %s %-4d: %-35s\n");
dcl_str(GSM_DBG1, "SIPCC-GSM: %s %-4d: %-35s: %s\n");
dcl_str(FSM_DBG_SM_DEFAULT_EVENT, "SIPCC-FSM: default - ignoring.\n");
dcl_str(FSM_DBG_SM_FTR_ENTRY, "SIPCC-FSM: feature= %s, src= %s\n");
dcl_str(FSM_DBG_FAC_ERR, "SIPCC-FSM_FAC_ERR: %-4d: %-35s:\n    %s, rc= %s\n");
dcl_str(FSM_DBG_FAC_FOUND, "SIPCC-FSM: %-4d: %-35s: facility found(%d)\n");
dcl_str(FSM_DBG_IGNORE_FTR, "SIPCC-FSM: %s %-4d: %8d: ignoring feature= %s\n");
dcl_str(FSM_DBG_IGNORE_SRC, "SIPCC-FSM: %s %-4d: %8d: ignoring src= %s\n");
dcl_str(FSM_DBG_CHANGE_STATE, "SIPCC-FSM: %s %-4d: %8d: %s -> %s\n");
dcl_str(FSM_DBG_SDP_BUILD_ERR, "SIPCC-FSM: Unable to build SDP. \n");
dcl_str(FSMDEF_DBG_PTR, "SIPCC-FSM: DEF %-4d/%d: %-35s: dcb= %p\n");
dcl_str(FSMDEF_DBG1, "SIPCC-FSM: DEF %-4d/%d: %-35s: %s\n");
dcl_str(FSMDEF_DBG2, "SIPCC-FSM: DEF %-4d/%d: %-35s: %s %d\n");
dcl_str(FSMDEF_DBG_SDP, "SIPCC-FSM: DEF %-4d/%d: %-35s: addr= %s, port= %d,\n    media_type(s)=");
dcl_str(FSMDEF_DBG_CLR_SPOOF_APPLD, "SIPCC-FSM: DEF %-4d/%d: %-35s: clearing spoof_ringout_applied.\n");
dcl_str(FSMDEF_DBG_CLR_SPOOF_RQSTD, "SIPCC-FSM: DEF %-4d/%d: %-35s: clearing spoof_ringout_requested.\n");
dcl_str(FSMDEF_DBG_INVALID_DCB, "SIPCC-FSM: DEF 0   : %-35s: invalid dcb\n");
dcl_str(FSMDEF_DBG_FTR_REQ_ACT, "SIPCC-FSM: DEF %-4d/%d: feature requested %s but %s is active.\n");
dcl_str(FSMDEF_DBG_TMR_CREATE_FAILED, "SIPCC-FSM: DEF %-4d/%d: %-35s: %s:\n    cprCreateTimer failed.\n");
dcl_str(FSMDEF_DBG_TMR_START_FAILED, "SIPCC-FSM: DEF %-4d/%d: %-35s: %s:\n    cprStartTimer failed, errno= %d.\n");
dcl_str(FSMDEF_DBG_TMR_CANCEL_FAILED, "SIPCC-FSM: DEF %-4d/%d: %-35s: %s:\n    cprCancelTimer failed, errno= %d.\n");
dcl_str(FSMXFR_DBG_XFR_INITIATED, "SIPCC-FSM: XFR %-4d/%d/%d: %8d: xfer initiated\n");
dcl_str(FSMXFR_DBG_PTR, "SIPCC-FSM: XFR %-4d/%d/%d: %-35s: xcb= %p\n");
dcl_str(FSMCNF_DBG_CNF_INITIATED, "SIPCC-FSM: CNF %-4d/%d/%d: %8d: conf initiated\n");
dcl_str(FSMCNF_DBG_PTR, "SIPCC-FSM: CNF %-4d/%d/%d: %-35s: ncb= %p\n");
dcl_str(FSMB2BCNF_DBG_CNF_INITIATED, "SIPCC-FSM: B2BCNF %-4d/%d/%d: %8d: b2bconf initiated\n");
dcl_str(FSMB2BCNF_DBG_PTR, "SIPCC-FSM: B2BCNF %-4d/%d/%d: %-35s: ncb= %p\n");
dcl_str(FSMSHR_DBG_BARGE_INITIATED, "SIPCC-FSM: SHR %-4d/%d/%d: %8d: Barge initiated\n");
dcl_str(LSM_DBG_ENTRY, "SIPCC-LSM: %-4d/%d: %-35s\n");
dcl_str(LSM_DBG_INT1, "SIPCC-LSM: %-4d/%d: %-35s: %s= %d\n");
dcl_str(LSM_DBG_CC_ERROR, "SIPCC-LSM: %-4d/%d: %-35s: (%d:%p) failure\n");
dcl_str(VCM_DEBUG_ENTRY, "SIPCC-VCM: %-4d: %-35s\n");
dcl_str(SM_PROCESS_EVENT_ERROR, "SIPCC-SM: %s: Error: sip_sm_process_event() returned error processing %d\n");
dcl_str(REG_SM_PROCESS_EVENT_ERROR, "SIPCC-SM: %s: Error: sip_reg_sm_process_event() returned error processing %d\n");
dcl_str(DEBUG_END, "SIPCC-END: \0");

/*
 * Debug string table NOT subject to localization
 */
debug_string_table_entry debug_string_table [] = {
    {0},                                            // DEBUG_START
    {use_str(DEBUG_SEPARATOR_BAR)},                 // DEBUG_SEPARATOR_BAR
    {use_str(DEBUG_CONSOLE_PASSWORD)},              // DEBUG_CONSOLE_PASSWORD
    {use_str(DEBUG_CONSOLE_KEYWORD_CONSOLE_STALL)}, // DEBUG_CONSOLE_KEYWORD_CONSOLE_STALL
    {use_str(DEBUG_CONSOLE_KEYWORD_MEMORYMAP)},     // DEBUG_CONSOLE_KEYWORD_MEMORYMAP
    {use_str(DEBUG_CONSOLE_KEYWORD_MALLOCTABLE)},   // DEBUG_CONSOLE_KEYWORD_MALLOCTABLE
    {use_str(DEBUG_CONSOLE_KEYWORD_MEMORYDUMP)},    // DEBUG_CONSOLE_KEYWORD_MEMORYDUMP
    {use_str(DEBUG_CONSOLE_KEYWORD_DNS)},           // DEBUG_CONSOLE_KEYWORD_DNS
    {use_str(DEBUG_CONSOLE_KEYWORD_DSPSTATE)},      // DEBUG_CONSOLE_KEYWORD_DSPSTATE
    {use_str(DEBUG_CONSOLE_USAGE_MEMORYDUMP)},      // DEBUG_CONSOLE_USAGE_MEMORYDUMP
    {use_str(DEBUG_CONSOLE_BREAK)},                 // DEBUG_CONSOLE_BREAK
    {use_str(DEBUG_FUNCTION_ENTRY)},                // DEBUG_FUNCTION_ENTRY
    {use_str(DEBUG_FUNCTION_ENTRY2)},               // DEBUG_FUNCTION_ENTRY2
    {use_str(DEBUG_SIP_ENTRY)},                     // DEBUG_SIP_ENTRY
    {use_str(DEBUG_SIP_URL_ERROR)},                 // DEBUG_SIP_URL_ERROR
    {use_str(DEBUG_LINE_NUMBER_INVALID)},           // DEBUG_LINE_NUMBER_INVALID
    {use_str(DEBUG_SIP_SPI_SEND_ERROR)},            // DEBUG_SIP_SPI_SEND_ERROR
    {use_str(DEBUG_SIP_SDP_CREATE_BUF_ERROR)},      // DEBUG_SIP_SDP_CREATE_BUF_ERROR
    {use_str(DEBUG_SIP_PARSE_SDP_ERROR)},           // DEBUG_SIP_PARSE_SDP_ERROR
    {use_str(DEBUG_SIP_FEATURE_UNSUPPORTED)},       // DEBUG_SIP_FEATURE_UNSUPPORTED
    {use_str(DEBUG_SIP_DEST_SDP)},                  // DEBUG_SIP_DEST_SDP
    {use_str(DEBUG_SIP_MSG_SENDING_REQUEST)},       // DEBUG_SIP_MSG_SENDING_REQUEST
    {use_str(DEBUG_SIP_MSG_SENDING_RESPONSE)},      // DEBUG_SIP_MSG_SENDING_RESPONSE
    {use_str(DEBUG_SIP_MSG_RECV)},                  // DEBUG_SIP_MSG_RECV
    {use_str(DEBUG_SIP_STATE_UNCHANGED)},           // DEBUG_SIP_STATE_UNCHANGED
    {use_str(DEBUG_SIP_FUNCTIONCALL_FAILED)},       // DEBUG_SIP_FUNCTIONCALL_FAILED
    {use_str(DEBUG_SIP_BUILDFLAG_ERROR)},           // DEBUG_SIP_BUILDFLAG_ERROR
    {use_str(DEBUG_GENERAL_FUNCTIONCALL_FAILED)},   // DEBUG_GENERAL_FUNCTIONCALL_FAILED
    {use_str(DEBUG_GENERAL_SYSTEMCALL_FAILED)},     // DEBUG_GENERAL_SYSTEMCALL_FAILED
    {use_str(DEBUG_GENERAL_FUNCTIONCALL_BADARGUMENT)}, // DEBUG_GENERAL_FUNCTIONCALL_BADARGUMENT
    {use_str(DEBUG_FUNCTIONNAME_SIPPMH_PARSE_FROM)},// DEBUG_FUNCTIONNAME_SIP_PARSE_FROM
    {use_str(DEBUG_FUNCTIONNAME_SIPPMH_PARSE_TO)},  // DEBUG_FUNCTIONNAME_SIP_PARSE_TO
    {use_str(DEBUG_FUNCTIONNAME_SIP_SM_REQUEST_CHECK_AND_STORE)}, // DEBUG_FUNCTIONNAME_SIP_SM_REQUEST_CHECK_AND_STORE
    {use_str(DEBUG_SNTP_LI_ERROR)},                 // DEBUG_SNTP_LI_ERROR
    {use_str(DEBUG_SNTP_MODE_ERROR)},               // DEBUG_SNTP_MODE_ERROR
    {use_str(DEBUG_SNTP_STRATUM_ERROR)},            // DEBUG_SNTP_STRATUM_ERROR
    {use_str(DEBUG_SNTP_TIMESTAMP_ERROR)},          // DEBUG_SNTP_TIMESTAMP_ERROR
    {use_str(DEBUG_SNTP_TIMESTAMP1)},               // DEBUG_SNTP_TIMESTAMP1
    {use_str(DEBUG_SNTP_TIMESTAMP2)},               // DEBUG_SNTP_TIMESTAMP2
    {use_str(DEBUG_SNTP_TIME_UPDATE)},              // DEBUG_SNTP_TIME_UPDATE
    {use_str(DEBUG_SNTP_TS_HEADER)},                // DEBUG_SNTP_TS_HEADER
    {use_str(DEBUG_SNTP_TS_PRINT)},                 // DEBUG_SNTP_TS_PRINT
    {use_str(DEBUG_SNTP_SOCKET_REOPEN)},            // DEBUG_SNTP_SOCKET_REOPEN
    {use_str(DEBUG_SNTP_DISABLED)},                 // DEBUG_SNTP_DISABLED
    {use_str(DEBUG_SNTP_REQUEST)},                  // DEBUG_SNTP_REQUEST
    {use_str(DEBUG_SNTP_RESPONSE)},                 // DEBUG_SNTP_RESPONSE
    {use_str(DEBUG_SNTP_RETRANSMIT)},               // DEBUG_SNTP_RETRANSMIT
    {use_str(DEBUG_SNTP_UNICAST_MODE)},             // DEBUG_SNTP_UNICAST_MODE
    {use_str(DEBUG_SNTP_MULTICAST_MODE)},           // DEBUG_SNTP_MULTICAST_MODE
    {use_str(DEBUG_SNTP_ANYCAST_MODE)},             // DEBUG_SNTP_ANYCAST_MODE
    {use_str(DEBUG_SNTP_VALIDATION)},               // DEBUG_SNTP_VALIDATION
    {use_str(DEBUG_SNTP_VALIDATION_PACKET)},        // DEBUG_SNTP_VALIDATION_PACKET
    {use_str(DEBUG_SNTP_WRONG_SERVER)},             // DEBUG_SNTP_WRONG_SERVER
    {use_str(DEBUG_SNTP_NO_REQUEST)},               // DEBUG_SNTP_NO_REQUEST
    {use_str(DEBUG_SNTP_ANYCAST_RESET)},            // DEBUG_SNTP_ANYCAST_RESET
    {use_str(DEBUG_SOCKET_UDP_RTP)},                // DEBUG_SOCKET_UDP_RTP
    {use_str(DEBUG_MAC_PRINT)},                     // DEBUG_MAC_PRINT
    {use_str(DEBUG_IP_PRINT)},                      // DEBUG_IP_PRINT
    {use_str(DEBUG_SYSBUF_UNAVAILABLE)},            // DEBUG_SYSBUF_UNAVAILABLE
    {use_str(DEBUG_MSG_BUFFER_TOO_BIG)},            // DEBUG_MSG_BUFFER_TOO_BIG
    {use_str(DEBUG_UNKNOWN_TIMER_BLOCK)},           // DEBUG_UNKNOWN_TIMER_BLOCK
    {use_str(DEBUG_CREDENTIALS_BAG_CORRUPTED)},     // DEBUG_CREDENTIALS_BAG_CORRUPTED
    {use_str(DEBUG_INPUT_NULL)},                    // DEBUG_INPUT_NULL
    {use_str(DEBUG_INPUT_EMPTY)},                   // DEBUG_INPUT_EMPTY
    {use_str(DEBUG_STRING_DUP_FAILED)},             // DEBUG_STRING_DUP_FAILED
    {use_str(DEBUG_PARSER_STRING_TOO_LARGE)},       // DEBUG_PARSER_STRING_TOO_LARGE
    {use_str(DEBUG_PARSER_NULL_KEY_TABLE)},         // DEBUG_PARSER_NULL_KEY_TABLE
    {use_str(DEBUG_PARSER_UNKNOWN_KEY)},            // DEBUG_PARSER_UNKNOWN_KEY
    {use_str(DEBUG_PARSER_UNKNOWN_KEY_ENUM)},       // DEBUG_PARSER_UNKNOWN_KEY_ENUM
    {use_str(DEBUG_PARSER_INVALID_START_VAR)},      // DEBUG_PARSER_INVALID_START_VAR
    {use_str(DEBUG_PARSER_INVALID_VAR_CHAR)},       // DEBUG_PARSER_INVALID_VAR_CHAR
    {use_str(DEBUG_PARSER_MISSING_COLON)},          // DEBUG_PARSER_MISSING_COLON
    {use_str(DEBUG_PARSER_NO_VALUE)},               // DEBUG_PARSER_NO_VALUE
    {use_str(DEBUG_PARSER_EARLY_EOL)},              // DEBUG_PARSER_EARLY_EOL
    {use_str(DEBUG_PARSER_INVALID_VAR_NAME)},       // DEBUG_PARSER_INVALID_VAR_NAME
    {use_str(DEBUG_PARSER_INVALID_VAR_VALUE)},      // DEBUG_PARSER_INVALID_VAR_VALUE
    {use_str(DEBUG_PARSER_UNKNOWN_VAR)},            // DEBUG_PARSER_UNKNOWN_VAR
    {use_str(DEBUG_PARSER_NAME_VALUE)},             // DEBUG_PARSER_NAME_VALUE
    {use_str(DEBUG_PARSER_UNKNOWN_NAME_VALUE)},     // DEBUG_PARSER_UNKNOWN_NAME_VALUE
    {use_str(DEBUG_PARSER_UNKNOWN_ERROR)},          // DEBUG_PARSER_UNKNOWN_ERROR
    {use_str(DEBUG_PARSER_NUM_ERRORS)},             // DEBUG_PARSER_NUM_ERRORS
    {use_str(DEBUG_PARSER_SET_DEFAULT)},            // DEBUG_PARSER_SET_DEFAULT
    {use_str(DEBUG_SDP_ERROR_BODY_FIELD)},          // DEBUG_SDP_ERROR_BODY_FIELD
    {use_str(DEBUG_UDP_OPEN_FAIL)},                 // DEBUG_UDP_OPEN_FAIL
    {use_str(DEBUG_UDP_PAYLOAD_TOO_LARGE)},         // DEBUG_UDP_PAYLOAD_TOO_LARGE
    {use_str(DEBUG_RTP_TRANSPORT)},                 // DEBUG_RTP_TRANSPORT
    {use_str(DEBUG_RTP_INVALID_VOIP_TYPE)},         // DEBUG_RTP_INVALID_VOIP_TYPE
    {use_str(DEBUG_RTP_INVALID_RTP_TYPE)},          // DEBUG_RTP_INVALID_RTP_TYPE
    {use_str(DEBUG_MEMORY_ALLOC)},                  // DEBUG_MEMORY_ALLOC
    {use_str(DEBUG_MEMORY_FREE)},                   // DEBUG_MEMORY_FREE
    {use_str(DEBUG_MEMORY_MALLOC_ERROR)},           // DEBUG_MEMORY_MALLOC_ERROR
    {use_str(DEBUG_MEMORY_REALLOC_ERROR)},          // DEBUG_MEMORY_REALLOC_ERROR
    {use_str(DEBUG_MEMORY_OUT_OF_MEM)},             // DEBUG_MEMORY_OUT_OF_MEM
    {use_str(DEBUG_MEMORY_ENTRY)},                  // DEBUG_MEMORY_ENTRY
    {use_str(DEBUG_MEMORY_SUMMARY)},                // DEBUG_MEMORY_SUMMARY
    {use_str(DEBUG_MEMORY_ADDRESS_HEADER)},         // DEBUG_MEMORY_ADDRESS_HEADER
    {use_str(DEBUG_MEMORY_DUMP)},                   // DEBUG_MEMORY_DUMP
    {use_str(DEBUG_DNS_GETHOSTBYNAME)},             // DEBUG_DNS_GETHOSTBYNAME
    {use_str(DEBUG_PMH_INCORRECT_SYNTAX)},          // DEBUG_PMH_INCORRECT_SYNTAX
    {use_str(DEBUG_PMH_INVALID_FIELD_VALUE)},       // DEBUG_PMH_INVALID_FIELD_VALUE
    {use_str(DEBUG_PMH_INVALID_SCHEME)},            // DEBUG_PMH_INVALID_SCHEME
    {use_str(DEBUG_PMH_UNKNOWN_SCHEME)},            // DEBUG_PMH_UNKNOWN_SCHEME
    {use_str(DEBUG_PMH_NOT_ENOUGH_PARAMETERS)},     // DEBUG_PMH_NOT_ENOUGH_PARAMETERS
    {use_str(DEBUG_REG_DISABLED)},                  // DEBUG_REG_DISABLED
    {use_str(DEBUG_REG_PROXY_EXPIRES)},             // DEBUG_REG_PROXY_EXPIRES
    {use_str(DEBUG_REG_SIP_DATE)},                  // DEBUG_REG_SIP_DATE
    {use_str(DEBUG_REG_SIP_RESP_CODE)},             // DEBUG_REG_SIP_RESP_CODE
    {use_str(DEBUG_REG_SIP_RESP_FAILURE)},          // DEBUG_REG_SIP_RESP_FAILURE
    {use_str(DEBUG_REG_INVALID_LINE)},              // DEBUG_REG_INVALID_LINE
    {use_str(CC_NO_MSG_BUFFER)},                    // CC_NO_MSG_BUFFER^M
    {use_str(CC_SEND_FAILURE)},                     // CC_SEND_FAILURE^M
    {use_str(GSM_UNDEFINED)},                       // GSM_UNDEFINED
    {use_str(GSM_DBG_PTR)},                         // GSM_DBG_PTR
    {use_str(GSM_FUNC_ENTER)},                      // GSM_FUNC_ENTER
    {use_str(GSM_DBG1)},                            // GSM_DBG1
    {use_str(FSM_DBG_SM_DEFAULT_EVENT)},            // FSM_DBG_SM_DEFAULT_EVENT
    {use_str(FSM_DBG_SM_FTR_ENTRY)},                // FSM_DBG_SM_FTR_ENTRY
    {use_str(FSM_DBG_FAC_ERR)},                     // FSM_DBG_FAC_ERR
    {use_str(FSM_DBG_FAC_FOUND)},                   // FSM_DBG_FAC_FOUND
    {use_str(FSM_DBG_IGNORE_FTR)},                  // FSM_DBG_IGNORE_FTR
    {use_str(FSM_DBG_IGNORE_SRC)},                  // FSM_DBG_IGNORE_SRC
    {use_str(FSM_DBG_CHANGE_STATE)},                // FSM_DBG_CHANGE_STATE
    {use_str(FSM_DBG_SDP_BUILD_ERR)},               // FSM_DBG_SDP_BUILD_ERR
    {use_str(FSMDEF_DBG_PTR)},                      // FSMDEF_DBG_PTR
    {use_str(FSMDEF_DBG1)},                         // FSMDEF_DBG1
    {use_str(FSMDEF_DBG2)},                         // FSMDEF_DBG2
    {use_str(FSMDEF_DBG_SDP)},                      // FSMDEF_DBG_SDP
    {use_str(FSMDEF_DBG_CLR_SPOOF_APPLD)},          // FSMDEF_DBG_CLEAR_SPOOF
    {use_str(FSMDEF_DBG_CLR_SPOOF_RQSTD)},          // FSMDEF_DBG_CLEAR_SPOOF
    {use_str(FSMDEF_DBG_INVALID_DCB)},              // FSMDEF_DBG_INVALID_DCB
    {use_str(FSMDEF_DBG_FTR_REQ_ACT)},              // FSMDEF_DBG_FTR_REQ_ACT
    {use_str(FSMDEF_DBG_TMR_CREATE_FAILED)},        // FSMDEF_DBG_TMR_CREATE_FAILED
    {use_str(FSMDEF_DBG_TMR_START_FAILED)},         // FSMDEF_DBG_TMR_START_FAILED
    {use_str(FSMDEF_DBG_TMR_CANCEL_FAILED)},        // FSMDEF_DBG_TMR_CANCEL_FAILED
    {use_str(FSMXFR_DBG_XFR_INITIATED)},            // FSMXFR_DBG_XFR_INITIATED
    {use_str(FSMXFR_DBG_PTR)},                      // FSMXFR_DBG_PTR
    {use_str(FSMCNF_DBG_CNF_INITIATED)},            // FSMCNF_DBG_CNF_INITIATED
    {use_str(FSMCNF_DBG_PTR)},                      // FSMCNF_DBG_PTR
    {use_str(FSMB2BCNF_DBG_CNF_INITIATED)},         // FSMB2BCNF_DBG_CNF_INITIATED
    {use_str(FSMB2BCNF_DBG_PTR)},                   // FSMB2BCNF_DBG_PTR
    {use_str(FSMSHR_DBG_BARGE_INITIATED)},          // FSMSHR_DBG_BARGE_INITIATED
    {use_str(LSM_DBG_ENTRY)},                       // LSM_DBG_ENTRY
    {use_str(LSM_DBG_INT1)},                        // LSM_DBG_INT1
    {use_str(LSM_DBG_CC_ERROR)},                    // LSM_DBG_CC_ERROR
    {use_str(VCM_DEBUG_ENTRY)},                     // VCM_DEBUG_ENTRY
    {use_str(SM_PROCESS_EVENT_ERROR)},              // SM_PROCESS_EVENT_ERROR
    {use_str(REG_SM_PROCESS_EVENT_ERROR)},          // REG_SM_PROCESS_EVENT_ERROR
    {0},
};



//************************************************************************

/*
 * Phrase string table subject to localization
 */
tnp_phrase_index_str_table_entry tnp_phrase_index_str_table [] = {
    {"\0"},                             // LOCALE_START
    {"\x80\x13"},                       // IDLE_PROMPT sccp 119
    {"Anonymous"},                      // ANONYMOUS - used for messaging; keep English
    {"\x80\x1e"},                       // CALL_PROCEEDING_IN
    {"\x80\x1e"},                       // CALL_PROCEEDING_OUT
    {"\x80\x16"},                       // CALL_ALERTING sccp 122
    {"\x80\x1b"},                       // CALL_ALERTING_SECONDARY sccp 127
    {"\x80\x16"},                       // CALL_ALERTING_LOCAL
    {"\x80\x18"},                       // CALL_CONNECTED sccp 124
    {"\x80\x03"},                       // CALL_INITIATE_HOLD sccp 103 or phone 786?
    {"\x80\x20"},                       // PROMPT_DIAL sccp 132
    {"\x80\x19"},                       // LINE_BUSY sccp 125
    {"\x80\x1b"},                       // CALL_WAITING
    {"\x80\x52"},                       // TRANSFER_FAILED sccp 182
    {"\x80\x26"},                       // Cannot complete the b2b conf
    {"\x80\x34"},                       // UI_CONFERENCE
    {"\x80\x38"},                       // UI_UNKNOWN
    {"\x80\x1f"},                       // REMOTE_IN_USE
    {"\x80\x55"},                       // NUM_NOT_CONFIGURED
    {"\x80\x17"},                       // UI_FROM
    {"\x80\x29"},                       // INVALID_CONF_PARTICIPANT
    {"\x80\x36"},                       // UI_PRIVATE
    {"\0"}                              // LOCALE_END
};
