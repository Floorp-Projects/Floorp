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
import java.security.Security;
import java.security.MessageDigest;
import java.security.Provider;
import java.security.*;
import javax.crypto.*;
import javax.crypto.spec.*;
import org.mozilla.jss.crypto.SecretKeyFacade;

public class HMACTest {

    public static void doHMAC(SecretKeyFacade sk, String alg) 
    throws Exception {
        
        String clearText = new String("Hi There");

        //Get the Mozilla-JSS HMAC
        Mac macJSS = Mac.getInstance(alg, "Mozilla-JSS");
        macJSS.init(sk);
        macJSS.update(clearText.getBytes());
        byte[] resultJSS = macJSS.doFinal(clearText.getBytes());

        //Get the SunJCE HMAC
        Mac macSunJCE = Mac.getInstance(alg, "SunJCE");
        macSunJCE.init(sk);
        macSunJCE.update(clearText.getBytes());
        byte[] resultSunJCE = macSunJCE.doFinal(clearText.getBytes());
        
        //Check to see if HMACs are equal
        if ( java.util.Arrays.equals(resultJSS, resultSunJCE) ) {
            System.out.println("Sun and Mozilla give same " + alg);
        } else {
            throw new Exception("ERROR: Sun and Mozilla give different "+ alg );
        }        
    }

    public static void main(String []argv) {

        try {
            if ( argv.length != 2 ) {
                System.out.println(
                              "Usage: java org.mozilla.jss.tests.HMACTest " + 
                              "<dbdir> <passwordFile>");
                System.exit(1);
            }
            String dbdir = argv[0];
            FileInputStream fis = new FileInputStream(argv[1]);
            byte[] toBeDigested = new byte[ fis.available() ];
            int read = fis.read( toBeDigested );
            System.out.println(read + " bytes to be digested");
            CryptoManager.initialize(dbdir);

            Debug.setLevel(Debug.ERROR);
            Provider[] providers = Security.getProviders();
            for ( int i=0; i < providers.length; i++ ) {
                System.out.println("Provider "+i+": "+providers[i].getName());
            }

            //The secret key must be a JSS key. That is, it must be an 
            //instanceof org.mozilla.jss.crypto.SecretKeyFacade.
            
            //Generate the secret key using PKCS # 5 password Based Encryption
            //we have to specify a salt and an iteration count.  

            PBEKeySpec pbeKeySpec;
            SecretKeyFactory keyFac;
            SecretKeyFacade sk;
            byte[] salt = {
                (byte)0x0a, (byte)0x6d, (byte)0x07, (byte)0xba,
                (byte)0x1e, (byte)0xbd, (byte)0x72, (byte)0xf1
            };
            int iterationCount = 7;

            pbeKeySpec = new PBEKeySpec("password".toCharArray(), 
                                         salt, iterationCount);
            keyFac = SecretKeyFactory.getInstance("PBEWithSHA1AndDESede", 
                                                  "Mozilla-JSS");
            sk = (SecretKeyFacade) keyFac.generateSecret(pbeKeySpec);

            //caculate HMAC
            doHMAC(sk, "HmacSHA1");
            
            //need to do bug https://bugzilla.mozilla.org/show_bug.cgi?id=263544
            //to support 
            //doHMAC(sk, "HmacSHA256");
            //doHMAC(sk, "HmacSHA384");
            //doHMAC(sk, "HmacSHA512");
            //also we should add HmacMD2 and HmacMD5
            //doHMAC(sk, "HmacMD2"); 
            //doHMAC(sk, "HmacMD5");
            
        } catch ( Exception e ) {
            e.printStackTrace();
            System.exit(1);
        }
        System.exit(0);
    }
}

