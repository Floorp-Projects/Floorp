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

#ifndef _LOGMSG_INCLUDED_H
#define _LOGMSG_INCLUDED_H

#include "logger.h"

/*
 * Config messages
 * The number denotes the string location in the Phone dictionary file
 */
#define LOG_CFG_PARSE_DIAL 1074 //"%d Error(s) Parsing: %s"

/*
 * SIP messages
 * The number denotes the string location in the Phone dictionary file
 */
#define LOG_REG_MSG             1058 //"REG send failure: REGISTER"
#define LOG_REG_RED_MSG         1059 //"REG send failure: REGISTER Redirected"
#define LOG_REG_AUTH_MSG        1060 //"REG send failure: REGISTER auth"
#define LOG_REG_AUTH_HDR_MSG    1061 //"REG send failure: REGISTER auth hdr"
#define LOG_REG_AUTH_SCH_MSG    1062 //"REG send failure: REGISTER auth scheme"
#define LOG_REG_CANCEL_MSG      1063 //"REG send failure: CANCEL REGISTER"

#define LOG_REG_AUTH            1064 //"REG auth failed: %s"
#define LOG_REG_AUTH_ACK_TMR    1065 //"REG auth failed: ack timer"
#define LOG_REG_AUTH_NO_CRED    1066 //"REG auth failed: no more credentials"
#define LOG_REG_AUTH_UNREG_TMR  1067 //"REG auth failed: unreg ack timer"
#define LOG_REG_RETRY           1068 //"REG retries exceeded"
#define LOG_REG_UNSUPPORTED     1069 //"REG msg unsupported: %s"

#define LOG_REG_AUTH_SERVER_ERR 1070 //"REG auth failed: in %d, server error"
#define LOG_REG_AUTH_GLOBAL_ERR 1071 //"REG auth failed: in %d, global error"
#define LOG_REG_AUTH_UNKN_ERR   1072 //"REG auth failed: in %d, ??? error"

#define LOG_REG_BACKUP          1073 //"REG Not Registered to Backup Proxy"

#define LOG_REG_EXPIRE          1076 //"REG Expires time too small"

#endif /* _LOGMSG_INCLUDED_H */
