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
import java.util.Vector;
import javax.net.*;

/*
 * ClassServer.java -- JSSE_SSLServer implements this
 * class.
 */
public abstract class ClassServer implements Runnable {
    
    private ServerSocket server             = null;
    private Vector       supportedCiphers   = new Vector();
    
    /**
     * Constructs a ClassServer based on <b>ss</b>
     */
    protected ClassServer(ServerSocket ss) {
        server = ss;
        newListener();
    }
    
    /**
     * The "listen" thread that accepts a connection to the
     * server, parses the header to obtain the file name
     * and sends back the bytes for the file (or error
     * if the file is not found or the response was malformed).
     */
    public void run() {
        Socket  socket             = null;
        boolean socketListenStatus = true;
        
        // accept a connection
        while ( socketListenStatus ) {
            try {
                socket = server.accept();
            } catch (Exception ex) {
                System.exit(1);
            }
            
            newListener();
            
            //try to read some bytes, to allow the handshake to go through
            try {
                InputStream is     = socket.getInputStream();
                BufferedReader bir = new BufferedReader(
                        new InputStreamReader(is));
                String socketData  = bir.readLine();
                if ( socketData.equals("null") )
                    socketListenStatus = false;
                else if ( socketData != null )
                    supportedCiphers.add(socketData);
                socket.close();
            } catch(EOFException e) {
            } catch(IOException ex) {
            } catch(NullPointerException npe) {
                socketListenStatus = false;
            }
        }
        
        try {
            server.close();
        } catch (Exception ex) {
            System.exit(1);
        }
        
        System.out.println("Server exiting");
        System.out.println("-------------------------------------------" +
                           "-------------");
        System.out.println("Summary of JSS client to JSSE server " +
                           "communication test :");
        System.out.println("-------------------------------------------" +
                           "-------------");
        System.out.println("supportedCiphers.size " + supportedCiphers.size());
        System.out.println("Constants.jssCiphersSuites "+  
                            Constants.jssCipherSuites.length);
        System.out.println("Constants.jssCiphersNames " + 
                            Constants.jssCipherNames.length);
        
        for ( int i=0; i<(supportedCiphers.size()-1); i++ ) {
            System.out.print(i + " SC " +
            new Integer((String)supportedCiphers.elementAt(i)).intValue()); 
            
            for ( int j=0; j<(Constants.jssCipherSuites.length-1); j++ ) {
               if (new Integer((String)supportedCiphers.elementAt(i)).intValue() 
                   == Constants.jssCipherSuites[j] ) {
                    System.out.print(" JSSC "  + Constants.jssCipherSuites[j] );
                    System.out.println(" ["+ i +"]\t" + 
                                       Constants.jssCipherNames[j]);
                    System.out.flush();
                }
            } 
        }
        System.out.println("-------------------------------------------" +
                           "-------------");
        System.out.flush();
        
        if( !socketListenStatus ) {
            System.exit(0);
        }
    }
    
    /**
     * Create a new thread to listen.
     */
    private void newListener() {
        (new Thread(this)).start();
    }
}
