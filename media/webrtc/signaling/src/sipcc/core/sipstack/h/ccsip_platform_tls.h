/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __CCSIP_PLATFORM_TLS__H__
#define __CCSIP_PLATFORM_TLS__H__

extern cpr_socket_t sip_tls_create_connection(sipSPIMessage_t *spi_msg,
                                              boolean blocking,
                                              sec_level_t sec);

#endif /* __CCSIP_PLATFORM_TLS__H__ */
