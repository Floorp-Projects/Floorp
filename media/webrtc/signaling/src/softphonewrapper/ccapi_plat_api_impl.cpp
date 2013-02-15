/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"

#include "CC_Common.h"
#include "csf_common.h"
#ifdef WIN32
#include <windows.h>
#else
#ifdef LINUX
// for platGetIPAddr
#include <sys/ioctl.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <fcntl.h>
#endif
#endif

#include "cpr_string.h"

static const char* logTag = "sipcc";

extern "C"
{
#include "plat_api.h"
#include <stdarg.h>


void NotifyStateChange (cc_callid_t callid, int32_t state) {
    //Don't need anything here.
    //Call state change are notified to us via SIPCC "high level" API
}

#ifndef OSX
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
int platGetFeatureAllowed(cc_sis_feature_id_e featureId) {
    return 1;
}

/**
 * Set the Status message for failure reasons
 * @param char *msg
 * @return void
 */
void platSetStatusMessage(char *msg) {
}

 /**
  * Sets the time based on Date header in 200 OK from REGISTER request
  * @param void
  * @return void
  */
void platSetCucmRegTime (void) {
}

/**
 * Enable / disable speaker
 *
 * @param[in] state - true -> enable speaker, false -> disable speaker
 *
 * @return void
 */
extern "C" void platSetSpeakerMode(cc_boolean state) {
}

/**
 * Get the status (on/off) of the audio device
 *
 * @param[in]  device_type - headset or speaker (see vcm_audio_device_t)
 *
 * @return 1 -> On, 0 -> off, ERROR -> unknown (error)
 */
int platGetAudioDeviceStatus(plat_audio_device_t device_type) {
    //Tell SIPCC what the current audio path is by return 1 for one of either: headset or speaker.
    return 1;
}

/**
 * Check if the speaker or headset is enabled.
 *
 * @return boolean if the speaker or headset is enabled, returns true.
 */
boolean platGetSpeakerHeadsetMode() {
    return TRUE;
}
#endif

/**
 * Provides the local MAC address
 *
 * @param *maddr the pointer to the string holding MAC address
 *             in the MAC address format after converting from string format.
 * @return void
 */
void platGetMacAddr(char *maddr) {
    //Strictly speaking the code using this is not treating this as a string.
    //It's taking the first 6 bytes out of the buffer, and printing these
    //directly, so it's not enough to just make the first byte '\0' need
    //to set all of the bytes in range 0-5 equal to '\0'.
    //Have to assume here that the buffer is big enough.
    for (int i=0; i<6; i++)
    {
        *(maddr+i) = '\0';
    }
}

#ifndef OSX
/**
 * Called by the thread to initialize any thread specific data
 * once the thread is created.
 *
 * @param[in] tname       thread name
 *
 * @return 0 - SUCCESS
 *        -1 - FAILURE
 */
int platThreadInit(char * tname) {
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
int platInit() {
    return 0;
}

/**
 * The initial initialization function for the debugging/logging
 * modules
 *
 */
void debugInit() {
    return ;
}

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
	unsigned char  protocol) {
    //Needed only if using WiFi. If not using Wifi please provide a stub
}

/**
 * Remove cc control classifier.
 *
 * Undo platAddCallControlClassifiers
 */
void platRemoveCallControlClassifiers() {
    //Needed only if using WiFi. If not using Wifi please provide a stub
}

/**
 * Set ip address mode
 * e.g.
 *
 */
cpr_ip_mode_e platGetIpAddressMode() {
    return CPR_IP_MODE_IPV4;
}

/**
 * Tell whether wifi is supported and active
 *
 * @return boolean wether WLAN is active or not
 */
cc_boolean	platWlanISActive() {
    return FALSE;
}

/**
 * Check if the network interface changed.
 *
 * @return boolean returns TRUE if the network interface has changed
 *
 * @note Most common case is for softphone clients where if a PC is
 * undocked the network interface changes from wired to wireless.
 */
boolean	platIsNetworkInterfaceChanged() {
    //We're OK with this
    return FALSE;
}

/**
 * Get active phone load name
 *
 * Returns the phone images in the active and inactive partitions
 * The phone reports these phone loads to CUCM for display on the Admin page
 *
 * @param[in] image_a : Populate the image name from partition a
 * @param[in] image_b : Populate the image name from partition b
 * @param[in] len : Length of the pointers for image_a and image_b
 * @return 1 - image_a is active.
 *         Anything other than 1 - image_b is active
 */
int platGetActiveInactivePhoneLoadName(char * image_a, char * image_b, int len) {
    if (image_a != NULL)
    {
        sstrncpy(image_a, "image_a", len);
    }

    if (image_b != NULL)
    {
        sstrncpy(image_b, "image_b", len);
    }

    return 1;
}

/**
 * Get or Set user defined phrases
 * @param index  the phrase index, see
 * @param phrase the return phrase holder
 * @param len the input length to cap the maximum value
 * @return SUCCESS or FAILURE
 */
int platGetPhraseText(int index, char* phrase, unsigned int len) {
    //Need to copy something into "phrase" as this is used as a prefix
    //in a starts with comparison (use strncmp) that will match against
    //any string if the prefix is empty. Also in some places an
    //uninitialized buffer is passed in as "phrase", so if we don't
    //do something then SIPCC will go on the use the uninitialized
    //buffer.

    if (phrase == NULL)
    {
        return CC_FAILURE;
    }

    sstrncpy(phrase, "?????", len);

    return (int) CC_SUCCESS;
}

/**
 * Get the unregistration reason code.
 * @return reason code for unregistration, see the definition.
 */
int platGetUnregReason() {
    return 0;
}

/**
 * Set the unregistration reason
 * @param reason see the unregister reason definitions.
 * @return void
 */
void platSetUnregReason(int reason) {
    //We may need to persist this for CUCM. WHen we restart next time call to platGetUnregReason above tells CUCM what why we unregistered last time.
    typedef struct _unRegRreasonEnumPair {
        int reason;
        const char * pReasonStr;
    } unRegRreasonEnumPair;

    static unRegRreasonEnumPair unRegReasons[] = {
      { CC_UNREG_REASON_UNSPECIFIED, "CC_UNREG_REASON_UNSPECIFIED" },
      { CC_UNREG_REASON_TCP_TIMEOUT, "CC_UNREG_REASON_TCP_TIMEOUT" },
      { CC_UNREG_REASON_CM_RESET_TCP, "CC_UNREG_REASON_CM_RESET_TCP" },
      { CC_UNREG_REASON_CM_ABORTED_TCP, "CC_UNREG_REASON_CM_ABORTED_TCP" },
      { CC_UNREG_REASON_CM_CLOSED_TCP, "C_UNREG_REASON_CM_CLOSED_TCP" },
      { CC_UNREG_REASON_REG_TIMEOUT, "CC_UNREG_REASON_REG_TIMEOUT" },
      { CC_UNREG_REASON_FALLBACK, "CC_UNREG_REASON_FALLBACK" },
      { CC_UNREG_REASON_PHONE_KEYPAD, "CC_UNREG_REASON_PHONE_KEYPAD" },
      { CC_UNREG_REASON_RESET_RESET, "CC_UNREG_REASON_RESET_RESET" },
      { CC_UNREG_REASON_RESET_RESTART, "CC_UNREG_REASON_RESET_RESTART" },
      { CC_UNREG_REASON_PHONE_REG_REJ, "CC_UNREG_REASON_PHONE_REG_REJ" },
      { CC_UNREG_REASON_PHONE_INITIALIZED, "CC_UNREG_REASON_PHONE_INITIALIZED" },
      { CC_UNREG_REASON_VOICE_VLAN_CHANGED, "CC_UNREG_REASON_VOICE_VLAN_CHANGED" },
      { CC_UNREG_REASON_POWER_SAVE_PLUS, "CC_UNREG_REASON_POWER_SAVE_PLUS" },
      { CC_UNREG_REASON_VERSION_STAMP_MISMATCH, "CC_UNREG_REASON_VERSION_STAMP_MISMATCH" },
      { CC_UNREG_REASON_VERSION_STAMP_MISMATCH_CONFIG, "CC_UNREG_REASON_VERSION_STAMP_MISMATCH_CONFIG" },
      { CC_UNREG_REASON_VERSION_STAMP_MISMATCH_SOFTKEY, "CC_UNREG_REASON_VERSION_STAMP_MISMATCH_SOFTKEY" },
      { CC_UNREG_REASON_VERSION_STAMP_MISMATCH_DIALPLAN, "CC_UNREG_REASON_VERSION_STAMP_MISMATCH_DIALPLAN" },
      { CC_UNREG_REASON_APPLY_CONFIG_RESTART, "CC_UNREG_REASON_APPLY_CONFIG_RESTART" },
      { CC_UNREG_REASON_CONFIG_RETRY_RESTART, "CC_UNREG_REASON_CONFIG_RETRY_RESTART" },
      { CC_UNREG_REASON_TLS_ERROR, "CC_UNREG_REASON_TLS_ERROR" },
      { CC_UNREG_REASON_RESET_TO_INACTIVE_PARTITION, "CC_UNREG_REASON_RESET_TO_INACTIVE_PARTITION" },
      { CC_UNREG_REASON_VPN_CONNECTIVITY_LOST, "CC_UNREG_REASON_VPN_CONNECTIVITY_LOST" }
    };

    for (int i=0; i< (int) csf_countof(unRegReasons); i++)
    {
        unRegRreasonEnumPair * pCurrentUnRegReasonPair = &unRegReasons[i];

        if (pCurrentUnRegReasonPair->reason == reason)
        {
            CSFLogDebug( logTag, "platSetUnregReason(%s)", pCurrentUnRegReasonPair->pReasonStr);
            return;
        }
    }

    CSFLogError( logTag, "Unknown reason code (%d) passed to platSetUnregReason()", reason);
}


#endif

#ifndef OSX
/**
 * Set the kpml value for application.
 * @param kpml_config the kpml value
 * @return void
 */
void platSetKPMLConfig(cc_kpml_config_t kpml_config) {
}

/**
 * Check if a line has active MWI status
 * @param line
 * @return boolean
 */
boolean platGetMWIStatus(cc_lineid_t line) {
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
plat_soc_status_e platSecIsServerSecure(void) {
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
                  uint16_t *localPort) {
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
plat_soc_connect_status_e platSecSockIsConnected (cpr_socket_t sock) {
    return PLAT_SOCK_CONN_OK;
}
#endif //#endif !OSX

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
#ifdef LINUX
int platGenerateCryptoRand(uint8_t *buf, int *len) {
    int fd;
    int rc = 0;
    ssize_t s;

    if ((fd = open("/dev/urandom", O_RDONLY)) == -1) {
          CSFLogDebug( logTag, "Failed to open prng driver");
        return 0;
    }

    /*
     * Try to read the given amount of bytes from the PRNG device.  We do not
     * handle short reads but just return the number of bytes read from the
     * device.  The caller has to manage this.
     * E.g. gsmsdp_generate_key() in core/gsm/gsm_sdp_crypto.c
     */

    s = read(fd, buf, (size_t) *len);

    if (s > 0) {
        *len = s;
        rc = 1; /* Success */
    } else {
        *len = 0;
        rc = 0; /* Failure */
    }


    (void) close(fd);
    return rc;
}
#else
int platGenerateCryptoRand(uint8_t *buf, int *len) {
    return 0;
}
#endif//!LINUX

#ifndef OSX

/*
  <Umesh> The version is to regulate certain features like Join, which are added in later releases.
  This is exchanged during registration with CUCM. It is a sheer luck that join may be working in some cases.
  In this case Set API you have to store each of those API and return the same value in get.

  We can go over all platform API’ and see which one are mandatory, we can do this in an email or in a  meeting.
  Please send out list methods for which you need clarification.
*/
static cc_uint32_t majorSIS=1, minorSIS=0, addtnlSIS =0;
static char sis_ver_name[CC_MAX_LEN_REQ_SUPP_PARAM_CISCO_SISTAG] = {0};

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
void platSetSISProtocolVer(cc_uint32_t a, cc_uint32_t b, cc_uint32_t c, char* name) {
   majorSIS = a;
   minorSIS = b;
   addtnlSIS = c;

   if (name) {
       sstrncpy(sis_ver_name, name, csf_countof(sis_ver_name));
   } else {
       *sis_ver_name = '\0';
   }
}

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
platGetSISProtocolVer (cc_uint32_t *a, cc_uint32_t *b, cc_uint32_t *c, char* name) {

    if (a != NULL)
    {
        *a = majorSIS;
    }

    if (b != NULL)
    {
        *b = minorSIS;
    }

    if (c != NULL)
    {
        *c = addtnlSIS;
    }

    if (name != NULL)
    {
       sstrncpy(name, sis_ver_name, CC_MAX_LEN_REQ_SUPP_PARAM_CISCO_SISTAG);
    }

    return;
}

void debug_bind_keyword(const char *cmd, int32_t *flag_ptr) {
    return;
}

void debugif_add_keyword(const char *x, const char *y) {
    return;
}
#endif

// Keep this when we are building the sipcc library without it, so that we capture logging
int debugif_printf(const char *_format, ...) {
    va_list ap;
    va_start(ap, _format);
	CSFLogDebugV( logTag, _format, ap);
    va_end(ap);
	return 1;	// Fake a "happy" return value.
}

}

