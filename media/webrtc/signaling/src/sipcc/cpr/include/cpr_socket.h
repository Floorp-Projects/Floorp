/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_SOCKET_H_
#define _CPR_SOCKET_H_

#include "cpr_types.h"
#include "cpr_time.h"

__BEGIN_DECLS

/*
 * By default enable use of sockets
 */
#define CPR_USE_SOCKETS

#if defined SIP_OS_LINUX
#include "../linux/cpr_linux_socket.h"
#elif defined SIP_OS_WINDOWS
#include "../win32/cpr_win_socket.h"
#elif defined SIP_OS_OSX
#include "../darwin/cpr_darwin_socket.h"
#endif


/**
 * Socket shutdown status
 */
typedef enum
{
    CPR_SHUTDOWN_RECEIVE,
    CPR_SHUTDOWN_SEND,
    CPR_SHUTDOWN_BOTH
} cpr_shutdown_e;


/**
 * Socket address structures
 */
typedef struct
{
    uint16_t sa_family;
    int8_t   sa_data[14];
} cpr_sockaddr_t;

typedef struct
{
    uint16_t         sin_family;
    uint16_t         sin_port;
    struct in_addr_s sin_addr;
    /* Pad to size of cpr_sockaddr_t */
    uint8_t          sin_zero[sizeof(cpr_sockaddr_t) -
                              sizeof(struct in_addr_s) -
                              sizeof(uint16_t) * 2];
} cpr_sockaddr_in_t;


typedef struct
{
     uint16_t       sin6_family;
     uint16_t       sin6_port;
     uint32_t       sin6_flowinfo;
     cpr_in6_addr_t sin6_addr;
     uint32_t       sin6_scope_id;
} cpr_sockaddr_in6_t;

typedef struct sockaddr_storage cpr_sockaddr_storage; // need uniform the definition of sockaddr_storage in win and linux

/*
 * Functions
 */

#ifdef CPR_USE_SOCKETS


/**
 * cprBind
 *
 * @brief The cprBind function is the CPR wrapper for the "Bind" socket call.
 *
 * The cprBind() function shall assign a local socket address address to a
 * socket identified by descriptor socket that has no local socket address
 * assigned. Sockets created with the cprSocket() function are initially
 * unnamed; they are identified only by their address family.
 *
 * @param[in] soc  - The socket previously created using cprAccept that is to be
 *                   bound
 * @param[out] addr - The address of the socket that is to be bound
 * @param[in] addr_len Points to a cpr_socklen_t structure which on specifies the length
 *                   of the supplied cpr_sockaddr_t structure.
 *
 * @return CPR_SUCCESS on success otherwise, CPR_FAILURE. cpr_errno needs to be set in this case.
 *
 * @note The possible error values this function should return are
 *         @li [CPR_EBADF]      socket is not a valid socket descriptor.
 *         @li [CPR_ENOTSOCK]   The descriptor references a file, not a socket
 *
 */
cpr_status_e
cprBind(cpr_socket_t socket,
        CONST cpr_sockaddr_t * RESTRICT addr,
        cpr_socklen_t addr_len);

/**
 * cprCloseSocket
 *
 * @brief The cprCloseSocket function shall destroy the socket
 *
 * The cprCloseSocket() function shall destroy the socket descriptor indicated
 * by socket.  The descriptor may be made available for return by subsequent
 * calls to cprSocket().  If the linger option is set with a non-zero timeout
 * and the socket has untransmitted data, then cprCloseSocket() shall block for
 * up to the current linger interval until all data is transmitted.
 *
 * @param[in] soc  - The socket that needs to be destroyed
 *
 * @return CPR_SUCCESS on success otherwise, CPR_FAILURE. cpr_errno needs to be set in this case.
 *
 * @note The possible error values this function should return are
 *         @li [CPR_EBADF]      socket is not a valid socket descriptor.
 */
cpr_status_e
cprCloseSocket(cpr_socket_t socket);

/**
 * cprConnect
 *
 * @brief The cprConnect function is the wrapper for the "connect" socket API
 *
 * The cprConnect() function shall attempt to make a connection on a socket.
 * If the connection cannot be established immediately and non-blocking is set for
 * the socket, cprConnect() shall fail and set cpr_errno to [CPR_EINPROGRESS], but
 * the connection request shall not be aborted, and the connection shall be
 * established asynchronously. When the connection has been established
 * asynchronously, cprSelect() shall indicate that the file descriptor for the
 * socket is ready for writing.
 * If the initiating socket is connection-mode, then cprConnect() shall attempt to
 * establish a connection to the address specified by the address argument.If the
 * connection cannot be established immediately and non-blocking is not set for
 * the socket, cprConnect() shall block for up to an unspecified timeout interval
 * until the connection is established. If the timeout interval expires before the
 * connection is established, cprConnect() shall fail and the connection attempt
 * shall be aborted.
 * If the initiating socket is connectionless (i.e. SOCK_DGRAM), then cprConnect()
 * shall set the socket's peer address, and no connection is made. The peeraddress
 * identifies where all datagrams are sent on subsequent cprSend() functions, and
 * limits the remote sender for subsequent cprRecv() functions. If address is a
 * null address for the protocol, the socket's peer address shall be reset.
 *
 * @param[in] soc  - Specifies the socket created with cprSocket() to connect
 * @param[in] addr - A pointer to a cpr_sockaddr_t structure containing the peer address.
 * @param[in] addr_len  - Points to a cpr_socklen_t structure which specifies
 *                        the length of the supplied cpr_sockaddr_t structure.
 *
 * @return CPR_SUCCESS on success otherwise, CPR_FAILURE. cpr_errno needs to be set in this case.
 *
 * @note The possible error values this function should return are
 *         @li [CPR_EBADF]      socket is not a valid socket descriptor.
 */
cpr_status_e
cprConnect(cpr_socket_t socket,
           SUPPORT_CONNECT_CONST cpr_sockaddr_t * RESTRICT addr,
           cpr_socklen_t addr_len);

/**
 * cprGetSockName
 *
 * @brief The cprGetSockName retrieves the locally-bound name of the socket
 *
 * The cprGetSockName() function shall retrieve the locally-bound name of the
 * specified socket, store this address in the cpr_sockaddr_t struct
 * structure pointed to by the "addr" argument, and store the length of this address in
 * the object pointed to by the "addr_len" argument.  If the actual length
 * of the address is greater than the length of the supplied cpr_sockaddr_t
 * structure, the stored address shall be truncated.  If the socket has not been
 * bound to a local name, the value stored in the object pointed to by address is
 * unspecified.
 *
 * @param[in] soc  - Specifies the socket to get the peer address from
 * @param[out] addr - A pointer to a cpr_sockaddr_t structure containing the peer address.
 * @param[out] addr_len  - Points to a cpr_socklen_t structure which specifies
 *                        the length of the supplied cpr_sockaddr_t structure.
 * @return CPR_SUCCESS on success otherwise, CPR_FAILURE. cpr_errno needs to be set in this case.
 *
 *  @note If successful, the address argument shall point to the address of the socket
 *  @note The possible error values this function should return are
 *        @li [CPR_EBADF]  The socket argument is not a valid file descriptor
 *        @li [CPR_EINVAL] cprListen() has not been called on the socket descriptor.
 *        @li [CPR_ENOTCONN]  The socket is not connected
 *        @li [CPR_ENOTSOCK]  The descriptor references a file, not a socket.
 *        @li [CPR_OPNOTSUPP] The operation is not supported for the socket
 */
cpr_status_e
cprGetSockName(cpr_socket_t socket,
               cpr_sockaddr_t * RESTRICT addr,
               cpr_socklen_t * RESTRICT addr_len);

/**
 * cprListen
 *
 * @brief The cprListen is the CPR wrapper for the "listen" socket API.
 *
 * The cprListen() function shall mark a connection-mode socket, specified by
 * the "soc" argument, as accepting connections.  The "backlog" argument
 * provides a hint to the implementation which the implementation shall use to
 * limit the number of outstanding connections in the socket's listen queue.
 * Implementations may impose a limit on backlog and silently reduce the
 * specified value. Implementations shall support values of backlog up to
 * SOMAXCONN.
 * If listen() is called with a backlog argument value that is less than zero
 * (0), the function behaves as if it had been called with a backlog argument
 * value of 0.  A backlog argument of zero (0) may allow the socket to accept
 * connections, in which case the length of the listen queue may be set to an
 * implementation-defined minimum value.
 *
 * @param[in] soc  - Specifies the socket to get the peer address
 * @param[in] backlog  - The limit on the number of outstanding connections
 *
 * @return CPR_SUCCESS on success otherwise, CPR_FAILURE. cpr_errno needs to be set in this case.
 *  @note The possible error values this function should return are
 *        @li [CPR_EBADF]  The socket argument is not a valid file descriptor
 *        @li [CPR_EINVAL] cprListen() has not been called on the socket descriptor.
 *        @li [CPR_ENOTSOCK]  The descriptor references a file, not a socket.
 *        @li [CPR_EDESTADDRREQ]  The socket is not bound to a local address
 */
cpr_status_e
cprListen(cpr_socket_t socket,
          uint16_t backlog);

/**
 * cprRecv
 *
 * @brief The cprRecv() function shall receive a message from a socket.
 *
 * This function is normally used with connected sockets because it does not permit
 * the application to retrieve the source address of received data.  The cprRecv()
 * function shall return the length of the message written to the buffer pointed
 * to by the "buf" argument.
 * For message-based sockets, e.g. SOCK_DGRAM, the entire message shall be read in
 * a single operation.  If the message is too long to fit in the supplied buffer
 * and the "flags" argument does not have MSG_PEEK set, the excess bytes are
 * discarded.  If the MSG_WAITALL flag is not set, data shall be returned onlyup
 * to the end of the first message.
 * For stream-based sockets, e.g. SOCK_STREAM, message boundaries are ignored and
 * data is returned as it becomes available; therefore, no data is discarded.
 * If no messages are available at the socket and non-blocking is not set on the
 * socket's file descriptor, cprRecv() shall block until a message arrives. If no
 * messages are available at the socket and non-blocking is set on the socket's
 * file descriptor, cprRecv() shall fail and set cpr_errno to [CPR_EAGAIN]or
 * [CPR_EWOULDBLOCK].
 * The cprSelect() function can be used to determine when data is available to be
 * received.  The cprRecv() function is the same as cprRecvFrom() with a zero
 * address_len argument.
 *
 * @param[in] soc  - Specifies the socket to receive data
 * @param[out] buf  - Contains the data received
 * @param[out] len  - The length of the data received
 * @param[in] flags  - The options used for the recv.
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
cprRecv(cpr_socket_t socket,
        void * RESTRICT buf,
        size_t len,
        int32_t flags);

/**
 * cprRecvFrom
 *
 * @brief The cprRecvFrom() function shall receive a message from a specific socket.
 *
 * The cprRecvFrom() function shall receive a message from a socket and is
 * normally used with connectionless-mode sockets because it permits the
 * application to retrieve the source address of received data.  This function
 * shall return the length of the message written to the buffer pointed to bythe
 * buffer argument.
 * For message-based sockets, e.g. SOCK_DGRAM, the entire message shall be read in
 * a single operation.  If the message is too long to fit in the supplied buffer
 * and the flags argument does not have MSG_PEEK set, the excess bytes are
 * discarded.  If the MSG_WAITALL flag is not set, data shall be returned onlyup
 * to the end of the first message.
 * For stream-based sockets, e.g. SOCK_STREAM, message boundaries are ignored and
 * data is returned as it becomes available; therefore, no data is discarded.
 * If no messages are available at the socket and non-blocking is not set on the
 * socket's file descriptor, cprRecvFrom() shall block until a message arrives. If no
 * messages are available at the socket and non-blocking is set on the socket's
 * file descriptor, cprRecvFrom() shall fail and set cpr_errno to [CPR_EAGAIN]or
 * [CPR_EWOULDBLOCK].
 *
 * @param[in] soc  - Specifies the socket to receive data
 * @param[out] buf  - Contains the data received
 * @param[out] len  - The length of the data received
 * @param[in] flags  - The options used for the recvFrom
 * @param[out] from - A null pointer or pointer to a cpr_sockaddr_t structure in
 *                   which the sending address is to be stored.
 * @param[out] fromlen - The length of the cpr_sockaddr_t structure pointed to by
 *                    the "from" argument.
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
cprRecvFrom(cpr_socket_t socket,
            void * RESTRICT buf,
            size_t len,
            int32_t flags,
            cpr_sockaddr_t * RESTRICT from,
            cpr_socklen_t * RESTRICT from_len);

/**
 * cprRecvmsg
 *
 * Receive a message from a socket.
 * +++ NOT SUPPORTED +++
 * Windows does not support the BSD 4.2 functions recvmsg/sendmsg; therefore,
 * these functions are not supported within this portable runtime API layer.
 */
/* ssize_t cprRecvmsg (cpr_socket_t socket, struct msghdr_t *msg, uint32_t flags) */

/**
 * cprSelect
 *
 * @brief The cprSelect() function is the CPR wrapper for the "select" socket API.
 *
 * The cprSelect() function returns which of the specified file descriptors is ready for
 * reading, ready for writing, or has an exception pending.  The function will
 * block up to the specified timeout interval for one of the conditions to be
 * true or until interrupted by a signal.
 *
 * File descriptor masks of type fd_set can be initialized and tested with
 * FD_CLR(), FD_ISSET(), FD_SET(), and FD_ZERO().  The OS-implementation may
 * implement these calls either as a macro definition or an actual function.
 * void FD_CLR(cpr_socket_t, fd_set *) shall remove the file descriptor fd from the
 * set. If fd is not a member of this set, there shall be no effect on theset.
 * int FD_ISSET(cpr_socket_t, fd_set *) shall evaluate to non-zero if the file
 * descriptor fd is a member of the set, and shall evaluate to zero otherwise.
 * void FD_SET(cpr_socket_t, fd_set *) shall add the file descriptor fd to the set.
 * If the file descriptor fd is already in this set, there shall be no effect on
 * the set.
 * void FD_ZERO(fd_set *) shall initialize the descriptor set to the null set.
 *
 * @param[in] nfds    Specifies the argument range of file descriptors to be tested. The
 *            descriptors from zero through nfds-1 in the descriptor sets shall be examined.
 *
 * @param[in] read_fds    If not a null pointer, this is a pointer to an fd_set object.
 *    @li On input this specifies the file descriptors to be checked for being ready to read.
 *    @li On output this specifies the file descriptors that are ready to read.
 *
 * @param[in] write_fds   If not a null pointer, this is a pointer to an fd_set object.
 *    @li On input this specifies the file descriptors to be checked for being ready to write.
 *    @li On output this specifies the file descriptors that are ready to write.
 *
 * @note If you specify a non-NULL value for write_fds then note that this function will
 *       return immediately if the socket is OK to write to, to indicate that a send, sendto,
 *       (or on Windows a WSASendto()) are guaranteed to succeed. You generally would not keep
 *       checking for write access in a loop, as cprSelect() would never get a chance to time
 *       out if the socket was generally available for a write operation. You should really
 *       only check for writability when you know you have something to write to a socket.
 *
 * @param[in] except_fds   If not a null pointer, this is a pointer to an fd_set object.
 *    @li On input this specifies the file descriptors to be checked for errors/exceptions pending.
 *    @li On output this specifies the file descriptors that have errors/exceptions pending.
 *
 * @param[in] timeout If not a null pointer, this  points to an object of type struct cpr_timeval
 *       that specifies the maximum time interval to wait for the selection to complete.
 *       If timeout expires, the function shall return.  If the parameter is a null pointer, the function
 *       will block indefinitely until at least one file descriptor meets the criteria.
 *
 * @note While this function supports multiple file descriptor types, only file descriptors referring to a
 *       socket are guaranteed to be supported.
 * @note Note that the "nfds" parameter is not used in Windows.
 *
 * @return Upon successful completion, cprSelect() shall return the number of file descriptors ready.
 *       Otherwise, SOCKET_ERROR shall be returned and cpr_errno set to indicate the error where read_fds,
 *       write_fds and error_fds are not modified.
 *
 *        The possible error values this function should return are
 *        @li [CPR_EBADF]  The socket argument is not a valid file descriptor
 *        @li [CPR_INTR]   The function was interrupted before an event or
 *                         timeout occurred
 *        @li [CPR_INVAL]  An invalid timeout was specified or nfds is less
 *                        than 0 or greater than FD_SETSIZE
 *
 */
int16_t
cprSelect(uint32_t nfds,
          fd_set * RESTRICT read_fds,
          fd_set * RESTRICT write_fds,
          fd_set * RESTRICT except_fds,
          struct cpr_timeval * RESTRICT timeout);

/**
 * Supported cprSelect macros are provided as follows
 */

/**
 * FD_CLR(s, *set)
 *
 * Removes the descriptor s from set.
 *
 * Parameters: s   - the descriptor
 *             set - pointer to the fd_set
 *
 * Return Value: None
 */

/**
 * FD_ISSET(s, *set)
 *
 * Nonzero if s is a member of the set. Otherwise, zero.
 *
 * Parameters: s   - the descriptor
 *             set - pointer to the fd_set
 *
 * Return Value: None
 */

/**
 * FD_SET(s, *set)
 *
 * Adds descriptor s to set.
 *
 * Parameters: s   - the descriptor
 *             set - pointer to the fd_set
 *
 * Return Value: None
 */

/**
 * FD_ZERO(*set)
 *
 * Initializes the set to the null set.
 *
 * Parameters: s   - the descriptor
 *             set - pointer to the fd_set
 *
 * Return Value: None
 */

/**
 * cprSend
 *
 * @brief The cprSend() function is the CPR wrapper for the "send" socket API.
 *
 * The cprSend() function shall transmit a message from the specified socket to
 * its peer. The cprSend() function shall send a message only when the socket is
 * connected. The length of the message to be sent is specified by the length
 * argument. If the message is too long to pass through the underlying protocol,
 * cprSend() shall fail and no data is transmitted.  Delivery of the message is
 * not guaranteed.
 * If space is not available at the sending socket to hold the message to be
 * transmitted, and the socket does not have non-blocking set, cprSend() shall
 * block until space is available; otherwise, if non-blocking is set, the
 * cprSend() call shall fail.
 *
 * @param[in] soc  Specifies the socket created with cprSocket() to send
 * @param[in] buf  A pointer to the buffer of the message to send.
 * @param[in] len  Specifies the length in bytes of the message pointed to by the buffer argument.
 * @param[in] flags   The socket options
 *
 * @return Upon successful completion, cprSend() shall return the number of
 *     bytes sent. Otherwise, SOCKET_ERROR shall be returned and cpr_errno set to
 *     indicate the error.
 *
 * @note The possible error values this function should return are
 *        @li [CPR_EBADF]  The socket argument is not a valid file descriptor
 *        @li [CPR_ENOTSOCK]  socket does not refer to a socket descriptor
 *        @li [CPR_EAGAIN]    The socket is marked non-blocking and no data can
 *                          be sent
 *        @li [CPR_EWOULDBLOCK]   Same as CPR_EAGAIN
 *        @li [CPR_ENOTCONN]  A connection-mode socket that is not connected
 *        @li [CPR_ENOTSUPP]  The specified flags are not supported for this
 *                            type of socket or protocol.
 *        @li [CPR_EMSGSIZE]  The message is too large to be sent all at once
 *        @li [CPR_EDESTADDRREQ]  The socket has no peer address set
 *
 */
ssize_t
cprSend(cpr_socket_t socket,
        CONST void *buf,
        size_t len,
        int32_t flags);

/**
 * cprSendTo
 *
 * @brief The cprSendTo() function is the CPR wrapper for the "send" socket API.
 *
 * The cprSendTo() function shall send a message through a socket. If the socket
 * is connectionless-mode, the message shall be sent to the address specified by
 * address. If the socket is connection-mode, address shall be ignored.
 * Delivery of the message is not guaranteed.
 * If space is not available at the sending socket to hold the message to be
 * transmitted, and the socket does not have non-blocking set, cprSendTo() shall
 * block until space is available; otherwise, if non-blocking is set, the
 * cprSendTo() call shall fail.
 * The cprSelect() function can be used to determine when it is possible to send
 * more data.
 *
 * @param[in] soc  Specifies the socket created with cprSocket() to send
 * @param[in] msg  A pointer to the buffer of the message to send.
 * @param[in] len  Specifies the length in bytes of the message pointed to by the buffer argument.
 * @param[in] flags   The socket options
 * @param[in] dest_addr Points to a cpr_sockaddr_t structure containing the destination
 *                    address.
 * @param[in] dest_len Specifies the length of the cpr_sockaddr_t structure pointed to by
 *                     the "dest_addr" argument.
 *
 * @return Upon successful completion, cprSend() shall return the number of
 *     bytes sent. Otherwise, SOCKET_ERROR shall be returned and cpr_errno set to
 *     indicate the error.
 *
 * @note The possible error values this function should return are
 *        @li [CPR_EBADF]  The socket argument is not a valid file descriptor
 *        @li [CPR_ENOTSOCK]  socket does not refer to a socket descriptor
 *        @li [CPR_EAGAIN]    The socket is marked non-blocking and no data can
 *                          be sent
 *        @li [CPR_EWOULDBLOCK]   Same as CPR_EAGAIN
 *        @li [CPR_ENOTCONN]  A connection-mode socket that is not connected
 *        @li [CPR_ENOTSUPP]  The specified flags are not supported for this
 *                            type of socket or protocol.
 *        @li [CPR_EMSGSIZE]  The message is too large to be sent all at once
 *        @li [CPR_EDESTADDRREQ]  The socket has no peer address set
 *
 */
ssize_t
cprSendTo(cpr_socket_t socket,
          CONST void *msg,
          size_t len,
          int32_t flags,
          CONST cpr_sockaddr_t *dest_addr,
          cpr_socklen_t dest_len);


/**
 * cprSendmsg
 *
 * Send a message from a socket.
 * +++ NOT SUPPORTED +++
 * Windows does not support the BSD 4.2 functions recvmsg/sendmsg; therefore,
 * these functions are not supported within this portable runtime API layer.
 */
/* ssize_t sendmsg (cpr_socket_t socket, CONST struct msghdr *msg, uint32_t flags) */


/**
 * cprSetSockOpt
 *
 * @brief The cprSetSockOpt() function is used to set the socket options
 *
 * The cprSetSockOpt() function shall set the option specified by the
 * option_name argument, at the protocol level specified by the "level" argument,
 * to the value pointed to by the "opt_val" argument for the socket specified
 * by the "soc" argument.
 * The level argument specifies the protocol level at which the option resides. To
 * set options at the socket level, specify the level argument as SOL_SOCKET. To
 * set options at other levels, supply the appropriate level identifier for the
 * protocol controlling the option. For example, to indicate that an option is
 * interpreted by the TCP (Transport Control Protocol), set level to IPPROTO_TCP
 * as defined in the <cpr_in.h> header.
 * The opt_name argument specifies a single option to set. The option_name
 * argument and any specified options are passed uninterpreted to the appropriate
 * protocol module. The <cpr_socket.h> header defines the socket-level options.
 *
 * @param[in] soc The socket on which the options need to be set
 * @param[in] level The protocol level at which the option resides
 * @param[in] opt_name This specifies the single option that is being set
 * @param[in] opt_val The values for the option
 * @param[in] opt_len The length field for the option values
 *
 * @return Upon successful completion, CPR_SUCCESS shall be returned;
 * otherwise, CPR_FAILURE shall be returned and cpr_errno set to indicate the
 * error.
 *
 * @note The possible error values this function should return are
 *        @li [CPR_EBADF]  The socket argument is not a valid file descriptor
 *        @li [CPR_ENOTSOCK]  socket does not refer to a socket descriptor
 *        @li [CPR_EINVAL]    The specified option is invalid or the socket is
 *                          shut down
 *        @li [CPR_EISCONN]   The specified socket is already connected and can
 *                          not be changed
 *        @li [CPR_ENOPROTOOPT]   The option is not supported by the protocol
 *
 */
cpr_status_e
cprSetSockOpt(cpr_socket_t socket,
              uint32_t level,
              uint32_t opt_name,
              CONST void *opt_val,
              cpr_socklen_t opt_len);

/**
 * cprSetSockNonBlock
 *
 * @brief The cprSetSockNonBlock() function is used to set the socket options
 *
 * The cprSetSockNonBlock() function shall set a socket to be non blocking. It
 * uses the fcntl function on the socket desriptor to achieve this. If the fcntl
 * operation fails, a CPR_FAILURE is returned and errno is set by the OS
 * implementation.
 *
 * @param[in] soc The socket that needs to be set to non-blocking
 *
 * @return Upon successful completion, CPR_SUCCESS shall be returned;
 * otherwise, CPR_FAILURE shall be returned and cpr_errno set to indicate the
 * error.
 */
cpr_status_e
cprSetSockNonBlock(cpr_socket_t socket);

/**
 * cprSocket
 *
 * @brief The cprSocket() is the CPR wrapper for the "socket" API
 *
 * The cprSocket() function shall create an unbound socket in a
 * communications domain, and return a file descriptor that can be used
 * in later function calls that operate on sockets.
 *
 * @param[in] domain  The communications domain, i.e. address family, in which a socket is to
 *                   be created
 * @param[in] type    The type of socket to be created. The following types must
 *                   be supported:
 *             @li SOCK_STREAM Provides sequenced, reliable, bidirectional, connection-mode
 *                               byte streams, i.e. TCP
 *             @li SOCK_DGRAM  Provides connectionless-mode, unreliable
 *                           datagrams of fixed maximum length, i.e. UDP
 *             @li SOCK_SEQPACKET   Provides sequenced, reliable, bidirectional, connection-mode
 *                          transmission paths for records.  A single operation never transfers part of
 *                          more than one record.  Record boundaries are visible to the receiver via the
 *                          MSG_EOR flag.
 * @param[in] protocol    The protocol to be used with the socket.
 *
 * @return Upon successful completion, a socket handle defined by
 *      cpr_socket_t shall be returned; otherwise, INVALID_SOCKET shall be returned and
 *       cpr_errno set to indicate the error.
 *
 * @note The possible error values this function should return are
 *        @li [CPR_EBADF]  The socket argument is not a valid file descriptor
 *        @li [CPR_ENOTSOCK]  socket does not refer to a socket descriptor
 *        @li [CPR_EINVAL]   The specified option is invalid or the socket is
 *                           shut down
 *        @li [CPR_EMFILE]   No more socket descriptors available for the
 *                           process
 *        @li [CPR_ENFILE]   No more socket descriptors available for the
 *                           system
 *        @li [CPR_EPROTOTYPE] The socket type is not supported by the
 *                              protocol
 *        @li [CPR_EPROTONOSUPPORT]   The protocol is not supported for the
 *                                    domain
 *
 */
cpr_socket_t
cprSocket(uint32_t domain,
          uint32_t type,
          uint32_t protocol);

/* cpr_inet_pton
 *	Convert from presentation format (which usually means ASCII printable)
 *	to network format (which is usually some kind of binary format).
 * @param[in] af The address family IPv4 or IPv6
 * @param[in] src The address that needs to be converted
 * @param[out] dst The address after the conversion
 * @return
 *	1 if the address was valid for the specified address family
 *	0 if the address wasn't valid (`dst' is untouched in this case)
 *	-1 if some other error occurred (`dst' is untouched in this case, too)
 */
int
cpr_inet_pton (int af, const char *src, void *dst);


#endif /* CPR_USE_SOCKETS */

__END_DECLS

#endif
