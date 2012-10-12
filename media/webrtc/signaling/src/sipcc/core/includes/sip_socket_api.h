/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
