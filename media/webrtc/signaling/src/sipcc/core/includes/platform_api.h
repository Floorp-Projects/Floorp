/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _PLATFORM_API_H_
#define _PLATFORM_API_H_

#include "cpr_types.h"
#include "cpr_socket.h"
#include "ccsip_pmh.h"
#include "plat_api.h"
#include "sessionTypes.h"

void     platform_get_wired_mac_address(uint8_t *addr);
void     platform_get_active_mac_address(uint8_t *addr);
void platform_get_ip_address(cpr_ip_addr_t *ip_addr);
cpr_ip_mode_e platform_get_ip_address_mode(void);
void platform_apply_config (char * configVersionStamp, char * dialplanVersionStamp, char * fcpVersionStamp, char * cucmResult, char * loadId, char * inactiveLoadId, char * loadServer, char * logServer, boolean ppid);

/**
 * Set ip address mode
 * e.g.
 */
cpr_ip_mode_e platGetIpAddressMode();

/**
 * @brief Given a msg buffer, returns a pointer to the buffer's header
 *
 * The cprGetSysHeader function retrieves the system header buffer for the
 * passed in message buffer.
 *
 * @param[in] buffer  pointer to the buffer whose sysHdr to return
 *
 * @return        Abstract pointer to the msg buffer's system header
 *                or #NULL if failure
 */
void *
cprGetSysHeader (void *buffer);

/**
 * @brief Called when the application is done with this system header
 *
 * The cprReleaseSysHeader function returns the system header buffer to the
 * system.
 * @param[in] syshdr  pointer to the sysHdr to be released
 *
 * @return        none
 */
void
cprReleaseSysHeader (void *syshdr);

#endif
