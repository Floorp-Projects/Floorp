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
 * Portions created by the Initial Developer are Copyright (C) 1998-2001
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
import java.net.SocketTimeoutException;
import java.io.*;
import java.io.IOException;
import java.util.Vector;

/**
 * SSL client socket.
 */
public class SSLSocket extends java.net.Socket {

    /*
     * Locking strategy of SSLSocket
     *
     * isClosed, inRead, and inWrite must be accessed with the object
     * locked.
     *
     * readLock must be locked throughout the read method.  It is used
     * to serialize read calls.
     *
     * writeLock must be locked throughout the write method. It is used
     * to serialize write calls.
     */

    private java.lang.Object readLock = new java.lang.Object();
    private java.lang.Object writeLock = new java.lang.Object();
    private boolean isClosed = false;
    private boolean inRead = false;
    private boolean inWrite = false;
    private InetAddress inetAddress;
    private int port;
    private SocketProxy sockProxy = null;
    private boolean open = false;
    private boolean handshakeAsClient = true;
    private SocketBase base = new SocketBase();
    static final public int SSL_REQUIRE_NEVER =  
           org.mozilla.jss.ssl.SocketBase.SSL_REQUIRE_NEVER;
    static final public int SSL_REQUIRE_ALWAYS = 
           org.mozilla.jss.ssl.SocketBase.SSL_REQUIRE_ALWAYS;
    static final public int SSL_REQUIRE_FIRST_HANDSHAKE = 
           org.mozilla.jss.ssl.SocketBase.SSL_REQUIRE_FIRST_HANDSHAKE;
    static final public int SSL_REQUIRE_NO_ERROR = 
           org.mozilla.jss.ssl.SocketBase.SSL_REQUIRE_NO_ERROR;

    /**
     * For sockets that get created by accept().
     */
    SSLSocket() throws IOException {
    }

    /**
     * Should only be called by SSLServerSocket after a successful
     * accept().
     */
    void setSockProxy(SocketProxy sp) {
        sockProxy = sp;
        base.setProxy(sp);
    }

    /**
     * Creates an SSL client socket and connects to the specified host and
     *  port.
     *
     * @param host The hostname to connect to.
     * @param port The port to connect to.
     */
    public SSLSocket(String host, int port)
        throws UnknownHostException, IOException
    {
        this(InetAddress.getByName(host), port, null, 0);
    }

    /**
     * Creates an SSL client socket and connects to the specified address and
     *  port.
     *
     * @param address The IP address to connect to.
     * @param port The port to connect to.
     */
    public SSLSocket(InetAddress address, int port)
        throws IOException
    {
        this(address, port, null, 0);
    }

    /**
     * Creates an SSL client socket and connects to the specified host and
     *  port. Binds to the given local address and port.
     *
     * @param host The hostname to connect to.
     * @param port The port to connect to.
     * @param localAddr The local address to bind to. It can be null, in which
     *      case an unspecified local address will be chosen.
     * @param localPort The local port to bind to. If 0, a random port will be
     *      assigned to the socket.
     */
    public SSLSocket(String host, int port, InetAddress localAddr,
        int localPort) throws IOException
    {
        this(InetAddress.getByName(host), port, localAddr, localPort);
    }

    /**
     * Creates an SSL client socket and connects to the specified address and
     *  port. Binds to the given local address and port.
     *
     * @param address The IP address to connect to.
     * @param port The port to connect to.
     * @param localAddr The local address to bind to. It can be null, in which
     *      case an unspecified local address will be chosen.
     * @param localPort The local port to bind to. If 0, a random port will be
     *      assigned to the socket.
     */
    public SSLSocket(InetAddress address, int port, InetAddress localAddr,
        int localPort) throws IOException
    {
        this(address, port, localAddr, localPort, null, null);
    }

    /**
     * Creates an SSL client socket and connects to the specified host and
     *  port. Binds to the given local address and port. Installs the given
     *  callbacks for certificate approval and client certificate selection.
     *
     * @param host The hostname to connect to.
     * @param port The port to connect to.
     * @param localAddr The local address to bind to. It can be null, in which
     *      case an unspecified local address will be chosen.
     * @param localPort The local port to bind to. If 0, a random port will be
     *      assigned to the socket.
     * @param certApprovalCallback A callback that can be used to override
     *      approval of the peer's certificate.
     * @param clientCertSelectionCallback A callback to select the client
     *      certificate to present to the peer.
     */
    public SSLSocket(String host, int port, InetAddress localAddr,
        int localPort, SSLCertificateApprovalCallback certApprovalCallback,
        SSLClientCertificateSelectionCallback clientCertSelectionCallback)
            throws IOException
    {
        this(InetAddress.getByName(host), port, localAddr, localPort,
                certApprovalCallback, clientCertSelectionCallback);
    }

    /**
     * Creates an SSL client socket and connects to the specified host and
     *  port. Binds to the given local address and port. Installs the given
     *  callbacks for certificate approval and client certificate selection.
     *
     * @param host The hostname to connect to.
     * @param port The port to connect to.
     * @param localAddr The local address to bind to. It can be null, in which
     *      case an unspecified local address will be chosen.
     * @param localPort The local port to bind to. If 0, a random port will be
     *      assigned to the socket.
     * @param stream This parameter is ignored. All SSLSockets are stream
     *      sockets.
     * @param certApprovalCallback A callback that can be used to override
     *      approval of the peer's certificate.
     * @param clientCertSelectionCallback A callback to select the client
     *      certificate to present to the peer.
     * @deprecated As of JSS 3.0. The stream parameter is ignored, because
     *      only stream sockets are supported.
     */
    public SSLSocket(InetAddress address, int port, InetAddress localAddr,
        int localPort, boolean stream,
        SSLCertificateApprovalCallback certApprovalCallback,
        SSLClientCertificateSelectionCallback clientCertSelectionCallback)
            throws IOException
    {
        this(address, port, localAddr, localPort,
            certApprovalCallback, clientCertSelectionCallback);
    }

    /**
     * Creates an SSL client socket and connects to the specified address and
     *  port. Binds to the given local address and port. Installs the given
     *  callbacks for certificate approval and client certificate selection.
     *
     * @param address The IP address to connect to.
     * @param port The port to connect to.
     * @param localAddr The local address to bind to. It can be null, in which
     *      case an unspecified local address will be chosen.
     * @param localPort The local port to bind to. If 0, a random port will be
     *      assigned to the socket.
     * @param certApprovalCallback A callback that can be used to override
     *      approval of the peer's certificate.
     * @param clientCertSelectionCallback A callback to select the client
     *      certificate to present to the peer.
     */
    public SSLSocket(InetAddress address, int port,
        InetAddress localAddr,
        int localPort, SSLCertificateApprovalCallback certApprovalCallback,
        SSLClientCertificateSelectionCallback clientCertSelectionCallback)
            throws IOException
    {
        this(address, address.getHostName(), port, localAddr, localPort,
            certApprovalCallback, clientCertSelectionCallback);
    }

    private SSLSocket(InetAddress address, String hostname, int port,
        InetAddress localAddr,
        int localPort, SSLCertificateApprovalCallback certApprovalCallback,
        SSLClientCertificateSelectionCallback clientCertSelectionCallback)
            throws IOException
    {
        // create the socket
        sockProxy =
            new SocketProxy(
                base.socketCreate(
                    this, certApprovalCallback, clientCertSelectionCallback) );

        base.setProxy(sockProxy);

        // bind it to local address and port
        if( localAddr != null || localPort > 0 ) {
            // bind because they specified a local address
            byte[] addrBA = null;
            if( localAddr != null ) {
                addrBA = localAddr.getAddress();
            }
            base.socketBind(addrBA, localPort);
        }

        /* connect to the remote socket */
        socketConnect(address.getAddress(), hostname, port);
    }

    /**
     * Creates an SSL client socket using the given Java socket for underlying
     *  I/O. Installs the given callbacks for certificate approval and
     *  client certificate selection.
     *
     * @param s The Java socket to use for underlying I/O.
     * @param host The hostname of the remote side of the connection.
     *      This name is used to verify the server's certificate.
     * @param certApprovalCallback A callback that can be used to override
     *      approval of the peer's certificate.
     * @param clientCertSelectionCallback A callback to select the client
     *      certificate to present to the peer.
     */
    public SSLSocket(java.net.Socket s, String host,
        SSLCertificateApprovalCallback certApprovalCallback,
        SSLClientCertificateSelectionCallback clientCertSelectionCallback)
            throws IOException
    {
        // create the socket
        sockProxy =
            new SocketProxy(
                base.socketCreate(
                    this, certApprovalCallback, clientCertSelectionCallback,
                    s, host ) );

        base.setProxy(sockProxy);
        resetHandshake();
    }

    /**
     * @return The remote peer's IP address or null if the SSLSocket is closed.
     */
    public InetAddress getInetAddress() {
        synchronized (this) {
            if( isClosed ) {
                return null;
            }
            else return base.getInetAddress();
        }
    }

    /**
     * @return The local IP address or null if the SSLSocket is closed.
     */
    public InetAddress getLocalAddress() {
        synchronized (this) {
            if( isClosed ) {
                return null;
            }
            else return base.getLocalAddress();
        }
    }

    /**
     * @return The local port or -1 if the SSLSocket is closed.
     */
    public int getLocalPort() {
        synchronized (this) {
            if( isClosed ) {
                return -1;
            }
            else  return base.getLocalPort();
        }
    }

    /**
     * @return The remote port.
     */
    public native int getPort();

    /**
     * Returns the input stream for reading from this socket.
     */
    public InputStream getInputStream() throws IOException {
        return new SSLInputStream(this);
    }

    /**
     * Returns the output stream for writing to this socket.
     */
    public OutputStream getOutputStream() throws IOException {
        return new SSLOutputStream(this);
    }

    /**
     * Enables or disables the TCP_NO_DELAY socket option. Enabling this
     * option will <i>disable</i> the Nagle algorithm.
     */
    public native void setTcpNoDelay(boolean on) throws SocketException;

    /**
     * Returns the current setting of the TCP_NO_DELAY socket option.
     */
    public native boolean getTcpNoDelay() throws SocketException;

    /**
     * Enables or disables the SO_KEEPALIVE socket option.
     */
    public native void setKeepAlive(boolean on) throws SocketException;

    /**
     * Returns the current setting of the SO_KEEPALIVE socket option.
     */
    public native boolean getKeepAlive() throws SocketException;

    /**
     * Shuts down the input side of the socket.
     */
    public void shutdownInput() throws IOException {
        shutdownNative(SocketBase.PR_SHUTDOWN_RCV);
    }

    /**
     * Shuts down the output side of the socket.
     */
    public void shutdownOutput() throws IOException {
        shutdownNative(SocketBase.PR_SHUTDOWN_SEND);
    }

    private native void shutdownNative(int how) throws IOException;
    private native void abortReadWrite() throws IOException;
    /**
     * Sets the SO_LINGER socket option.
     * param linger The time (in seconds) to linger for.
     */
    public native void setSoLinger(boolean on, int linger)
        throws SocketException;

    /**
     * Returns the current value of the SO_LINGER socket option.
     */
    public native int getSoLinger() throws SocketException;

    /**
     * Sets the SO_TIMEOUT socket option.
     * @param timeout The timeout time in milliseconds.
     */
    public void setSoTimeout(int timeout) throws SocketException {
        base.setTimeout(timeout);
    }

    /**
     * Returns the current value of the SO_TIMEOUT socket option.
     * @return The timeout time in milliseconds.
     */
    public int getSoTimeout() throws SocketException {
        return base.getTimeout();
    }

    /**
     * Sets the size (in bytes) of the send buffer.
     */
    public native void setSendBufferSize(int size) throws SocketException;

    /**
     * Returns the size (in bytes) of the send buffer.
     */
    public native int getSendBufferSize() throws SocketException;

    /**
     * Sets the size (in bytes) of the receive buffer.
     */
    public native void setReceiveBufferSize(int size) throws SocketException;

    /** 
     * Returnst he size (in bytes) of the receive buffer.
     */
    public native int getReceiveBufferSize() throws SocketException;

    /** 
     * Closes this socket.
     */
    public void close() throws IOException {
        synchronized (this) {
            if( isClosed ) {
                /* finalize calls close or user calls close more than once */
                return;
            }
            isClosed = true;
            if( sockProxy == null ) {
                /* nothing to do */
                return;
            }
            /*
             * If a read or write is occuring, abort the I/O.  Any
             * further attempts to read/write will fail since isClosed
             * is true
             */
            if ( inRead || inWrite ) {
                abortReadWrite();
            }
        }
        /*
         * Lock readLock and writeLock to ensure that read and write
         * have been aborted.
         */
        synchronized (readLock) {
            synchronized (writeLock) {
                base.close();
                sockProxy = null;
                base.setProxy(null);
            }
        }
    }

    private native void socketConnect(byte[] addr, String hostname, int port)
        throws SocketException;

    ////////////////////////////////////////////////////////////////////
    // SSL-specific stuff
    ////////////////////////////////////////////////////////////////////
    private Vector handshakeCompletedListeners = new Vector();

    /**
     * Adds a listener to be notified when an SSL handshake completes.
     */
    public void addHandshakeCompletedListener(SSLHandshakeCompletedListener l) {
        handshakeCompletedListeners.addElement(l);
    }

    /**
     * Removes a previously registered listener for handshake completion.
     */
    public void removeHandshakeCompletedListener(
            SSLHandshakeCompletedListener l) {
        handshakeCompletedListeners.removeElement(l);
    }

    private void notifyAllHandshakeListeners() {
        SSLHandshakeCompletedEvent event =
            new SSLHandshakeCompletedEvent(this);

        /* XXX NOT THREAD SAFE */
        int i;
        for( i=0; i < handshakeCompletedListeners.size(); ++i) {
            SSLHandshakeCompletedListener l =
                (SSLHandshakeCompletedListener)
                 handshakeCompletedListeners.elementAt(i);
            l.handshakeCompleted(event);
        }
    }
               

    /**
     * Enables SSL v2 on this socket. It is enabled  by default, unless the
     * default has been changed with <code>enableSSL2Default</code>.
     */
    public void enableSSL2(boolean enable) throws SocketException {
        base.enableSSL2(enable);
    }

    /**
     * Sets the default for SSL v2 for all new sockets.
     */
    static public void enableSSL2Default(boolean enable) throws SocketException{
        setSSLDefaultOption(SocketBase.SSL_ENABLE_SSL2, enable);
    }

    /**
     * Enables SSL v3 on this socket. It is enabled by default, unless the
     * default has been changed with <code>enableSSL3Default</code>.
     */
    public void enableSSL3(boolean enable) throws SocketException {
        base.enableSSL3(enable);
    }

    /**
     * Sets the default for SSL v3 for all new sockets.
     */
    static public void enableSSL3Default(boolean enable) throws SocketException{
        setSSLDefaultOption(SocketBase.SSL_ENABLE_SSL3, enable);
    }

    /**
     * Enables TLS on this socket.  It is enabled by default, unless the
     * default has been changed with <code>enableTLSDefault</code>.
     */
    public void enableTLS(boolean enable) throws SocketException {
        base.enableTLS(enable);
    }

    /**
     * Sets the default for TLS for all new sockets.
     */
    static public void enableTLSDefault(boolean enable) throws SocketException{
        setSSLDefaultOption(SocketBase.SSL_ENABLE_TLS, enable);
    }

    /**
     * Enables bypass of PKCS11 on this socket.  
     * It is disabled by default, unless the default has been changed 
     * with <code>bypassPKCS11Default</code>.
     */
    public void bypassPKCS11(boolean enable) throws SocketException {
        base.bypassPKCS11(enable);
    }

    /**
     * Sets the default to bypass PKCS11 layer (except for public keys)
     * for all new sockets.
     */
    static public void bypassPKCS11Default(boolean enable) 
        throws SocketException{
        setSSLDefaultOption(SocketBase.SSL_BYPASS_PKCS11, enable);
    }

    /**
     * Enable rollback detection for this socket.
     * It is enabled by default, unless the default has been changed 
     * with <code>enableRollbackDetectionDefault</code>.
     */
    public void enableRollbackDetection(boolean enable) 
        throws SocketException 
    {
        base.enableRollbackDetection(enable);
    }
    
    /**
     * Sets the default rollback detection for all new sockets.
     */
    static void enableRollbackDetectionDefault(boolean enable) 
        throws SocketException 
    {
        setSSLDefaultOption(SocketBase.SSL_ROLLBACK_DETECTION, enable);
    }
    
    /**
     * This option, enableStepDown, is concerned with the generation 
     * of step-down keys which are used with export suites.
     * If the server cert's public key is 512 bits or less
     * this option is ignored because step-down keys don't
     * need to be generated.
     * If the server cert's public key is more than 512 bits,
     * this option has the following effect:
     * enable=true:  generate step-down keys
     * enable=false: don't generate step-down keys; disable
     * export cipher suites
     *
     * This option is enabled by default; unless the default has  
     * been changed with <code>SSLSocket.enableStepDownDefault</code>.
     */
    public void enableStepDown(boolean enable) throws SocketException {
        base.enableStepDown(enable);
    }
    /**
     * This option, enableStepDownDefault, is concerned with the  
     * generation of step-down keys which are used with export suites. 
     * This options will set the default for all sockets.
     * If the server cert's public key is 512 bits of less,
     * this option is ignored because step-down keys don't
     * need to be generated.
     * If the server cert's public key is more than 512 bits,
     * this option has the following effect:
     * enable=true:  generate step-down keys
     * enable=false: don't generate step-down keys; disable
     *			export cipher suites
     *
     * This option is enabled by default for all sockets.
     */
    static void enableStepDownDefault(boolean enable) 
    throws SocketException 
    {
        setSSLDefaultOption(SocketBase.SSL_NO_STEP_DOWN, enable);
    }

    /**
     * Enable simultaneous read/write by separate read and write threads 
     * (full duplex) for this socket.
     * It is disabled by default, unless the default has been changed 
     * with <code>enableFDXDefault</code>.
     */
    public void enableFDX(boolean enable) 
    throws SocketException 
    {
        base.enableFDX(enable);
    }
    
    /**
     * Sets the default to permit simultaneous read/write 
     * by separate read and write threads (full duplex)
     * for all new sockets.
     */
    static void enableFDXDefault(boolean enable) 
    throws SocketException 
    {
        setSSLDefaultOption(SocketBase.SSL_ENABLE_FDX, enable);
    }

    /**
     * Enable sending v3 client hello in v2 format for this socket.
     * It is enabled by default, unless the default has been changed 
     * with <code>enableV2CompatibleHelloDefault</code>.
     */
    public void enableV2CompatibleHello(boolean enable) 
    throws SocketException 
    {
        base.enableV2CompatibleHello(enable);
    }
    
    /**    
     * Sets the default to send v3 client hello in v2 format
     * for all new sockets.
     */
    static void enableV2CompatibleHelloDefault(boolean enable) 
    throws SocketException 
    {
        setSSLDefaultOption(SocketBase.SSL_V2_COMPATIBLE_HELLO, enable);
    }
    
    /**
     * @return a String listing the current SSLOptions for this SSLSocket.
     */
    public String getSSLOptions() {
        return base.getSSLOptions();
    }
    
    /**
     * 
     * @param option 
     * @return 0 for option disabled 1 for option enabled. 
     */
    static private native int getSSLDefaultOption(int option)
        throws SocketException;

    /**
     * 
     * @return a String listing  the Default SSLOptions for all SSLSockets.
     */
    static public String getSSLDefaultOptions() {
        StringBuffer buf = new StringBuffer();
        try {
            buf.append("Default Options configured for all SSLSockets: ");
            buf.append("\nSSL_ENABLE_SSL2" + 
                ((getSSLDefaultOption(SocketBase.SSL_ENABLE_SSL2) != 0)
                ? "=on" :  "=off"));
            buf.append("\nSSL_ENABLE_SSL3"  + 
                ((getSSLDefaultOption(SocketBase.SSL_ENABLE_SSL3) != 0) 
                ? "=on" :  "=off"));
            buf.append("\nSSL_ENABLE_TLS"  + 
                ((getSSLDefaultOption(SocketBase.SSL_ENABLE_TLS) != 0) 
                ? "=on" :  "=off"));
            buf.append("\nSSL_REQUIRE_CERTIFICATE"); 
            switch (getSSLDefaultOption(SocketBase.SSL_REQUIRE_CERTIFICATE)) {
                case 0:
                    buf.append("=Never");
                    break;
                case 1:
                    buf.append("=Always");
                    break;
               case 2:
                    buf.append("=First Handshake");
                    break;
               case 3:
                   buf.append("=No Error");
                   break;
              default:
                   buf.append("=Report JSS Bug this option has a status.");
                   break;
            } //end switch
            buf.append("\nSSL_REQUEST_CERTIFICATE"  + 
                ((getSSLDefaultOption(SocketBase.SSL_REQUEST_CERTIFICATE) != 0) 
                ? "=on" :  "=off"));
            buf.append("\nSSL_NO_CACHE"  + 
                ((getSSLDefaultOption(SocketBase.SSL_NO_CACHE) != 0)
                ? "=on" :  "=off"));
            buf.append("\nSSL_BYPASS_PKCS11"  + 
                ((getSSLDefaultOption(SocketBase.SSL_BYPASS_PKCS11) != 0) 
                ? "=on" :  "=off"));
            buf.append("\nSSL_ROLLBACK_DETECTION"  + 
                ((getSSLDefaultOption(SocketBase.SSL_ROLLBACK_DETECTION) != 0)
                ? "=on" :  "=off"));
            buf.append("\nSSL_NO_STEP_DOWN"  + 
                ((getSSLDefaultOption(SocketBase.SSL_NO_STEP_DOWN) != 0)
                ? "=on" :  "=off"));
            buf.append("\nSSL_ENABLE_FDX"  + 
                ((getSSLDefaultOption(SocketBase.SSL_ENABLE_FDX) != 0)
                ? "=on" :  "=off"));
            buf.append("\nSSL_V2_COMPATIBLE_HELLO"  + 
                ((getSSLDefaultOption(SocketBase.SSL_V2_COMPATIBLE_HELLO) != 0) 
                ? "=on" :  "=off"));
        } catch (SocketException e) {
            buf.append("\ngetSSLDefaultOptions exception " + e.getMessage());
        }
        return buf.toString();
    }
    
    /**
     *  Sets whether the socket requires client authentication from the remote
     *  peer. If requestClientAuth() has not already been called, this
     *  method will tell the socket to request client auth as well as requiring
     *  it.
     * @deprecated use requireClientAuth(int)
     */
    public void requireClientAuth(boolean require, boolean onRedo)
            throws SocketException
    {
        base.requireClientAuth(require, onRedo);
    }

    /**
     *  Sets whether the socket requires client authentication from the remote
     *  peer. If requestClientAuth() has not already been called, this method
     *  will tell the socket to request client auth as well as requiring it.
     *  This is only meaningful for the server end of the SSL connection. 
     *  During the next handshake, the remote peer will be asked to 
     *  authenticate itself with the requirement that was set.
     *
     *  @param mode One of:  SSLSocket.SSL_REQUIRE_NEVER, 
     *                       SSLSocket.SSL_REQUIRE_ALWAYS, 
     *                       SSLSocket.SSL_REQUIRE_FIRST_HANDSHAKE, 
     *                       SSLSocket.SSL_REQUIRE_NO_ERROR
     */
    public void requireClientAuth(int mode)
            throws SocketException
    {
        if (mode >= SocketBase.SSL_REQUIRE_NEVER && 
            mode <= SocketBase.SSL_REQUIRE_NO_ERROR) {
            base.requireClientAuth(mode);
        } else {
            throw new SocketException("Incorrect input value.");
        }
    }

    /**
     *  Sets the default setting for requiring client authorization.
     *  All subsequently created sockets will use this default setting.
     * @deprecated use requireClientAuthDefault(int)
     */
    public void requireClientAuthDefault(boolean require, boolean onRedo)
            throws SocketException
    {
        setSSLDefaultOption(SocketBase.SSL_REQUIRE_CERTIFICATE,
                            require ? (onRedo ? 1 : 2) : 0);
    }

    /**
     *  Sets the default setting for requiring client authorization.
     *  All subsequently created sockets will use this default setting
     *  This is only meaningful for the server end of the SSL connection.
     *
     *  @param mode One of:  SSLSocket.SSL_REQUIRE_NEVER, 
     *                       SSLSocket.SSL_REQUIRE_ALWAYS, 
     *                       SSLSocket.SSL_REQUIRE_FIRST_HANDSHAKE, 
     *                       SSLSocket.SSL_REQUIRE_NO_ERROR
     */
    static public void requireClientAuthDefault(int mode)
            throws SocketException
    {
        if (mode >= SocketBase.SSL_REQUIRE_NEVER && 
            mode <= SocketBase.SSL_REQUIRE_NO_ERROR) {
            setSSLDefaultOption(SocketBase.SSL_REQUEST_CERTIFICATE, true);
            setSSLDefaultOptionMode(SocketBase.SSL_REQUIRE_CERTIFICATE,mode);
        } else {

            throw new SocketException("Incorrect input value.");
        }
    }

    /**
     * Force an already started SSL handshake to complete.
     * This method should block until the handshake has completed.
     */
    public native void forceHandshake() throws SocketException;

    /** 
     * Determines whether this end of the socket is the client or the server
     *  for purposes of the SSL protocol. By default, it is the client.
     * @param b true if this end of the socket is the SSL slient, false
     *      if it is the SSL server.
     */
    public void setUseClientMode(boolean b) {
        handshakeAsClient = b;
    }

    /**
     * @return true if this end of the socket is the SSL client, false
     *  if it is the SSL server.
     */
    public boolean getUseClientMode() {
        return handshakeAsClient;
    }

    /**
     * Resets the handshake state.
     */
    public void resetHandshake() throws SocketException {
        resetHandshakeNative(handshakeAsClient);
    }

    private native void resetHandshakeNative(boolean asClient)
        throws SocketException;

    /**
     * Returns the security status of this socket.
     */
    public native SSLSecurityStatus getStatus() throws SocketException;

    /**
     * Sets the nickname of the certificate to use for client authentication.
     * Alternately, you can specify an SSLClientCertificateSelectionCallback,
     * which will receive a list of certificates that are valid for client
     * authentication.
     * @see org.mozilla.jss.ssl.SSLClientCertificateSelectionCallback
     */
    public void setClientCertNickname(String nick) throws SocketException {
        base.setClientCertNickname(nick);
    }

    /**
     * Sets the certificate to use for client authentication.
     * Alternately, you can specify an SSLClientCertificateSelectionCallback,
     * which will receive a list of certificates that are valid for client
     * authentication.
     * @see org.mozilla.jss.ssl.SSLClientCertificateSelectionCallback
     */
    public void setClientCert( org.mozilla.jss.crypto.X509Certificate cert)
        throws SocketException
    {
        base.setClientCert(cert);
    }


    /**
     * Enables/disables the request of client authentication. This is only
     *  meaningful for the server end of the SSL connection. During the next
     *  handshake, the remote peer will be asked to authenticate itself.
     * @see org.mozilla.jss.ssl.SSLSocket#requireClientAuth
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
     * @see org.mozilla.jss.ssl.SSLSocket#requireClientAuth
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
     * Enables/disables the session cache. By default, the session cache
     * is enabled.
     */
    public void useCache(boolean b) throws SocketException {
        base.useCache(b);
    }

    /** 
     * Sets the default setting for use of the session cache.
     */
    public void useCacheDefault(boolean b) throws SocketException {
        setSSLDefaultOption(SocketBase.SSL_NO_CACHE, !b);
    }


    private static void setSSLDefaultOption(int option, boolean on)
        throws SocketException
    {
        setSSLDefaultOption(option, on ? 1 : 0);
    }

    /** 
     * Sets SSL Default options that have simple enable/disable values.
     */
    private static native void setSSLDefaultOption(int option, int on)
        throws SocketException;

    /** 
     * Set SSL default options that have more modes than enable/disable.
     */
    private static native void setSSLDefaultOptionMode(int option, int mode)
        throws SocketException;

    /**
     * Enables/disables the cipher on this socket.
     */
    public native void setCipherPreference(int cipher, boolean enable)
        throws SocketException;

    /**
     * Returns whether this cipher is enabled or disabled on this socket.
     */
    public native boolean getCipherPreference( int cipher)
        throws SocketException;

    /**
     * Sets the default for whether this cipher is enabled or disabled.
     */
    public static native void setCipherPreferenceDefault(int cipher,
        boolean enable) throws SocketException;

    /**
     * Returns the default for whether this cipher is enabled or disabled.
     */
    public static native boolean getCipherPreferenceDefault(int cipher)
        throws SocketException;

    native int socketAvailable()
        throws IOException;

    int read(byte[] b, int off, int len) 
        throws IOException, SocketTimeoutException {
        synchronized (readLock) {
            synchronized (this) {
                if ( isClosed ) { /* abort read if socket is closed */
                    throw new IOException(
                        "Socket has been closed, and cannot be reused."); 
                }
                inRead = true;            
            }
            int iRet;
            try {
                iRet = socketRead(b, off, len, base.getTimeout()); 
            } catch (SocketTimeoutException ste) {
                throw new SocketTimeoutException(
                    "SocketTimeoutException cannot read on socket");
            } catch (IOException ioe) {
                throw new IOException(
                    "SocketException cannot read on socket");
            } finally {
                synchronized (this) {
                    inRead = false;
                }
            }
            return iRet;
        }
    }

    void write(byte[] b, int off, int len) 
        throws IOException, SocketTimeoutException {
        synchronized (writeLock) {
            synchronized (this) {
                if ( isClosed ) { /* abort write if socket is closed */
                    throw new IOException(
                        "Socket has been closed, and cannot be reused."); 
                }
                inWrite = true;
            }
            try {
                socketWrite(b, off, len, base.getTimeout());
            } catch (SocketTimeoutException ste) {
                throw new SocketTimeoutException(
                    "SocketTimeoutException cannot write on socket");
            } catch (IOException ioe) {
                throw new IOException(
                    "SocketException cannot write on socket");
            } finally {
                synchronized (this) {
                    inWrite = false;
                }
            }
        }
    }

    private native int socketRead(byte[] b, int off, int len, int timeout)
        throws IOException;

    private native void socketWrite(byte[] b, int off, int len, int timeout)
        throws IOException;

    /**
     * Removes the current session from the session cache.
     */
    public native void invalidateSession() throws SocketException;

    /**
     * Causes SSL to begin a full, new SSL 3.0 handshake from scratch
     * on a connection that has already completed one handshake.
     * <p>Does not flush the SSL3 cache entry first, so a full handshake
     *  will not take place. Instead only the symmetric session keys will
     *  be regenerated.
     */
    public void redoHandshake() throws SocketException {
        redoHandshake(false);
    }

    /**
     * Causes SSL to begin a full, new SSL 3.0 handshake from scratch
     * on a connection that has already completed one handshake.
     * @param flushCache If true, this session will be flushed from the cache.
     *  This will force a complete SSL handshake with a private key operation.
     *  If false, only the session key will be regenerated.
     */
    public native void redoHandshake(boolean flushCache) throws SocketException;

    protected void finalize() throws Throwable {
        close(); /* in case user did not call close */
    }

    public static class CipherPolicy {
        private int _enum;
        private CipherPolicy(int _enum) { }

        int getEnum() { return _enum; }

        public static final CipherPolicy DOMESTIC =
            new CipherPolicy(SocketBase.SSL_POLICY_DOMESTIC);
        public static final CipherPolicy EXPORT =
            new CipherPolicy(SocketBase.SSL_POLICY_EXPORT);
        public static final CipherPolicy FRANCE =
            new CipherPolicy(SocketBase.SSL_POLICY_FRANCE);

    }

    /**
     * Sets the SSL cipher policy. This must be called before creating any
     *  SSL sockets.
     */
    public static void setCipherPolicy(CipherPolicy cp) throws SocketException {
        setCipherPolicyNative(cp.getEnum());
    }

    private static native void setCipherPolicyNative(int policyEnum)
        throws SocketException;

    /**
     * Returns the addresses and ports of this socket
     * or an error message if the socket is not in a valid state.
     */
    public String toString() {

        try {
            InetAddress inetAddr  = getInetAddress();
            InetAddress localAddr = getLocalAddress();
            int port              = getPort();
            int localPort         = getLocalPort();
            StringBuffer buf      = new StringBuffer();
            buf.append("SSLSocket[addr=");
            buf.append(inetAddr);
            buf.append(",localaddr=");
            buf.append(localAddr);
            buf.append(",port=");
            buf.append(port);
            buf.append(",localport=");
            buf.append(localPort);
            buf.append("]");
            return buf.toString();
        } catch (Exception e) {
            return "Exception caught in toString(): " + e.getMessage();
        }
    }

    /**
     * Returns a list of cipher suites that are implemented by NSS.
     * Each element in the array will be one of the cipher suite constants
     * defined in this class (for example,
     * <tt>TLS_RSA_WITH_AES_128_CBC_SHA</tt>).
     */
    public static native int[] getImplementedCipherSuites();

    public final static int SSL2_RC4_128_WITH_MD5                  = 0xFF01;
    public final static int SSL2_RC4_128_EXPORT40_WITH_MD5         = 0xFF02;
    public final static int SSL2_RC2_128_CBC_WITH_MD5              = 0xFF03;
    public final static int SSL2_RC2_128_CBC_EXPORT40_WITH_MD5     = 0xFF04;
    public final static int SSL2_IDEA_128_CBC_WITH_MD5             = 0xFF05;
    public final static int SSL2_DES_64_CBC_WITH_MD5               = 0xFF06;
    public final static int SSL2_DES_192_EDE3_CBC_WITH_MD5         = 0xFF07;

    public final static int SSL3_RSA_WITH_NULL_MD5                 = 0x0001;
    public final static int SSL3_RSA_WITH_NULL_SHA                 = 0x0002;
    public final static int SSL3_RSA_EXPORT_WITH_RC4_40_MD5        = 0x0003;
    public final static int SSL3_RSA_WITH_RC4_128_MD5              = 0x0004;
    public final static int SSL3_RSA_WITH_RC4_128_SHA              = 0x0005;
    public final static int SSL3_RSA_EXPORT_WITH_RC2_CBC_40_MD5    = 0x0006;
    public final static int SSL3_RSA_WITH_IDEA_CBC_SHA             = 0x0007;
    public final static int SSL3_RSA_EXPORT_WITH_DES40_CBC_SHA     = 0x0008;
    public final static int SSL3_RSA_WITH_DES_CBC_SHA              = 0x0009;
    public final static int SSL3_RSA_WITH_3DES_EDE_CBC_SHA         = 0x000a;

    public final static int SSL3_DH_DSS_EXPORT_WITH_DES40_CBC_SHA  = 0x000b;
    public final static int SSL3_DH_DSS_WITH_DES_CBC_SHA           = 0x000c;
    public final static int SSL3_DH_DSS_WITH_3DES_EDE_CBC_SHA      = 0x000d;
    public final static int SSL3_DH_RSA_EXPORT_WITH_DES40_CBC_SHA  = 0x000e;
    public final static int SSL3_DH_RSA_WITH_DES_CBC_SHA           = 0x000f;
    public final static int SSL3_DH_RSA_WITH_3DES_EDE_CBC_SHA      = 0x0010;

    public final static int SSL3_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA = 0x0011;
    public final static int SSL3_DHE_DSS_WITH_DES_CBC_SHA          = 0x0012;
    public final static int SSL3_DHE_DSS_WITH_3DES_EDE_CBC_SHA     = 0x0013;
    public final static int SSL3_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA = 0x0014;
    public final static int SSL3_DHE_RSA_WITH_DES_CBC_SHA          = 0x0015;
    public final static int SSL3_DHE_RSA_WITH_3DES_EDE_CBC_SHA     = 0x0016;

    public final static int SSL3_DH_ANON_EXPORT_WITH_RC4_40_MD5    = 0x0017;
    public final static int SSL3_DH_ANON_WITH_RC4_128_MD5          = 0x0018;
    public final static int SSL3_DH_ANON_EXPORT_WITH_DES40_CBC_SHA = 0x0019;
    public final static int SSL3_DH_ANON_WITH_DES_CBC_SHA          = 0x001a;
    public final static int SSL3_DH_ANON_WITH_3DES_EDE_CBC_SHA     = 0x001b;

    /**
     * @deprecated As of NSS 3.11, FORTEZZA is no longer supported.
     * SSL3_FORTEZZA_DMS_WITH_NULL_SHA, SSL3_FORTEZZA_DMS_WITH_RC4_128_SHA
     * and SSL3_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA are placeholders for
     * backward compatibility.
     */
    public final static int SSL3_FORTEZZA_DMS_WITH_NULL_SHA        = 0x001c;
    public final static int SSL3_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA= 0x001d;
    public final static int SSL3_FORTEZZA_DMS_WITH_RC4_128_SHA     = 0x001e;

    public final static int SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA     = 0xfeff;
    public final static int SSL_RSA_FIPS_WITH_DES_CBC_SHA          = 0xfefe;

    public final static int TLS_RSA_EXPORT1024_WITH_DES_CBC_SHA    = 0x0062;
    public final static int TLS_RSA_EXPORT1024_WITH_RC4_56_SHA     = 0x0064;
 
    public final static int TLS_DHE_DSS_EXPORT1024_WITH_DES_CBC_SHA = 0x0063;
    public final static int TLS_DHE_DSS_EXPORT1024_WITH_RC4_56_SHA  = 0x0065;
    public final static int TLS_DHE_DSS_WITH_RC4_128_SHA            = 0x0066;

// New TLS cipher suites in NSS 3.4 
    public final static int TLS_RSA_WITH_AES_128_CBC_SHA          =  0x002F;
    public final static int TLS_DH_DSS_WITH_AES_128_CBC_SHA       =  0x0030;
    public final static int TLS_DH_RSA_WITH_AES_128_CBC_SHA       =  0x0031;
    public final static int TLS_DHE_DSS_WITH_AES_128_CBC_SHA      =  0x0032;
    public final static int TLS_DHE_RSA_WITH_AES_128_CBC_SHA      =  0x0033;
    public final static int TLS_DH_ANON_WITH_AES_128_CBC_SHA      =  0x0034;

    public final static int TLS_RSA_WITH_AES_256_CBC_SHA          =  0x0035;
    public final static int TLS_DH_DSS_WITH_AES_256_CBC_SHA       =  0x0036;
    public final static int TLS_DH_RSA_WITH_AES_256_CBC_SHA       =  0x0037;
    public final static int TLS_DHE_DSS_WITH_AES_256_CBC_SHA      =  0x0038;
    public final static int TLS_DHE_RSA_WITH_AES_256_CBC_SHA      =  0x0039;
    public final static int TLS_DH_ANON_WITH_AES_256_CBC_SHA      =  0x003A;

}

