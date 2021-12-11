/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __tls13err_h_
#define __tls13err_h_

/* Use this instead of FATAL_ERROR when an alert isn't possible. */
#define LOG_ERROR(ss, prError)                                                     \
    do {                                                                           \
        SSL_TRC(3, ("%d: TLS13[%d]: fatal error %d in %s (%s:%d)",                 \
                    SSL_GETPID(), ss->fd, prError, __func__, __FILE__, __LINE__)); \
        PORT_SetError(prError);                                                    \
    } while (0)

/* Log an error and generate an alert because something is irreparably wrong. */
#define FATAL_ERROR(ss, prError, desc)       \
    do {                                     \
        LOG_ERROR(ss, prError);              \
        tls13_FatalError(ss, prError, desc); \
    } while (0)

void tls13_FatalError(sslSocket *ss, PRErrorCode prError, SSL3AlertDescription desc);
#endif
