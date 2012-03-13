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

#include "cpr.h"
#include "cpr_socket.h"
#include "errno.h"
#include "platform_api.h"

/**
 * platSecSocSend
 *
 * @brief The platSecSocSend() function is used to send data over a secure
 * socket.
 *
 * This function maps to the cprSend function for the RT/TNP implementations.
 *
 * @param[in] soc  Specifies the socket created with cprSocket() to send
 * @param[in] buf  A pointer to the buffer of the message to send.
 * @param[in] len  Specifies the length in bytes of the message pointed to by the buffer argument.
 *
 * @return Upon successful completion, platSecSocSend() shall return the number of
 *     bytes sent. Otherwise, SOCKET_ERROR shall be returned and cpr_errno set to
 *     indicate the error.
 *
 *
 */
ssize_t
platSecSocSend (cpr_socket_t soc,
         CONST void *buf,
         size_t len )
{
    return cprSend(soc, buf, len, 0);
}

/**
 * platSecSocRecv
 *
 * @brief The platSecSocRecv() function shall receive a message from a secure socket.
 *
 * This function is normally used with connected sockets because it does not permit 
 * the application to retrieve the source address of received data.  The
 * platSecSocRecv() function shall return the length of the message written to 
 * the buffer pointed to by the "buf" argument.
 *
 * @param[in] soc  - Specifies the socket to receive data
 * @param[out] buf  - Contains the data received
 * @param[out] len  - The length of the data received
 *
 * @return On success the length of the message in bytes (including zero).
 *         On failure SOCKET_ERROR shall be returned and cpr_errno set to
 *         indicate the error.
 *
 * @note The possible error values this function should return are
 *        @li [CPR_EBADF]  The socket argument is not a valid file descriptor
 *        @li [CPR_ENOTSOCK]  The descriptor references a file, not a socket.
 *        @li [CPR_EAGAIN]    The socket is marked non-blocking and no data is
 *                        waiting to be received.
 *        @li [CPR_EWOULDBLOCK]   Same as CPR_EAGAIN
 *        @li [CPR_ENOTCONN]  A receive attempt is made on a connection-mode socket that is not connected
 *        @li [CPR_ENOTSUPP]  The specified flags are not supported for this type of socket or protocol
 *
 */
ssize_t
platSecSocRecv (cpr_socket_t soc,
         void * RESTRICT buf,
         size_t len)
{
    return cprRecv(soc, buf, len, 0);
}

/**
 * platSecSocClose
 *
 * @brief The platSecSocClose function shall close a secure socket
 *
 * The platSecSocClose() function shall destroy the socket descriptor indicated
 * by socket.  
 *
 * @param[in] soc  - The socket that needs to be destroyed
 *
 * @return CPR_SUCCESS on success otherwise, CPR_FAILURE. cpr_errno needs to be set in this case. 
 *
 * @note The possible error values this function should return are
 *         @li [CPR_EBADF]      socket is not a valid socket descriptor.
 */
cpr_status_e
platSecSocClose (cpr_socket_t soc)
{
    return cprCloseSocket(soc);
}
