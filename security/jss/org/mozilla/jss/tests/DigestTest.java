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
import java.security.MessageDigest;
import java.security.Provider;
import java.security.Security;

public class DigestTest {

    /**
     * This is the name of the JSS crypto provider for use with
     * MessageDigest.getInstance().
     */
    static final String MOZ_PROVIDER_NAME = "Mozilla-JSS";

    /**
     * List all the Digest Algorithms that JSS implenents.
     */
    static final String JSS_Digest_Algs[] = { "MD2", "MD5", "SHA-1",
                                            "SHA-256", "SHA-384","SHA-512"};

    public static boolean messageDigestCompare(String alg, byte[] toBeDigested)
    throws Exception {
        byte[] otherDigestOut;
        byte[] mozillaDigestOut;
        boolean bTested = false;

        // get the digest for the Mozilla-JSS provider
        java.security.MessageDigest mozillaDigest =
                java.security.MessageDigest.getInstance(alg,
                MOZ_PROVIDER_NAME);
        mozillaDigestOut = mozillaDigest.digest(toBeDigested);

        // loop through all the providers that support the algorithm
        // compare the result to Mozilla-JSS's digest
        Provider[] providers = Security.getProviders("MessageDigest." + alg);
        String provider = null;

        for (int i = 0; i < providers.length; ++i) {

            provider = providers[i].getName();
            if (provider.equals(MOZ_PROVIDER_NAME)) {
                continue;
            }

            java.security.MessageDigest otherDigest =
                    java.security.MessageDigest.getInstance(alg, provider);

            otherDigestOut =
                    otherDigest.digest(toBeDigested);

            if( MessageDigest.isEqual(mozillaDigestOut, otherDigestOut) ) {
                System.out.println(provider + " and " + MOZ_PROVIDER_NAME +
                                   " give same " + alg + " message digests");
                bTested = true;
            } else {
                throw new Exception("ERROR: " + provider + " and " +
                                    MOZ_PROVIDER_NAME + " give different " +
                                    alg + " message digests");
            }
        }

        return bTested;
    }

    public static boolean testJSSDigest(String alg, byte[] toBeDigested)
    throws Exception {
        byte[] mozillaDigestOut;
 
        java.security.MessageDigest mozillaDigest =
                java.security.MessageDigest.getInstance(alg, MOZ_PROVIDER_NAME);

        mozillaDigestOut = mozillaDigest.digest(toBeDigested);

        if( mozillaDigestOut.length == mozillaDigest.getDigestLength() ) {
            System.out.println(mozillaDigest.getAlgorithm() + " " +
                    " digest output size is " + mozillaDigestOut.length);
        } else {
            throw new Exception("ERROR: digest output size is "+
                    mozillaDigestOut.length + ", should be "+ 
                    mozillaDigest.getDigestLength() );
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
            // Test all available algorithms
            /////////////////////////////////////////////////////////////
            String javaVersion = System.getProperty("java.version");
            System.out.println("The Java version is: " + javaVersion);

            for (int i = 0; i < JSS_Digest_Algs.length; i++) {
                // compare Mozilla-JSS implemenation with all providers
                // that also support the given algorithm
                if (messageDigestCompare(JSS_Digest_Algs[i], toBeDigested) 
                    == false) {
                    // no provider to compare results with
                    testJSSDigest(JSS_Digest_Algs[i], toBeDigested);
                }
            }

            //HMAC examples in org.mozilla.jss.tests.HMACTest

        } catch( Exception e ) {
            e.printStackTrace();
            System.exit(1);
        }
        System.exit(0);
    }
}
