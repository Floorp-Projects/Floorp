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
 * The Original Code is the Netscape Security Services for Java.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
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

package org.mozilla.jss.ssl;

import java.net.*;
import java.net.SocketException;
import java.io.*;
import java.io.IOException;
import java.util.Vector;
import java.util.Enumeration;
import java.lang.reflect.Constructor;
import org.mozilla.jss.util.Assert;
import org.mozilla.jss.CryptoManager;
import org.mozilla.jss.crypto.ObjectNotFoundException;
import org.mozilla.jss.crypto.TokenException;

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
        SSLClientCertificateSelectionCallback clientCertSelectionCallback,
        java.net.Socket javaSock, String host)
            throws SocketException;

    byte[] socketCreate(Object socketObject,
        SSLCertificateApprovalCallback certApprovalCallback,
        SSLClientCertificateSelectionCallback clientCertSelectionCallback)
            throws SocketException
    {
        return socketCreate(socketObject, certApprovalCallback,
            clientCertSelectionCallback, null, null);
    }

    native void socketBind(byte[] addrBA, int port) throws SocketException;

    /**
     * Enums. These must match the enums table in common.c. This is
     * safer than copying the values of the C constants, which are subject
     * to change, into Java code.
     */
    static final int SSL_ENABLE_SSL2 = 0;
    static final int SSL_ENABLE_SSL3 = 1;
    static final int SSL_ENABLE_TLS = 2;
    static final int TCP_NODELAY = 3;
    static final int SO_KEEPALIVE = 4;
    static final int PR_SHUTDOWN_RCV = 5;
    static final int PR_SHUTDOWN_SEND = 6;
    static final int SSL_REQUIRE_CERTIFICATE = 7;
    static final int SSL_REQUEST_CERTIFICATE = 8;
    static final int SSL_NO_CACHE = 9;
    static final int SSL_POLICY_DOMESTIC = 10;
    static final int SSL_POLICY_EXPORT = 11;
    static final int SSL_POLICY_FRANCE = 12;

    void close() throws IOException {
        socketClose();
    }

    // This method is synchronized because there is a potential race
    // condition in the native code.
    native synchronized void socketClose() throws IOException;

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

    void enableTLS(boolean enable) throws SocketException {
        setSSLOption(SSL_ENABLE_TLS, enable);
    }

    void setSSLOption(int option, boolean on)
        throws SocketException
    {
        setSSLOption(option, on ? 1 : 0);
    }

    native void setSSLOption(int option, int on)
        throws SocketException;

    /**
     * Converts a host-ordered 4-byte internet address into an InetAddress.
     * Unfortunately InetAddress provides no more efficient means
     * of construction than getByName(), and it is final.
     *
     * @return The InetAddress corresponding to the given integer,
     *      or <tt>null</tt> if the InetAddress could not be constructed.
     */
    private static InetAddress
    convertIntToInetAddress(int intAddr) {
        InetAddress in;
        int[] addr = new int[4];
        addr[0] = ((intAddr >>> 24) & 0xff);
        addr[1] = ((intAddr >>> 16) & 0xff);
        addr[2] = ((intAddr >>>  8) & 0xff);
        addr[3] = ((intAddr       ) & 0xff);
        try {
            in = InetAddress.getByName(
                addr[0] + "." + addr[1] + "." + addr[2] + "." + addr[3] );
        } catch (java.net.UnknownHostException e) {
            in = null;
        }
        return in;
    }

    /**
     * @return the InetAddress of the peer end of the socket.
     */
    InetAddress getInetAddress()
    {
        try {
            return convertIntToInetAddress( getPeerAddressNative() );
        } catch(SocketException e) {
            return null;
        }
    }
    private native int getPeerAddressNative() throws SocketException;

    /**
     * @return The local IP address.
     */
    InetAddress getLocalAddress() {
        try {
            return convertIntToInetAddress( getLocalAddressNative() );
        } catch(SocketException e) {
            return null;
        }
    }
    private native int getLocalAddressNative() throws SocketException;

    public int getLocalPort() {
        try {
            return getLocalPortNative();
        } catch(SocketException e) {
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

    /**
     * Sets the nickname of the certificate to use for client authentication.
     */
    public void setClientCertNickname(String nick) throws SocketException {
      try {
        setClientCert( CryptoManager.getInstance().findCertByNickname(nick) );
      } catch(CryptoManager.NotInitializedException nie) {
        throw new SocketException("CryptoManager not initialized");
      } catch(ObjectNotFoundException onfe) {
        throw new SocketException("Object not found: " + onfe);
      } catch(TokenException te) {
        throw new SocketException("Token Exception: " + te);
      }
    }

    native void setClientCert(org.mozilla.jss.crypto.X509Certificate cert)
        throws SocketException;

    void useCache(boolean b) throws SocketException {
        setSSLOption(SSL_NO_CACHE, !b);
    }

    static Throwable processExceptions(Throwable topException,
        Throwable bottomException)
    {
      try {
        StringBuffer strBuf;
        strBuf = new StringBuffer( topException.toString() );

        if( bottomException != null ) {
            strBuf.append(" --> ");
            strBuf.append( bottomException.toString() );
        }

        Class excepClass = topException.getClass();
        Class stringClass = java.lang.String.class;
        Constructor cons = excepClass.getConstructor(new Class[] {stringClass});

        return (Throwable) cons.newInstance(new Object[] { strBuf.toString() });
      } catch(Exception e ) {
        Assert.notReached("Problem constructing exception container");
        return topException;
      }
    }
}
