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
 * Copyright (C) 1998-2001 Netscape Communications Corporation.  All
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

public class SSLSocket extends java.net.Socket {

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

    public SSLSocket(String host, int port)
        throws UnknownHostException, IOException
    {
        this(InetAddress.getByName(host), port, null, 0);
    }

    public SSLSocket(InetAddress address, int port)
        throws IOException
    {
        this(address, port, null, 0);
    }

    public SSLSocket(String host, int port, InetAddress localAddr,
        int localPort) throws IOException
    {
        this(InetAddress.getByName(host), port, localAddr, localPort);
    }

    public SSLSocket(InetAddress address, int port, InetAddress localAddr,
        int localPort) throws IOException
    {
        this(address, port, localAddr, localPort, null, null);
    }

    public SSLSocket(String host, int port, InetAddress localAddr,
        int localPort, SSLCertificateApprovalCallback certApprovalCallback,
        SSLClientCertificateSelectionCallback clientCertSelectionCallback)
            throws IOException
    {
        this(InetAddress.getByName(host), port, localAddr, localPort,
                certApprovalCallback, clientCertSelectionCallback);
    }

    /*
     * @param stream Ignored.
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
        /* create the socket */
        sockProxy =
            new SocketProxy(
                base.socketCreate(
                    this, certApprovalCallback, clientCertSelectionCallback) );

        base.setProxy(sockProxy);

        /* bind it to local address and port */
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


    public InetAddress getInetAddress() {
        return base.getInetAddress();
    }

    public InetAddress getLocalAddress() {
        try {
            int intAddr = getLocalAddressNative();
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
    private native int getLocalAddressNative() throws SocketException;

    public int getLocalPort() {
        return base.getLocalPort();
    }

    public native int getPort();

    public InputStream getInputStream() throws IOException {
        return new SSLInputStream(this);
    }

    public OutputStream getOutputStream() throws IOException {
        return new SSLOutputStream(this);
    }

    public native void setTcpNoDelay(boolean on) throws SocketException;

    public native boolean getTcpNoDelay() throws SocketException;

    public native void setKeepAlive(boolean on) throws SocketException;

    public native boolean getKeepAlive() throws SocketException;

    public void shutdownInput() throws IOException {
        shutdownNative(SocketBase.PR_SHUTDOWN_RCV);
    }

    public void shutdownOutput() throws IOException {
        shutdownNative(SocketBase.PR_SHUTDOWN_SEND);
    }

    private native void shutdownNative(int how) throws IOException;

    /**
     * param linger The time (in hundredths of a second) to linger for.
     */
    public native void setSoLinger(boolean on, int linger)
        throws SocketException;

    public native int getSoLinger() throws SocketException;

    public void setSoTimeout(int timeout) throws SocketException {
        base.setTimeout(timeout);
    }

    public int getSoTimeout() throws SocketException {
        return base.getTimeout();
    }

    //
    // XXX These aren't implemented by native code. Perhaps they could be.
    //
    public native void setSendBufferSize(int size) throws SocketException;
    public native int getSendBufferSize() throws SocketException;
    public native void setReceiveBufferSize(int size) throws SocketException;
    public native int getReceiveBufferSize() throws SocketException;

    public void close() throws IOException {
        if( sockProxy != null ) {
            base.close();
            sockProxy = null;
        }
    }

    private native void socketConnect(byte[] addr, String hostname, int port)
        throws SocketException;

    ////////////////////////////////////////////////////////////////////
    // SSL-specific stuff
    ////////////////////////////////////////////////////////////////////
    private Vector handshakeCompletedListeners = new Vector();

    public void addHandshakeCompletedListener(SSLHandshakeCompletedListener l) {
        handshakeCompletedListeners.addElement(l);
    }

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
               

    public void enableSSL2(boolean enable) throws SocketException {
        base.enableSSL2(enable);
    }

    static public void enableSSL2Default(boolean enable) throws SocketException{
        setSSLDefaultOption(SocketBase.SSL_ENABLE_SSL2, enable);
    }

    public void enableSSL3(boolean enable) throws SocketException {
        base.enableSSL3(enable);
    }

    static public void enableSSL3Default(boolean enable) throws SocketException{
        setSSLDefaultOption(SocketBase.SSL_ENABLE_SSL3, enable);
    }

    public void requireClientAuth(boolean require, boolean onRedo)
            throws SocketException
    {
        base.requireClientAuth(require, onRedo);
    }

    public void requireClientAuthDefault(boolean require, boolean onRedo)
            throws SocketException
    {
        setSSLDefaultOption(SocketBase.SSL_REQUIRE_CERTIFICATE,
                            require ? (onRedo ? 1 : 2) : 0);
    }

    public native void forceHandshake() throws SocketException;

    public void setUseClientMode(boolean b) {
        handshakeAsClient = b;
    }

    public boolean getUseClientMode() {
        return handshakeAsClient;
    }

    public void resetHandshake() throws SocketException {
        resetHandshakeNative(handshakeAsClient);
    }

    private native void resetHandshakeNative(boolean asClient)
        throws SocketException;

    public native SSLSecurityStatus getStatus() throws SocketException;

    public void setClientCertNickname(String nick) throws SocketException {
        base.setClientCertNickname(nick);
    }

    public void setNeedClientAuth(boolean b) throws SocketException {
        base.setNeedClientAuth(b);
    }

    public native void setNeedClientAuthNoExpiryCheck(boolean b)
        throws SocketException;

    public void useCache(boolean b) throws SocketException {
        base.useCache(b);
    }

    public void useCacheDefault(boolean b) throws SocketException {
        setSSLDefaultOption(SocketBase.SSL_NO_CACHE, !b);
    }

    private InetAddress inetAddress;
    private int port;
    private SocketProxy sockProxy;
    private boolean open = false;
    private boolean handshakeAsClient=true;
    private SocketBase base = new SocketBase();

    private static void setSSLDefaultOption(int option, boolean on)
        throws SocketException
    {
        setSSLDefaultOption(option, on ? 1 : 0);
    }
    private static native void setSSLDefaultOption(int option, int on)
        throws SocketException;

    public static native void setCipherPreference( int cipher,
        boolean enable);

    native int socketAvailable()
        throws IOException;

    int read(byte[] b, int off, int len) throws IOException {
        return socketRead(b, off, len, base.getTimeout());
    }

    void write(byte[] b, int off, int len) throws IOException {
        socketWrite(b, off, len, base.getTimeout());
    }

    private native int socketRead(byte[] b, int off, int len, int timeout)
        throws IOException;

    private native void socketWrite(byte[] b, int off, int len, int timeout)
        throws IOException;

    public native void invalidateSession() throws SocketException;

    public void redoHandshake() throws SocketException {
        redoHandshake(false);
    }

    public native void redoHandshake(boolean flushCache) throws SocketException;

    protected void finalize() throws Throwable {
        close();
    }

    public static class CipherPolicy {
        private int enum;
        private CipherPolicy(int enum) { }

        int getEnum() { return enum; }

        public static final CipherPolicy DOMESTIC =
            new CipherPolicy(SocketBase.SSL_POLICY_DOMESTIC);
        public static final CipherPolicy EXPORT =
            new CipherPolicy(SocketBase.SSL_POLICY_EXPORT);
        public static final CipherPolicy FRANCE =
            new CipherPolicy(SocketBase.SSL_POLICY_FRANCE);

    }

    public static void setCipherPolicy(CipherPolicy cp) throws SocketException {
        setCipherPolicyNative(cp.getEnum());
    }

    private static native void setCipherPolicyNative(int policyEnum)
        throws SocketException;

    public final static int SSL2_RC4_128_WITH_MD5                  = 0xFF01;
    public final static int SSL2_RC4_128_EXPORT40_WITH_MD5         = 0xFF02;
    public final static int SSL2_RC2_128_CBC_WITH_MD5              = 0xFF03;
    public final static int SSL2_RC2_128_CBC_EXPORT40_WITH_MD5     = 0xFF04;
//  public final static int SSL2_IDEA_128_CBC_WITH_MD5             = 0xFF05;
    public final static int SSL2_DES_64_CBC_WITH_MD5               = 0xFF06;
    public final static int SSL2_DES_192_EDE3_CBC_WITH_MD5         = 0xFF07;

    public final static int SSL3_RSA_WITH_NULL_MD5                 = 0x0001;
//  public final static int SSL3_RSA_WITH_NULL_SHA                 = 0x0002;
    public final static int SSL3_RSA_EXPORT_WITH_RC4_40_MD5        = 0x0003;
    public final static int SSL3_RSA_WITH_RC4_128_MD5              = 0x0004;
    public final static int SSL3_RSA_WITH_RC4_128_SHA              = 0x0005;
    public final static int SSL3_RSA_EXPORT_WITH_RC2_CBC_40_MD5    = 0x0006;
//  public final static int SSL3_RSA_WITH_IDEA_CBC_SHA             = 0x0007;
//  public final static int SSL3_RSA_EXPORT_WITH_DES40_CBC_SHA     = 0x0008;
    public final static int SSL3_RSA_WITH_DES_CBC_SHA              = 0x0009;
    public final static int SSL3_RSA_WITH_3DES_EDE_CBC_SHA         = 0x000a;

//  public final static int SSL3_DH_DSS_EXPORT_WITH_DES40_CBC_SHA  = 0x000b;
//  public final static int SSL3_DH_DSS_WITH_DES_CBC_SHA           = 0x000c;
//  public final static int SSL3_DH_DSS_WITH_3DES_EDE_CBC_SHA      = 0x000d;
//  public final static int SSL3_DH_RSA_EXPORT_WITH_DES40_CBC_SHA  = 0x000e;
//  public final static int SSL3_DH_RSA_WITH_DES_CBC_SHA           = 0x000f;
//  public final static int SSL3_DH_RSA_WITH_3DES_EDE_CBC_SHA      = 0x0010;

//  public final static int SSL3_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA = 0x0011;
//  public final static int SSL3_DHE_DSS_WITH_DES_CBC_SHA          = 0x0012;
//  public final static int SSL3_DHE_DSS_WITH_3DES_EDE_CBC_SHA     = 0x0013;
//  public final static int SSL3_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA = 0x0014;
//  public final static int SSL3_DHE_RSA_WITH_DES_CBC_SHA          = 0x0015;
//  public final static int SSL3_DHE_RSA_WITH_3DES_EDE_CBC_SHA     = 0x0016;

//  public final static int SSL3_DH_ANON_EXPORT_WITH_RC4_40_MD5    = 0x0017;
//  public final static int SSL3_DH_ANON_WITH_RC4_128_MD5          = 0x0018;
//  public final static int SSL3_DH_ANON_EXPORT_WITH_DES40_CBC_SHA = 0x0019;
//  public final static int SSL3_DH_ANON_WITH_DES_CBC_SHA          = 0x001a;
//  public final static int SSL3_DH_ANON_WITH_3DES_EDE_CBC_SHA     = 0x001b;

    public final static int SSL3_FORTEZZA_DMS_WITH_NULL_SHA        = 0x001c;
    public final static int SSL3_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA= 0x001d;
    public final static int SSL3_FORTEZZA_DMS_WITH_RC4_128_SHA     = 0x001e;

    public final static int SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA     = 0xffe0;
    public final static int SSL_RSA_FIPS_WITH_DES_CBC_SHA          = 0xffe1;

    public final static int TLS_RSA_EXPORT1024_WITH_DES_CBC_SHA    = 0x0062;
    public final static int TLS_RSA_EXPORT1024_WITH_RC4_56_SHA     = 0x0064;
 
    public final static int TLS_DHE_DSS_EXPORT1024_WITH_DES_CBC_SHA = 0x0063;
    public final static int TLS_DHE_DSS_EXPORT1024_WITH_RC4_56_SHA  = 0x0065;
    public final static int TLS_DHE_DSS_WITH_RC4_128_SHA            = 0x0066;

}

