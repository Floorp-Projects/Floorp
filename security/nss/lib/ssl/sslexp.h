/*
 * This file contains prototypes for experimental SSL functions.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __sslexp_h_
#define __sslexp_h_

#include "ssl.h"
#include "sslerr.h"

SEC_BEGIN_PROTOS

/* The functions in this header file are not guaranteed to remain available in
 * future NSS versions. Code that uses these functions needs to safeguard
 * against the function not being available. */

#define SSL_EXPERIMENTAL_API(name, arglist, args)                   \
    (SSL_GetExperimentalAPI(name)                                   \
         ? ((SECStatus(*) arglist)SSL_GetExperimentalAPI(name))args \
         : SECFailure)

/* Allow the ServerHello to be record type 24. Experiment to test:
 * https://github.com/tlswg/tls13-spec/pull/1051
 * This will either become part of the standard or be disabled
 * after we have tested it.
 */
#define SSL_UseAltServerHelloType(fd, enable)                \
    SSL_EXPERIMENTAL_API("SSL_UseAltServerHelloType",        \
                         (PRFileDesc * _fd, PRBool _enable), \
                         (fd, enable))

SEC_END_PROTOS

#endif /* __sslexp_h_ */
