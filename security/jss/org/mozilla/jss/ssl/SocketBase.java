/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Netscape Security Services for Java.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable
 * instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

package org.mozilla.jss.ssl;

import java.net.*;
import java.net.SocketException;
import java.io.*;
import java.io.IOException;
import java.util.Vector;


class SocketBase {

    // This is just another reference to the same proxy object
    // that is held by the SSLSocket or SSLServerSocket.
    private SocketProxy sockProxy;

    private int timeout;

    int getTimeout() {
        return timeout;
    }
    void setTimeout(int timeout) {
        this.timeout = timeout;
    }

    void setProxy(SocketProxy sockProxy) {
        this.sockProxy = sockProxy;
    }

    native byte[] socketCreate(Object socketObject,
        SSLCertificateApprovalCallback certApprovalCallback,
        SSLClientCertificateSelectionCallback clientCertSelectionCallback)
            throws SocketException;

    native void socketBind(byte[] addrBA, int port) throws SocketException;

    /**
     * Enums. These must match the enums table in SSLSocket.c. This is
     * safer than copying the values of the C constants, which are subject
     * to change, into Java code.
     */
    static final int SSL_ENABLE_SSL2 = 0;
    static final int SSL_ENABLE_SSL3 = 1;
    static final int TCP_NODELAY = 2;
    static final int SO_KEEPALIVE = 3;
    static final int PR_SHUTDOWN_RCV = 4;
    static final int PR_SHUTDOWN_SEND = 5;
    static final int SSL_REQUIRE_CERTIFICATE = 6;
    static final int SSL_REQUEST_CERTIFICATE = 7;
    static final int SSL_NO_CACHE = 8;
    static final int SSL_POLICY_DOMESTIC = 9;
    static final int SSL_POLICY_EXPORT = 10;
    static final int SSL_POLICY_FRANCE = 11;

    void close() throws IOException {
        socketClose();
        sockProxy = null;
    }

    native void socketClose() throws IOException;

    private boolean requestingClientAuth = false;

    void requestClientAuth(boolean b) throws SocketException {
        requestingClientAuth = b;
        setSSLOption(SSL_REQUEST_CERTIFICATE, b);
    }

    public void requestClientAuthNoExpiryCheck(boolean b)
        throws SocketException
    {
        requestingClientAuth = b;
        requestClientAuthNoExpiryCheckNative(b);
    }

    private native void requestClientAuthNoExpiryCheckNative(boolean b)
        throws SocketException;

    void enableSSL2(boolean enable) throws SocketException {
        setSSLOption(SSL_ENABLE_SSL2, enable);
    }

    void enableSSL3(boolean enable) throws SocketException {
        setSSLOption(SSL_ENABLE_SSL3, enable);
    }

    void setSSLOption(int option, boolean on)
        throws SocketException
    {
        setSSLOption(option, on ? 1 : 0);
    }

    native void setSSLOption(int option, int on)
        throws SocketException;

    InetAddress getInetAddress()
    {
        try {
            int intAddr = getPeerAddressNative();
            InetAddress in;
            byte[] addr = new byte[4];
            addr[0] = (byte)((intAddr >>> 24) & 0xff);
            addr[1] = (byte)((intAddr >>> 16) & 0xff);
            addr[2] = (byte)((intAddr >>>  8) & 0xff);
            addr[3] = (byte)((intAddr       ) & 0xff);
            try {
            in = InetAddress.getByName(
                addr[0] + "." + addr[1] + "." + addr[2] + "." + addr[3] );
            } catch (java.net.UnknownHostException e) {
                in = null;
            }
            return in;
        } catch(SocketException e) {
            e.printStackTrace();
            return null;
        }
    }

    private native int getPeerAddressNative() throws SocketException;

    public int getLocalPort() {
        try {
            return getLocalPortNative();
        } catch(SocketException e) {
            e.printStackTrace();
            return 0;
        }
    }

    private native int getLocalPortNative() throws SocketException;

    void requireClientAuth(boolean require, boolean onRedo)
            throws SocketException
    {
        if( require && !requestingClientAuth ) {
            requestClientAuth(true);
        }
        setSSLOption(SSL_REQUIRE_CERTIFICATE, require ? (onRedo ? 1 : 2) : 0);
    }

    void setClientCertNickname(String nick) throws SocketException {
        if( nick != null && nick.length() > 0 ) {
            setClientCertNicknameNative(nick);
        }
    }

    private native void setClientCertNicknameNative(String nick)
        throws SocketException;

    void useCache(boolean b) throws SocketException {
        setSSLOption(SSL_NO_CACHE, !b);
    }
}
