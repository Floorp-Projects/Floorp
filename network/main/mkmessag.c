/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "rosetta.h"
#include "mkutils.h"
#include "mktcp.h"
#include "gui.h"
#include "merrors.h"
#include "xpgetstr.h"
/*
 * hack alert: this file contains a switch statement with a gazillion cases,
 * so I'd prefer to use enums instead of extern ints in this case -- erik
 */
#define WANT_ENUM_STRING_IDS
#include "allxpstr.h"

#ifndef XP_UNIX
#include <stdarg.h>
#endif /* !XP_UNIX */


/*
 * This function takes an error code and associated error data
 * and creates a string containing a textual description of
 * what the error is and why it happened.
 *
 * The returned string is allocated and thus should be freed
 * once it has been used.
 */
PUBLIC char *
NET_ExplainErrorDetails (int code, ...)
{
  va_list args;
  char *msg = 0;
  int sub_error;

  va_start (args, code);

  if (IS_SSL_ERROR(code) || IS_SEC_ERROR(code)) {
	  const char *s = XP_GetString(code);
	  msg = (s ? XP_STRDUP(s) : 0);
  }

  if (!msg)
    switch(code) {
	case MK_INTERRUPTED:
	case MK_USE_FTP_INSTEAD:
	case MK_USE_COPY_FROM_CACHE:
	case MK_MAILTO_NOT_READY:
	case MK_UNABLE_TO_LOGIN:
	case MK_UNABLE_TO_CONVERT:
	case MK_IMAGE_LOSSAGE:  /* image library generic error */
	case MK_ERROR_SENDING_DATA_COMMAND:
	case MK_OFFLINE:
		msg = NULL;
		break;

	case MK_REDIRECT_ATTEMPT_NOT_ALLOWED:
	case MK_SERVER_TIMEOUT:
	case MK_CONNECTION_TIMED_OUT:
	case MK_OUT_OF_MEMORY:
	case MK_TIMEBOMB_URL_PROHIBIT:
	case MK_TIMEBOMB_MESSAGE:
	case MK_RELATIVE_TIMEBOMB_MESSAGE:
	case MK_NO_WAIS_PROXY:
	case MK_CREATING_NEWSRC_FILE:
	case MK_NNTP_SERVER_NOT_CONFIGURED:
	case MK_NNTP_NEWSGROUP_SCAN_ERROR:
	case MK_ZERO_LENGTH_FILE:
	case MK_BAD_CONNECT:
	case MK_UNABLE_TO_USE_PASV_FTP:
	case MK_UNABLE_TO_CHANGE_FTP_MODE:
	case MK_UNABLE_TO_FTP_CWD:
	case MK_UNABLE_TO_SEND_PORT_COMMAND:
	case MK_UNABLE_TO_ACCEPT_SOCKET:
	case MK_UNABLE_TO_CONNECT2:
	case MK_BAD_NNTP_CONNECTION:
	case MK_NNTP_SERVER_ERROR:
	case MK_SERVER_DISCONNECTED:
	case MK_NEWS_ITEM_UNAVAILABLE:
	case MK_UNABLE_TO_OPEN_NEWSRC:
	case MK_COULD_NOT_LOGIN_TO_SMTP_SERVER:
	case MK_MSG_NO_SMTP_HOST:
	case MK_COULD_NOT_GET_USERS_MAIL_ADDRESS:
	case MK_UNABLE_TO_CONNECT_TO_PROXY:
	case MK_UNABLE_TO_LOCATE_PROXY:
	case MK_DISK_FULL:
	case MK_PRINT_LOSSAGE:
	case MK_SECURE_NEWS_PROXY_ERROR:
	case MK_SIGNATURE_TOO_LONG:
	case MK_SIGNATURE_TOO_WIDE:
	case MK_POP3_SERVER_ERROR:
	case MK_POP3_USERNAME_UNDEFINED:
	case MK_POP3_PASSWORD_UNDEFINED:
	case MK_POP3_USERNAME_FAILURE:
	case MK_POP3_PASSWORD_FAILURE:
	case MK_POP3_NO_MESSAGES:
	case MK_POP3_LIST_FAILURE:
	case MK_POP3_LAST_FAILURE:
	case MK_POP3_RETR_FAILURE:
	case MK_POP3_DELE_FAILURE:
	case MK_POP3_OUT_OF_DISK_SPACE:
	case MK_POP3_MESSAGE_WRITE_ERROR:
	case MK_MIME_NO_SENDER:
	case MK_MIME_NO_RECIPIENTS:
	case MK_MIME_NO_SUBJECT:
	case MK_MIME_ERROR_WRITING_FILE:
	case MK_MIME_MULTIPART_BLURB:
	case MK_MSG_CANT_COPY_TO_SAME_FOLDER:
	case MK_MSG_CANT_COPY_TO_QUEUE_FOLDER:
	case MK_MSG_CANT_COPY_TO_QUEUE_FOLDER_OLD:
	case MK_MSG_CANT_COPY_TO_DRAFTS_FOLDER:
	case MK_MSG_CANT_CREATE_FOLDER:
	case MK_MSG_FOLDER_ALREADY_EXISTS:
	case MK_MSG_FOLDER_NOT_EMPTY:
	case MK_MSG_CANT_DELETE_FOLDER:
	case MK_MSG_CANT_CREATE_INBOX:
	case MK_MSG_CANT_CREATE_MAIL_DIR:
	case MK_MSG_NO_POP_HOST:
	case MK_MSG_MESSAGE_CANCELLED:
	case MK_MSG_FOLDER_UNREADABLE:
	case MK_MSG_FOLDER_SUMMARY_UNREADABLE:
	case MK_MSG_TMP_FOLDER_UNWRITABLE:
	case MK_MSG_ID_NOT_IN_FOLDER:
	case MK_MSG_NEWSRC_UNPARSABLE:
	case MK_MSG_NO_RETURN_ADDRESS:
	case MK_MSG_ERROR_WRITING_NEWSRC:
	case MK_MSG_ERROR_WRITING_MAIL_FOLDER:
	case MK_MSG_SEARCH_FAILED:
	case MK_MSG_FOLDER_BUSY:
		msg = XP_STRDUP(XP_GetString(code));
		break;

	case MK_TCP_READ_ERROR:
	case MK_TCP_WRITE_ERROR:
	case MK_UNABLE_TO_CREATE_SOCKET:
	case MK_UNABLE_TO_CONNECT:
	case MK_HTTP_TYPE_CONFLICT:
	case MK_TCP_ERROR:
		sub_error = va_arg(args, int);
		if (IS_SSL_ERROR(sub_error) || IS_SEC_ERROR(sub_error)) {
			/*
			 * For SSL/SEC errors, use the message without a wrapper.
			 */
			msg = XP_STRDUP(XP_GetString(sub_error));
		} else if (code == MK_UNABLE_TO_CONNECT &&
				   (sub_error == XP_ERRNO_EINVAL
					|| sub_error == XP_ERRNO_EADDRINUSE)) {
			/*
			 * With unable-to-connect errors, some errno values/strings
			 * are not more helpful, so just use a plain message for these.
			 */
			msg = XP_STRDUP(XP_GetString(MK_UNABLE_TO_CONNECT2));
		} else {
			msg = PR_smprintf(XP_GetString(code), XP_GetString(sub_error));
		}
		break;

	case MK_MALFORMED_URL_ERROR:
	case MK_COULD_NOT_PUT_FILE:
	case MK_UNABLE_TO_LOCATE_FILE:
	case MK_NNTP_AUTH_FAILED:
	case MK_UNABLE_TO_LOCATE_HOST:
	case MK_UNABLE_TO_LOCATE_SOCKS_HOST:
	case MK_UNABLE_TO_OPEN_FILE:
	case MK_UNABLE_TO_OPEN_TMP_FILE:
	case MK_CONNECTION_REFUSED:
	case MK_NNTP_ERROR_MESSAGE:
	case MK_MSG_COULDNT_OPEN_FCC_FILE:
	case MK_TIMEBOMB_WARNING_MESSAGE:
	case MK_RELATIVE_TIMEBOMB_WARNING_MESSAGE:
	case MK_ERROR_SENDING_FROM_COMMAND:
	case MK_ERROR_SENDING_RCPT_COMMAND:
	case MK_ERROR_SENDING_MESSAGE:
	case MK_SMTP_SERVER_ERROR:
		msg = PR_vsmprintf(XP_GetString(code), args);
		break;

	case -1:
	default:
		msg = PR_smprintf(XP_GetString(MK_COMMUNICATIONS_ERROR), code);
		break;
	}

  va_end (args);

  TRACEMSG(("NET_ExplainErrorDetails generated: %s", msg ? msg : "(none)"));

  return(msg);
}
