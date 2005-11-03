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
 * The Original Code is Netscape Security Services for Java.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
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

package org.mozilla.jss.tests;

import java.net.*;
import java.io.*;
import javax.net.ssl.*;
import java.security.cert.*;
import javax.security.cert.X509Certificate;
import java.security.KeyStore;

/*
 * This program connects to any SSL Server to exercise
 * all ciphers supported by JSSE.  The result is listing
 * of common ciphers between the server and JSSE.
 */
public class JSSE_SSLClient {
    
    // Local members
    private String  sslRevision         = "SSLv3";
    private String  host                = null;
    private int     port                = -1;
    private String  cipherName          = null;
    private String  path                = null;
    private String  tunnelHost          = null;
    private int     tunnelPort          = 0;
    private int     debug_level         = 0;
    private boolean handshakeCompleted  = false;
    private String  EOF                 = "test";
    private String  keystoreLoc         = "keystore.pfx";
    
    /**
     * Set the protocol type and revision
     * @param String sslRevision
     */
    public void setSslRevision(String fSslRevision) {
        this.sslRevision = fSslRevision;
    }
    
    /**
     * Set the host name to connect to.
     * @param String hostname
     */
    public void setHost(String fHost) {
        this.host = fHost;
    }
    
    /**
     * Set the port number to connect to.
     * @param int portnumber
     */
    public void setPort(int fPort) {
        this.port = fPort;
    }
    
    /**
     * Set the cipher suite name to use.
     * @param String cipherSuiteName
     */
    public void setCipherSuite(String fCipherSuite) {
        this.cipherName = fCipherSuite;
    }
    
    /**
     * Set tunnel host name
     * @param String tunnelHostName
     */
    public void setTunnelHost(String fTunnelHost) {
        this.tunnelHost = fTunnelHost;
    }
    
    /**
     * Set tunnel port number
     * @param int tunnelPortNumber
     */
    public void setTunnelPort(int fTunnelPort) {
        this.tunnelPort = fTunnelPort;
    }
    
    /**
     * Return true if handshake is completed
     * else return false;
     * @return boolean handshake status
     */
    public boolean isHandshakeCompleted() {
        return this.handshakeCompleted;
    }
    
    /**
     * Set handshakeCompleted flag to indicate
     * that the socket handshake is coplete.
     */
    public void setHandshakeCompleted() {
        this.handshakeCompleted = true;
    }
    
    /**
     * Clear handshakeCompleted flag to indicate
     * that the system is now ready for another
     * socket connection.
     */
    public void clearHandshakeCompleted() {
        this.handshakeCompleted = false;
    }
    
    /**
     * Set EOF for closinng server socket
     * @param null for closing server socket
     */
    public void setEOF(String fEof) {
        this.EOF = fEof;
    }
    
    /**
     * Set the location of keystore.pfx
     * @param String fKeystoreLoc
     */
    public void setKeystoreLoc(String fKeystoreLoc) {
        keystoreLoc = fKeystoreLoc + "/" + keystoreLoc;
    }
    
    /**
     * Get the location of keystore.pfx
     * @return String fKeystoreLoc
     */
    public String getKeystoreLoc() {
        return keystoreLoc;
    }
    
    /**
     * Return true or false based on
     * tunnel parameters being set.
     * @return boolean true/false
     */
    public boolean isTunnel() {
        if ( this.tunnelHost != null &&
                this.tunnelPort != 0)
            return true;
        else
            return false;
    }
    
    /**
     * Default constructor.
     */
    public JSSE_SSLClient() {
        //Do nothing.
    }
    
    /**
     * Writer thread class that takes a
     * PrintWriter as input and sleeps
     * for 5 sec after sending some test
     * data.
     */
    private class writeThread extends Thread {
        
        private PrintWriter w;
        
        public writeThread(PrintWriter out) {
            w = out;
        }
        
        public void run() {
            try {
                while (true) {
                    w.println("Client saying hi ");
                    w.flush();
                    sleep(5);
                }
            } catch (Exception exception) {
                System.out.println("WriteThread interrupted: " +
                        exception.getMessage());
                System.exit(1);
            }
        }
    }
    
    /**
     * Reader thread class that takes a
     * BufferedReader as input and sleeps
     * for 5 sec after readinng test data.
     * This is to test the behaviour when
     * the inputStream is shutdown externally.
     */
    private class readThread extends Thread {
        
        private BufferedReader bir;
        
        public readThread(BufferedReader in) {
            bir = in;
        }
        
        public void run() {
            try {
                while (true) {
                    System.out.println("Client reading=======================");
                    String socketData  = bir.readLine();
                    System.out.println("Client Read==" + socketData);
                    sleep(5);
                }
            } catch (EOFException e) {
                System.out.println("ReadThread got EOF");
                e.printStackTrace();
                System.exit(1);
            } catch (IOException ex) {
                System.out.println("ReadThread IO exception caught : " +
                        ex.getMessage());
                ex.printStackTrace();
                System.exit(1);
            } catch (NullPointerException npe) {
                System.out.println("ReadThread Null pointer exception caught");
                npe.printStackTrace();
                System.exit(1);
            } catch (InterruptedException exception) {
                System.out.println("ReadThread interrupted");
                exception.printStackTrace();
                System.exit(1);
            } catch (Exception exception) {
                System.out.println("ReadThread interrupted: " +
                        exception.getMessage());
                exception.printStackTrace();
                System.exit(1);
            }
        }
    }
    
    /**
     * validate connection to the initialized host:port
     * using the preset cipherSuiteName.
     */
    public String validateConnection() {
        
        try {
            
        /*
         * Let's setup the SSLContext first, as there's a lot of
         * computations to be done.  If the socket were created
         * before the SSLContext, the server/proxy might timeout
         * waiting for the client to actually send something.
         */
            SSLSocketFactory    factory = null;
            SSLSocket           socket  = null;
            
            SSLContext          ctx     = null;
            KeyManagerFactory   kmf     = null;
            TrustManagerFactory tmf     = null;
            KeyStore            ks      = null;
            KeyStore            ksTrust = null;
            
        /*
         * Set up a key manager for client authentication
         * if asked by the server.  Use the implementation's
         * default TrustStore and secureRandom routines.
         */
            char[] passphrase      = "netscape".toCharArray();
            char[] trustpassphrase = "changeit".toCharArray();
            
            // Initialize the system
            System.setProperty("java.protocol.handler.pkgs",
                    "com.sun.net.ssl.internal.www.protocol");
            java.security.Security.addProvider(
                    new com.sun.net.ssl.internal.ssl.Provider());
            
            // Load the keystore that contains the certificate
            kmf     = KeyManagerFactory.getInstance("SunX509");
            ks      = KeyStore.getInstance("PKCS12");
            try {
                ks.load(new FileInputStream(getKeystoreLoc()), passphrase);
            } catch (Exception keyEx) {
                System.out.println(keyEx.getMessage());
                System.exit(1);
            }
            kmf.init(ks, passphrase);
            
            // trust manager that trusts all cetificates
            TrustManager[] trustAllCerts = new TrustManager[]{
                new X509TrustManager() {
                    public boolean checkClientTrusted(
                            java.security.cert.X509Certificate[] chain){
                        return true;
                    }
                    public boolean isServerTrusted(
                            java.security.cert.X509Certificate[] chain){
                        return true;
                    }
                    public boolean isClientTrusted(
                            java.security.cert.X509Certificate[] chain){
                        return true;
                    }
                    public java.security.cert.X509Certificate[]
                            getAcceptedIssuers() {
                        return null;
                    }
                    public void checkClientTrusted(
                            java.security.cert.X509Certificate[] chain,
                            String authType) {}
                    public void checkServerTrusted(
                            java.security.cert.X509Certificate[] chain,
                            String authType) {}
                }
            };
            
            ctx = SSLContext.getInstance(sslRevision);
            ctx.init(kmf.getKeyManagers(), trustAllCerts, null);
            factory = ctx.getSocketFactory();
            
            Socket tunnel = null;
            if ( isTunnel() ) {
            /*
             * Set up a socket to do tunneling through the proxy.
             * Start it off as a regular socket, then layer SSL
             * over the top of it.
             */
                tunnel = new Socket(tunnelHost, tunnelPort);
                doTunnelHandshake(tunnel, host, port);
                
            /*
             * Ok, let's overlay the tunnel socket with SSL.
             */
                socket =
                      (SSLSocket)factory.createSocket(tunnel, host, port, true);
            } else {
                socket = (SSLSocket)factory.createSocket(host, port);
            }
            
        /*
         * register a callback for handshaking completion event
         */
            try {
                socket.addHandshakeCompletedListener(
                        new HandshakeCompletedListener() {
                    public void handshakeCompleted(
                            HandshakeCompletedEvent event) {
                        if ( Constants.debug_level >= 3 ) {
                            System.out.println(
                                    "SessionId "+ event.getSession() +
                                    " Test Status : PASS");
                            System.out.flush();
                        }
                        setHandshakeCompleted();
                    }
                }
                );
            } catch (Exception handshakeEx) {
                return null;
            }
            
        /*
         * send http request
         *
         * See SSLSocketClient.java for more information about why
         * there is a forced handshake here when using PrintWriters.
         */
            String [] Ciphers = {cipherName};
            socket.setEnabledCipherSuites(Ciphers);
            // Set socket timeout to 10 sec
            socket.setSoTimeout(10 * 1000);
            socket.startHandshake();
            
            PrintWriter out = new PrintWriter(
                    new BufferedWriter(
                    new OutputStreamWriter(
                    socket.getOutputStream())));
            //writeThread wthread = new writeThread(out);
            //wthread.start();
            
            //out.println("GET " + path + " HTTP/1.0");
            out.println(EOF);
            out.flush();
            
        /*
         * Make sure there were no surprises
         */
            if (out.checkError())
                System.out.println("SSLSocketClient: " +
                        "java.io.PrintWriter error");
            
            /* read response */
            BufferedReader in  = new BufferedReader(
                    new InputStreamReader(
                    socket.getInputStream()));
            
            //readThread rthread = new readThread(in);
            //rthread.start();
            
            String inputLine;
            
            while ((inputLine  = in.readLine()) != null)
                System.out.println(inputLine);
            
            //System.out.println("Shutdown the input stream ...");
            //socket.shutdownInput();
            in.close();
            out.close();
            socket.close();
        } catch (Exception e) {
            setHandshakeCompleted();
            return e.getMessage();
        }
        
        return "success";
    }
    
    /**
     * Tell our tunnel where we want to CONNECT, and look for the
     * right reply.  Throw IOException if anything goes wrong.
     * @param Socket tunneling socket
     * @param String hostname
     * @param int    portnumber
     */
    private void doTunnelHandshake(Socket tunnel, String host, int port)
    throws IOException {
        OutputStream out = tunnel.getOutputStream();
        String msg = "CONNECT " + host + ":" + port + " HTTP/1.0\n"
                + "User-Agent: "
                + sun.net.www.protocol.http.HttpURLConnection.userAgent
                + "\r\n\r\n";
        byte b[];
        try {
        /*
         * We really do want ASCII7 -- the http protocol doesn't change
         * with locale.
         */
            b = msg.getBytes("ASCII7");
        } catch (UnsupportedEncodingException ignored) {
        /*
         * If ASCII7 isn't there, something serious is wrong, but
         * Paranoia Is Good (tm)
         */
            b = msg.getBytes();
        }
        out.write(b);
        out.flush();
        
    /*
     * We need to store the reply so we can create a detailed
     * error message to the user.
     */
        byte		reply[] = new byte[200];
        int		replyLen = 0;
        int		newlinesSeen = 0;
        boolean		headerDone = false;	/* Done on first newline */
        
        InputStream	in = tunnel.getInputStream();
        boolean		error = false;
        
        while (newlinesSeen < 2) {
            int i = in.read();
            if (i < 0) {
                throw new IOException("Unexpected EOF from proxy");
            }
            if (i == '\n') {
                headerDone = true;
                ++newlinesSeen;
            } else if (i != '\r') {
                newlinesSeen = 0;
                if (!headerDone && replyLen < reply.length) {
                    reply[replyLen++] = (byte) i;
                }
            }
        }
        
    /*
     * Converting the byte array to a string is slightly wasteful
     * in the case where the connection was successful, but it's
     * insignificant compared to the network overhead.
     */
        String replyStr;
        try {
            replyStr = new String(reply, 0, replyLen, "ASCII7");
        } catch (UnsupportedEncodingException ignored) {
            replyStr = new String(reply, 0, replyLen);
        }
        
        /* We asked for HTTP/1.0, so we should get that back */
        if (!replyStr.startsWith("HTTP/1.0 200")) {
            throw new IOException("Unable to tunnel through "
                    + tunnelHost + ":" + tunnelPort
                    + ".  Proxy returns \"" + replyStr + "\"");
        }
        
        /* tunneling Handshake was successful! */
    }
    
    /**
     * Test communication with SSL server using TLS
     */
    public void testTlsClient(String testCipher,
            String testHost,
            int testPort,
            String keystoreLocation) {
        
        String javaVersion = System.getProperty("java.version");
        String lastCipher  = null;
        System.out.println("\nUsing java version " + javaVersion + "\n");
        System.out.println("Testing TLS Cipher list ...");
        JSSE_SSLClient sslSock = new JSSE_SSLClient();
        sslSock.setSslRevision("TLS");
        sslSock.setHost(testHost);
        sslSock.setPort(testPort);
        sslSock.setKeystoreLoc(keystoreLocation);
        
        if ( javaVersion.indexOf("1.4") == -1 ) {
            // Validate Ciphers supported for TLS
            
            if ( testCipher != null ) {
                // This try is for catching non supported cipher exception
                try {
                    sslSock.setCipherSuite(testCipher);
                    sslSock.setEOF(testCipher);
                    String errStr = sslSock.validateConnection();
                    Thread.currentThread().sleep(1000);
                } catch (Exception ex) {
                    System.out.println("JSSE_SSLCLient: Did not find " +
                                       "any supported ciphers for JDK 1.4.x");
                }
            } else {
                // This try is for catching non supported cipher exception
                try {
                    for(int i=0;i<Constants.sslciphersarray_jdk150.length;i++){
                        sslSock.setCipherSuite(
                                Constants.sslciphersarray_jdk150[i]);
                        sslSock.setEOF(Constants.sslciphersarray_jdk150[i]);
                        String errStr = sslSock.validateConnection();
                        Thread.currentThread().sleep(1000);
                    }
                } catch (Exception ex) {
                    System.out.println("JSSE_SSLCLient: Did not find " +
                                       "any supported ciphers for JDK 1.5.x");
                }
            }
            System.out.println("Testing TLS Cipher list complete\n");
        }
    }
    
    /**
     * Test communication with SSL server using SSLv3
     */
    public void testSslClient(String testCipher,
                              String testHost,
                              int testPort,
                              String keystoreLocation) {
        String javaVersion = System.getProperty("java.version");
        String lastCipher  = null;
        // Validate Ciphers supported for SSLv3
        System.out.println("Testing SSLv3 Cipher list ...");
        JSSE_SSLClient sslSock = new JSSE_SSLClient();
        sslSock.setSslRevision("SSLv3");
        sslSock.setHost(testHost);
        sslSock.setPort(testPort);
        sslSock.setKeystoreLoc(keystoreLocation);
        
        if ( javaVersion.indexOf("1.4") != -1 ) {
            if ( testCipher != null ) {
                // This try is for catching non supported cipher exception
                try {
                    sslSock.setCipherSuite(testCipher);
                    sslSock.setEOF(testCipher);
                    String errStr = sslSock.validateConnection();
                    Thread.currentThread().sleep(1000);
                } catch (Exception ex) {
                    System.out.println("JSSE_SSLCLient: Did not find " +
                                       "any supported ciphers for JDK 1.4.x");
                }
            } else {
                // This try is for catching non supported cipher exception
                try {
                    for(int i=0;i<Constants.sslciphersarray_jdk142.length;i++){
                        lastCipher = Constants.sslciphersarray_jdk142[i];
                        sslSock.setCipherSuite(lastCipher);
                        sslSock.setEOF(Constants.sslciphersarray_jdk142[i]);
                        String errStr = sslSock.validateConnection();
                        Thread.currentThread().sleep(1000);
                    }
                } catch (Exception ex) {
                    System.out.println("JSSE_SSLCLient: Did not find " +
                                       "any supported ciphers for JDK 1.4.x");
                }
            }
            sslSock.setEOF("null");
            String errStr = sslSock.validateConnection();
        } else {
            if ( testCipher != null ) {
                // This try is for catching non supported cipher exception
                try {
                    sslSock.setCipherSuite(testCipher);
                    sslSock.setEOF(testCipher);
                    String errStr = sslSock.validateConnection();
                } catch (Exception ex) {
                    System.out.println("JSSE_SSLCLient: Did not find " +
                                       "any supported ciphers for JDK 1.5.x");
                }
            } else {
                // This try is for catching non supported cipher exception
                try {
                    for(int i=0;i<Constants.sslciphersarray_jdk150.length;i++){
                        lastCipher = Constants.sslciphersarray_jdk150[i];
                        sslSock.setCipherSuite(
                                Constants.sslciphersarray_jdk150[i]);
                        sslSock.setEOF(Constants.sslciphersarray_jdk150[i]);
                        String errStr = sslSock.validateConnection();
                    }
                } catch (Exception ex) {
                    System.out.println("JSSE_SSLCLient: Did not find " +
                                       "any supported ciphers for JDK 1.5.x");
                }
            }
            sslSock.setEOF("null");
            String errStr = sslSock.validateConnection();
        }
        System.out.println("Testing SSLv3 Cipher list complete\n");
    }
    
    /**
     * Main method for local unit testing.
     */
    public static void main(String [] args) {
        
        String testCipher       = null;
        String testHost         = "localhost";
        String keystoreLocation = "keystore.pfx";
        int    testPort         = 29750;
        String usage            = "java org.mozilla.jss.tests.JSSE_SSLClient" +
                "\n<keystore location> " +
                "<test port> <test cipher> <test host> ";
        
        try {
            if ( args[0].toLowerCase().equals("-h") || args.length < 1) {
                System.out.println(usage);
                System.exit(1);
            }
            
            if ( args.length >= 1 ) {
                keystoreLocation = (String)args[0];
            }
            if ( args.length >= 2) {
                testPort         = new Integer(args[1]).intValue();
                System.out.println("using port: " + testPort);
            }
            if ( args.length >= 3) {
                testCipher       = (String)args[2];
            }
            if ( args.length == 4) {
                testHost         = (String)args[3];
            }
        } catch (Exception e) { }
        
        JSSE_SSLClient sslSock = new JSSE_SSLClient();
        
        // Call TLS client cipher test
        try {
            Thread.currentThread().sleep(1000);
        } catch (Exception e) { }
        sslSock.testTlsClient(testCipher, testHost, testPort, keystoreLocation);
        
        // Call SSLv3 client cipher test
        try {
            Thread.currentThread().sleep(1000);
        } catch (Exception e) { }
        sslSock.testSslClient(testCipher, testHost, testPort, keystoreLocation);
        System.exit(0);
    }
}
