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

import java.net.InetAddress;
import java.io.IOException;
import java.net.Socket;
import java.net.SocketException;
import org.mozilla.jss.CryptoManager;
import org.mozilla.jss.crypto.ObjectNotFoundException;
import org.mozilla.jss.crypto.TokenException;

/**
 * SSL server socket.
 */
public class SSLServerSocket extends java.net.ServerSocket {

    /**
     * The default size of the listen queue.
     */
    public static final int DEFAULT_BACKLOG = 50;

    /**
     * Creates a server socket listening on the given port.
     * The listen queue will be of size DEFAULT_BACKLOG.
     */
    public SSLServerSocket(int port) throws IOException {
        this(port, DEFAULT_BACKLOG, null);
    }

    /**
     * Creates a server socket listening on the given port.
     * @param backlog The size of the socket's listen queue.
     */
    public SSLServerSocket(int port, int backlog) throws IOException {
        this(port, backlog, null);
    }

    /**
     * Creates a server socket listening on the given port.
     * @param backlog The size of the socket's listen queue.
     * @param bindAddr The local address to which to bind. If null, an
     *      unspecified local address will be bound to.
     */
    public SSLServerSocket(int port, int backlog, InetAddress bindAddr)
        throws IOException 
    {
        this(port, backlog, bindAddr, null);
    }

    /**
     * Creates a server socket listening on the given port.
     * @param backlog The size of the socket's listen queue.
     * @param bindAddr The local address to which to bind. If null, an
     *      unspecified local address will be bound to.
     * @param certApprovalCallback Will get called to approve any certificate
     *      presented by the client.
     */
    public SSLServerSocket(int port, int backlog, InetAddress bindAddr,
                SSLCertificateApprovalCallback certApprovalCallback)
        throws IOException 
    {
        this(port,backlog, bindAddr, certApprovalCallback, false);
    }

    /**
     * Creates a server socket listening on the given port.
     * @param backlog The size of the socket's listen queue.
     * @param bindAddr The local address to which to bind. If null, an
     *      unspecified local address will be bound to.
     * @param certApprovalCallback Will get called to approve any certificate
     *      presented by the client.
     * @param reuseAddr Reuse the local bind port; this parameter sets
     *      the <tt>SO_REUSEADDR</tt> option on the socket before calling
     *      <tt>bind()</tt>. The default is <tt>false</tt> for backward
     *      compatibility.
     */
    public SSLServerSocket(int port, int backlog, InetAddress bindAddr,
                SSLCertificateApprovalCallback certApprovalCallback,
                boolean reuseAddr)
        throws IOException 
    {
        // Dance the dance of fools.  The superclass doesn't have a default
        // constructor, so we have to trick it here. This is an example
        // of WHY WE SHOULDN'T BE EXTENDING SERVERSOCKET.
        super(0);
        super.close();

        // create the socket
        sockProxy = new SocketProxy(
            base.socketCreate(this, certApprovalCallback, null) );

        base.setProxy(sockProxy);

        setReuseAddress(reuseAddr);

        // bind it to the local address and port
        if( bindAddr == null ) {
            bindAddr = anyLocalAddr;
        }
        byte[] bindAddrBA = null;
        if( bindAddr != null ) {
            bindAddrBA = bindAddr.getAddress();
        }
        base.socketBind(bindAddrBA, port);
        socketListen(backlog);
    }

    private SocketProxy sockProxy;
    private boolean handshakeAsClient=false;
    private SocketBase base = new SocketBase();

    private native void socketListen(int backlog) throws SocketException;

    private static InetAddress anyLocalAddr;
    static {
        try {
            anyLocalAddr = InetAddress.getByName("0.0.0.0");
        } catch (java.net.UnknownHostException e) { }
    }

    /**
     * Accepts a connection. This call will block until a connection is made
     *   or the timeout is reached.
     */
    public Socket accept() throws IOException {
        SSLSocket s = new SSLSocket();
        /* socketAccept can throw an exception for timeouts or IO errors */
        /* so first get a socket pointer, and if successful create the SocketProxy */
        byte[] socketPointer = null;
        socketPointer = socketAccept(s, base.getTimeout(), handshakeAsClient);
        SocketProxy sp = new SocketProxy(socketPointer );
        s.setSockProxy(sp);
        return s;
    }

    /**
     * Sets the SO_TIMEOUT socket option.
     * @param timeout The timeout time in milliseconds.
     */
    public void setSoTimeout(int timeout) {
        base.setTimeout(timeout);
    }

    /**
     * Returns the current value of the SO_TIMEOUT socket option.
     * @return The timeout time in milliseconds.
     */
    public int getSoTimeout() {
        return base.getTimeout();
    }

    public native void setReuseAddress(boolean reuse) throws SocketException;
    public native boolean getReuseAddress() throws SocketException;

    private native byte[] socketAccept(SSLSocket s, int timeout,
        boolean handshakeAsClient) throws SocketException;

    /**
     * Empties the SSL client session ID cache.
     */
    public static native void clearSessionCache();

    protected void finalize() throws Throwable { }


    /**
     * @return The local port.
     */
    public int getLocalPort() {
        return base.getLocalPort();
    }

    /**
     * Closes this socket.
     */
    public void close() throws IOException {
        base.close();
    }

    // This directory is used as the default for the Session ID cache
    private final static String UNIX_TEMP_DIR = "/tmp";
    private final static String WINDOWS_TEMP_DIR = "\\temp";

    /**
     * Configures the session ID cache.
     * @param maxSidEntries The maximum number of entries in the cache. If
     *  0 is passed, the default of 10,000 is used.
     * @param ssl2EntryTimeout The lifetime in seconds of an SSL2 session.
     *  The minimum timeout value is 5 seconds and the maximum is 24 hours.
     *  Values outside this range are replaced by the server default value
     *  of 100 seconds.
     * @param ssl3EntryTimeout The lifetime in seconds of an SSL3 session.
     *  The minimum timeout value is 5 seconds and the maximum is 24 hours.
     *  Values outside this range are replaced by the server default value
     *  of 100 seconds.
     * @param cacheFileDirectory The pathname of the directory that
     *  will contain the session cache. If null is passed, the server default
     *  is used: <code>/tmp</code> on Unix and <code>\\temp</code> on Windows.
     */
    public static native void configServerSessionIDCache(int maxSidEntries,
        int ssl2EntryTimeout, int ssl3EntryTimeout, String cacheFileDirectory)
        throws SocketException;

    /**
     * Sets the certificate to use for server authentication.
     */
    public void setServerCertNickname(String nick) throws SocketException
    {
      try {
        setServerCert( CryptoManager.getInstance().findCertByNickname(nick) );
      } catch(CryptoManager.NotInitializedException nie) {
        throw new SocketException("CryptoManager not initialized");
      } catch(ObjectNotFoundException onfe) {
        throw new SocketException("Object not found: " + onfe);
      } catch(TokenException te) {
        throw new SocketException("Token Exception: " + te);
      }
    }

    /**
     * Sets the certificate to use for server authentication.
     */
    public native void setServerCert(
        org.mozilla.jss.crypto.X509Certificate certnickname)
        throws SocketException;

    /**
     * Enables/disables the request of client authentication. This is only
     *  meaningful for the server end of the SSL connection. During the next
     *  handshake, the remote peer will be asked to authenticate itself.
     * @see org.mozilla.jss.ssl.SSLServerSocket#requireClientAuth
     */
    public void requestClientAuth(boolean b) throws SocketException {
        base.requestClientAuth(b);
    }

    /**
     * @deprecated As of JSS 3.0. This method is misnamed. Use
     *  <code>requestClientAuth</code> instead.
     */
    public void setNeedClientAuth(boolean b) throws SocketException {
        base.requestClientAuth(b);
    }

    /**
     * Enables/disables the request of client authentication. This is only
     *  meaningful for the server end of the SSL connection. During the next
     *  handshake, the remote peer will be asked to authenticate itself.
     *  <p>In addition, the client certificate's expiration will not
     *  prevent it from being accepted.
     * @see org.mozilla.jss.ssl.SSLServerSocket#requireClientAuth
    public void requestClientAuthNoExpiryCheck(boolean b)
        throws SocketException
    {
        base.requestClientAuthNoExpiryCheck(b);
    }

    /**
     * @deprecated As of JSS 3.0. This method is misnamed. Use
     *  <code>requestClientAuthNoExpiryCheck</code> instead.
     */
    public void setNeedClientAuthNoExpiryCheck(boolean b)
        throws SocketException
    {
        base.requestClientAuthNoExpiryCheck(b);
    }

    /**
     * Enables SSL v2 on this socket. It is enabled  by default, unless the
     * default has been changed with <code>SSLSocket.enableSSL2Default</code>.
     */
    public void enableSSL2(boolean enable) throws SocketException {
        base.enableSSL2(enable);
    }

    /**
     * Enables SSL v3 on this socket. It is enabled by default, unless the
     *  default has been changed with <code>SSLSocket.enableSSL3Default</code>.
     */
    public void enableSSL3(boolean enable) throws SocketException {
        base.enableSSL3(enable);
    }

    /**
     * @return the local address of this server socket.
     */
    public InetAddress getInetAddress() {
        return base.getLocalAddress();
    }

    /**
     * Sets whether the socket requires client authentication from the remote
     *  peer. If requestClientAuth() has not already been called, this
     *  method will tell the socket to request client auth as well as requiring
     *  it.
     */
    public void requireClientAuth(boolean require, boolean onRedo)
            throws SocketException
    {
        base.requireClientAuth(require, onRedo);
    }

    /**
     * Sets the nickname of the certificate to use for client authentication.
     */
    public void setClientCertNickname(String nick) throws SocketException {
        base.setClientCertNickname(nick);
    }

    /**
     * Sets the certificate to use for client authentication.
     */
    public void setClientCert(org.mozilla.jss.crypto.X509Certificate cert)
        throws SocketException
    {
        base.setClientCert(cert);
    }

    /**
     * Determines whether this end of the socket is the client or the server
     *  for purposes of the SSL protocol. By default, it is the server.
     * @param b true if this end of the socket is the SSL slient, false
     *      if it is the SSL server.
     */
    public void setUseClientMode(boolean b) {
        handshakeAsClient = b;
    }

    /**
     * Enables/disables the session cache. By default, the session cache
     * is enabled.
     */
    public void useCache(boolean b) throws SocketException {
        base.useCache(b);
    }

    /**
     * Returns the addresses and ports of this socket.
     */
    public String toString() {
        StringBuffer buf = new StringBuffer();
        buf.append("SSLServerSocket[addr=");
        buf.append(getInetAddress());
        buf.append(",port=0,localport=");
        buf.append(getLocalPort());
        buf.append("]");
        return buf.toString();
    }
}
