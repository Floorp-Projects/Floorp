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

#include <string.h>

#include "cpr_types.h"
#include "cc_constants.h"
#include "cpr_socket.h"
#include "plat_api.h"

/**
 * Initialize the platform threa.
 * @todo add more explanation here.
 */
int platThreadInit(char * tname)
{
    return 0;
}

/**
 * The initial initialization function for any platform related 
 * modules
 *
 *
 * @return 0 - SUCCESS
 *        -1 - FAILURE
 */
int platInit()
{
    return 0;
}

/**
 * The initial initialization function for the debugging/logging
 * modules
 *
 *
 * @return 0 - SUCCESS
 *        -1 - FAILURE
 */
void debugInit()
{
    return ;
}

/**
 * Add cc control classifier
 */
void platAddCallControlClassifiers(unsigned long myIPAddr, unsigned short myPort,
	unsigned long cucm1IPAddr, unsigned short cucm1Port,
	unsigned long cucm2IPAddr, unsigned short cucm2Port,
	unsigned long cucm3IPAddr, unsigned short cucm3Port,
	unsigned char  protocol)
{
    return;
}

/**
 * Set ip address mode
 * e.g.
 *
 */
cpr_ip_mode_e platGetIpAddressMode()
{
    return CPR_IP_MODE_IPV4;
}

/**
 * Remove cc control classifier.
 */
void platRemoveCallControlClassifiers()
{
    return;
}

/**
 * Tell whether wifi is supported and active
 */
cc_boolean	platWlanISActive()
{
    return FALSE;
}

/**
 * Check if the netowrk interface changed.
 */
boolean	platIsNetworkInterfaceChanged()// (NOOP)
{
    return TRUE;
}

/**
 * Set active phone load name
 * @return FAILURE or true length. "-1" means no active load found.
 *
 */
int platGetActiveInactivePhoneLoadName(char * image_a, char * image_b, int len)
{
    memset(image_a, 0, len);
    memset(image_b, 0, len);

    return 0;
}

/**
 * Get or Set user defined phrases
 * @param index  the phrase index, see
 * @param phrase the return phrase holder
 * @param len the input length to cap the maximum value
 * @return SUCCESS or FAILURE
 */
int platGetPhraseText(int index, char* phrase, unsigned int len)
{
    return 0;
}

/**
 * Set the unregistration reason
 * @param reason see the unregister reason definitions.
 * @return void
 */
void platSetUnregReason(int reason)
{
    return;
}

/**
 * Get the unregistration reason code.
 * @return reason code for unregistration, see the definition.
 */
int platGetUnregReason()
{
    return 0;
}

/**
 * Set the kpml value for application.
 * @param kpml_config the kpml value
 * @return void
 */
void platSetKPMLConfig(cc_kpml_config_t kpml_config)
{
    return ;
}

/**
 * Check if a line has active MWI status
 * @param line
 * @return boolean
 */
boolean platGetMWIStatus(cc_lineid_t line)
{
    return TRUE;
}


/**
 * Secure Socket API's.
 * The pSIPCC expects the following Secure Socket APIs to be implemented in the
 * vendor porting layer.
 */

/**
 * platSecIsServerSecure
 * 
 * @brief Lookup the secure status of the server 
 *
 * This function looks at the the CCM server type by using the security library
 * and returns appropriate indication to the pSIPCC.
 *
 *
 * @return   Server is security enabled or not
 *           PLAT_SOCK_SECURE or PLAT_SOCK_NONSECURE
 *             
 * @note This API maps to the following HandyIron API:
 *  int secIsServerSecure(SecServerType type) where type should be SRVR_TYPE_CCM
 */
plat_soc_status_e platSecIsServerSecure(void)
{
    return PLAT_SOCK_NONSECURE;
}


/**
 * platSecSocConnect
 * @brief  Securely connect to a remote server
 *
 * This function uses the security library APIs to connect to a remote server.
 * @param[in]  host         server addr 
 * @param[in]  port         port number
 * @param[in]  ipMode       IP mode to indicate v6, v4 or both
 * @param[in]  mode         blocking connect or not 
 *                          FALSE: non-blocking; TRUE: blocking
 * @param[in]  tos          TOS value
 * @param[in]  connectionType Are we talking to Call-Agent
 * @param[in]  connectionMode The mode of the connection
 *                            (Authenticated/Encrypted)
 * @param[out] localPort    local port used for the connection
 *
 * @return     client socket descriptor
 *             >=0: connected or in progress
 *             INVALID SOCKET: failed
 *
 * @pre        (hostAndPort not_eq NULL)
 * @pre        (localPort   not_eq NULL)
 *             
 * @note localPort is undefined when the return value is INVALID_SOCKET
 *
 * @note This API maps to the HandyIron APIs as follows:
 * If mode == TRUE (blocking): 
 *    int secEstablishSecureConnection(const char* serverAddr, *uint32_t port, secConnectionType type)
 *    @li ipMode is UNUSED
 *    @li "host" maps to "serverAddr", "connectionType" maps to "type"
 *    @li localPort is passed in as 0
 * If mode == FALSE (non-blocking): 
 *     int secConnect(const char* serverAddr, uint32_t port, *secConnectionType type, uint32_t localPort)
 *    @li ipMode is UNUSED
 *    @li "host" maps to "serverAddr", "connectionType" maps to "type"
 *
 * @note The implementation should use the "setsockopt" to set the "tos" value passed
 * in this API on the created socket.
 *               
 */
cpr_socket_t
platSecSocConnect (char *host,
                  int     port,
                  int     ipMode,
                  boolean mode,
                  unsigned int tos,
                  plat_soc_connect_mode_e connectionMode,
                  uint16_t *localPort)
{
    return 0;
}

/**
 * platSecSockIsConnected
 * Determine the status of a secure connection that was initiated
 * in non-blocking mode
 *
 * @param[in]    sock   socket descriptor 
 *
 * @return   connection status
 *           @li connection complete: PLAT_SOCK_CONN_OK
 *           @li connection waiting:  PLAT_SOCK_CONN_WAITING
 *           @li connection failed:   PLAT_SOCK_CONN_FAILED
 *               
 * @note This API maps to the following HandyIron API:
 * int secIsConnectionReady (int connDesc)
 * The "sock" is the connection descriptor.
 */
plat_soc_connect_status_e platSecSockIsConnected (cpr_socket_t sock)
{
    return PLAT_SOCK_CONN_OK;
}

/**
 * platSecSocSend
 *
 * @brief The platSecSocSend() function is used to send data over a secure
 * socket.
 *
 * The platSecSocSend() function shall transmit a message from the specified socket to
 * its peer. The platSecSocSend() function shall send a message only when the socket is
 * connected. The length of the message to be sent is specified by the length
 * argument. If the message is too long to pass through the underlying protocol,
 * platSecSocSend() shall fail and no data is transmitted.  Delivery of the message is
 * not guaranteed.
 *
 * @param[in] soc  Specifies the socket created with cprSocket() to send
 * @param[in] buf  A pointer to the buffer of the message to send.
 * @param[in] len  Specifies the length in bytes of the message pointed to by the buffer argument.
 *
 * @return Upon successful completion, platSecSocSend() shall return the number of
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
platSecSocSend (cpr_socket_t soc,
         CONST void *buf,
         size_t len)
{
    return 0;
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
    return 0;
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
    return CPR_SUCCESS;
}

/**
 * Sets the SIS protocol version
 *
 * @param a - major version
 * @param b - minor version
 * @param c - additional version information
 *
 * @return void
 * @note the platform should store this information and provide it when asked via the platGetSISProtocolVer()
 */
void platSetSISProtocolVer(uint32_t a, uint32_t b, uint32_t c, char* name)
{
    return;
}

/**
 * Provides the SIS protocol version
 *
 * @param *a pointer to fill in the major version
 * @param *b pointer to fill in the minor version
 * @param *c pointer to fill in the additonal version
 *
 * @return void
 */
void
platGetSISProtocolVer (uint32_t *a, uint32_t *b, uint32_t *c, char* name)
{
    return;
}


void debug_bind_keyword(const char *cmd, int32_t *flag_ptr)
{
    return;
}

void debugif_add_keyword(const char *x, const char *y)
{
    return;
}

void platSetSpeakerMode(cc_boolean state)
{
    return;
}

boolean platGetSpeakerHeadsetMode()
{
    return TRUE;
}

/**
 *  platGetFeatureAllowed
 *
 *      Get whether the feature is allowed
 *
 *  @param featureId - sis feature id
 *
 *  @return  1 - allowed, 0 - not allowed
 *
 */
int platGetFeatureAllowed(cc_sis_feature_id_e featureId)
{
    return TRUE;
}


int platGetAudioDeviceStatus(plat_audio_device_t device_type)
{
    return 0;
}

/**
 * Provides the default gateway
 *
 * @param *addr the pointer to the string holding default gw address (dhcp.xxxx.gateway)
 * @return void
 */
//void platGetDefaultGW (char* addr)
//{
//    return;
//}

/*
 * Returns the default gateway
 * 
 * @param void
 * @return u_long
 */
cc_ulong_t platGetDefaultgw(){
	return 0;
}

/**
 * Sets the time based on Date header in 200 OK from REGISTER request
 * @param void
 * @return void
 */
void platSetCucmRegTime (void) {
    //Noop
}

