/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _PLAT_API_H_
#define _PLAT_API_H_

#include "cc_constants.h"
#include "cpr_socket.h"
#include "cc_types.h"

/**
 * Define unregister reason
 */
#define CC_UNREG_REASON_UNSPECIFIED                       0
//Common with what SCCP uses...need to match with J-Side
#define CC_UNREG_REASON_TCP_TIMEOUT                       10
#define CC_UNREG_REASON_CM_RESET_TCP                      12
#define CC_UNREG_REASON_CM_ABORTED_TCP                    13
#define CC_UNREG_REASON_CM_CLOSED_TCP                     14
#define CC_UNREG_REASON_REG_TIMEOUT                       17
#define CC_UNREG_REASON_FALLBACK                          18
#define CC_UNREG_REASON_PHONE_KEYPAD                      20
#define CC_UNREG_REASON_RESET_RESET                       22
#define CC_UNREG_REASON_RESET_RESTART                     23
#define CC_UNREG_REASON_PHONE_REG_REJ                     24
#define CC_UNREG_REASON_PHONE_INITIALIZED                 25
#define CC_UNREG_REASON_VOICE_VLAN_CHANGED                26
#define CC_UNREG_REASON_POWER_SAVE_PLUS                   32
//sip specific ones...need to match with J-Side
#define CC_UNREG_REASON_VERSION_STAMP_MISMATCH            100
#define CC_UNREG_REASON_VERSION_STAMP_MISMATCH_CONFIG     101
#define CC_UNREG_REASON_VERSION_STAMP_MISMATCH_SOFTKEY    102
#define CC_UNREG_REASON_VERSION_STAMP_MISMATCH_DIALPLAN   103
#define CC_UNREG_REASON_APPLY_CONFIG_RESTART              104
#define CC_UNREG_REASON_CONFIG_RETRY_RESTART              105
#define CC_UNREG_REASON_TLS_ERROR                         106
#define CC_UNREG_REASON_RESET_TO_INACTIVE_PARTITION       107
#define CC_UNREG_REASON_VPN_CONNECTIVITY_LOST             108

#define CC_IPPROTO_UDP 17
#define CC_IPPROTO_TCP 6


/**
 * socket security status
 */
typedef enum
{
    PLAT_SOCK_SECURE,
    PLAT_SOCK_NONSECURE
} plat_soc_status_e;

/**
 * socket connection status
 */
typedef enum
{
    PLAT_SOCK_CONN_OK,
    PLAT_SOCK_CONN_WAITING,
    PLAT_SOCK_CONN_FAILED
} plat_soc_connect_status_e;

/**
 * socket connection type
 */
typedef enum
{
    PLAT_SOCK_CUCM
} plat_soc_connect_type_e;

/**
 * socket connection mode
 */
typedef enum
{
    PLAT_SOCK_AUTHENTICATED,
    PLAT_SOCK_ENCRYPTED,
    PLAT_SOCK_NON_SECURE
} plat_soc_connect_mode_e;

/**
 * psipcc core debug categories
 */
typedef enum
{
    CC_DEBUG_CCAPP,
    CC_DEBUG_CONFIG_CACHE,
    CC_DEBUG_SIP_ADAPTER,
    CC_DEBUG_CCAPI,
    CC_DEBUG_CC_MSG,
    CC_DEBUG_FIM,
    CC_DEBUG_FSM,
    CC_DEBUG_AUTH,
    CC_DEBUG_GSM,
    CC_DEBUG_LSM,
    CC_DEBUG_FSM_CAC,
    CC_DEBUG_DCSM,
    CC_DEBUG_SIP_TASK,
    CC_DEBUG_SIP_STATE,
    CC_DEBUG_SIP_MSG,
    CC_DEBUG_SIP_REG_STATE,
    CC_DEBUG_SIP_TRX,
    CC_DEBUG_TIMERS,
    CC_DEBUG_SIP_DM,
    CC_DEBUG_CCDEFAULT, /* Always ON by default */
    CC_DEBUG_DIALPLAN,
    CC_DEBUG_KPML,
    CC_DEBUG_REMOTE_CC,
    CC_DEBUG_SIP_PRESENCE,
    CC_DEBUG_CONFIG_APP,
    CC_DEBUG_CALL_EVENT,
    CC_DEBUG_PLAT,
    CC_DEBUG_NOTIFY,
    CC_DEBUG_CPR_MEMORY, /* Has additional parameters - Tracking/poison */
    CC_DEBUG_MAX        /* NOT USED */
} cc_debug_category_e;


/**
 * debug show categories
 */
typedef enum
{
    CC_DEBUG_SHOW_FSMCNF,
    CC_DEBUG_SHOW_FSMDEF,
    CC_DEBUG_SHOW_FSMXFR,
    CC_DEBUG_SHOW_FSMB2BCNF,
    CC_DEBUG_SHOW_DCSM,
    CC_DEBUG_SHOW_FIM,
    CC_DEBUG_SHOW_FSM,
    CC_DEBUG_SHOW_LSM,
    CC_DEBUG_SHOW_BULK_REGISTER,
    CC_DEBUG_SHOW_KPML,
    CC_DEBUG_SHOW_REMOTE_CC,
    CC_DEBUG_SHOW_CONFIG_CACHE,
    CC_DEBUG_SHOW_SUBS_STATS,
    CC_DEBUG_SHOW_PUBLISH_STATS,
    CC_DEBUG_SHOW_REGISTER,
    CC_DEBUG_SHOW_DIALPLAN,
    CC_DEBUG_SHOW_CPR_MEMORY, /* Has additional parameters -
                                 config/heap-gaurd/stat/tracking. */
    CC_DEBUG_SHOW_MAX
} cc_debug_show_options_e;

/**
 * debug clear categories
 */
typedef enum
{
    CC_DEBUG_CLEAR_CPR_MEMORY,
    CC_DEBUG_CLEAR_MAX
} cc_debug_clear_options_e;

/**
 * cpr memory debug sub-categories
 */
typedef enum
{
    CC_DEBUG_CPR_MEM_TRACKING,
    CC_DEBUG_CPR_MEM_POISON
} cc_debug_cpr_mem_options_e;

/**
 * cpr memory clear sub-commands
 */
typedef enum
{
    CC_DEBUG_CLEAR_CPR_TRACKING,
    CC_DEBUG_CLEAR_CPR_STATISTICS
} cc_debug_clear_cpr_options_e;

/**
  * cpr memory show sub-commands
  */
typedef enum
{
    CC_DEBUG_SHOW_CPR_CONFIG,
    CC_DEBUG_SHOW_CPR_HEAP_GUARD,
    CC_DEBUG_SHOW_CPR_STATISTICS,
    CC_DEBUG_SHOW_CPR_TRACKING
} cc_debug_show_cpr_options_e;

/**
 * Enabling/disabling debugs
 */
typedef enum
{
    CC_DEBUG_DISABLE,
    CC_DEBUG_ENABLE
} cc_debug_flag_e;

// Corresponds to the values for XML tags DHCPv4Status and DHCPv6Status
typedef enum {
       DHCP_STATUS_GOOD = 1,
       DHCP_STATUS_TIMEOUT,
       DHCP_STATUS_DISABLED
 } dhcp_status_e;


// Corresponds to the value for XML tag for DNSStatusUnifiedCMX
typedef enum {
   DNS_STATUS_GOOD = 1,
   DNS_STATUS_TIMEOUT,
   DNS_STATUS_DID_NOT_RESOLVE,
   DNS_STATUS_NA_IP_CONFIGURED
} ucm_dns_resolution_status_e;

#define LEN32   32
#define IP_ADDR_MAX_LEN       32
#define PORT_MAX_LEN          20
#define STATUS_MAX_LEN        4
#define LEN80   80
#define WIRED_PROP_PREFIX     "dhcp.eth0"
#define WIRELESS_PROP_PREFIX  "dhcp.mlan0"
#define WIRED_INT 1
#define WIFI_INT  2


/**
 * Called by the thread to initialize any thread specific data
 * once the thread is created.
 *
 * @param[in] tname       thread name
 *
 * @return 0 - SUCCESS
 *        -1 - FAILURE
 */
int platThreadInit(char * tname);

/**
 * The initial initialization function for any platform related
 * modules
 *
 *
 * @return 0 - SUCCESS
 *        -1 - FAILURE
 */
int platInit();

/**
 * The initial initialization function for the debugging/logging
 * modules
 *
 */
void debugInit();

/**
 * Get device model that will be sent to cucm in the UserAgent header
 *
 * @return char * Pointer to the string containing the model number of the phone.
 */
char *platGetModel();

/**
 * plat_audio_device_t
 * Enums for indicating audio device
 */
typedef enum vcm_audio_device_type {
    VCM_AUDIO_DEVICE_NONE,
    VCM_AUDIO_DEVICE_HEADSET,
    VCM_AUDIO_DEVICE_SPEAKER
} plat_audio_device_t;


/**
 * Add cc control classifier
 *
 * Called by SIP stack to specify addresses and ports that will be used for call control
 *
 * @param[in] myIPAddr - phone local interface IP Address
 * @param[in] myPort - phone local interface Port
 * @param[in] cucm1IPAddr - CUCM 1 IP Address
 * @param[in] cucm1Port - CUCM 1 Port
 * @param[in] cucm2IPAddr - CUCM 2 IP Address
 * @param[in] cucm2Port - CUCM 2 Port
 * @param[in] cucm3IPAddr - CUCM 3 IP Address
 * @param[in] cucm3Port - CUCM 3 Port
 * @param[in] protocol - CC_IPPROTO_UDP or CC_IP_PROTO_TCP
 *
 * @note : Needed only if using WiFi. If not using Wifi please provide a stub
 */
void platAddCallControlClassifiers(unsigned long myIPAddr, unsigned short myPort,
	unsigned long cucm1IPAddr, unsigned short cucm1Port,
	unsigned long cucm2IPAddr, unsigned short cucm2Port,
	unsigned long cucm3IPAddr, unsigned short cucm3Port,
	unsigned char  protocol);

/**
 * Remove cc control classifier.
 *
 * Undo platAddCallControlClassifiers
 */
void platRemoveCallControlClassifiers();

/**
 * Tell whether wifi is supported and active
 *
 * @return boolean wether WLAN is active or not
 */
cc_boolean	platWlanISActive();

/**
 * Check if the netowrk interface changed.
 *
 * @return boolean returns TRUE if the network interface has changed
 *
 * @note Most common case is for softphone clients where if a PC is
 * undocked the network interface changes from wired to wireless.
 */
boolean	platIsNetworkInterfaceChanged();

/**
 * Get active phone load name
 *
 * Returns the phone images in the active and inactive partitions
 * The phone reports these phone loads to CUCM for display on the Admin page
 *
 * @param[in] image_a : Populate the image name from partition a
 * @param[in] image_b : Populate the image name from partition b
 * @param[in] len : Length of the pointers for image_a and image_b
 * @return 1 - image_a is active
 *         2 - image_b is active
 *        -1 - Failure
 */
int platGetActiveInactivePhoneLoadName(char * image_a, char * image_b, int len);

/**
 * Get localized phrase for the specified index
 *
 * @param[in] index  the phrase index, see
 * @param[in] phrase the return phrase holder
 * @param[in] len the input length to cap the maximum value
 *
 * @return SUCCESS or FAILURE
 */
int platGetPhraseText(int index, char* phrase, unsigned int len);

/**
 * Set the unregistration reason
 *
 * @param[in] reason see the unregister reason definitions.
 *
 * @note we expect the platform to save this value in Non Volatile memory
 * This will be retrieved later by using platGetUnregReason. This reason is reported to CUCM
 */
void platSetUnregReason(int reason);


/**
 * Get the unregistration reason code.
 *
 * @return reason code for unregistration
 */
int platGetUnregReason();

/**
 * Sets the time based on Date header in 200 OK from REGISTER request
 * @param void
 * @return void
 */
void platSetCucmRegTime (void);

/**
 * Set the kpml value for application.
 *
 * @param kpml_config the kpml value
 */
void platSetKPMLConfig(cc_kpml_config_t kpml_config);

/**
 * Check if a line has active MWI status
 *
 * @param line
 *
 * @return boolean
 *
 * @note The stack doesn't store the MWI status and expects
 * the application to store that information. The stack
 * queries the mwi status from the application using this method.
 */
boolean platGetMWIStatus(cc_lineid_t line);

/**
 * Check if the speaker or headset is enabled.
 *
 * @return boolean if the speaker or headset is enabled, returns true.
 */
boolean platGetSpeakerHeadsetMode();

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
plat_soc_status_e platSecIsServerSecure(void);


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
 *    @li "host" maps to "serverAddr", "type" should be determined by an application and use the value from SecServerType.
 *    @li localPort is passed in as 0
 * If mode == FALSE (non-blocking):
 *     int secConnect(const char* serverAddr, uint32_t port, *secConnectionType type, uint32_t localPort)
 *    @li ipMode is UNUSED
 *    @li "host" maps to "serverAddr", "type" should be determined by an application and use the value from SecServerType.
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
                  cc_uint16_t *localPort);

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
plat_soc_connect_status_e platSecSockIsConnected (cpr_socket_t sock);

/**
 * platGenerateCryptoRand
 * @brief Generates a Random Number
 *
 * Generate crypto graphically random number for a desired length.
 * The function is expected to be much slower than the cpr_rand().
 * This function should be used when good random number is needed
 * such as random number that to be used for SRTP key for an example.
 *
 * @param[in] buf  - pointer to the buffer to store the result of random
 *                   bytes requested.
 * @param[in] len  - pointer to the length of the desired random bytes.
 *             When calling the function, the integer's value
 *             should be set to the desired number of random
 *             bytes ('buf' should be of at least this size).
 *             upon success, its value will be set to the
 *             actual number of random bytes being returned.
 *             (realistically, there is a maximum number of
 *             random bytes that can be returned at a time.
 *             if the caller request more than that, the
 *             'len' will indicate how many bytes are actually being
 *             returned) on failure, its value will be set to 0.
 *
 * @return
 *     1 - success.
 *     0 - fail.
 *
 * @note The intent of this function is to generate a cryptographically strong
 * random number. Vendors can map this to HandyIron or OpenSSL random number
 * generation functions.
 *
 * @note This API maps to the following HandyIron API:
 * int secGetRandomData(uint8_t *buf, uint32_t size). Also note that a
 * "secAddEntropy(...)" may be required the first time to feed entropy data to
 * the random number generator.
 */
int platGenerateCryptoRand(cc_uint8_t *buf, int *len);

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
         size_t len);

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
         size_t len);

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
platSecSocClose (cpr_socket_t soc);

/**
 * Sets the SIS protocol version
 *
 * @param a - major version
 * @param b - minor version
 * @param c - additional version information
 * @param name - version name
 *
 * @return void
 * @note the platform should store this information and provide it when asked via the platGetSISProtocolVer()
 */

void platSetSISProtocolVer(cc_uint32_t a, cc_uint32_t b, cc_uint32_t c, char* name);

/**
 * Provides the SIS protocol version
 *
 * @param *a pointer to fill in the major version
 * @param *b pointer to fill in the minor version
 * @param *c pointer to fill in the additonal version
 * @param *name pointer to fill in the version name
 *
 * @return void
 */
void
platGetSISProtocolVer (cc_uint32_t *a, cc_uint32_t *b, cc_uint32_t *c, char* name);

/**
 * Provides the local IP address
 *
 * @return char *  returns the local IP address
 */
char *platGetIPAddr();

/**
 * Provides the local MAC address
 *
 * @param *maddr the pointer to the string holding MAC address
 *             in the MAC address format after converting from string format.
 * @return void
 */
void platGetMacAddr(char *addr);


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
int platGetFeatureAllowed(cc_sis_feature_id_e featureId);


/**
 * Set the Status message for failure reasons
 * @param char *msg
 * @return void
 */
void platSetStatusMessage(char *msg);

/**
 * The equivalent of the printf function.
 *
 * This function MUST be implemented by the vendors. This is called by the core
 * library whenever some debugging information needs to be printed out.
 * call this function in order to clear the CPR Memory/Tracking statistics
 *
 * Please also be aware that cpr_stdio.h has other logging functions as well.
 * Vendors need to implement this function and the functions (err_msg,
 * buginf....etc) found in cpr_stdio.h
 *
 * @param[in] _format  format string
 * @param[in] ...     variable arg list
 *
 * @return  Return code from printf
 */
int debugif_printf(const char *_format, ...);

/**
 * Enable / disable speaker
 *
 * @param[in] state - true -> enable speaker, false -> disable speaker
 *
 * @return void
 */

void platSetSpeakerMode(cc_boolean state);

/**
 * Get the status (on/off) of the audio device
 *
 * @param[in]  device_type - headset or speaker (see vcm_audio_device_t)
 *
 * @return 1 -> On, 0 -> off, ERROR -> unknown (error)
 */

int platGetAudioDeviceStatus(plat_audio_device_t device_type);

/*
 * Returns the default gateway
 *
 * @param void
 * @return u_long
 */
cc_ulong_t platGetDefaultgw();


#endif /* _PLATFORM_API_H_ */
