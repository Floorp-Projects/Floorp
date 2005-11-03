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

import org.mozilla.jss.ssl.*;
import org.mozilla.jss.CryptoManager;
import org.mozilla.jss.crypto.*;
import org.mozilla.jss.asn1.*;
import org.mozilla.jss.pkix.primitive.*;
import org.mozilla.jss.pkix.cert.*;
import org.mozilla.jss.pkix.cert.Certificate;
import org.mozilla.jss.util.PasswordCallback;

import java.util.Calendar;
import java.util.Date;
import java.security.*;
import java.security.PrivateKey;
import java.net.*;
import java.io.*;

public class JSS_SSLClient {
    
    private String  clientCertNick      = null;
    private String  serverHost          = null;
    private boolean TestCertCallBack    = false;
    private boolean testBypass          = false;
    private boolean success             = true;
    private int     fCipher             = -1;
    private int     port                = 29753;
    private String  EOF                 = "test";
    private boolean handshakeCompleted  = false;
    
    private CryptoManager    cm          = null;
    private CryptoToken      tok         = null;
    private PasswordCallback cb          = null;
    private String  fPasswordFile        = "passwords";
    private static String  fCertDbPath   = ".";
    
    /**
     * Default Constructor, do not use.
     */
    public JSS_SSLClient() {
        try {
            CryptoManager.initialize(fCertDbPath);
            cm  = CryptoManager.getInstance();
            tok = cm.getInternalKeyStorageToken();
            cb  = new FilePasswordCallback(fPasswordFile);
            tok.login(cb);
        } catch (Exception e) {
        }
    }
    
    /**
     * Initialize the desired cipher to be set
     * on the socket.
     * @param int Cipher
     */
    public void setCipher(int aCipher) {
        fCipher = aCipher;
    }
    
    /**
     * Initialize the hostname to run the server
     * @param String ServerName
     */
    public void setHostName(String aHostName) {
        serverHost = aHostName;
    }
    
    /**
     * Initialize the port to run the server
     * @param int port
     */
    public void setPort(int aPort) {
        port = aPort;
    }
    
    /**
     * Initialize the passwords file name
     * @param String passwords
     */
    public void setPasswordFile(String aPasswordFile) {
        fPasswordFile = aPasswordFile;
    }
    
    /**
     * Initialize the cert db path name
     * @param String CertDbPath
     */
    public static void setCertDbPath(String aCertDbPath) {
        fCertDbPath = aCertDbPath;
    }
    
    /**
     * Fetch the cert db path name
     * @return String CertDbPath
     */
    public static String getCertDbPath() {
        return fCertDbPath;
    }
    
    /**
     * Enable/disable Test Cert Callback.
     * @param boolean
     */
    public void setBypass(boolean bypass) {
        testBypass = bypass;
    }

    /**
     * Enable/disable Test Cert Callback.
     * @param boolean
     */
    public void setTestCertCallback(boolean aTestCertCallback) {
        TestCertCallBack = aTestCertCallback;
    }
    
    /**
     * Set client certificate
     * @param String Certificate Nick Name
     */
    public void setClientCertNick(String aClientCertNick) {
        clientCertNick = aClientCertNick;
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
     * Initialize and create a socket connection to
     * SSLServer using the set parameters.
     */
    public void doIt() throws Exception {
        
        // connect to the server
        if ( Constants.debug_level >= 3 )
            System.out.println("client about to connect...");
        
        String hostAddr =
                InetAddress.getByName(serverHost).getHostAddress();
        
        if ( Constants.debug_level >= 3 )
            System.out.println("the host " + serverHost +
                    " and the address " + hostAddr);
        
        SSLCertificateApprovalCallback approvalCallback =
                new TestCertApprovalCallback();
        SSLClientCertificateSelectionCallback certSelectionCallback =
                new TestClientCertificateSelectionCallback();
        
        SSLSocket sock = null;
        
        if (TestCertCallBack) {
            if ( Constants.debug_level >= 3 )
                System.out.println("calling approvalCallBack");
            sock = new SSLSocket(InetAddress.getByName(hostAddr),
                    port,
                    null,
                    0,
                    new TestCertApprovalCallback(),
                    null);
        } else {
            if ( Constants.debug_level >= 3 )
                System.out.println("NOT calling approvalCallBack");
            sock = new SSLSocket(InetAddress.getByName(hostAddr),
                    port);
        }
        
        if (testBypass) {
            //enable bypass for this socket. 
            sock.bypassPKCS11(true); 
        }
        if ( Constants.debug_level >= 3 )
            System.out.println("clientCertNick=" + clientCertNick);
        sock.setClientCertNickname(clientCertNick);
        if ( fCipher != -1 ) {
            sock.setCipherPreference(fCipher, true);
        }
        if ( Constants.debug_level >= 3 ) {
            System.out.println("Client specified cert by nickname");
            System.out.println("client connected");
        }
        
        // Set socket timeout to 10 sec
        sock.setSoTimeout(10 * 1000);
        sock.addHandshakeCompletedListener(
                new HandshakeListener("client",this));
        
        // force the handshake
        sock.forceHandshake();
        if ( Constants.debug_level >= 3 )
            System.out.println("client forced handshake");
        
        PrintWriter out = new PrintWriter(
                new BufferedWriter(
                new OutputStreamWriter(sock.getOutputStream())));
        out.println(EOF);
        out.flush();
        
    /*
     * Make sure there were no surprises
     */
        if (out.checkError())
            System.out.println("SSLSocketClient: java.io.PrintWriter error");
        sock.close();
    }
    
    /**
     * SSL Handshake Listeren implementation.
     */
    public class HandshakeListener
            implements SSLHandshakeCompletedListener {
        private String who;
        private JSS_SSLClient boss;
        public HandshakeListener(String who, JSS_SSLClient boss) {
            this.who = who;
            this.boss = boss;
        }
        public void handshakeCompleted(SSLHandshakeCompletedEvent event) {
            try {
                String mesg = who + " got a completed handshake ";
                SSLSecurityStatus status = event.getStatus();
                if( status.isSecurityOn() ) {
                    mesg += "(security is ON)";
                } else {
                    mesg += "(security is OFF)";
                }
                if ( Constants.debug_level >= 3 )
                    System.out.println(mesg);
            } catch(Exception e) {
                e.printStackTrace();
                boss.setFailure();
            }
            setHandshakeCompleted();
        }
    }
    
    /**
     * Set status return value to false.
     */
    public synchronized void setFailure() {
        success = false;
    }
    
    /**
     * Set status return value to success.
     */
    public synchronized boolean getSuccess() {
        return success;
    }
    
    /**
     * Main method. Used for unit testing.
     */
    public static void main(String[] args) {
        
        String  certnick   = "JSSCATestCert";
        int     testCipher = 0;
        String  testhost   = "localhost";
        int     testport   = 29753;
        String  certDbPath = null;
        String  passwdFile = "passwords";
        boolean bypass = false;
        
        String  usage      = "USAGE:\n" +
                "java org.mozilla.jss.tests.JSS_SSLClient" +
                " <cert db path> <password file>\n" +
                " [server port] [bypass] [test cipher] [server host] ";
        
        try {
            if ( ((String)args[0]).toLowerCase().equals("-h") || 
                    args.length < 2) {
                System.out.println(usage);
                System.exit(1);
            }
            
            if ( args.length >= 2 ) {
                certDbPath = (String)args[0];
                passwdFile = (String)args[1];
            }
            
            if ( certDbPath != null)
                setCertDbPath(certDbPath);
            
            if ( args.length >= 3) {
                testport   = new Integer(args[2]).intValue();
                System.out.println("using port: " + testport);
            }
            
            if ((args.length >= 4) && 
                args[3].equalsIgnoreCase("bypass")== true) {
                bypass = true;
            }

            if ( args.length >= 5 ) {
                testCipher = new Integer(args[4]).intValue();
                System.out.println("testCipher " + testCipher);
            }
            if ( args.length == 6 ) {
                testhost   = (String)args[5];
                System.out.println("testhost" + testhost);
            }
            
            Thread.sleep(5000);
        } catch (Exception e) {
            System.out.println("Exception caught " + e.toString());
            e.printStackTrace();
        }
        
        JSS_SSLClient jssTest = new JSS_SSLClient();
        try {
            if ( !testhost.equals("localhost") )
                jssTest.setHostName(testhost);
            
            if ( testport != 29753 )
                jssTest.setPort(testport);
            
            jssTest.setBypass(bypass);
            jssTest.setTestCertCallback(true);
            jssTest.setClientCertNick(certnick);
            
            if ( passwdFile != null )
                jssTest.setPasswordFile(passwdFile);
            
            if ( testCipher != 0 ) {
                try {
                    jssTest.setCipher(testCipher);
                    jssTest.setEOF(new Integer(testCipher).toString());
                    jssTest.doIt();
                    while (!jssTest.isHandshakeCompleted()) {
                        // Put the main thread to sleep.  In case we do not
                        // get any response within 10 sec, then we shutdown.
                        try {
                            Thread.currentThread().sleep(1000);
                        } catch (InterruptedException e) {
                            System.out.println("Thread Interrupted ...\n");
                        }
                    }
                    jssTest.clearHandshakeCompleted();
                } catch (Exception ex) {
                    System.out.println("Exception caught " + ex.getMessage());
                    ex.printStackTrace();
                }
                // Set EOF to null to trigger server socket close
                jssTest.setCipher(testCipher);
                jssTest.setEOF("null");
                jssTest.doIt();
                while (!jssTest.isHandshakeCompleted()) {
                    // Put the main thread to sleep.  In case we do not
                    // get any response within 10 sec, then we shutdown.
                    try {
                        Thread.currentThread().sleep(1000);
                    } catch (InterruptedException e) {
                        System.out.println("Thread Interrupted ...\n");
                    }
                }
                jssTest.clearHandshakeCompleted();
            } else {
                for ( int i=0; i<Constants.jssCipherSuites.length; i++ ) {
                    try {
                        jssTest.setCipher(Constants.jssCipherSuites[i]);
                        jssTest.setEOF(new Integer(
                                Constants.jssCipherSuites[i]).toString());
                        jssTest.doIt();
                        while (!jssTest.isHandshakeCompleted()) {
                            // Put the main thread to sleep.  In case we do not
                            // get any response within 10 sec, then we shutdown.
                            try {
                                Thread.currentThread().sleep(1000);
                            } catch (InterruptedException e) {
                                System.out.println("Thread Interrupted ...\n");
                            }
                        }
                        jssTest.clearHandshakeCompleted();
                    } catch (Exception ex) {
                    }
                }
                // Set EOF to null to trigger server socket close
                jssTest.setCipher(1);
                jssTest.setEOF("null");
                jssTest.doIt();
                while (!jssTest.isHandshakeCompleted()) {
                    //Do nothing
                }
                jssTest.clearHandshakeCompleted();
            }
        } catch (Exception ex) {
            System.out.println(ex.getMessage());
            ex.printStackTrace();
        }
    }
}
