/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
