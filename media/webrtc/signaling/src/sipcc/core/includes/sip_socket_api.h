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

#ifndef __SIP_SOCKET_API_H__
#define __SIP_SOCKET_API_H__

#include "cpr.h"
#include "cpr_socket.h"

/**
 * sipSocketSend
 *
 * @brief The sipSocketSend() function is a wrapper used by the sipstack to send
 * data over a socket. This function decides to use the secure versus unsecure
 * connection based on the "secure" flag.
 * 
 * @note - The implementation of both secure/non-secure is the same in RT/TNP
 * products. It is different for the other vendors and hence we need this
 * flexibility.
 *
 * @param[in] soc  Specifies the socket created with cprSocket() to send
 * @param[in] buf  A pointer to the buffer of the message to send.
 * @param[in] len  Specifies the length in bytes of the message pointed to by the buffer argument.
 * @param[in] flags  - The options used for the send.
 *
 *
 */
ssize_t
sipSocketSend (cpr_socket_t soc,
         CONST void *buf,
         size_t len, 
         int32_t flags,
         boolean secure);

/**
 * sipSocketRecv
 *
 * @brief The sipSocketRecv() function is a wrapper used by the sipstack to send
 * data over a socket. This function decides to use the secure versus unsecure
 * connection based on the "secure" flag.
 * 
 * @note - The implementation of both secure/non-secure is the same in RT/TNP
 * products. It is different for the other vendors and hence we need this
 * flexibility.
 *
 * @param[in] soc  Specifies the socket created with cprSocket() to send
 * @param[in] buf  A pointer to the buffer of the message to send.
 * @param[in] len  Specifies the length in bytes of the message pointed to by the buffer argument.
 * @param[in] flags  - The options used for the recv.
 */
ssize_t
sipSocketRecv (cpr_socket_t soc,
         void * RESTRICT buf,
         size_t len,
         int32_t flags,
         boolean secure);

/**
 * sipSocketClose
 *
 * @brief The sipSocketClose() function is a wrapper used by the sipstack to
 * close a socket. This function decides to use the secure versus unsecure
 * connection based on the "secure" flag.
 * 
 * @note - The implementation of both secure/non-secure is the same in RT/TNP
 * products. It is different for the other vendors and hence we need this
 * flexibility.
 *
 * @param[in] soc  - The socket that needs to be destroyed
 *
 * @return CPR_SUCCESS on success otherwise, CPR_FAILURE. cpr_errno needs to be set in this case. 
 *
 * @note The possible error values this function should return are
 *         @li [CPR_EBADF]      socket is not a valid socket descriptor.
 */
cpr_status_e
sipSocketClose (cpr_socket_t soc,
                boolean secure);
#endif
