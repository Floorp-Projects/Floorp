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

#include "ctype.h"
#include "cpr_types.h"
#include "cpr_debug.h"
#include "cpr_socket.h"
#include "cpr_stdlib.h"
#include "cpr_string.h"
#include "cpr_timers.h"
#include "cpr_errno.h"
#include "cpr_win_defines.h"

#pragma comment(lib, "wsock32.lib")
#pragma comment(lib, "ws2_32.lib")

#define IN6ADDRSZ   16 
#define INT16SZ     2 
#define	INADDRSZ	4
#define IS_DIGIT(ch)   ((ch >= '0') && (ch <= '9'))

//const cpr_in6_addr_t in6addr_any = IN6ADDR_ANY_INIT;
const cpr_ip_addr_t ip_addr_invalid = {0};  

// Function prototypes for windows calls until CPR is in such a state that it can include windows sockets.


/*
 * WinSock 2 extension -- manifest constants for shutdown()
 */
#define SD_RECEIVE      0x00
#define SD_SEND         0x01
#define SD_BOTH         0x02



#define BYTE uint8_t
#define WORD uint16_t
#define DWORD_PTR unsigned long

#if !defined(MAKEWORD)
#define MAKEWORD(low,high) \
        ((WORD)(((BYTE)(low)) | ((WORD)((BYTE)(high))) << 8))
#endif
#ifndef LOBYTE
#define LOBYTE(w)           ((BYTE)((DWORD_PTR)(w) & 0xff))
#endif
#ifndef HIBYTE
#define HIBYTE(w)           ((BYTE)((DWORD_PTR)(w) >> 8))
#endif

#define WSADESCRIPTION_LEN      256
#define WSASYS_STATUS_LEN       128



/*
 * Commands for ioctlsocket(),  taken from the BSD file fcntl.h.
 *
 *
 * Ioctl's have the command encoded in the lower word,
 * and the size of any in or out parameters in the upper
 * word.  The high 2 bits of the upper word are used
 * to encode the in/out status of the parameter; for now
 * we restrict parameters to at most 128 bytes.
 */
#define IOCPARM_MASK    0x7f        /* parameters must be < 128 bytes */
#define IOC_VOID        0x20000000  /* no parameters */
#define IOC_OUT         0x40000000  /* copy out parameters */
#define IOC_IN          0x80000000  /* copy in parameters */
#define IOC_INOUT       (IOC_IN|IOC_OUT)
                                        /* 0x20000000 distinguishes new &
                                         * old ioctl's */
#define _IO(x,y)        (IOC_VOID|((x)<<8)|(y))

#define _IOR(x,y,t)     (IOC_OUT|(((long)sizeof(t)&IOCPARM_MASK)<<16)|((x)<<8)|(y))

#define _IOW(x,y,t)     (IOC_IN|(((long)sizeof(t)&IOCPARM_MASK)<<16)|((x)<<8)|(y))

#define FIONREAD    _IOR('f', 127, u_long) /* get # bytes to read */
#define FIONBIO     _IOW('f', 126, u_long) /* set/clear non-blocking i/o */
#define FIOASYNC    _IOW('f', 125, u_long) /* set/clear async i/o */




//////////////////////////////////////////////////////////////////////////
/*
 * Secure socket implementation
 */

#define SRVR_SECURE		9
#define SRVR_TYPE_CCM	2	/* CCM server  */

int SECReq_LookupSrvr(char* serverAddr, int serverType) ;

int SECSock_connect (int    appType, 
					 char * srvrAddrAndPort, 
					 int    blockingConnect, 
					 int    connTimeout,
					 int    ipTOS) ;
int SECSock_getFD(int nSSL) ;
int SECSock_isConnected(int nSSL) ;

int SECSock_close(int nSSL) ;
int SECSock_read(int fd, char* buff, int nLen) ;
int SECSock_write(int fd, char* buff, int nLen) ;

#define secReq_LookupSrvr	SECReq_LookupSrvr
#define secSock_connect		SECSock_connect
#define secSock_getFD		SECSock_getFD
#define secSock_isConnected	SECSock_isConnected
#define secSock_close		SECSock_close
#define secSock_read		SECSock_read
#define secSock_write		SECSock_write

#define CLNT_SCCP			1
#define CLNT_TMO_DEFAULT	0
#define TCP_PORT_RETRY_CNT	5
#define MAX_CONNECTIONS		5

#define CLNT_CONN_OK       0   /* secure connection setup complete  */
#define CLNT_CONN_FAILED   1   /* secure connection setup failed.*/
#define CLNT_CONN_WAITING  2   /* secure connection setup is in progress */

int
SECReq_LookupSrvr(char* serverAddr, int serverType)
{
    CPR_ERROR("SECReq_LookupSrvr: %s %d\n", serverAddr, serverType);
    return -1;
}

int
SECSock_connect(int    appType,
				char * srvrAddrAndPort,
				int    blockingConnect,
				int    connTimeout,
				int    ipTOS)
{
    CPR_ERROR("SECSock_connect: %d %s %d %d %d\n", appType, srvrAddrAndPort, blockingConnect, connTimeout, ipTOS);
    return -1;
}

int
SECSock_getFD(int nSSL)
{
    CPR_ERROR("SECSock_getFD: %d\n", nSSL);
    return -1;
}

#ifdef NON_BLOCKING_MODE_IS_USED
int
SECSock_isConnected(int nSSL)
{
    CPR_ERROR("SECSock_isConnected: %d\n", nSSL);
    return CLNT_CONN_FAILED;
}
#endif

int
SECSock_close(int nSSL)
{
    CPR_ERROR("SECSock_close: %d\n", nSSL);
    return -1;
}

int
SECSock_read(int fd, char* buff, int nLen)
{
    CPR_ERROR("SECSock_read: %d %d\n", fd, nLen);
    return -1;
}

int
SECSock_write(int fd, char* buff, int nLen)
{
    CPR_ERROR("SECSock_write: %d %d\n", fd, nLen);
    return -1;
}


/* Macro definition for chacking a valid connid (TCP/UDP) */
#define VALID_CONNID(connid) \
       (connid >= 0 && connid < MAX_CONNECTIONS)


typedef struct
{
	cpr_socket_t      fd;           /* Socket file descriptor */
	int		ssl ;
} cpr_sec_conn_t ;

cpr_sec_conn_t cpr_sec_conn_tab[MAX_CONNECTIONS];

/**
 *
 * cpr_sec_fd_to_connid
 *
 * returns the tcp conn table index for a particular socket
 *
 * Parameters:   fd - the file descriptor
 *
 * Return Value: cpr_sec_conn_tab index
 *
 */
int
cpr_sec_fd_to_connid (cpr_socket_t fd)
{
    int i;

	if(fd == -1)
		return -1 ;

    for (i = 0; i < MAX_CONNECTIONS; ++i) {
        if (cpr_sec_conn_tab[i].fd == fd) {
            return i;
        }
    }
    return -1;
}

/*
 * cpr_sec_get_free_conn_entry ()
 *
 * Description  : This procedure returns the first free entry from the TCP
 *                conn table.
 *
 * Input Params : None.
 *
 * Returns      : Index to the Connection table (if SUCCESSFUL)
 *                -1 in case of failure.
 */
int
cpr_sec_get_free_conn_entry (void)
{
    int i;

    for (i = 0; i < MAX_CONNECTIONS; ++i) {
        if (cpr_sec_conn_tab[i].fd == -1) {
            /* Zero the connection table entry */
            memset((cpr_sec_conn_tab + i), 0, sizeof(cpr_sec_conn_t));
            return i;
        }
    }

    //CPR_ERROR("cpr_sec_get_free_conn_entry: Connection table full\n");

    return -1;
}

/*
 * cpr_sec_init_conn_table()
 * Description : Cleans up the entry associated with the connid
 *               from the cpr_sec_conn_tab
 *
 * Input : void
 *
 * Output : void
 *
 */
void
cpr_sec_init_conn_table (void)
{
    static boolean initial_call = TRUE;
    int idx;

    if (initial_call) {
        /*
         * Initialize the tcp conn table
         */
        for (idx = 0; idx < MAX_CONNECTIONS; ++idx) {
            cpr_sec_conn_tab[idx].fd = -1;
        }
        initial_call = FALSE;
    }
}

/*
 * cpr_sec_purge_entry()
 * Description : Cleans up the entry associated with the connid
 *               from the cpr_sec_conn_tab
 *
 * Input : connid
 *
 * Output : None
 *
 */
void
cpr_sec_purge_entry (int connid)
{
    cpr_sec_conn_t *entry = cpr_sec_conn_tab + connid;

    if (!VALID_CONNID(connid)) {
        //CPR_INFO("TCP:INVALID_CONNID (%ld) in TCPSOCK_CLOSE_CONNECTION", connid);
        return;
    }

    entry->fd = -1;  /* Free the connection table entry in the BEGINNING ! */
    entry->ssl= 0;

    return;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/*
 *  Copyright (c) 2004-2009 by Cisco Systems, Inc.
 *  All Rights Reserved.
 *
 *  FILENAME
 *     cpr_socket.c
 *
 *  DESCRIPTION
 *     Cisco Portable Runtime WIN layer for the socket interface
 */

#include "cpr_assert.h"

static boolean initialized = 0;

cpr_status_e
cprSocketInit (void)
{
    WSADATA wsaData;
    const int nMajor = 2;
    const int nMinor = 2;

    if (initialized == TRUE) {
        return CPR_SUCCESS;
    }

    if (WSAStartup(MAKEWORD(nMinor, nMajor), &wsaData)) {
        /* startup failed */
        return CPR_FAILURE;
    }
    if ((LOBYTE(wsaData.wVersion) != nMinor) ||
        (HIBYTE(wsaData.wVersion) != nMajor)) {
        /* startup failed */
        return CPR_FAILURE;
    }
	cpr_sec_init_conn_table() ;

    initialized = TRUE;

    return CPR_SUCCESS;
}

cpr_status_e
cprSocketCleanup (void)
{
    if (initialized == FALSE) {
        return CPR_SUCCESS;
    }

    /* wait for all of the sockets to close before cleaning up */
    cprSleep(500);

    if (WSACleanup() == SOCKET_ERROR) {
        /* startup failed */
        return CPR_FAILURE;
    }
    initialized = FALSE;

    return CPR_SUCCESS;
}

cpr_socket_t
cprSocket (uint32_t domain,
           uint32_t type,
           uint32_t protocol)
{
    cpr_socket_t s;

    s = socket(domain, type, protocol);
    if (s == -1) {
    	return INVALID_SOCKET;
    }
    return s;
}

cpr_socket_t
cprAccept (cpr_socket_t socket,
           cpr_sockaddr_t * RESTRICT addr,
           cpr_socklen_t * RESTRICT addr_len)
{
    cpr_socket_t s;

    cprAssert(addr != NULL, INVALID_SOCKET);
    cprAssert(addr_len != NULL, INVALID_SOCKET);

    s = accept(socket, (struct sockaddr *) addr, addr_len);
    if (s == -1) {
        return INVALID_SOCKET;
    }
    return s;
}

cpr_status_e
cprBind (cpr_socket_t socket,
         CONST cpr_sockaddr_t * RESTRICT addr,
         cpr_socklen_t addr_len)
{
    cprAssert(addr != NULL, CPR_FAILURE);

    return ((bind(socket, (struct sockaddr *) addr, addr_len) != 0) ?
            CPR_FAILURE : CPR_SUCCESS);
}

cpr_status_e
cprCloseSocket (cpr_socket_t socket)
{
	int	connid ;
	
	connid = cpr_sec_fd_to_connid(socket);
	if(connid == -1)
		return ((closesocket(socket) != 0) ? CPR_FAILURE : CPR_SUCCESS);
	secSock_close(cpr_sec_conn_tab[connid].ssl) ;
	cpr_sec_purge_entry(connid) ;

	return CPR_SUCCESS ;	
}

cpr_status_e
cprConnect (cpr_socket_t socket,
            SUPPORT_CONNECT_CONST cpr_sockaddr_t * RESTRICT addr,
            cpr_socklen_t addr_len)
{
	int retry = 0, retval = 0;
	cpr_status_e returnValue = CPR_FAILURE;

    cprAssert(addr != NULL, CPR_FAILURE);

    retval = connect(socket, (struct sockaddr *) addr, addr_len );

    while (retval == -1 && (cpr_errno == CPR_EAGAIN || cpr_errno == CPR_EINPROGRESS || cpr_errno == CPR_EALREADY) && retry < MAX_RETRY_FOR_EAGAIN)
    {
    	cprSleep(100);
    	retry++;
    	retval = connect(socket, (struct sockaddr *) addr, addr_len);
    }

	if (0 == retval || (-1 == retval && cpr_errno == CPR_EISCONN))
    {
    	returnValue = CPR_SUCCESS;
    }

    return returnValue;
}

cpr_status_e
cprGetPeerName (cpr_socket_t socket,
                cpr_sockaddr_t * RESTRICT addr,
                cpr_socklen_t * RESTRICT addr_len)
{
    cprAssert(addr != NULL, CPR_FAILURE);
    cprAssert(addr_len != NULL, CPR_FAILURE);

    return ((getpeername(socket, (struct sockaddr *) addr, addr_len) != 0) ?
            CPR_FAILURE : CPR_SUCCESS);
}

cpr_status_e
cprGetSockName (cpr_socket_t socket,
                cpr_sockaddr_t * RESTRICT addr,
                cpr_socklen_t * RESTRICT addr_len)
{
    cprAssert(addr != NULL, CPR_FAILURE);
    cprAssert(addr_len != NULL, CPR_FAILURE);

    return ((getsockname(socket, (struct sockaddr *) addr, addr_len) != 0) ?
            CPR_FAILURE : CPR_SUCCESS);
}

cpr_status_e
cprGetSockOpt (cpr_socket_t socket,
               uint32_t level,
               uint32_t opt_name,
               void * RESTRICT opt_val,
               cpr_socklen_t * RESTRICT opt_len)
{
    cprAssert(opt_val != NULL, CPR_FAILURE);
    cprAssert(opt_len != NULL, CPR_FAILURE);

    if (getsockopt(socket, level, opt_name, (char*)opt_val, opt_len) != 0) {
        return CPR_FAILURE;
    }
    return CPR_SUCCESS;
}

cpr_status_e
cprListen (cpr_socket_t socket,
           uint16_t backlog)
{
    return ((listen(socket, backlog) != 0) ? CPR_FAILURE : CPR_SUCCESS);
}

ssize_t
cprRecv (cpr_socket_t socket,
         void * RESTRICT buf,
         size_t len,
         int32_t flags)
{
    ssize_t rc;
	int	connid ;

    cprAssert(buf != NULL, CPR_FAILURE);

	connid = cpr_sec_fd_to_connid(socket);
	if(connid == -1)
  		rc = recv(socket, (char*)buf, len, flags);
	else
		rc = secSock_read(cpr_sec_conn_tab[connid].ssl, (char*)buf, len) ;
    if (rc == -1) {
        return SOCKET_ERROR;
    }

    return rc;
}

ssize_t
cprRecvFrom (cpr_socket_t socket,
             void * RESTRICT buf,
             size_t len,
             int32_t flags,
             cpr_sockaddr_t * RESTRICT from,
             cpr_socklen_t * RESTRICT fromlen)
{
    ssize_t rc;

    cprAssert(buf != NULL, CPR_FAILURE);
    cprAssert(from != NULL, CPR_FAILURE);
    cprAssert(fromlen != NULL, CPR_FAILURE);

    rc = recvfrom(socket, (char*)buf, len, flags, (struct sockaddr *) from, fromlen);
    if (rc == -1) {
        int err = WSAGetLastError();
        return SOCKET_ERROR;
    }
    return rc;
}

int16_t
cprSelect (uint32_t nfds,
           fd_set * RESTRICT read_fds,
           fd_set * RESTRICT write_fds,
           fd_set * RESTRICT except_fds,
           struct cpr_timeval * RESTRICT timeout)
{
    int rc;
    struct timeval tv;

    tv.tv_sec = (long) (timeout->tv_sec + (timeout->tv_usec / 1000000));
    tv.tv_usec = (long) timeout->tv_usec % 1000000;

    if ((read_fds && (read_fds->fd_count != 0)) ||
        (write_fds && (write_fds->fd_count != 0)) ||
        (except_fds && (except_fds->fd_count != 0))) {

        rc = select(nfds, read_fds, write_fds, except_fds, &tv);

        if (rc == -1) {
            int error = WSAGetLastError();

            return SOCKET_ERROR;
        }
    } else {
        /* Convert to milliseconds (seconds * 1000) + (micro seconds / 1000) */
        uint32_t sleepTime = (uint32_t)((long)tv.tv_sec * 1000) + (tv.tv_usec / 1000);
        cprSleep(sleepTime);
        rc = 0;
    }
    return (int16_t) rc;
}

ssize_t
cprSend (cpr_socket_t socket,
         CONST void *buf,
         size_t len,
         int32_t flags)
{
    ssize_t rc;
	int	connid ;

    cprAssert(buf != NULL, CPR_FAILURE);

	connid = cpr_sec_fd_to_connid(socket);
	if(connid == -1)
  	    rc = send(socket, (char*)buf, len, flags);
	else
		rc = secSock_write(cpr_sec_conn_tab[connid].ssl, (char*)buf, len) ;
    if (rc == -1) {
        return SOCKET_ERROR;
    }
    return rc;
}

ssize_t
cprSendTo (cpr_socket_t socket,
           CONST void *msg,
           size_t len,
           int32_t flags,
           CONST cpr_sockaddr_t *dest_addr,
           cpr_socklen_t dest_len)
{
    ssize_t rc;

    cprAssert(msg != NULL, CPR_FAILURE);
    cprAssert(dest_addr != NULL, CPR_FAILURE);

    rc = sendto(socket, (char*)msg, len, flags,
                (struct sockaddr *) dest_addr, dest_len);
    if (rc == -1) {
        return SOCKET_ERROR;
    }
    return rc;
}

cpr_status_e
cprSetSockOpt (cpr_socket_t socket,
               uint32_t level,
               uint32_t opt_name,
               CONST void *opt_val,
               cpr_socklen_t opt_len)
{
#ifndef NEED_TO_CALL_FOR_WIN32
	cprAssert(opt_val != NULL, CPR_FAILURE);

    return ((setsockopt(socket, level, opt_name, (char*)opt_val, opt_len) != 0) ?
            CPR_FAILURE : CPR_SUCCESS);
#else
    // Found out that the above call fails for windows. So the
    // socket created is in blocking mode. Need to enable
    // ioctlsocket() call to put the socket in non-blocking mode.
    //
    //-------------------------
    // Set the socket I/O mode: In this case FIONBIO
    // enables or disables the blocking mode for the
    // socket based on the numerical value of iMode.
    // If iMode = 0, blocking is enabled;
    // If iMode != 0, non-blocking mode is enabled.
    int iMode = 1;

    return ((ioctlsocket(socket, FIONBIO, (u_long FAR *) &iMode) != 0) ?
            CPR_FAILURE : CPR_SUCCESS);
#endif /* NEED_TO_CALL_FOR_WIN32 */

}

cpr_status_e
cprSetSockNonBlock (cpr_socket_t socket)
{
	unsigned long iMode = 1;
	return ((ioctlsocket(socket, FIONBIO, (u_long FAR *) &iMode) != 0) ? CPR_FAILURE : CPR_SUCCESS);
}

cpr_status_e
cprShutDown (cpr_socket_t socket,
             cpr_shutdown_e how)
{
    int os_how;

    switch (how) {
    case CPR_SHUTDOWN_RECEIVE:
        os_how = SD_RECEIVE;
        break;
    case CPR_SHUTDOWN_SEND:
        os_how = SD_SEND;
        break;
    default:
    case CPR_SHUTDOWN_BOTH:
        os_how = SD_BOTH;
        break;
    }

    return ((shutdown(socket, os_how) != 0) ? CPR_FAILURE : CPR_SUCCESS);
}

/**
 *
 * Lookup the secure status of the server in the CTL
 *
 * @param   ipaddr_str   server ip address
 *
 * @return  Server is not enabled for security
 *          CPR_SOC_NONSECURE
 *             
 * @todo    Security is not implemented for win32
 *       
 */
cpr_soc_sec_status_e
cprSecLookupSrvr (char* ipaddr_str)
{
    int ret;

    cprAssert(ipaddr_str != NULL, CPR_FAILURE);
    ret = secReq_LookupSrvr(ipaddr_str, SRVR_TYPE_CCM);
    if (ret == SRVR_SECURE) {
        return CPR_SOC_SECURE;
    }
    return CPR_SOC_NONSECURE;
}

#define HOST_AND_PORT_SIZE 64

/**
 * 
 * Securely connect to a remote server
 *
 * @param[in]  hostAndPort  server addr and port in host:port form
 * @param[in]  mode         blocking connect or not 
 *                          FALSE : non-blocking; TRUE: blocking
 * @param[in]  tos          TOS value
 *                          
 * @param[out] localPort    local port used for the connection
 *
 * @return     INVALID SOCKET, connect failed
 *
 * @todo       Security is not implemented for win32            
 *               
 */
cpr_socket_t
cprSecSocConnect (char *host,
                  int     port,
                  int     ipMode,
                  boolean mode,
                  uint32_t tos,
                  uint16_t *localPort)
{
	int ssl = 0 ;
	int idx;
    uint16_t loopCnt  = 0;
    uint16_t sec_port = 0;
    cpr_socket_t sock = INVALID_SOCKET;
    char hostAndPort[HOST_AND_PORT_SIZE];

    cprAssert(host != NULL, CPR_FAILURE);
    cprAssert(localPort != NULL, CPR_FAILURE);
    snprintf(hostAndPort, HOST_AND_PORT_SIZE, "%s:%d", host, port);
    while (loopCnt < TCP_PORT_RETRY_CNT) {
        ssl = secSock_connect(CLNT_SCCP,       /* type          */
                               hostAndPort,          /* host:port     */
                               mode,                 /* 1 - block     */
                               CLNT_TMO_DEFAULT,     /* timeout value */
                               0);                   /* TOS value     */
        if (ssl == -1) {
			return INVALID_SOCKET;
        }
		else
		{
			cpr_sockaddr_t local_addr;
			cpr_socklen_t local_addr_len = sizeof(cpr_sockaddr_t);

			sock = secSock_getFD(ssl) ;

			if (cprGetSockName(sock, &local_addr, &local_addr_len) != CPR_FAILURE)
			{
				cpr_sockaddr_in_t *local_addr_ptr;
				local_addr_ptr = (cpr_sockaddr_in_t *)&local_addr;
				*localPort = ntohs(local_addr_ptr->sin_port);
			}
			else
			{
				secSock_close(ssl) ;
				ssl = 0 ;
				sock = INVALID_SOCKET ;

				return INVALID_SOCKET;
			}

			// Setup the secure connection structure
		    idx = cpr_sec_get_free_conn_entry();
			if (idx == -1) {
				secSock_close(ssl) ;
				*localPort = 0 ;
				//CPR_ERROR("cpr_sec_get_free_conn_entry failed\n");
				return INVALID_SOCKET;
			}
			cpr_sec_conn_tab[idx].fd = sock ;
			cpr_sec_conn_tab[idx].ssl = ssl ;
            return (sock);
        }
    }
    return INVALID_SOCKET;
}

/**
 *
 * Determine the status of a secure connection that was initiated
 * in non-blocking mode
 *
 * @param    sock   socket descriptor
 *
 * @return   connection failed, CPR_SOC_CONN_FAILED
 * 
 * @todo     Security is not implemented for win32  
 *         
 */
cpr_soc_connect_status_e
cprSecSockIsConnected (cpr_socket_t sock)
{
#ifdef NON_BLOCKING_MODE_IS_USED
    int conn_status;
	int	connid ;

	connid = cpr_sec_fd_to_connid(sock);
	if(connid == -1)
		return CPR_SOC_CONN_FAILED;

    conn_status = secSock_isConnected(cpr_sec_conn_tab[connid].ssl);
    if (conn_status == CLNT_CONN_OK) {
        return CPR_SOC_CONN_OK;
    } else if (conn_status == CLNT_CONN_WAITING) {
        return CPR_SOC_CONN_WAITING;
    }
    return CPR_SOC_CONN_FAILED;
#else
	return CPR_SOC_CONN_OK;
#endif;
}

#ifdef CPR_USE_SOCKETPAIR
/*
 * This is really only used in the UNIX domain and not recommended
 * for use.  The API is provided only for consistency to the POSIX
 * definitions.
 */
cpr_status_e
cprSocketPair (uint32_t domain,
               uint32_t type,
               uint32_t protocol,
               int socket_vector[2])
{
    return ((socketpair(domain, type, protocol, socket_vector) != 0) ?
            CPR_FAILURE : CPR_SUCCESS);
}
#endif


/* int
 * inet_pton4(src, dst, pton)
 *	when last arg is 0: inet_aton(). with hexadecimal, octal and shorthand.
 *	when last arg is 1: inet_pton(). decimal dotted-quad only.
 * return:
 *	1 if `src' is a valid input, else 0.
 * notice:
 *	does not touch `dst' unless it's returning 1.
 */
static int
cpr_inet_pton4(const char *src, uint8_t *dst, int pton)
{
	uint32_t val;
	uint32_t digit;
	int base, n;
	unsigned char c;
	uint32_t parts[4];
	uint32_t *pp = parts;

	c = *src;
	for (;;) {
		/*
		 * Collect number up to ``.''.
		 * Values are specified as for C:
		 * 0x=hex, 0=octal, isdigit=decimal.
		 */
		if (!isdigit(c))
			return (0);
		val = 0; base = 10;
		if (c == '0') {
			c = *++src;
			if (c == 'x' || c == 'X')
				base = 16, c = *++src;
			else if (isdigit(c) && c != '9')
				base = 8;
		}
		/* inet_pton() takes decimal only */
		if (pton && base != 10)
			return (0);
		for (;;) {
			if (isdigit(c)) {
				digit = c - '0';
				if (digit >= (uint16_t)base)
					break;
				val = (val * base) + digit;
				c = *++src;
			} else if (base == 16 && isxdigit(c)) {
				digit = c + 10 - (islower(c) ? 'a' : 'A');
				if (digit >= 16)
					break;
				val = (val << 4) | digit;
				c = *++src;
			} else
				break;
		}
		if (c == '.') {
			/*
			 * Internet format:
			 *	a.b.c.d
			 *	a.b.c	(with c treated as 16 bits)
			 *	a.b	(with b treated as 24 bits)
			 *	a	(with a treated as 32 bits)
			 */
			if (pp >= parts + 3)
				return (0);
			*pp++ = val;
			c = *++src;
		} else
			break;
	}
	/*
	 * Check for trailing characters.
	 */
	if (c != '\0' && !isspace(c))
		return (0);
	/*
	 * Concoct the address according to
	 * the number of parts specified.
	 */
	n = pp - parts + 1;
	/* inet_pton() takes dotted-quad only.  it does not take shorthand. */
	if (pton && n != 4)
		return (0);
	switch (n) {

	case 0:
		return (0);		/* initial nondigit */

	case 1:				/* a -- 32 bits */
		break;

	case 2:				/* a.b -- 8.24 bits */
		if (parts[0] > 0xff || val > 0xffffff)
			return (0);
		val |= parts[0] << 24;
		break;

	case 3:				/* a.b.c -- 8.8.16 bits */
		if ((parts[0] | parts[1]) > 0xff || val > 0xffff)
			return (0);
		val |= (parts[0] << 24) | (parts[1] << 16);
		break;

	case 4:				/* a.b.c.d -- 8.8.8.8 bits */
		if ((parts[0] | parts[1] | parts[2] | val) > 0xff)
			return (0);
		val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
		break;
	}
	if (dst) {
		val = htonl(val);
		memcpy(dst, &val, INADDRSZ);
	}
	return (1);
}

/* int
 * inet_pton6(src, dst)
 *	convert presentation level address to network order binary form.
 * return:
 *	1 if `src' is a valid [RFC1884 2.2] address, else 0.
 * notice:
 *	(1) does not touch `dst' unless it's returning 1.
 *	(2) :: in a full address is silently ignored.
 */
static int
cpr_inet_pton6(const char *src, uint8_t *dst)
{
	static const char xdigits_l[] = "0123456789abcdef",
			  xdigits_u[] = "0123456789ABCDEF";
	uint8_t tmp[IN6ADDRSZ], *tp, *endp, *colonp;
	const char *xdigits, *curtok;
	int ch, saw_xdigit;
	unsigned int val;

	memset((tp = tmp), '\0', IN6ADDRSZ);
	endp = tp + IN6ADDRSZ;
	colonp = NULL;
	/* Leading :: requires some special handling. */
	if (*src == ':')
		if (*++src != ':')
			return (0);
	curtok = src;
	saw_xdigit = 0;
	val = 0;
	while ((ch = *src++) != '\0') {
		const char *pch;

		if ((pch = strchr((xdigits = xdigits_l), ch)) == NULL)
			pch = strchr((xdigits = xdigits_u), ch);
		if (pch != NULL) {
			val <<= 4;
			val |= (pch - xdigits);
			if (val > 0xffff)
				return (0);
			saw_xdigit = 1;
			continue;
		}
		if (ch == ':') {
			curtok = src;
			if (!saw_xdigit) {
				if (colonp)
					return (0);
				colonp = tp;
				continue;
			} else if (*src == '\0')
				return (0);
			if (tp + INT16SZ > endp)
				return (0);
			*tp++ = (uint8_t) (val >> 8) & 0xff;
			*tp++ = (uint8_t) val & 0xff;
			saw_xdigit = 0;
			val = 0;
			continue;
		}
		if (ch == '.' && ((tp + INADDRSZ) <= endp) &&
		    cpr_inet_pton4(curtok, tp, 1) > 0) {
			tp += INADDRSZ;
			saw_xdigit = 0;
			break;	/* '\0' was seen by inet_pton4(). */
		}
		return (0);
	}
	if (saw_xdigit) {
		if (tp + INT16SZ > endp)
			return (0);
		*tp++ = (uint8_t) (val >> 8) & 0xff;
		*tp++ = (uint8_t) val & 0xff;
	}
	if (colonp != NULL) {
		/*
		 * Since some memmove()'s erroneously fail to handle
		 * overlapping regions, we'll do the shift by hand.
		 */
		const int n = tp - colonp;
		int i;

		if (tp == endp)
			return (0);
		for (i = 1; i <= n; i++) {
			endp[- i] = colonp[n - i];
			colonp[n - i] = 0;
		}
		tp = endp;
	}
	if (tp != endp)
		return (0);
	memcpy(dst, tmp, IN6ADDRSZ);
	return (1);
}

/* int
 * inet_pton(af, src, dst)
 *	convert from presentation format (which usually means ASCII printable)
 *	to network format (which is usually some kind of binary format).
 * return:
 *	1 if the address was valid for the specified address family
 *	0 if the address wasn't valid (`dst' is untouched in this case)
 *	-1 if some other error occurred (`dst' is untouched in this case, too)
 */
int
cpr_inet_pton (int af, const char *src, void *dst)
{

	switch (af) {
	case AF_INET:
		return (cpr_inet_pton4(src, (uint8_t*)dst, 1));
	case AF_INET6:
		return (cpr_inet_pton6(src, (uint8_t*)dst));
	default:
		return (-1);
	}
	/* NOTREACHED */
}

void cpr_set_sockun_addr(cpr_sockaddr_un_t *addr, const char *name)
{
	/* COPIED FROM OTHER PLATFOMR AS A PLACE HOLDER */
	/* Bind to the local socket */
	memset(addr, 0, sizeof(cpr_sockaddr_un_t));
	addr->sun_family = AF_INET;
	sstrncpy(addr->sun_path, name, sizeof(addr->sun_path));
}
