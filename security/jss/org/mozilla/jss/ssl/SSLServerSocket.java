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

import java.net.InetAddress;
import java.io.IOException;
import java.net.Socket;
import java.net.SocketException;

public class SSLServerSocket {

    private static final int DEFAULT_BACKLOG = 50;

    public SSLServerSocket(int port) throws IOException {
        this(port, DEFAULT_BACKLOG, null);
    }

    public SSLServerSocket(int port, int backlog) throws IOException {
        this(port, backlog, null);
    }

    public SSLServerSocket(int port, int backlog, InetAddress bindAddr)
        throws IOException 
    {
        // Dance the dance of fools.  The superclass doesn't have a default
        // constructor, so we have to trick it here. This is an example
        // of WHY WE SHOULDN'T BE EXTENDING SERVERSOCKET.
        //super(0);
        //super.close();

        // create the socket
        sockProxy = new SocketProxy( socketCreate() );

        // bind it to the local address and port
        if( bindAddr == null ) {
            bindAddr = anyLocalAddr;
        }
        socketBind(bindAddr.getAddress(), port);
        socketListen(backlog);
    }

    private SocketProxy sockProxy;
    private boolean handshakeAsClient=false;

    private native byte[] socketCreate() throws SocketException;

    private native void socketBind(byte[] addrBA, int port)
        throws SocketException;

    private native void socketListen(int backlog) throws SocketException;

    private static InetAddress anyLocalAddr;
    static {
        try {
            anyLocalAddr = InetAddress.getByName("0.0.0.0");
        } catch (java.net.UnknownHostException e) { }
    }

    public Socket accept() throws IOException {
        SSLSocket s = new SSLSocket();
        s.setSockProxy( new SocketProxy( socketAccept(s, timeout) ) );
        return s;
    }

    private int timeout;

    public void setSoTimeout(int timeout) {
        this.timeout = timeout;
    }

    public int getSoTimeout() {
        return timeout;
    }

    private native byte[] socketAccept(SSLSocket s, int timeout)
        throws SocketException;

    public static native void clearSessionCache();

    protected void finalize() throws Throwable {
        close();
    }

    public void close() throws IOException {
        if( sockProxy != null ) {
            socketClose();
            sockProxy = null;
        }
    }

    private native void socketClose() throws IOException;

    // This directory is used as the default for the Session ID cache
    private final static String UNIX_TEMP_DIR = "/tmp";
    private final static String WINDOWS_TEMP_DIR = "\\temp";

    public static native void configServerSessionIDCache(int maxSidEntries,
        int ssl2EntryTimeout, int ssl3EntryTimeout, String cacheFileDirectory);

    public native void setServerCertNickname(String nickname)
            throws SocketException;

    public native void setNeedClientAuth(boolean b) throws SocketException;

    public native void setNeedClientAuthNoExpiryCheck(boolean b)
        throws SocketException;
}
