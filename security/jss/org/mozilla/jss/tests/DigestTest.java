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
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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
import org.mozilla.jss.CryptoManager;
import org.mozilla.jss.util.Password;
import org.mozilla.jss.util.Debug;
import org.mozilla.jss.crypto.CryptoToken;
import java.security.MessageDigest;

public class DigestTest {
    public static boolean messageDigest(String alg, byte[] toBeDigested)
    throws Exception {
        byte[] nsdigestOut;
        byte[] sundigestOut;
        
        java.security.MessageDigest nsdigest =
                java.security.MessageDigest.getInstance(alg, "Mozilla-JSS");
        java.security.MessageDigest sundigest =
                java.security.MessageDigest.getInstance(alg, "SUN");

        nsdigestOut = nsdigest.digest(toBeDigested);
        sundigestOut = sundigest.digest(toBeDigested);

        if( MessageDigest.isEqual(nsdigestOut, sundigestOut) ) {
            System.out.println("Sun and Mozilla give same " + alg + " hash");
        } else {
            throw new Exception("ERROR: Sun and Mozilla give different "+
                alg + " hashes");
        }        
        return true;
    }
    
    public static boolean testJSSDigest(String alg, byte[] toBeDigested)
    throws Exception {
        byte[] nsdigestOut;
        byte[] sundigestOut;
        
        java.security.MessageDigest nsdigest =
                java.security.MessageDigest.getInstance(alg, "Mozilla-JSS");

        nsdigestOut = nsdigest.digest(toBeDigested);

        System.out.println("Provider " + nsdigest.getProvider());
        System.out.println("algorithm " + nsdigest.getAlgorithm());
        System.out.println("length of digest " + nsdigest.getDigestLength());
        
        if( nsdigestOut.length == nsdigest.getDigestLength() ) {
            System.out.println("digest output size is " + nsdigestOut.length);
        } else {
            throw new Exception("ERROR: digest output size is "+
            nsdigestOut.length + ", should be "+nsdigest.getDigestLength() );
        }
        
        return true;
    }
    
    

    public static void main(String []argv) {

      try {

        if( argv.length != 2 ) {
	    System.out.println(
		"Usage: java org.mozilla.jss.tests.DigestTest " + 
		"<dbdir> <File>");
            System.exit(1);
        }
        String dbdir = argv[0];
        FileInputStream fis = new FileInputStream(argv[1]);
        byte[] toBeDigested = new byte[ fis.available() ];
        int read = fis.read( toBeDigested );
        System.out.println(read + " bytes to be digested");

        CryptoManager.initialize(dbdir);

        Debug.setLevel(Debug.OBNOXIOUS);

        /////////////////////////////////////////////////////////////
        // Install SUN provider
        java.security.Security.addProvider(new sun.security.provider.Sun() );

        /////////////////////////////////////////////////////////////
        // Test all available algorithms
        /////////////////////////////////////////////////////////////
        String javaVersion = System.getProperty("java.version");
        System.out.println("the java version is: " + javaVersion);
        messageDigest("SHA1", toBeDigested);
        if  ( javaVersion.indexOf("1.4") == -1) {
            // JDK 1.5 or greater 
            messageDigest("MD2", toBeDigested);
        }  else {
            System.out.println("JDK 1.4  does not implement MD2");
            testJSSDigest("MD2", toBeDigested);
        }
        messageDigest("MD5", toBeDigested);
        messageDigest("SHA-256", toBeDigested);
        messageDigest("SHA-384", toBeDigested);
        messageDigest("SHA-512", toBeDigested);
            
         //HMAC examples in org.mozilla.jss.tests.HMACTest
      
      } catch( Exception e ) {
            e.printStackTrace();
            System.exit(1);
      }
      System.exit(0);
    }
}
