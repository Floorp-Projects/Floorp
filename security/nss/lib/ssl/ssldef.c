/*
 * "Default" SSLSocket methods, used by sockets that do neither SSL nor socks.
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
/* $Id: ssldef.c,v 1.10 2005/09/09 03:02:16 nelsonb%netscape.com Exp $ */

#include "cert.h"
#include "ssl.h"
#include "sslimpl.h"

#if defined(WIN32)
#define MAP_ERROR(from,to) if (err == from) { PORT_SetError(to); }
#define DEFINE_ERROR       PRErrorCode err = PR_GetError();
#else
#define MAP_ERROR(from,to)
#define DEFINE_ERROR
#endif

int ssl_DefConnect(sslSocket *ss, const PRNetAddr *sa)
{
    PRFileDesc *lower = ss->fd->lower;
    int rv;

    rv = lower->methods->connect(lower, sa, ss->cTimeout);
    return rv;
}

int ssl_DefBind(sslSocket *ss, const PRNetAddr *addr)
{
    PRFileDesc *lower = ss->fd->lower;
    int rv;

    rv = lower->methods->bind(lower, addr);
    return rv;
}

int ssl_DefListen(sslSocket *ss, int backlog)
{
    PRFileDesc *lower = ss->fd->lower;
    int rv;

    rv = lower->methods->listen(lower, backlog);
    return rv;
}

int ssl_DefShutdown(sslSocket *ss, int how)
{
    PRFileDesc *lower = ss->fd->lower;
    int rv;

    rv = lower->methods->shutdown(lower, how);
    return rv;
}

int ssl_DefRecv(sslSocket *ss, unsigned char *buf, int len, int flags)
{
    PRFileDesc *lower = ss->fd->lower;
    int rv;

    rv = lower->methods->recv(lower, (void *)buf, len, flags, ss->rTimeout);
    if (rv < 0) {
	DEFINE_ERROR
	MAP_ERROR(PR_SOCKET_SHUTDOWN_ERROR, PR_CONNECT_RESET_ERROR)
    } else if (rv > len) {
	PORT_Assert(rv <= len);
	PORT_SetError(PR_BUFFER_OVERFLOW_ERROR);
	rv = SECFailure;
    }
    return rv;
}

/* Default (unencrypted) send.
 * Returns SECSuccess or SECFailure,  NOT SECWouldBlock. 
 * Returns positive count if any data was written. 
 * ALWAYS check for a short write after calling ssl_DefSend.
 */
int ssl_DefSend(sslSocket *ss, const unsigned char *buf, int len, int flags)
{
    PRFileDesc *lower = ss->fd->lower;
    int rv, count;

#if NSS_DISABLE_NAGLE_DELAYS
    /* Although this is overkill, we disable Nagle delays completely for 
    ** SSL sockets.
    */
    if (ss->opt.useSecurity && !ss->delayDisabled) {
	ssl_EnableNagleDelay(ss, PR_FALSE);   /* ignore error */
    	ss->delayDisabled = 1;
    }
#endif
    count = 0;
    for (;;) {
	rv = lower->methods->send(lower, (const void *)buf, len,
				 flags, ss->wTimeout);
	if (rv < 0) {
	    PRErrorCode err = PR_GetError();
	    if (err == PR_WOULD_BLOCK_ERROR) {
		ss->lastWriteBlocked = 1;
		return count ? count : rv;
	    }
	    ss->lastWriteBlocked = 0;
	    MAP_ERROR(PR_CONNECT_ABORTED_ERROR, PR_CONNECT_RESET_ERROR)
	    /* Loser */
	    return rv;
	}
	count += rv;
	if (rv < len) {
	    /* Short send. Send the rest in the next call */
	    buf += rv;
	    len -= rv;
	    continue;
	}
	break;
    }
    ss->lastWriteBlocked = 0;
    return count;
}

int ssl_DefRead(sslSocket *ss, unsigned char *buf, int len)
{
    PRFileDesc *lower = ss->fd->lower;
    int rv;

    rv = lower->methods->read(lower, (void *)buf, len);
    if (rv < 0) {
	DEFINE_ERROR
	MAP_ERROR(PR_SOCKET_SHUTDOWN_ERROR, PR_CONNECT_RESET_ERROR)
    }
    return rv;
}

int ssl_DefWrite(sslSocket *ss, const unsigned char *buf, int len)
{
    PRFileDesc *lower = ss->fd->lower;
    int rv, count;

    count = 0;
    for (;;) {
	rv = lower->methods->write(lower, (void *)buf, len);
	if (rv < 0) {
	    PRErrorCode err = PR_GetError();
	    if (err == PR_WOULD_BLOCK_ERROR) {
		ss->lastWriteBlocked = 1;
		return count ? count : rv;
	    }
	    ss->lastWriteBlocked = 0;
	    MAP_ERROR(PR_CONNECT_ABORTED_ERROR, PR_CONNECT_RESET_ERROR)
	    /* Loser */
	    return rv;
	}
	count += rv;
	if (rv != len) {
	    /* Short write. Send the rest in the next call */
	    buf += rv;
	    len -= rv;
	    continue;
	}
	break;
    }
    ss->lastWriteBlocked = 0;
    return count;
}

int ssl_DefGetpeername(sslSocket *ss, PRNetAddr *name)
{
    PRFileDesc *lower = ss->fd->lower;
    int rv;

    rv = lower->methods->getpeername(lower, name);
    return rv;
}

int ssl_DefGetsockname(sslSocket *ss, PRNetAddr *name)
{
    PRFileDesc *lower = ss->fd->lower;
    int rv;

    rv = lower->methods->getsockname(lower, name);
    return rv;
}

int ssl_DefClose(sslSocket *ss)
{
    PRFileDesc *fd;
    PRFileDesc *popped;
    int         rv;

    fd    = ss->fd;

    /* First, remove the SSL layer PRFileDesc from the socket's stack, 
    ** then invoke the SSL layer's PRFileDesc destructor.
    ** This must happen before the next layer down is closed.
    */
    PORT_Assert(fd->higher == NULL);
    if (fd->higher) {
	PORT_SetError(PR_BAD_DESCRIPTOR_ERROR);
	return SECFailure;
    }
    ss->fd = NULL;

    /* PR_PopIOLayer will swap the contents of the top two PRFileDescs on
    ** the stack, and then remove the second one.  This way, the address
    ** of the PRFileDesc on the top of the stack doesn't change.
    */
    popped = PR_PopIOLayer(fd, PR_TOP_IO_LAYER); 
    popped->dtor(popped);

    /* fd is now the PRFileDesc for the next layer down.
    ** Now close the underlying socket. 
    */
    rv = fd->methods->close(fd);

    ssl_FreeSocket(ss);

    SSL_TRC(5, ("%d: SSL[%d]: closing, rv=%d errno=%d",
		SSL_GETPID(), fd, rv, PORT_GetError()));
    return rv;
}
