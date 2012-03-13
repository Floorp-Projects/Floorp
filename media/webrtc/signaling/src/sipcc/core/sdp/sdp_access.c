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

#include "sdp_os_defs.h"
#include "sdp.h"
#include "sdp_private.h"
#include "ccsip_sdp.h"
#include "rtp_defs.h"

/* Function:    sdp_find_media_level
 * Description: Find and return a pointer to the specified media level,
 *              if it exists.
 *              Note: This is not an API for the application but an internal
 *              routine used by the SDP library.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The media level to find.
 * Returns:     Pointer to the media level or NULL if not found.
 */
sdp_mca_t *sdp_find_media_level (sdp_t *sdp_p, u16 level)
{
    int i;
    sdp_mca_t *mca_p = NULL;

    if ((level >= 1) && (level <= sdp_p->mca_count)) {
        for (i=1, mca_p = sdp_p->mca_p; 
             ((i < level) && (mca_p != NULL));
             mca_p = mca_p->next_p, i++) {

             /*sa_ignore EMPTYLOOP*/
             ; /* Do nothing. */
        }
    }
    
    return (mca_p);
}

/* Function:    sdp_version_valid
 * Description: Returns true or false depending on whether the version
 *              set for this SDP is valid.  Currently the only valid
 *              version is 0.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 * Returns:     TRUE or FALSE.
 */
tinybool sdp_version_valid (void *sdp_ptr) 
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
    }

    if (sdp_p->version == SDP_INVALID_VALUE) {
        return (FALSE);
    } else {
        return (TRUE);
    }
}

/* Function:    sdp_get_version
 * Description: Returns the version value set for the given SDP.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 * Returns:     Version value.
 */
int32 sdp_get_version (void *sdp_ptr) 
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    return (sdp_p->version);
}

/* Function:    sdp_set_version
 * Description: Sets the value of the version parameter for the v= version
 *              token line.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              version     Version to set.
 * Returns:     SDP_SUCCESS
 */
sdp_result_e sdp_set_version (void *sdp_ptr, int32 version) 
{
    sdp_t *sdp_p = (sdp_t*)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    sdp_p->version = version;
    return (SDP_SUCCESS);
}

/* Function:    sdp_owner_valid
 * Description: Returns true or false depending on whether the owner
 *              token line has been defined for this SDP.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 * Returns:     TRUE or FALSE.
 */
tinybool sdp_owner_valid (void *sdp_ptr) 
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
    }

    if ((sdp_p->owner_name[0] == '\0') ||
        (sdp_p->owner_network_type == SDP_NT_INVALID) ||
        (sdp_p->owner_addr_type == SDP_AT_INVALID) ||
        (sdp_p->owner_addr[0] == '\0')) {
        return (FALSE);
    } else {
        return (TRUE);
    }
}

/* Function:    sdp_get_owner_username
 * Description: Returns a pointer to the value of the username parameter
 *              from the o= owner token line.  Value is returned as a 
 *              const ptr and so cannot be modified by the application.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 * Returns:     Version value.
 */
const char *sdp_get_owner_username (void *sdp_ptr) 
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (NULL);
    }

    return (sdp_p->owner_name);
}

/* Function:    sdp_get_owner_sessionid
 * Description: Returns the session id parameter from the o= owner token 
 *              line.  Because the value may be larger than 32 bits, this
 *              parameter is returned as a string, though has been verified
 *              to be numeric.  Value is returned as a const ptr and so 
 *              cannot be modified by the application.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 * Returns:     Ptr to owner session id or NULL.
 */
const char *sdp_get_owner_sessionid (void *sdp_ptr)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (NULL);
    }

    return (sdp_p->owner_sessid);
}

/* Function:    sdp_get_owner_version
 * Description: Returns the version parameter from the o= owner token 
 *              line.  Because the value may be larger than 32 bits, this
 *              parameter is returned as a string, though has been verified
 *              to be numeric.  Value is returned as a const ptr and so 
 *              cannot be modified by the application.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 * Returns:     Ptr to owner version or NULL.
 */
const char *sdp_get_owner_version (void *sdp_ptr)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (NULL);
    }

    return (sdp_p->owner_version);
}

/* Function:    sdp_get_owner_network_type
 * Description: Returns the network type parameter from the o= owner token 
 *              line.  If network type has not been set SDP_NT_INVALID will
 *              be returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 * Returns:     Network type or SDP_NT_INVALID.
 */
sdp_nettype_e sdp_get_owner_network_type (void *sdp_ptr)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_NT_INVALID);
    }

    return (sdp_p->owner_network_type);
}

/* Function:    sdp_get_owner_address_type
 * Description: Returns the address type parameter from the o= owner token 
 *              line.  If address type has not been set SDP_AT_INVALID will
 *              be returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 * Returns:     Address type or SDP_AT_INVALID.
 */
sdp_addrtype_e sdp_get_owner_address_type (void *sdp_ptr)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_AT_INVALID);
    }

    return (sdp_p->owner_addr_type);
}

/* Function:    sdp_get_owner_address
 * Description: Returns the address parameter from the o= owner token 
 *              line.  Value is returned as a const ptr and so 
 *              cannot be modified by the application.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 * Returns:     Ptr to address or NULL.
 */
const char *sdp_get_owner_address (void *sdp_ptr)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (NULL);
    }

    return (sdp_p->owner_addr);
}

/* Function:    sdp_set_owner_username
 * Description: Sets the value of the username parameter for the o= owner
 *              token line.  The string is copied into the SDP structure
 *              so application memory will not be referenced by the SDP lib.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              username    Ptr to the username string.
 * Returns:     SDP_SUCCESS
 */
sdp_result_e sdp_set_owner_username (void *sdp_ptr, const char *username)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    sstrncpy(sdp_p->owner_name, username, sizeof(sdp_p->owner_name));
    return (SDP_SUCCESS);
}

/* Function:    sdp_set_owner_username
 * Description: Sets the value of the session id parameter for the o= owner
 *              token line.  The string is copied into the SDP structure
 *              so application memory will not be referenced by the SDP lib.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              sessionid   Ptr to the sessionid string.
 * Returns:     SDP_SUCCESS
 */
sdp_result_e sdp_set_owner_sessionid (void *sdp_ptr, const char *sessionid)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    sstrncpy(sdp_p->owner_sessid, sessionid, sizeof(sdp_p->owner_sessid));
    return (SDP_SUCCESS);
}

/* Function:    sdp_set_owner_version
 * Description: Sets the value of the version parameter for the o= owner
 *              token line.  The string is copied into the SDP structure
 *              so application memory will not be referenced by the SDP lib.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              version     Ptr to the version string.
 * Returns:     SDP_SUCCESS
 */
sdp_result_e sdp_set_owner_version (void *sdp_ptr, const char *version)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    sstrncpy(sdp_p->owner_version, version, sizeof(sdp_p->owner_version));
    return (SDP_SUCCESS);
}

/* Function:    sdp_set_owner_network_type
 * Description: Sets the value of the network type parameter for the o= owner
 *              token line.
 * Parameters:  sdp_ptr       The SDP handle returned by sdp_init_description.
 *              network_type  Network type for the owner line.
 * Returns:     SDP_SUCCESS
 */
sdp_result_e sdp_set_owner_network_type (void *sdp_ptr, 
                                         sdp_nettype_e network_type)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    sdp_p->owner_network_type = network_type;
    return (SDP_SUCCESS);
}

/* Function:    sdp_set_owner_address_type
 * Description: Sets the value of the address type parameter for the o= owner
 *              token line.
 * Parameters:  sdp_ptr       The SDP handle returned by sdp_init_description.
 *              address_type  Address type for the owner line.
 * Returns:     SDP_SUCCESS
 */
sdp_result_e sdp_set_owner_address_type (void *sdp_ptr, 
                                         sdp_addrtype_e address_type)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    sdp_p->owner_addr_type = address_type;
    return (SDP_SUCCESS);
}

/* Function:    sdp_set_owner_address
 * Description: Sets the value of the address parameter for the o= owner
 *              token line.  The string is copied into the SDP structure
 *              so application memory will not be referenced by the SDP lib.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              version     Ptr to the version string.
 * Returns:     SDP_SUCCESS
 */
sdp_result_e sdp_set_owner_address (void *sdp_ptr, const char *address)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    sstrncpy(sdp_p->owner_addr, address, sizeof(sdp_p->owner_addr));
    return (SDP_SUCCESS);
}

/* Function:    sdp_session_name_valid
 * Description: Returns true or false depending on whether the session name
 *              s= token line has been defined for this SDP.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 * Returns:     TRUE or FALSE.
 */
tinybool sdp_session_name_valid (void *sdp_ptr)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
    }

    if (sdp_p->sessname[0] == '\0') {
        return (FALSE);
    } else {
        return (TRUE);
    }
}

/* Function:    sdp_get_session_name
 * Description: Returns the session name parameter from the s= session
 *              name token line.  Value is returned as a const ptr and so 
 *              cannot be modified by the application.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 * Returns:     Ptr to session name or NULL.
 */
const char *sdp_get_session_name (void *sdp_ptr)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (NULL);
    }

    return (sdp_p->sessname);
}

/* Function:    sdp_set_session_name
 * Description: Sets the value of the session name parameter for the s= 
 *              session name token line.  The string is copied into the 
 *              SDP structure so application memory will not be 
 *              referenced by the SDP lib.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              sessname    Ptr to the session name string.
 * Returns:     SDP_SUCCESS
 */
sdp_result_e sdp_set_session_name (void *sdp_ptr, const char *sessname)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    sstrncpy(sdp_p->sessname, sessname, sizeof(sdp_p->sessname));
    return (SDP_SUCCESS);
}

/* Function:    sdp_timespec_valid
 * Description: Returns true or false depending on whether the timespec t=
 *              token line has been defined for this SDP.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 * Returns:     TRUE or FALSE.
 */
tinybool sdp_timespec_valid (void *sdp_ptr)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
    }

    if ((sdp_p->timespec_p == NULL) ||
        (sdp_p->timespec_p->start_time == '\0') ||
        (sdp_p->timespec_p->stop_time == '\0')) {
        return (FALSE);
    } else {
        return (TRUE);
    }
}

/* Function:    sdp_get_time_start
 * Description: Returns the start time parameter from the t= timespec token 
 *              line.  Because the value may be larger than 32 bits, this
 *              parameter is returned as a string, though has been verified
 *              to be numeric.  Value is returned as a const ptr and so 
 *              cannot be modified by the application.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 * Returns:     Ptr to start time or NULL.
 */
const char *sdp_get_time_start (void *sdp_ptr)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (NULL);
    }

    if (sdp_p->timespec_p != NULL) {
        return (sdp_p->timespec_p->start_time);
    } else {
        return (NULL);
    }
}

/* Function:    sdp_get_time_stop
 * Description: Returns the stop time parameter from the t= timespec token 
 *              line.  Because the value may be larger than 32 bits, this
 *              parameter is returned as a string, though has been verified
 *              to be numeric.  Value is returned as a const ptr and so 
 *              cannot be modified by the application.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 * Returns:     Ptr to stop time or NULL.
 */
const char *sdp_get_time_stop (void *sdp_ptr)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (NULL);
    }

    if (sdp_p->timespec_p != NULL) {
        return (sdp_p->timespec_p->stop_time);
    } else {
        return (NULL);
    }
}

/* Function:    sdp_set_time_start
 * Description: Sets the value of the start time parameter for the t= 
 *              timespec token line.  The string is copied into the 
 *              SDP structure so application memory will not be 
 *              referenced by the SDP lib.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              start_time  Ptr to the start time string.
 * Returns:     SDP_SUCCESS
 */
sdp_result_e sdp_set_time_start (void *sdp_ptr, const char *start_time)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if (sdp_p->timespec_p == NULL) {
        sdp_p->timespec_p = (sdp_timespec_t *)SDP_MALLOC(sizeof(sdp_timespec_t));
        if (sdp_p->timespec_p == NULL) {
            sdp_p->conf_p->num_no_resource++;
            return (SDP_NO_RESOURCE);
        }
        sdp_p->timespec_p->start_time[0] = '\0';
        sdp_p->timespec_p->stop_time[0] = '\0';
    }
    sstrncpy(sdp_p->timespec_p->start_time, start_time, 
             sizeof(sdp_p->timespec_p->start_time));
    return (SDP_SUCCESS);
}

/* Function:    sdp_set_time_stop
 * Description: Sets the value of the stop time parameter for the t= 
 *              timespec token line.  The string is copied into the 
 *              SDP structure so application memory will not be 
 *              referenced by the SDP lib.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              stop_time  Ptr to the stop time string.
 * Returns:     SDP_SUCCESS
 */
sdp_result_e sdp_set_time_stop (void *sdp_ptr, const char *stop_time)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if (sdp_p->timespec_p == NULL) {
        sdp_p->timespec_p = (sdp_timespec_t *)SDP_MALLOC(sizeof(sdp_timespec_t));
        if (sdp_p->timespec_p == NULL) {
            sdp_p->conf_p->num_no_resource++;
            return (SDP_NO_RESOURCE);
        }
        sdp_p->timespec_p->start_time[0] = '\0';
        sdp_p->timespec_p->stop_time[0] = '\0';
    }
    sstrncpy(sdp_p->timespec_p->stop_time, stop_time, 
             sizeof(sdp_p->timespec_p->stop_time));
    return (SDP_SUCCESS);
}

/* Function:    sdp_encryption_valid
 * Description: Returns true or false depending on whether the encryption k=
 *              token line has been defined for this SDP at the given level.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the k= line.  Will be 
 *                          either SDP_SESSION_LEVEL or 1-n specifying a 
 *                          media line level.
 * Returns:     TRUE or FALSE.
 */
tinybool sdp_encryption_valid (void *sdp_ptr, u16 level)
{
    sdp_t               *sdp_p = (sdp_t *)sdp_ptr;
    sdp_encryptspec_t   *encrypt_p;
    sdp_mca_t           *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
    }

    if (level == SDP_SESSION_LEVEL) {
        encrypt_p = &(sdp_p->encrypt);
    } else {
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            return (FALSE);
        }
        encrypt_p = &(mca_p->encrypt);
    }

    if ((encrypt_p->encrypt_type == SDP_ENCRYPT_INVALID) ||   
        ((encrypt_p->encrypt_type != SDP_ENCRYPT_PROMPT) &&
         (encrypt_p->encrypt_key[0] == '\0'))) {
        return (FALSE);
    } else {
        return (TRUE);
    }
}

/* Function:    sdp_get_encryption_method
 * Description: Returns the encryption method parameter from the k= 
 *              encryption token line.  If encryption method has not been 
 *              set SDP_ENCRYPT_INVALID will be returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the c= line.  Will be 
 *                          either SDP_SESSION_LEVEL or 1-n specifying a 
 *                          media line level.
 * Returns:     Encryption method or SDP_ENCRYPT_INVALID.
 */
sdp_encrypt_type_e sdp_get_encryption_method (void *sdp_ptr, u16 level)
{
    sdp_t               *sdp_p = (sdp_t *)sdp_ptr;
    sdp_encryptspec_t   *encrypt_p;
    sdp_mca_t           *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_ENCRYPT_INVALID);
    }

    if (level == SDP_SESSION_LEVEL) {
        encrypt_p = &(sdp_p->encrypt);
    } else {
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            return (SDP_ENCRYPT_INVALID);
        }
        encrypt_p = &(mca_p->encrypt);
    }

    return (encrypt_p->encrypt_type);
}

/* Function:    sdp_get_encryption_key
 * Description: Returns a pointer to the encryption key parameter
 *              from the k= encryption token line.  Value is returned as a 
 *              const ptr and so cannot be modified by the application.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the c= line.  Will be 
 *                          either SDP_SESSION_LEVEL or 1-n specifying a 
 *                          media line level.
 * Returns:     Ptr to encryption key or NULL.
 */
const char *sdp_get_encryption_key (void *sdp_ptr, u16 level)
{
    sdp_t               *sdp_p = (sdp_t *)sdp_ptr;
    sdp_encryptspec_t   *encrypt_p;
    sdp_mca_t           *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (NULL);
    }

    if (level == SDP_SESSION_LEVEL) {
        encrypt_p = &(sdp_p->encrypt);
    } else {
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            return (NULL);
        }
        encrypt_p = &(mca_p->encrypt);
    }

    return (encrypt_p->encrypt_key);
}

/* Function:    sdp_set_encryption_method
 * Description: Sets the value of the encryption method param for the k= 
 *              encryption token line.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the k= line.  Will be 
 *                          either SDP_SESSION_LEVEL or 1-n specifying a 
 *                          media line level.
 *              type        The encryption type.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_set_encryption_method (void *sdp_ptr, u16 level,
                                        sdp_encrypt_type_e type)
{
    sdp_t               *sdp_p = (sdp_t *)sdp_ptr;
    sdp_encryptspec_t   *encrypt_p;
    sdp_mca_t           *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if (level == SDP_SESSION_LEVEL) {
        encrypt_p = &(sdp_p->encrypt);
    } else {
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }
        encrypt_p = &(mca_p->encrypt);
    }
    
    encrypt_p->encrypt_type = type;
    return (SDP_SUCCESS);
}

/* Function:    sdp_set_encryption_key
 * Description: Sets the value of the encryption key parameter for the k= 
 *              encryption token line.  The string is copied into the 
 *              SDP structure so application memory will not be 
 *              referenced by the SDP lib.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the k= line.  Will be 
 *                          either SDP_SESSION_LEVEL or 1-n specifying a 
 *                          media line level.
 *              key         Ptr to the encryption key string.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_set_encryption_key (void *sdp_ptr, u16 level, const char *key)
{
    sdp_t               *sdp_p = (sdp_t *)sdp_ptr;
    sdp_encryptspec_t   *encrypt_p;
    sdp_mca_t           *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if (level == SDP_SESSION_LEVEL) {
        encrypt_p = &(sdp_p->encrypt);
    } else {
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }
        encrypt_p = &(mca_p->encrypt);
    }
    
    sstrncpy(encrypt_p->encrypt_key, key, sizeof(encrypt_p->encrypt_key));
    return (SDP_SUCCESS);
}

/* Function:    sdp_connection_valid
 * Description: Returns true or false depending on whether the connection c=
 *              token line has been defined for this SDP at the given level.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the c= line.  Will be 
 *                          either SDP_SESSION_LEVEL or 1-n specifying a 
 *                          media line level.
 * Returns:     TRUE or FALSE.
 */
tinybool sdp_connection_valid (void *sdp_ptr, u16 level)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_conn_t *conn_p;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
    }

    if (level == SDP_SESSION_LEVEL) {
        conn_p = &(sdp_p->default_conn);
    } else {
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            return (FALSE);
        }
        conn_p = &(mca_p->conn);
    }
    
    /*if network type is ATM . then allow c= line without address type 
     * and address . This is a special case to cover PVC 
     */ 
    if (conn_p->nettype == SDP_NT_ATM && 
        conn_p->addrtype == SDP_AT_INVALID) { 
        return TRUE; 
    } 

    if ((conn_p->nettype >= SDP_MAX_NETWORK_TYPES) ||
        (conn_p->addrtype >= SDP_MAX_ADDR_TYPES) ||
        (conn_p->conn_addr[0] == '\0')) {
        return (FALSE);
    } else {
        return (TRUE);
    }
}

/* Function:    sdp_bandwidth_valid
 * Description: Returns true or false depending on whether the bandwidth b=
 *              token line has been defined for this SDP at the given level.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the c= line.  Will be 
 *                          either SDP_SESSION_LEVEL or 1-n specifying a 
 *                          media line level.
 *              inst_num    instance number of bw line at that level. The first 
 *                          instance has a inst_num of 1 and so on.
 * Returns:     TRUE or FALSE.
 */
tinybool sdp_bandwidth_valid (void *sdp_ptr, u16 level, u16 inst_num)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_bw_data_t *bw_data_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return FALSE;
    }

    bw_data_p = sdp_find_bw_line(sdp_p, level, inst_num); 
    if (bw_data_p != NULL) {
        if ((bw_data_p->bw_modifier < SDP_BW_MODIFIER_AS) || 
            (bw_data_p->bw_modifier >= SDP_MAX_BW_MODIFIER_VAL)) {
            return FALSE;
        } else {
            return TRUE;
        }
    } else {
        return FALSE;
    }
}

/*
 * sdp_bw_line_exists
 *
 * Description: This api retruns true if there exists a bw line at the
 *              instance and level specified. 
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the c= line.  Will be 
 *                          either SDP_SESSION_LEVEL or 1-n specifying a 
 *                          media line level.
 *              inst_num    instance number of bw line at that level. The first 
 *                          instance has a inst_num of 1 and so on.
 * Returns:     TRUE or FALSE
 */
tinybool sdp_bw_line_exists (void *sdp_ptr, u16 level, u16 inst_num)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_bw_data_t *bw_data_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return FALSE;
    }

    bw_data_p = sdp_find_bw_line(sdp_p, level, inst_num); 
    if (bw_data_p != NULL) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/* Function:    sdp_get_conn_nettype
 * Description: Returns the network type parameter from the c= 
 *              connection token line.  If network type has not been 
 *              set SDP_NT_INVALID will be returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the c= line.  Will be 
 *                          either SDP_SESSION_LEVEL or 1-n specifying a 
 *                          media line level.
 * Returns:     Network type or SDP_NT_INVALID.
 */
sdp_nettype_e sdp_get_conn_nettype (void *sdp_ptr, u16 level)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_conn_t *conn_p;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_NT_INVALID);
    }

    if (level == SDP_SESSION_LEVEL) {
        conn_p = &(sdp_p->default_conn);
    } else {
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            return (SDP_NT_INVALID);
        }
        conn_p = &(mca_p->conn);
    }

    return (conn_p->nettype);
}

/* Function:    sdp_get_conn_addrtype
 * Description: Returns the address type parameter from the c= 
 *              connection token line.  If address type has not been 
 *              set SDP_AT_INVALID will be returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the c= line.  Will be 
 *                          either SDP_SESSION_LEVEL or 1-n specifying a 
 *                          media line level.
 * Returns:     Address type or SDP_AT_INVALID.
 */
sdp_addrtype_e sdp_get_conn_addrtype (void *sdp_ptr, u16 level)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_conn_t *conn_p;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_AT_INVALID);
    }

    if (level == SDP_SESSION_LEVEL) {
        conn_p = &(sdp_p->default_conn);
    } else {
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            return (SDP_AT_INVALID);
        }
        conn_p = &(mca_p->conn);
    }

    return (conn_p->addrtype);
}

/* Function:    sdp_get_conn_address
 * Description: Returns a pointer to the address parameter
 *              from the c= connection token line.  Value is returned as a 
 *              const ptr and so cannot be modified by the application.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the c= line.  Will be 
 *                          either SDP_SESSION_LEVEL or 1-n specifying a 
 *                          media line level.
 * Returns:     Ptr to address or NULL.
 */
const char *sdp_get_conn_address (void *sdp_ptr, u16 level)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_conn_t *conn_p;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (NULL);
    }

    if (level == SDP_SESSION_LEVEL) {
        conn_p = &(sdp_p->default_conn);
    } else {
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            return (NULL);
        }
        conn_p = &(mca_p->conn);
    }

    return (conn_p->conn_addr);
}

/* Function:    sdp_is_mcast_addr
 * Description: Returns a boolean to indicate if the addr is multicast in 
 *              the c=line.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the c= line.  Will be 
 *                          either SDP_SESSION_LEVEL or 1-n specifying a 
 *                          media line level.
 * Returns:     TRUE if the addr is multicast, FALSE if not.
 */

tinybool sdp_is_mcast_addr (void *sdp_ptr, u16 level)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_conn_t *conn_p;
    sdp_mca_t  *mca_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
    }
    
    if (level == SDP_SESSION_LEVEL) {
        conn_p = &(sdp_p->default_conn);
    } else {
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p != NULL) {
            conn_p = &(mca_p->conn);
	} else { 
            return (FALSE);
	}
    }
    
    if ((conn_p) && (conn_p->is_multicast)) {
        return (TRUE);
    } else {
        return (FALSE);
    }
}

/* Function:    sdp_get_mcast_ttl
 * Description: Get the time to live(ttl) value for the multicast address 
 *              if present.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the c= line.  Will be 
 *                          either SDP_SESSION_LEVEL or 1-n specifying a 
 *                          media line level.
 * Returns:     Multicast address - Time to live (ttl) value
 */

int32 sdp_get_mcast_ttl (void *sdp_ptr, u16 level)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_conn_t *conn_p;
    sdp_mca_t  *mca_p;
    u16 ttl=0;
    
    if (sdp_verify_sdp_ptr(sdp_p) != FALSE) {
        if (level == SDP_SESSION_LEVEL) {
            conn_p = &(sdp_p->default_conn);
        } else {
            mca_p = sdp_find_media_level(sdp_p, level);
            if (mca_p != NULL) {
                conn_p = &(mca_p->conn);
            } else {
                return SDP_INVALID_VALUE;
            }
        }
    } else {
        return SDP_INVALID_VALUE;
    }
    
    if (conn_p) {
	ttl = conn_p->ttl;
    }
    return ttl;
}

/* Function:    sdp_get_mcast_num_of_addresses
 * Description: Get the number of addresses value for the multicast address 
 *              if present.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the c= line.  Will be 
 *                          either SDP_SESSION_LEVEL or 1-n specifying a 
 *                          media line level.
 * Returns:     Multicast address - number of addresses value
 */

int32 sdp_get_mcast_num_of_addresses (void *sdp_ptr, u16 level)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_conn_t *conn_p;
    sdp_mca_t  *mca_p;
    u16 num_addr = 0;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    } else {
        if (level == SDP_SESSION_LEVEL) {
            conn_p = &(sdp_p->default_conn);
        } else {
            mca_p = sdp_find_media_level(sdp_p, level);
            if (mca_p != NULL) {
                conn_p = &(mca_p->conn);
            } else {
                return (SDP_INVALID_VALUE);
	    }
        }
    }
    
    if (conn_p) {
	num_addr = conn_p->num_of_addresses;
    }
    return num_addr;
}
/* Function:    sdp_set_conn_nettype
 * Description: Sets the value of the network type parameter for the c= 
 *              connection token line.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              nettype     Network type for the connection line.
 *              level       The level to check for the c= line.  Will be 
 *                          either SDP_SESSION_LEVEL or 1-n specifying a 
 *                          media line level.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_set_conn_nettype (void *sdp_ptr, u16 level, 
                                   sdp_nettype_e nettype)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_conn_t *conn_p;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if (level == SDP_SESSION_LEVEL) {
        conn_p = &(sdp_p->default_conn);
    } else {
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }
        conn_p = &(mca_p->conn);
    }

    conn_p->nettype = nettype;
    return (SDP_SUCCESS);
}

/* Function:    sdp_set_conn_addrtype
 * Description: Sets the value of the address type parameter for the c= 
 *              connection token line.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              addrtype    Address type for the connection line.
 *              level       The level to check for the c= line.  Will be 
 *                          either SDP_SESSION_LEVEL or 1-n specifying a 
 *                          media line level.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_set_conn_addrtype (void *sdp_ptr, u16 level, 
                                    sdp_addrtype_e addrtype)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_conn_t *conn_p;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if (level == SDP_SESSION_LEVEL) {
        conn_p = &(sdp_p->default_conn);
    } else {
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }
        conn_p = &(mca_p->conn);
    }

    conn_p->addrtype = addrtype;
    return (SDP_SUCCESS);
}

/* Function:    sdp_set_conn_address
 * Description: Sets the value of the address parameter for the c= 
 *              connection token line.  The string is copied into the 
 *              SDP structure so application memory will not be 
 *              referenced by the SDP lib.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the c= line.  Will be 
 *                          either SDP_SESSION_LEVEL or 1-n specifying a 
 *                          media line level.
 *              address     Ptr to the address string.
 * Returns:     SDP_SUCCESS
 */
sdp_result_e sdp_set_conn_address (void *sdp_ptr, u16 level, 
                                   const char *address)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_conn_t *conn_p;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if (level == SDP_SESSION_LEVEL) {
        conn_p = &(sdp_p->default_conn);
    } else {
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }
        conn_p = &(mca_p->conn);
    }

    sstrncpy(conn_p->conn_addr, address, sizeof(conn_p->conn_addr));
    return (SDP_SUCCESS);
}

/* Function:    sdp_set_mcast_addr_fields
 * Description: Sets the value of the ttl and num of addresses for 
 *              a multicast address.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the c= line.  Will be 
 *                          either SDP_SESSION_LEVEL or 1-n specifying a 
 *                          media line level.
 *              ttl         Time to live (ttl) value.
 *              num_of_addresses number of addresses
 .
 * Returns:     SDP_SUCCESS
 */
sdp_result_e sdp_set_mcast_addr_fields(void *sdp_ptr, u16 level, 
				       u16 ttl, u16 num_of_addresses)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_conn_t *conn_p;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if (level == SDP_SESSION_LEVEL) {
        conn_p = &(sdp_p->default_conn);
    } else {
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }
        conn_p = &(mca_p->conn);
    }

    if (conn_p) {
       conn_p->is_multicast = TRUE;
       if ((conn_p->ttl >0) && (conn_p->ttl <= SDP_MAX_TTL_VALUE)) {
          conn_p->ttl = ttl;
       }
       conn_p->num_of_addresses = num_of_addresses;
    } else {
       return (SDP_FAILURE); 
    }
    return (SDP_SUCCESS);
}


/* Function:    sdp_media_line_valid
 * Description: Returns true or false depending on whether the specified 
 *              media line m= has been defined for this SDP.  The
 *              SDP_SESSION_LEVEL level is not valid for this check since,
 *              by definition, this is a media level.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the c= line.  Will be 
 *                          1-n specifying a media line level.
 * Returns:     TRUE or FALSE.
 */
tinybool sdp_media_line_valid (void *sdp_ptr, u16 level)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        return (FALSE);
    }

    /* Validate params for this media line */
    if ((mca_p->media >= SDP_MAX_MEDIA_TYPES) ||
        (mca_p->port_format >= SDP_MAX_PORT_FORMAT_TYPES) ||
        (mca_p->transport >= SDP_MAX_TRANSPORT_TYPES) ||
        (mca_p->num_payloads == 0)) {
        return (FALSE);
    } else {
        return (TRUE);
    }
}

/* Function:    sdp_get_num_media_lines
 * Description: Returns the number of media lines associated with the SDP.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 * Returns:     Number of media lines.
 */
u16 sdp_get_num_media_lines (void *sdp_ptr)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    return (sdp_p->mca_count);
}

/* Function:    sdp_get_media_type
 * Description: Returns the media type parameter from the m= 
 *              media token line.  If media type has not been 
 *              set SDP_MEDIA_INVALID will be returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to of the m= media line.  Will be 1-n.
 * Returns:     Media type or SDP_MEDIA_INVALID.
 */
sdp_media_e sdp_get_media_type (void *sdp_ptr, u16 level)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_MEDIA_INVALID);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        return (SDP_MEDIA_INVALID);
    }

    return (mca_p->media);
}

/* Function:    sdp_get_media_port_format
 * Description: Returns the port format type associated with the m= 
 *              media token line.  If port format type has not been 
 *              set SDP_PORT_FORMAT_INVALID will be returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to of the m= media line.  Will be 1-n.
 * Returns:     Port format type or SDP_PORT_FORMAT_INVALID.
 */
sdp_port_format_e sdp_get_media_port_format (void *sdp_ptr, u16 level)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_PORT_FORMAT_INVALID);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        return (SDP_PORT_FORMAT_INVALID);
    }

    return (mca_p->port_format);
}

/* Function:    sdp_get_media_portnum
 * Description: Returns the port number associated with the m= 
 *              media token line.  If port number has not been 
 *              set SDP_INVALID_VALUE will be returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to of the m= media line.  Will be 1-n.
 * Returns:     Port number or SDP_INVALID_VALUE.
 */
int32 sdp_get_media_portnum (void *sdp_ptr, u16 level)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        return (SDP_INVALID_VALUE);
    }

    /* Make sure port number is valid for the specified format. */
    if ((mca_p->port_format != SDP_PORT_NUM_ONLY) &&
        (mca_p->port_format != SDP_PORT_NUM_COUNT) &&
        (mca_p->port_format != SDP_PORT_NUM_VPI_VCI) &&
        (mca_p->port_format != SDP_PORT_NUM_VPI_VCI_CID)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Port num not valid for media line %u", 
                      sdp_p->debug_str, level);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    }

    return (mca_p->port);
}

/* Function:    sdp_get_media_portcount
 * Description: Returns the port count associated with the m= 
 *              media token line.  If port count has not been 
 *              set SDP_INVALID_VALUE will be returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to of the m= media line.  Will be 1-n.
 * Returns:     Port count or SDP_INVALID_VALUE.
 */
int32 sdp_get_media_portcount (void *sdp_ptr, u16 level)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        return (SDP_INVALID_VALUE);
    }

    /* Make sure port number is valid for the specified format. */
    if (mca_p->port_format != SDP_PORT_NUM_COUNT) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Port count not valid for media line %u", 
                      sdp_p->debug_str, level);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    }

    return (mca_p->num_ports);
}

/* Function:    sdp_get_media_vpi
 * Description: Returns the VPI parameter associated with the m= 
 *              media token line.  If VPI has not been set 
 *              SDP_INVALID_VALUE will be returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to of the m= media line.  Will be 1-n.
 * Returns:     VPI or SDP_INVALID_VALUE.
 */
int32 sdp_get_media_vpi (void *sdp_ptr, u16 level)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        return (SDP_INVALID_VALUE);
    }

    /* Make sure port number is valid for the specified format. */
    if ((mca_p->port_format != SDP_PORT_VPI_VCI) && 
        (mca_p->port_format != SDP_PORT_NUM_VPI_VCI) && 
        (mca_p->port_format != SDP_PORT_NUM_VPI_VCI_CID)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s VPI not valid for media line %u", 
                      sdp_p->debug_str, level);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    }

    return (mca_p->vpi);
}

/* Function:    sdp_get_media_vci
 * Description: Returns the VCI parameter associated with the m= 
 *              media token line.  If VCI has not been set 
 *              SDP_INVALID_VALUE will be returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to of the m= media line.  Will be 1-n.
 * Returns:     VCI or zero.
 */
u32 sdp_get_media_vci (void *sdp_ptr, u16 level)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        return (0);
    }

    /* Make sure port number is valid for the specified format. */
    if ((mca_p->port_format != SDP_PORT_VPI_VCI) && 
        (mca_p->port_format != SDP_PORT_NUM_VPI_VCI) && 
        (mca_p->port_format != SDP_PORT_NUM_VPI_VCI_CID)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s VCI not valid for media line %u", 
                      sdp_p->debug_str, level);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    }

    return (mca_p->vci);
}

/* Function:    sdp_get_media_vcci
 * Description: Returns the VCCI parameter associated with the m= 
 *              media token line.  If VCCI has not been set 
 *              SDP_INVALID_VALUE will be returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to of the m= media line.  Will be 1-n.
 * Returns:     VCCI or SDP_INVALID_VALUE.
 */
int32 sdp_get_media_vcci (void *sdp_ptr, u16 level)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        return (SDP_INVALID_VALUE);
    }

    /* Make sure port number is valid for the specified format. */
    if ((mca_p->port_format != SDP_PORT_VCCI) && 
        (mca_p->port_format != SDP_PORT_VCCI_CID)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s VCCI not valid for media line %u", 
                      sdp_p->debug_str, level);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    }

    return (mca_p->vcci);
}

/* Function:    sdp_get_media_cid
 * Description: Returns the CID parameter associated with the m= 
 *              media token line.  If CID has not been set 
 *              SDP_INVALID_VALUE will be returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to of the m= media line.  Will be 1-n.
 * Returns:     CID or SDP_INVALID_VALUE.
 */
int32 sdp_get_media_cid (void *sdp_ptr, u16 level)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        return (SDP_INVALID_VALUE);
    }

    /* Make sure port number is valid for the specified format. */
    if ((mca_p->port_format != SDP_PORT_VCCI_CID) && 
        (mca_p->port_format != SDP_PORT_NUM_VPI_VCI_CID)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s CID not valid for media line %u", 
                      sdp_p->debug_str, level);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    }

    return (mca_p->cid);
}

/* Function:    sdp_get_media_transport
 * Description: Returns the transport type parameter associated with the m= 
 *              media token line.  If transport type has not been set 
 *              SDP_TRANSPORT_INVALID will be returned.  If the transport
 *              type is one of the AAL2 variants, the profile routines below
 *              should be used to access multiple profile types and payload
 *              lists per m= line.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to of the m= media line.  Will be 1-n.
 * Returns:     CID or SDP_TRANSPORT_INVALID.
 */
sdp_transport_e sdp_get_media_transport (void *sdp_ptr, u16 level)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_TRANSPORT_INVALID);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        return (SDP_TRANSPORT_INVALID);
    }

    return (mca_p->transport);
}

/* Function:    sdp_get_media_num_profiles
 * Description: Returns the number of profiles associated with the m= 
 *              media token line.  If the media line is invalid, zero will
 *              be returned.  Application must validate the media line
 *              before using this routine.  Multiple profile types per
 *              media line is currently only used for AAL2.  If the appl
 *              detects that the transport type is one of the AAL2 types,
 *              it should use these profile access routines to access the
 *              profile types and payload list for each.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to of the m= media line.  Will be 1-n.
 * Returns:     Number of profiles or zero.
 */
u16 sdp_get_media_num_profiles (void *sdp_ptr, u16 level)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        return (0);
    }

    if (mca_p->media_profiles_p == NULL) {
        return (0);
    } else {
        return (mca_p->media_profiles_p->num_profiles);
    }
}

/* Function:    sdp_get_media_profile
 * Description: Returns the specified profile type associated with the m= 
 *              media token line.  If the media line or profile number is 
 *              invalid, SDP_TRANSPORT_INVALID will be returned.  
 *              Applications must validate the media line before using this 
 *              routine.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to of the m= media line.  Will be 1-n.
 *              profile_num The specific profile type number to be retrieved.
 * Returns:     The profile type or SDP_TRANSPORT_INVALID.
 */
sdp_transport_e sdp_get_media_profile (void *sdp_ptr, u16 level, 
                                       u16 profile_num)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_TRANSPORT_INVALID);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        return (SDP_TRANSPORT_INVALID);
    }

    if ((profile_num < 1) ||
        (profile_num > mca_p->media_profiles_p->num_profiles)) {
        return (SDP_TRANSPORT_INVALID);
    } else {
        return (mca_p->media_profiles_p->profile[profile_num-1]);
    }
}

/* Function:    sdp_get_media_num_payload_types
 * Description: Returns the number of payload types associated with the m= 
 *              media token line.  If the media line is invalid, zero will
 *              be returned.  Application must validate the media line
 *              before using this routine.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to of the m= media line.  Will be 1-n.
 * Returns:     Number of payload types or zero.
 */
u16 sdp_get_media_num_payload_types (void *sdp_ptr, u16 level)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        return (0);
    }

    return (mca_p->num_payloads);
}

/* Function:    sdp_get_media_profile_num_payload_types
 * Description: Returns the number of payload types associated with the 
 *              specified profile on the m= media token line.  If the 
 *              media line or profile number is invalid, zero will
 *              be returned.  Application must validate the media line
 *              and profile before using this routine.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to of the m= media line.  Will be 1-n.
 *              profile_num The specific profile number. Will be 1-n.
 * Returns:     Number of payload types or zero.
 */
u16 sdp_get_media_profile_num_payload_types (void *sdp_ptr, u16 level,
                                             u16 profile_num)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        return (0);
    }

    if ((profile_num < 1) ||
        (profile_num > mca_p->media_profiles_p->num_profiles)) {
        return (0);
    } else {
        return (mca_p->media_profiles_p->num_payloads[profile_num-1]);
    }
}

/* Function:    sdp_get_media_payload_type
 * Description: Returns the payload type of the specified payload for the m= 
 *              media token line.  If the media line or payload number is 
 *              invalid, zero will be returned.  Application must validate 
 *              the media line before using this routine.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to of the m= media line.  Will be 1-n.
 *              payload_num Number of the payload type to retrieve.  The
 *                          range is (1 - max num payloads).
 *              indicator   Returns the type of payload returned, either
 *                          NUMERIC or ENUM.
 * Returns:     Payload type or zero.
 */
u32 sdp_get_media_payload_type (void *sdp_ptr, u16 level, u16 payload_num,
                                sdp_payload_ind_e *indicator)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;
    uint16_t    num_a_lines = 0;
    int         i;
    uint16_t    ptype;
    uint16_t    pack_mode = 0; /*default 0, if remote did not provide any */
    const char *encname = NULL;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        return (0);
    }

    if ((payload_num < 1) || (payload_num > mca_p->num_payloads)) {
        return (0);
    }

    *indicator = mca_p->payload_indicator[payload_num-1];
    if ((mca_p->payload_type[payload_num-1] >= SDP_MIN_DYNAMIC_PAYLOAD) &&
        (mca_p->payload_type[payload_num-1] <= SDP_MAX_DYNAMIC_PAYLOAD)) {
        /*
         * Get number of RTPMAP attributes for the AUDIO line
         */
        (void) sdp_attr_num_instances(sdp_p, level, 0, SDP_ATTR_RTPMAP,
                                      &num_a_lines);
        /*
         * Loop through AUDIO media line RTPMAP attributes.
         * NET dynamic payload type will be returned.
         */
        for (i = 0; i < num_a_lines; i++) {
            ptype = sdp_attr_get_rtpmap_payload_type(sdp_p, level, 0,
                                                     (uint16_t) (i + 1));
            if (ptype == mca_p->payload_type[payload_num-1] ) {
                encname = sdp_attr_get_rtpmap_encname(sdp_p, level, 0,
                                                  (uint16_t) (i + 1));
                if (encname) {
                    if (cpr_strcasecmp(encname, SIPSDP_ATTR_ENCNAME_ILBC) == 0) {
                        return (SET_PAYLOAD_TYPE_WITH_DYNAMIC(ptype, RTP_ILBC));
                    }
                    if (cpr_strcasecmp(encname, SIPSDP_ATTR_ENCNAME_L16_256K) == 0) {
                        return (SET_PAYLOAD_TYPE_WITH_DYNAMIC(ptype, RTP_L16));
                    }                    
                    if (cpr_strcasecmp(encname, SIPSDP_ATTR_ENCNAME_ISAC) == 0) {
                        return (SET_PAYLOAD_TYPE_WITH_DYNAMIC(ptype, RTP_ISAC));
                    }
                    if (cpr_strcasecmp(encname, SIPSDP_ATTR_ENCNAME_H264) == 0) {
                        sdp_attr_get_fmtp_pack_mode(sdp_p, level, 0, (uint16_t) (i + 1), &pack_mode);
                        if (pack_mode == SDP_DEFAULT_PACKETIZATION_MODE_VALUE) {
                            return (SET_PAYLOAD_TYPE_WITH_DYNAMIC(ptype, RTP_H264_P0));
                        } else {
                            return (SET_PAYLOAD_TYPE_WITH_DYNAMIC(ptype, RTP_H264_P1));
                        }
                    }                    
                }
            }
        }
    }
    return (mca_p->payload_type[payload_num-1]);
}

/* Function:    sdp_get_media_profile_payload_type
 * Description: Returns the payload type of the specified payload for the m= 
 *              media token line.  If the media line or payload number is 
 *              invalid, zero will be returned.  Application must validate 
 *              the media line before using this routine.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to of the m= media line.  Will be 1-n.
 *              payload_num Number of the payload type to retrieve.  The
 *                          range is (1 - max num payloads).
 *              indicator   Returns the type of payload returned, either
 *                          NUMERIC or ENUM.
 * Returns:     Payload type or zero.
 */
u32 sdp_get_media_profile_payload_type (void *sdp_ptr, u16 level, u16 prof_num,
                                        u16 payload_num, 
                                        sdp_payload_ind_e *indicator)
{
    sdp_t                *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t            *mca_p;
    sdp_media_profiles_t *prof_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        return (0);
    }

    prof_p = mca_p->media_profiles_p;
    if ((prof_num < 1) ||
        (prof_num > prof_p->num_profiles)) {
        return (0);
    }

    if ((payload_num < 1) || 
        (payload_num > prof_p->num_payloads[prof_num-1])) {
        return (0);
    }

    *indicator = prof_p->payload_indicator[prof_num-1][payload_num-1];
    return (prof_p->payload_type[prof_num-1][payload_num-1]);
}

/* Function:    sdp_insert_media_line
 * Description: Insert a new media line at the level specified for the 
 *              given SDP.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The new media level to insert.  Will be 1-n.
 * Returns:     SDP_SUCCESS, SDP_NO_RESOURCE, or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_insert_media_line (void *sdp_ptr, u16 level)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;
    sdp_mca_t  *new_mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if ((level < 1) || (level > (sdp_p->mca_count+1))) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Invalid media line (%u) to insert, max is "
                      "(%u).", sdp_p->debug_str, level, sdp_p->mca_count);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Allocate resource for new media stream. */
    new_mca_p = sdp_alloc_mca();
    if (new_mca_p == NULL) {
        sdp_p->conf_p->num_no_resource++;
        return (SDP_NO_RESOURCE);
    }

    if (level == 1) {
        /* We're inserting the first media line */
        new_mca_p->next_p = sdp_p->mca_p;
        sdp_p->mca_p = new_mca_p;
    } else {
        /* Find the pointer to the media stream just prior to where 
         * we want to insert the new stream.
         */
        mca_p = sdp_find_media_level(sdp_p, (u16)(level-1));
        if (mca_p == NULL) {
            SDP_FREE(new_mca_p);
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }

        new_mca_p->next_p = mca_p->next_p;
        mca_p->next_p = new_mca_p;
    }

    sdp_p->mca_count++;
    return (SDP_SUCCESS);
}

/* Function:    sdp_delete_media_line
 * Description: Delete the media line at the level specified for the 
 *              given SDP.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The media level to delete.  Will be 1-n.
 * Returns:     SDP_SUCCESS, SDP_NO_RESOURCE, or SDP_INVALID_PARAMETER
 */
void sdp_delete_media_line (void *sdp_ptr, u16 level)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;
    sdp_mca_t  *prev_mca_p = NULL;
    sdp_attr_t *attr_p;
    sdp_attr_t *next_attr_p;
    sdp_bw_t        *bw_p;
    sdp_bw_data_t   *bw_data_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return;
    }

    /* If we're not deleting media line 1, then we need a pointer 
     * to the previous media line so we can relink. */
    if (level == 1) {
        mca_p = sdp_find_media_level(sdp_p, level);
    } else {
        prev_mca_p = sdp_find_media_level(sdp_p, (u16)(level-1));
        if (prev_mca_p == NULL) {
            sdp_p->conf_p->num_invalid_param++;
            return;
        }
        mca_p = prev_mca_p->next_p;
    }
    if (mca_p == NULL) {
        sdp_p->conf_p->num_invalid_param++;
        return;
    }

    /* Delete all attributes from this level. */
    for (attr_p = mca_p->media_attrs_p; attr_p != NULL;) {
        next_attr_p = attr_p->next_p;
        sdp_free_attr(attr_p);
        attr_p = next_attr_p;
    }

     /* Delete bw line */
     bw_p = &(mca_p->bw);
     bw_data_p = bw_p->bw_data_list;
     while (bw_data_p != NULL) {
         bw_p->bw_data_list = bw_data_p->next_p;
         SDP_FREE(bw_data_p);
         bw_data_p = bw_p->bw_data_list;
     }

    /* Now relink the media levels and delete the specified one. */
    if (prev_mca_p == NULL) {
        sdp_p->mca_p = mca_p->next_p;
    } else {
        prev_mca_p->next_p = mca_p->next_p;
    }
    SDP_FREE(mca_p);
    sdp_p->mca_count--;
    return;
}

/* Function:    sdp_set_media_type
 * Description: Sets the value of the media type parameter for the m= 
 *              media token line.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The media level to set the param.  Will be 1-n.
 *              media       Media type for the media line.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_set_media_type (void *sdp_ptr, u16 level, sdp_media_e media)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    mca_p->media = media;
    return (SDP_SUCCESS);
}

/* Function:    sdp_set_media_port_format
 * Description: Sets the value of the port format parameter for the m= 
 *              media token line.  Note that this parameter must be set
 *              before any of the port type specific parameters.  If a 
 *              parameter is not valid according to the port format 
 *              specified, an attempt to set the parameter will fail.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The media level to set the param.  Will be 1-n.
 *              port_format Media type for the media line.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_set_media_port_format (void *sdp_ptr, u16 level, 
                                        sdp_port_format_e port_format)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    mca_p->port_format = port_format;
    return (SDP_SUCCESS);
}

/* Function:    sdp_set_media_portnum
 * Description: Sets the value of the port number parameter for the m= 
 *              media token line.  If the port number is not valid with the
 *              port format specified for the media line, this call will
 *              fail.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The media level to set the param.  Will be 1-n.
 *              portnum     Port number to set.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_set_media_portnum (void *sdp_ptr, u16 level, int32 portnum)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    mca_p->port = portnum;
    return (SDP_SUCCESS);
}

/* Function:    sdp_set_media_portcount
 * Description: Sets the value of the port count parameter for the m= 
 *              media token line.  If the port count is not valid with the
 *              port format specified for the media line, this call will
 *              fail.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The media level to set the param.  Will be 1-n.
 *              num_ports   Port count to set.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_set_media_portcount (void *sdp_ptr, u16 level, 
                                      int32 num_ports)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    mca_p->num_ports = num_ports;
    return (SDP_SUCCESS);
}

/* Function:    sdp_set_media_vpi
 * Description: Sets the value of the VPI parameter for the m= 
 *              media token line.  If the VPI is not valid with the
 *              port format specified for the media line, this call will
 *              fail.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The media level to set the param.  Will be 1-n.
 *              vpi         The VPI value to set.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_set_media_vpi (void *sdp_ptr, u16 level, int32 vpi)
{
    sdp_t      *sdp_p = (sdp_t*)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    mca_p->vpi = vpi;
    return (SDP_SUCCESS);
}

/* Function:    sdp_set_media_vci
 * Description: Sets the value of the VCI parameter for the m= 
 *              media token line.  If the VCI is not valid with the
 *              port format specified for the media line, this call will
 *              fail.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The media level to set the param.  Will be 1-n.
 *              vci         The VCI value to set.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_set_media_vci (void *sdp_ptr, u16 level, u32 vci)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    mca_p->vci = vci;
    return (SDP_SUCCESS);
}

/* Function:    sdp_set_media_vcci
 * Description: Sets the value of the VCCI parameter for the m= 
 *              media token line.  If the VCCI is not valid with the
 *              port format specified for the media line, this call will
 *              fail.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The media level to set the param.  Will be 1-n.
 *              vcci        The VCCI value to set.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_set_media_vcci (void *sdp_ptr, u16 level, int32 vcci)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    mca_p->vcci = vcci;
    return (SDP_SUCCESS);
}

/* Function:    sdp_set_media_cid
 * Description: Sets the value of the CID parameter for the m= 
 *              media token line.  If the CID is not valid with the
 *              port format specified for the media line, this call will
 *              fail.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The media level to set the param.  Will be 1-n.
 *              cid         The CID value to set.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_set_media_cid (void *sdp_ptr, u16 level, int32 cid)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    mca_p->cid = cid;
    return (SDP_SUCCESS);
}

/* Function:    sdp_set_media_transport
 * Description: Sets the value of the transport type parameter for the m= 
 *              media token line.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The media level to set the param.  Will be 1-n.
 *              transport   The transport type to set.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_set_media_transport (void *sdp_ptr, u16 level, 
                                      sdp_transport_e transport)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    mca_p->transport = transport;
    return (SDP_SUCCESS);
}

/* Function:    sdp_add_media_profile
 * Description: Add a new profile type for the m= media token line.  This is
 *              used for AAL2 transport/profile types where more than one can
 *              be specified per media line.  All other transport types should
 *              use the other transport access routines rather than this.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The media level to add the param.  Will be 1-n.
 *              profile     The profile type to add.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_add_media_profile (void *sdp_ptr, u16 level, 
                                    sdp_transport_e profile)
{
    u16         prof_num;
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if (mca_p->media_profiles_p == NULL) {
        mca_p->media_profiles_p = (sdp_media_profiles_t *) \
            SDP_MALLOC(sizeof(sdp_media_profiles_t));
        if (mca_p->media_profiles_p == NULL) {
            sdp_p->conf_p->num_no_resource++;
            return (SDP_NO_RESOURCE);
        } else {
            mca_p->media_profiles_p->num_profiles = 0;
            /* Set the transport type to this first profile type. */
            mca_p->transport = profile;
        }
    }

    if (mca_p->media_profiles_p->num_profiles >= SDP_MAX_PROFILES) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Max number of media profiles already specified"
                      " for media level %u", sdp_p->debug_str, level);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    prof_num = mca_p->media_profiles_p->num_profiles++;
    mca_p->media_profiles_p->profile[prof_num] = profile;
    mca_p->media_profiles_p->num_payloads[prof_num] = 0;
    return (SDP_SUCCESS);
}

/* Function:    sdp_add_media_payload_type
 * Description: Add a new payload type for the media line at the level 
 *              specified. The new payload type will be added at the end
 *              of the payload type list.
 * Parameters:  sdp_ptr      The SDP handle returned by sdp_init_description.
 *              level        The media level to add the payload.  Will be 1-n.
 *              payload_type The new payload type.
 *              indicator    Defines the type of payload returned, either
 *                           NUMERIC or ENUM.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_add_media_payload_type (void *sdp_ptr, u16 level, 
                                         u16 payload_type, 
                                         sdp_payload_ind_e indicator)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if (mca_p->num_payloads == SDP_MAX_PAYLOAD_TYPES) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Max number of payload types already defined "
                      "for media line %u", sdp_p->debug_str, level);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    mca_p->payload_indicator[mca_p->num_payloads] = indicator;
    mca_p->payload_type[mca_p->num_payloads++] = payload_type;
    return (SDP_SUCCESS);
}

/* Function:    sdp_add_media_profile_payload_type
 * Description: Add a new payload type for the media line at the level 
 *              specified. The new payload type will be added at the end
 *              of the payload type list.
 * Parameters:  sdp_ptr      The SDP handle returned by sdp_init_description.
 *              level        The media level to add the payload.  Will be 1-n.
 *              prof_num     The profile number to add the payload type.
 *              payload_type The new payload type.
 *              indicator    Defines the type of payload returned, either
 *                           NUMERIC or ENUM.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_add_media_profile_payload_type (void *sdp_ptr, u16 level, 
                                                u16 prof_num, u16 payload_type,
                                                sdp_payload_ind_e indicator)
{
    u16         num_payloads;
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t  *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if ((prof_num < 1) ||
        (prof_num > mca_p->media_profiles_p->num_profiles)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Invalid profile number (%u) for set profile "
                      " payload type", sdp_p->debug_str, level);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if (mca_p->media_profiles_p->num_payloads[prof_num-1] == 
        SDP_MAX_PAYLOAD_TYPES) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Max number of profile payload types already "
                      "defined profile %u on media line %u", 
                      sdp_p->debug_str, prof_num, level);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Get the current num payloads for this profile, and inc the number
     * of payloads at the same time.  Then store the new payload type. */
    num_payloads = mca_p->media_profiles_p->num_payloads[prof_num-1]++;
    mca_p->media_profiles_p->payload_indicator[prof_num-1][num_payloads] =
                                                           indicator;
    mca_p->media_profiles_p->payload_type[prof_num-1][num_payloads] =
                                                           payload_type;
    return (SDP_SUCCESS);
}

/*
 * sdp_find_bw_line
 *
 * This helper function locates a specific bw line instance given the
 * sdp, the level and the instance number of the bw line.
 *
 * Returns: Pointer to the sdp_bw_data_t instance, or NULL.
 */
sdp_bw_data_t* sdp_find_bw_line (void *sdp_ptr, u16 level, u16 inst_num)
{
    sdp_t               *sdp_p = (sdp_t *)sdp_ptr;
    sdp_bw_t            *bw_p;
    sdp_bw_data_t       *bw_data_p;
    sdp_mca_t           *mca_p;
    int                 bw_attr_count=0;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (NULL);
    }
    
    if (level == SDP_SESSION_LEVEL) {
        bw_p = &(sdp_p->bw);
    } else {
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            sdp_p->conf_p->num_invalid_param++;
            return (NULL);
        }
        bw_p = &(mca_p->bw);
    }

    for (bw_data_p = bw_p->bw_data_list; 
         bw_data_p != NULL; 
         bw_data_p = bw_data_p->next_p) {
        bw_attr_count++;
        if (bw_attr_count == inst_num) {
            return bw_data_p;
        }
    }

    return NULL;
}

/*
 * sdp_copy_all_bw_lines
 *
 * Appends all the bw lines from the specified level of the orig sdp to the
 * specified level of the dst sdp.
 *
 * Parameters:  src_sdp_ptr The source SDP handle.
 *              dst_sdp_ptr The dest SDP handle.
 *              src_level   The level in the src sdp from where to get the
 *                          attributes.
 *              dst_level   The level in the dst sdp where to put the
 *                          attributes. 
 * Returns:     SDP_SUCCESS Attributes were successfully copied.
 */
sdp_result_e sdp_copy_all_bw_lines (void *src_sdp_ptr, void *dst_sdp_ptr,
                                    u16 src_level, u16 dst_level)
{
    sdp_t                *src_sdp_p = (sdp_t *)src_sdp_ptr;
    sdp_t                *dst_sdp_p = (sdp_t *)dst_sdp_ptr;
    sdp_bw_data_t        *orig_bw_data_p;
    sdp_bw_data_t        *new_bw_data_p;
    sdp_bw_data_t        *bw_data_p;
    sdp_bw_t             *src_bw_p;
    sdp_bw_t             *dst_bw_p;
    sdp_mca_t            *mca_p;

    if (sdp_verify_sdp_ptr(src_sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if (sdp_verify_sdp_ptr(dst_sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
       
    /* Find src bw list */ 
    if (src_level == SDP_SESSION_LEVEL) {
        src_bw_p = &(src_sdp_p->bw);
    } else {
        mca_p = sdp_find_media_level(src_sdp_p, src_level);
        if (mca_p == NULL) {
            if (src_sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s Invalid src media level (%u) for copy all "
                          "attrs ", src_sdp_p->debug_str, src_level);
            }
            return (SDP_INVALID_PARAMETER);
        }
        src_bw_p = &(mca_p->bw);
    }

    /* Find dst bw list */ 
    if (dst_level == SDP_SESSION_LEVEL) {
        dst_bw_p = &(dst_sdp_p->bw);
    } else {
        mca_p = sdp_find_media_level(dst_sdp_p, dst_level);
        if (mca_p == NULL) {
            if (src_sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s Invalid dst media level (%u) for copy all "
                          "attrs ", src_sdp_p->debug_str, dst_level);
            }
            return (SDP_INVALID_PARAMETER);
        }
        dst_bw_p = &(mca_p->bw);
    }

    orig_bw_data_p = src_bw_p->bw_data_list;
    while (orig_bw_data_p) {
        /* For ever bw line in the src, allocate a new one for the dst */
        new_bw_data_p = (sdp_bw_data_t*)SDP_MALLOC(sizeof(sdp_bw_data_t));
        if (new_bw_data_p == NULL) {
            return (SDP_NO_RESOURCE);
        }
        new_bw_data_p->next_p = NULL;
        new_bw_data_p->bw_modifier = orig_bw_data_p->bw_modifier;
        new_bw_data_p->bw_val = orig_bw_data_p->bw_val;

        /*
         * Enqueue the sdp_bw_data_t instance at the end of the list of
         * sdp_bw_data_t instances. 
         */
        if (dst_bw_p->bw_data_list == NULL) {
            dst_bw_p->bw_data_list = new_bw_data_p;
        } else {
            for (bw_data_p = dst_bw_p->bw_data_list; 
                 bw_data_p->next_p != NULL;
                 bw_data_p = bw_data_p->next_p) {
                 
                /*sa_ignore EMPTYLOOP*/
                ; /* Do nothing. */
            }

            bw_data_p->next_p = new_bw_data_p;
        }
        dst_bw_p->bw_data_count++;

        orig_bw_data_p = orig_bw_data_p->next_p;
    }

    return (SDP_SUCCESS);
}

/* Function:    sdp_get_bw_modifier
 * Description: Returns the bandwidth modifier parameter from the b= 
 *              line.  If no bw modifier has been set ,
 *              SDP_BW_MODIFIER_UNSUPPORTED will be returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level from which to get the bw modifier.
 *              inst_num    instance number of bw line at that level. The first 
 *                          instance has a inst_num of 1 and so on. 
 * Returns:     Valid modifer value or SDP_BW_MODIFIER_UNSUPPORTED.
 */
sdp_bw_modifier_e sdp_get_bw_modifier (void *sdp_ptr, u16 level, u16 inst_num)
{
    sdp_t               *sdp_p = (sdp_t *)sdp_ptr;
    sdp_bw_data_t       *bw_data_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_BW_MODIFIER_UNSUPPORTED);
    }
    
    bw_data_p = sdp_find_bw_line(sdp_p, level, inst_num);

    if (bw_data_p) {
        return (bw_data_p->bw_modifier);
    } else {
        return (SDP_BW_MODIFIER_UNSUPPORTED);
    } 
}

/* Function:    sdp_get_bw_value
 * Description: Returns the bandwidth value parameter from the b= 
 *              line.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level from which to get the bw value.
 *              inst_num    instance number of bw line at the level. The first 
 *                          instance has a inst_num of 1 and so on. 
 * Returns:     A valid numerical bw value or SDP_INVALID_VALUE.
 */
int32 sdp_get_bw_value (void *sdp_ptr, u16 level, u16 inst_num)
{
    sdp_t               *sdp_p = (sdp_t *)sdp_ptr;
    sdp_bw_data_t       *bw_data_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    
    bw_data_p = sdp_find_bw_line(sdp_p, level, inst_num);

    if (bw_data_p) {
        return (bw_data_p->bw_val);
    } else {
        return (SDP_INVALID_VALUE);
    }
}

/*
 * sdp_get_num_bw_lines
 *
 * Returns the number of bw lines are present at a given level.
 *
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level at which the count of bw lines is required
 *
 * Returns: A valid count or SDP_INVALID_VALUE
 */
int32 sdp_get_num_bw_lines (void *sdp_ptr, u16 level)
{
    sdp_t               *sdp_p = (sdp_t *)sdp_ptr;
    sdp_bw_t            *bw_p;
    sdp_mca_t           *mca_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    
    if (level == SDP_SESSION_LEVEL) {
        bw_p = &(sdp_p->bw);
    } else {
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_VALUE);
        }
        bw_p = &(mca_p->bw);
    }

    return (bw_p->bw_data_count);
}

/*
 * sdp_add_new_bw_line
 *
 * To specify bandwidth parameters at any level, a bw line must first be
 * added at that level using this function. After this addition, you can set
 * the properties of the added bw line by using sdp_set_bw(). 
 *
 * Note carefully though, that since there can be multiple instances of bw
 * lines at any level, you must specify the instance number when setting 
 * or getting the properties of a bw line at any level.      
 *
 * This function returns within the inst_num variable, the instance number
 * of the created bw_line at that level. The instance number is 1-based. 
 * For example:
 *             v=0                               #Session Level
 *             o=mhandley 2890844526 2890842807 IN IP4 126.16.64.4
 *             s=SDP Seminar
 *             c=IN IP4 10.1.0.2
 *             t=0 0
 *             b=AS:60                           # instance number 1
 *             b=TIAS:50780                      # instance number 2
 *             m=audio 1234 RTP/AVP 0 101 102    # 1st Media level
 *             b=AS:12                           # instance number 1
 *             b=TIAS:8480                       # instance number 2
 *             m=audio 1234 RTP/AVP 0 101 102    # 2nd Media level
 *             b=AS:20                           # instance number 1
 * 
 * Parameters:
 * sdp_ptr     The SDP handle returned by sdp_init_description.
 * level       The level to create the bw line.  
 * bw_modifier The Type of bandwidth, CT, AS or TIAS.
 * *inst_num   This memory is set with the instance number of the newly
 *             created bw line instance.
 */
sdp_result_e sdp_add_new_bw_line (void *sdp_ptr, u16 level, sdp_bw_modifier_e bw_modifier, u16 *inst_num)
{
    sdp_t               *sdp_p = (sdp_t *)sdp_ptr;
    sdp_bw_t            *bw_p;
    sdp_mca_t           *mca_p;
    sdp_bw_data_t       *new_bw_data_p;
    sdp_bw_data_t       *bw_data_p = NULL;

    *inst_num = 0;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
    
    if (level == SDP_SESSION_LEVEL) {
        bw_p = &(sdp_p->bw);
    } else {
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }
        bw_p = &(mca_p->bw);
    }

    //see if a bw line already exist for this bw_modifier.
    for(bw_data_p = bw_p->bw_data_list; bw_data_p != NULL; bw_data_p = bw_data_p->next_p) {
        ++(*inst_num);
        if (bw_data_p->bw_modifier == bw_modifier) {
            return (SDP_SUCCESS);
        }
    }

    /*
     * Allocate a new sdp_bw_data_t instance and set it's values from the
     * input parameters.
     */
    new_bw_data_p = (sdp_bw_data_t*)SDP_MALLOC(sizeof(sdp_bw_data_t));
    if (new_bw_data_p == NULL) {
        sdp_p->conf_p->num_no_resource++;
        return (SDP_NO_RESOURCE);
    }
    new_bw_data_p->next_p = NULL;
    new_bw_data_p->bw_modifier = SDP_BW_MODIFIER_UNSUPPORTED;
    new_bw_data_p->bw_val = 0;
  
    /*
     * Enqueue the sdp_bw_data_t instance at the end of the list of
     * sdp_bw_data_t instances. 
     */
    if (bw_p->bw_data_list == NULL) {
        bw_p->bw_data_list = new_bw_data_p;
    } else {
        for (bw_data_p = bw_p->bw_data_list; 
             bw_data_p->next_p != NULL;
             bw_data_p = bw_data_p->next_p) {

             /*sa_ignore EMPTYLOOP*/
             ; /* Do nothing. */
        }
                      
        bw_data_p->next_p = new_bw_data_p;
    }
    *inst_num = ++bw_p->bw_data_count;
 
    return (SDP_SUCCESS);
}

/*
 * sdp_delete_bw_line
 *
 * Deletes the bw line instance at the specified level.
 * 
 * sdp_ptr     The SDP handle returned by sdp_init_description.
 * level       The level to delete the bw line.  
 * inst_num   The instance of the bw line to delete.
 */
sdp_result_e sdp_delete_bw_line (void *sdp_ptr, u16 level, u16 inst_num)
{
    sdp_t               *sdp_p = (sdp_t *)sdp_ptr;
    sdp_bw_t            *bw_p;
    sdp_mca_t           *mca_p;
    sdp_bw_data_t       *bw_data_p = NULL;
    sdp_bw_data_t       *prev_bw_data_p = NULL;
    int                 bw_data_count = 0;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
    
    if (level == SDP_SESSION_LEVEL) {
        bw_p = &(sdp_p->bw);
    } else {
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }
        bw_p = &(mca_p->bw);
    }

    bw_data_p = bw_p->bw_data_list;
    while (bw_data_p != NULL) {
        bw_data_count++;
        if (bw_data_count == inst_num) {
            break;
        }

        prev_bw_data_p = bw_data_p;
        bw_data_p = bw_data_p->next_p;
    }

    if (bw_data_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s bw line instance %d not found.", 
                      sdp_p->debug_str, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if (prev_bw_data_p == NULL) {
        bw_p->bw_data_list = bw_data_p->next_p;
    } else {
        prev_bw_data_p->next_p = bw_data_p->next_p; 
    }
    bw_p->bw_data_count--;
    
    SDP_FREE(bw_data_p);
    return (SDP_SUCCESS);
}

/*
 * sdp_set_bw
 *
 * Once a bandwidth line is added under a level, this function can be used to
 * set the properties of that bandwidth line.
 *
 * Parameters:
 * sdp_ptr     The SDP handle returned by sdp_init_description.
 * level       The level to at which the bw line resides.  
 * inst_num    The instance number of the bw line that is to be set.
 * bw_modifier The Type of bandwidth, CT, AS or TIAS.
 * bw_val      Numerical bandwidth value.
 * 
 * NOTE: Before calling this function to set the bw line, the bw line must
 * be added using sdp_add_new_bw_line at the required level.
 */
sdp_result_e sdp_set_bw (void *sdp_ptr, u16 level, u16 inst_num,
                         sdp_bw_modifier_e bw_modifier, u32 bw_val)
{
    sdp_t               *sdp_p = (sdp_t *)sdp_ptr;
    sdp_bw_data_t       *bw_data_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if ((bw_modifier < SDP_BW_MODIFIER_AS) || 
        (bw_modifier >= SDP_MAX_BW_MODIFIER_VAL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Invalid bw modifier type: %d.",
                      sdp_p->debug_str, bw_modifier);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    
    bw_data_p = sdp_find_bw_line(sdp_p, level, inst_num);
    if (bw_data_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s The %u instance of a b= line was not found at level %u.",
                      sdp_p->debug_str, inst_num, level);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    bw_data_p->bw_modifier = bw_modifier;
    bw_data_p->bw_val = bw_val;

    return (SDP_SUCCESS);
}

/* Function:    sdp_get_mid_value
 * Description: Returns the mid value parameter from the a= mid: line.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       SDP_MEDIA_LEVEL
 * Returns:     mid value.
 */
int32 sdp_get_mid_value (void *sdp_ptr, u16 level)
{
    sdp_t               *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t           *mca_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    
    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    }
    return (mca_p->mid);
}

/* Function:    sdp_set_mid_value
 * Description: Sets the value of the mid value for the 
 *              a= mid:<val> line.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       SDP_MEDIA_LEVEL
 *              mid_val     mid value .
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
*/
sdp_result_e sdp_set_mid_value (void *sdp_ptr, u16 level, u32 mid_val)
{
    sdp_t               *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t           *mca_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
    
    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    mca_p->mid = mid_val;
    return (SDP_SUCCESS);
}
