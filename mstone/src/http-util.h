/* -*- Mode: C; c-file-style: "stroustrup"; comment-column: 40 -*- */
/* 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Mailstone utility, 
 * released March 17, 2000.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1999-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s):	Dan Christian <robodan@netscape.com>
 *			Marcel DePaolis <marcel@netcape.com>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License Version 2 or later (the "GPL"), in
 * which case the provisions of the GPL are applicable instead of
 * those above.  If you wish to allow use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the NPL or the GPL.
 */
/* http-util.h -- Shared HTTP functions */

#define HTTP_HEADER_BREAK	"\r\n\r\n"
#define HTTP_CONTENT_TYPE_POST	"Content-type: application/x-www-form-urlencoded"
#define HTTP_CONTENT_LENGTH	"Content-length:"
#define HTTP_LOCATION	    	"Location:"
#define HTTP_PROTOCOL		"http://"

#define HTTP_MAX_RECONNECTS	5	/* max to reconnect/retry a command */

/* Callback type for parsing messages on the fly */
typedef int (*parseMsgPtr_t)(
    ptcx_t, pmail_command_t, void *me,
    char *buffer, int bytesInBuffer, char **searchStr, const char **findStr);

extern int HttpConnect(ptcx_t ptcx, resolved_addr_t *hostInfo,
		       event_timer_t *timer);

extern int HttpReadResponse(ptcx_t ptcx, mail_command_t *cmd, void *me,
			    char *responce, int resplen,
			    parseMsgPtr_t msgParse);

extern int HttpCommandResponse (ptcx_t ptcx, mail_command_t *cmd, void *me,
    resolved_addr_t	*hostInfo, event_timer_t *reconnect,
    event_timer_t *timer, char *command, char *response, int	resplen,
    parseMsgPtr_t	msgParse);
