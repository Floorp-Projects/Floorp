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
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
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
package org.mozilla.jss.crypto;

import java.io.*;
import org.mozilla.jss.CryptoManager;
import org.mozilla.jss.util.Password;
import org.mozilla.jss.util.Debug;
import java.security.MessageDigest;

public class DigestTest {

    public static void main(String []argv) {

      try {

        Debug.setLevel(Debug.OBNOXIOUS);

        if( argv.length != 2 ) {
            System.out.println("Usage: DigestTest <dbdir> <infile>");
            System.exit(0);
        }
        String dbdir = argv[0];
        FileInputStream fis = new FileInputStream(argv[1]);
        byte[] toBeDigested = new byte[ fis.available() ];
        int read = fis.read( toBeDigested );
        System.out.println(read + " bytes to be digested");
        byte[] nsdigestOut;
        byte[] sundigestOut;

        CryptoManager.initialize(dbdir+"/secmod.db", dbdir+"/key3.db",
            dbdir+"/cert7.db");

        /////////////////////////////////////////////////////////////
        // Install SUN provider
        java.security.Security.addProvider(new sun.security.provider.Sun() );

        /////////////////////////////////////////////////////////////
        // Test SHA-1
        /////////////////////////////////////////////////////////////
        java.security.MessageDigest nsdigest =
                java.security.MessageDigest.getInstance("SHA-1", "Netscape");
        java.security.MessageDigest sundigest =
                java.security.MessageDigest.getInstance("SHA-1", "SUN");

        nsdigestOut = nsdigest.digest(toBeDigested);
        sundigestOut = sundigest.digest(toBeDigested);

        if( MessageDigest.isEqual(nsdigestOut, sundigestOut) ) {
            System.out.println("Sun and Netscape give same SHA-1 hash");
        } else {
            System.out.println("ERROR: Sun and Netscape give different"+
                " SHA-1 hashes");
        }

        /////////////////////////////////////////////////////////////
        // Test MD5
        /////////////////////////////////////////////////////////////
        nsdigest = java.security.MessageDigest.getInstance("MD5", "Netscape");
        sundigest = java.security.MessageDigest.getInstance("MD5", "SUN");

        nsdigestOut = nsdigest.digest(toBeDigested);
        sundigestOut = sundigest.digest(toBeDigested);

        if( MessageDigest.isEqual(nsdigestOut, sundigestOut) ) {
            System.out.println("Sun and Netscape give same MD5 hash");
        } else {
            System.out.println("ERROR: Sun and Netscape give different"+
                " MD5 hashes");
        }

        /////////////////////////////////////////////////////////////
        // Test SHA-1 HMAC
        /////////////////////////////////////////////////////////////
        CryptoManager cm = CryptoManager.getInstance();
        CryptoToken token = cm.getInternalCryptoToken();
        byte[] salt = { 0, 1, 2, 3,4 ,5 ,6 ,7 };
        PBEKeyGenParams pbe = new PBEKeyGenParams(
                    new Password("password".toCharArray()),
                    salt,
                    1 );
        KeyGenerator kg = token.getKeyGenerator(
                                PBEAlgorithm.PBE_SHA1_DES3_CBC );
        kg.initialize(pbe);
        SymmetricKey symkey = kg.generate();
        org.mozilla.jss.crypto.JSSMessageDigest digest =
                token.getDigestContext( HMACAlgorithm.SHA1 );
        digest.initHMAC(symkey);
        digest.update(toBeDigested);
        byte[] digestOut = digest.digest();
        if( digestOut.length == digest.getOutputSize() ) {
            System.out.println("digest output size is " + digestOut.length);
        } else {
            System.out.println("ERROR: digest output size is "+
                digestOut.length + ", should be "+digest.getOutputSize() );
        }


      } catch( Exception e ) {
            e.printStackTrace();
      }
    }
}
