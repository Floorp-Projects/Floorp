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

import java.io.*;
import java.net.*;
import javax.net.*;
import javax.net.ssl.*;
import java.security.KeyStore;
import javax.security.cert.X509Certificate;
import java.util.Vector;

/**
 * JSSE SSLServer class that acts as SSL Server
 *
 * @author  Sandeep.Konchady@Sun.COM
 * @version 1.0
 */

public class JSSE_SSLServer {
    
    private static int DefaultServerPort   = 29753;
    private static int port                = DefaultServerPort;
    private static String type             = "SSLv3";
    private static String keystoreLoc      = "keystore.pfx";
    private static boolean bClientAuth     = false;
    private static Vector supportedCiphers = new Vector();

    /**
     * Constructs a JSSE_SSLServer.
     */
    public JSSE_SSLServer() throws IOException {
    }
    
    /**
     * Set the location of keystore file.
     * @param String fKeystoreLoc
     */
    public static void setKeystoreLoc(String fKeystoreLoc) {
        keystoreLoc = fKeystoreLoc + "/" + keystoreLoc;
    }
    
    /**
     * Get the location of keystore file.
     * @return String keystoreLoc
     */
    public static String getKeystoreLoc() {
        return keystoreLoc;
    }
    
    /**
     * Main method to create the class server. This takes
     * one command line arguments, the port on which the
     * server accepts requests.
     */
    public static void main(String args[]) {
        try {
            (new JSSE_SSLServer()).startSSLServer(args);
        } catch (Exception e) {}
    }

    /**
     * Start SSLServer and accept connections.
     * @param args[]
     */
    public void startSSLServer(String[] args) throws Exception {
        String keystoreLoc = "keystore.pfx";
        if ( args.length <= 1 ) {
            System.out.println(
                    "USAGE: java JSSE_SSLServer [port] [TLS | SSLv3 [true]]");
            System.out.println("[keystore location]");
            System.out.println(
                    "\nIf the second argument is TLS, it will start as a\n" +
                    "TLS server, otherwise, it will be started in SSLv3 mode." +
                    "\nIf the third argument is true,it will require\n" +
                    "client authentication as well.");
            System.exit(1);
        }
        
        if (args.length >= 1) {
            port = Integer.parseInt(args[0]);
        }
        if (args.length >= 2) {
            type = args[1];
        }
        if (args.length >= 3 && args[2].equals("true")) {
            bClientAuth = true;
        }
        if (args.length >= 4) {
            keystoreLoc = args[3];
            if ( keystoreLoc != null ) {
                setKeystoreLoc(keystoreLoc);
            }
        }

        System.out.println("using port: " + port);
        System.out.println ("mode type " + type + 
                           (bClientAuth ? "true" : "false"));
        System.out.println("keystoreLoc" + keystoreLoc);
        
        try {
            SSLServerSocketFactory ssf =
                    JSSE_SSLServer.getServerSocketFactory(type);
            if ( ssf != null ) {
                SSLServerSocket ss = 
                    (SSLServerSocket)ssf.createServerSocket(port);
                // Set server socket timeout to 90 sec
                ss.setSoTimeout(15 * 1000);
            
                // Based on J2SE version, enable appropriate ciphers
                if ((System.getProperty("java.version")).indexOf("1.4") != -1 ){
                    System.out.println("*** Using J2SE 1.4.x ***");
                    ss.setEnabledCipherSuites(Constants.sslciphersarray_jdk142);
                } else {
                    System.out.println("*** Using J2SE 1.5.x ***");
                    ss.setEnabledCipherSuites(Constants.sslciphersarray_jdk150);
                }
                ((SSLServerSocket)ss).setNeedClientAuth(bClientAuth);
                JSSE_SSLServer JSSEServ = new JSSE_SSLServer();
                // accept an SSL connection
                while (true) {
                    try {
                        Socket socket = ss.accept();
                        readWriteThread rwThread = new readWriteThread (socket);
                        rwThread.start ();
                    } catch (IOException ex) {
                        if (Constants.debug_level > 3)
                        System.out.println("Exception caught in " + 
                                           "SSLServerSocket.accept():" + 
                                           ex.getMessage());
                        try {
                            ss.close();
                        } catch (Exception e) {}
                        break;
                    }
                }
            } else {
                if(System.getProperty("java.vendor").equals("IBM Corporation")){
                    System.out.println("Using IBM JDK: Cannot load keystore " +
                        "due to strong security encryption settings\nwith " +
                        "limited Jurisdiction policy files :\n http://" +
                        "www-1.ibm.com/support/docview.wss?uid=swg21169931");
                    System.exit(0);
                }
            }
        } catch (Exception e) {
            System.out.println("Unable to start JSSE_SSLServer: " +
                    e.getMessage());
            e.printStackTrace();
            System.exit(1);
        }

        System.out.println("Server exiting");
        System.out.println("supportedCiphers.size      :" +
                           (supportedCiphers.size()-1));
        System.out.println("Constants.jssCiphersSuites :" +
                            Constants.jssCipherSuites.length);
        System.out.println("Constants.jssCiphersNames  :" +
                            Constants.jssCipherNames.length);
        System.out.println("-------------------------------------------" +
                           "-------------");
        System.out.println("Summary of JSS client to JSSE server " +
                           "communication test :");
        System.out.println("-------------------------------------------" +
                           "-------------");

        for ( int i=0; i<(supportedCiphers.size()-1); i++ ) {
            System.out.print("[" + (i+1) + "]\t");

            for ( int j=0; j<(Constants.jssCipherSuites.length-1); j++ ) {
               if (new Integer((String)supportedCiphers.elementAt(i)).intValue()
                   == j ) {
                    int k = Constants.jssCipherSuites[j];
                    System.out.print(" JSSC\t" +
                                     Constants.jssCipherNames[j] + "\n");
                    System.out.flush();
                }
            }
        }
        System.out.println("-------------------------------------------" +
                           "-------------");
        System.out.flush();

        // Exit gracefully
        System.exit(0);
    }

    /**
     * ReadWrite thread class that takes a
     * SSLSocket and socket counter as inputs
     * and reads from and writes to socket.
     */
    private class readWriteThread extends Thread {
        private Socket socket              = null;
        private boolean socketListenStatus = true;

        /**
         * Constructor.
         * @param Socket
         */
        public readWriteThread (Socket sock) {
            this.socket     = sock;
        }

        /**
         * Thread run method that reads from and writes to
         * the local socket until the client closes the
         * socket.
         */
        public void run () {

            try {
                String socketData  = null;
                String inputLine   = null;
                InputStream  is    = socket.getInputStream ();
                OutputStream os    = socket.getOutputStream ();
                BufferedReader bir = new BufferedReader (
                                     new InputStreamReader (is));
                PrintWriter out    = new PrintWriter (new BufferedWriter (
                                     new OutputStreamWriter (os)));

                while ( socketListenStatus ) {
                    try {
                        socketData  = bir.readLine();
                        if ( socketData.equals("null") ) {
                            socketListenStatus = false;
                            if ( Constants.debug_level > 3 )
                            System.out.println("Received " + socketData +
                                               " on socket");
                        } else if ( socketData != null &&
                                    !socketData.equals("skip") ) {
                            synchronized(supportedCiphers) {
                                supportedCiphers.add(socketData);
                            }
                            if ( Constants.debug_level > 3 )
                            System.out.println("Received " + socketData +
                                               " on socket");
                            socketListenStatus = false;
                        } else if ( socketData == null ) {
                            socketListenStatus = false;
                            if ( Constants.debug_level > 3 )
                            System.out.println("Received " + socketData +
                                               " on socket");
                        }
                        socket.close();
                    } catch(EOFException e) {
                        if ( Constants.debug_level > 3 )
                        System.out.println("EOFException caught in : " +
                            e.getMessage());
                        socketListenStatus = false;
                    } catch(IOException ex) {
                        if ( Constants.debug_level > 3 )
                        System.out.println("IOException caught in : " +
                            ex.getMessage());
                        socketListenStatus = false;
                    } catch(NullPointerException npe) {
                        if ( Constants.debug_level > 3 )
                        System.out.println("NPException caught in : " +
                            npe.getMessage());
                        socketListenStatus = false;
                        socketListenStatus = false;
                    } catch (Exception exp) {
                        if ( Constants.debug_level > 3 )
                        System.out.println("Exception caught in : " +
                            exp.getMessage());
                        socketListenStatus = false;
                    }
                }
            } catch (Exception e) {
                System.out.println ("Exception caught\n");
                e.printStackTrace ();
                System.exit (1);
            }
        }
    }

    static SSLServerSocketFactory getServerSocketFactory(String type) {
        
        // set up key manager to do server authentication
        SSLContext             ctx = null;
        KeyManagerFactory      kmf = null;
        KeyStore                ks = null;
        char[]          passphrase = "netscape".toCharArray();
        SSLServerSocketFactory ssf = null;
        
        System.setProperty("javax.net.ssl.trustStore",
            System.getProperty("java.home") + "/jre/lib/security/cacerts");
        String certificate = "SunX509";
        String javaVendor  = System.getProperty("java.vendor");
        if (javaVendor.equals("IBM Corporation"))
            certificate = "IbmX509";
        
        if (type.equals("TLS")) {
            try {
                ctx = SSLContext.getInstance("TLS");
                kmf = KeyManagerFactory.getInstance(certificate);
                ks = KeyStore.getInstance("PKCS12");
                
                ks.load(new FileInputStream(getKeystoreLoc()), passphrase);
                kmf.init(ks, passphrase);
                ctx.init(kmf.getKeyManagers(), null, null);
                
                ssf = ctx.getServerSocketFactory();
                return ssf;
            } catch (Exception e) {
                if (Constants.debug_level > 3)
                    e.printStackTrace();
            }
        } else if (type.equals("SSLv3")) {
            try {
                ctx = SSLContext.getInstance("SSLv3");
                kmf = KeyManagerFactory.getInstance(certificate);
                ks = KeyStore.getInstance("PKCS12");
                
                ks.load(new FileInputStream(getKeystoreLoc()), passphrase);
                kmf.init(ks, passphrase);
                ctx.init(kmf.getKeyManagers(), null, null);
                
                ssf = ctx.getServerSocketFactory();
                return ssf;
            } catch (Exception e) {
                if (Constants.debug_level > 3)
                    e.printStackTrace();
            }
        }
        return ssf;
    }
}
