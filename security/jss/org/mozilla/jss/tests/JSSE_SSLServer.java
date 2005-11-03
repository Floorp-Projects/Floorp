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
import java.security.KeyStore;
import javax.net.*;
import javax.net.ssl.*;
import javax.security.cert.X509Certificate;

public class JSSE_SSLServer extends ClassServer {
    
    private static int DefaultServerPort = 29753;
    private static int port              = DefaultServerPort;
    private static String type           = "SSLv3";
    private static String keystoreLoc    = "keystore.pfx";
    private static boolean bClientAuth   = false;
    /**
     * Constructs a JSSE_SSLServer.
     * @param path the path where the server locates files
     */
    public JSSE_SSLServer(ServerSocket ss)
    throws IOException {
        super(ss);
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
     * server accepts requests. To start up the server:
     * <br><br>
     * <code>   java JSSE_SSLServer <port>
     * </code><br><br>
     *
     * <code>   new JSSE_SSLServer(port);
     * </code>
     */
    public static void main(String args[]) {
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
            SSLServerSocket ss = (SSLServerSocket)ssf.createServerSocket(port);
            // Set server socket timeout to 15 sec
            ss.setSoTimeout(15 * 1000);
            
            // Based on J2SE version, enable appropriate ciphers
            if ( (System.getProperty("java.version")).indexOf("1.4") != -1 ) {
                System.out.println("*** Using J2SE 1.4.x ***");
                ss.setEnabledCipherSuites(Constants.sslciphersarray_jdk142);
            } else {
                System.out.println("*** Using J2SE 1.5.x ***");
                ss.setEnabledCipherSuites(Constants.sslciphersarray_jdk150);
            }
               ((SSLServerSocket)ss).setNeedClientAuth(bClientAuth);
            new JSSE_SSLServer(ss);
        } catch (IOException e) {
            System.out.println("Unable to start ClassServer: " +
                    e.getMessage());
            e.printStackTrace();
            System.exit(1);
        }
        
        // Put the main thread to sleep.  In case we do not get any
        // response within 5 sec, then we shutdown the server.
        try {
            Thread.currentThread().sleep(5000);
        } catch (InterruptedException e) {
            System.out.println("Thread Interrupted, exiting normally ...\n");
            System.exit(0);
        }
    }
    
    static SSLServerSocketFactory getServerSocketFactory(String type) {
        
        // set up key manager to do server authentication
        SSLContext             ctx = null;
        KeyManagerFactory      kmf = null;
        KeyStore                ks = null;
        char[]          passphrase = "netscape".toCharArray();
        SSLServerSocketFactory ssf = null;
        
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
        
        if (type.equals("TLS")) {
            try {
                ctx = SSLContext.getInstance("TLS");
                kmf = KeyManagerFactory.getInstance("SunX509");
                ks = KeyStore.getInstance("PKCS12");
                
                ks.load(new FileInputStream(getKeystoreLoc()), passphrase);
                kmf.init(ks, passphrase);
                ctx.init(kmf.getKeyManagers(), trustAllCerts, null);
                
                ssf = ctx.getServerSocketFactory();
                return ssf;
            } catch (Exception e) {
                e.printStackTrace();
                System.exit(1);
            }
        } else if (type.equals("SSLv3")) {
            try {
                ctx = SSLContext.getInstance("SSLv3");
                kmf = KeyManagerFactory.getInstance("SunX509");
                ks = KeyStore.getInstance("PKCS12");
                
                ks.load(new FileInputStream("./" + getKeystoreLoc()), passphrase);
                kmf.init(ks, passphrase);
                ctx.init(kmf.getKeyManagers(), trustAllCerts, null);
                
                ssf = ctx.getServerSocketFactory();
                return ssf;
            } catch (Exception e) {
                e.printStackTrace();
                System.exit(1);
            }
        }
        return null;
    }
}
