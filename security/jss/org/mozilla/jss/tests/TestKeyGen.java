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

/* This file demonstrates the use of JSS api to generate RSA and
 * DSA keys. The key pairs are stored in key3.db
 */

/**
 * Note: when this program is run, it must have a key3.db WITH A PASSWORD
 * SET in the directory specified by the argument. The first time the
 * program is run, a key3.db file will be created, but it will not have
 * a password. This will result in the error:
 *  Token error: org.mozilla.jss.crypto.TokenException: unable to login to token
 *
 * To create a database with a password, you can:
 *   use the modutil or keyutil tool,
 *   use the JSS API CryptoToken.changePassword() to set the password
 *   run the test 'TokenAccessTest' 
 *            which will create db with the password 'netscape'.
 */

package org.mozilla.jss.tests;

import org.mozilla.jss.pkcs11.*;

import org.mozilla.jss.util.*;
import org.mozilla.jss.crypto.*;
import org.mozilla.jss.*;
import java.io.*;
import java.awt.*;
import java.security.cert.*;
import java.security.interfaces.*;
import java.math.BigInteger;

public class TestKeyGen {

    public static void main(String[] args) {
      try {
        CryptoToken token;
        CryptoManager manager;
        KeyPairGenerator keyPairGenerator;
        java.security.KeyPair keyPair;
        Base64OutputStream base64;

        if(args.length != 1) {
            System.err.println(
                "Usage: java org.mozilla.jss.pkcs11.TestKeyGen <dbdir>");
            System.exit(1);
            return;
        }

        CryptoManager.InitializationValues vals = new
            CryptoManager.InitializationValues( args[0] );
        CryptoManager.initialize(vals);
        manager = CryptoManager.getInstance();
        manager.setPasswordCallback(
            new Password( "netscape".toCharArray() ));

        java.util.Enumeration tokens =
                manager.getTokensSupportingAlgorithm(KeyPairAlgorithm.RSA);
        System.out.println("The following tokens support RSA keygen:");
        while(tokens.hasMoreElements()) {
            System.out.println("\t"+
                ((CryptoToken)tokens.nextElement()).getName() );
        }
        tokens = manager.getTokensSupportingAlgorithm(KeyPairAlgorithm.DSA);
        System.out.println("The following tokens support DSA keygen:");
        while(tokens.hasMoreElements()) {
            System.out.println("\t"+
                ((CryptoToken)tokens.nextElement()).getName() );
        }

        RSAPublicKey rsaPubKey;
        DSAPublicKey dsaPubKey;
        DSAParams dsaParams;
        RSAParameterSpec rsaParams;

        token = manager.getInternalKeyStorageToken();
        keyPairGenerator = token.getKeyPairGenerator(KeyPairAlgorithm.RSA);

        // 512-bit RSA with default exponent
        keyPairGenerator.initialize(512);
        keyPair = keyPairGenerator.genKeyPair();
        Assert.assert( keyPair.getPublic() instanceof RSAPublicKey);
        rsaPubKey = (RSAPublicKey) keyPair.getPublic();
        System.out.println("Generated 512-bit RSA KeyPair!");
        System.out.println("Modulus: "+rsaPubKey.getModulus());
        System.out.println("Exponent: "+rsaPubKey.getPublicExponent());

        // 1024-bit RSA with default exponent
        keyPairGenerator.initialize(1024);
        keyPair = keyPairGenerator.genKeyPair();
        Assert.assert( keyPair.getPublic() instanceof RSAPublicKey);
        rsaPubKey = (RSAPublicKey) keyPair.getPublic();
        System.out.println("Generated 1024-bit RSA KeyPair!");
        System.out.println("Modulus: "+rsaPubKey.getModulus());
        System.out.println("Exponent: "+rsaPubKey.getPublicExponent());

        // 512-bit RSA with exponent = 3
        rsaParams = new RSAParameterSpec(512, BigInteger.valueOf(3));
        keyPairGenerator.initialize(rsaParams);
        keyPair = keyPairGenerator.genKeyPair();
        Assert.assert( keyPair.getPublic() instanceof RSAPublicKey);
        rsaPubKey = (RSAPublicKey) keyPair.getPublic();
        System.out.println("Generated 512-bit RSA KeyPair with public exponent=3!");
        System.out.println("Modulus: "+rsaPubKey.getModulus());
        System.out.println("Exponent: "+rsaPubKey.getPublicExponent());

        // 512-bit DSA
        keyPairGenerator = token.getKeyPairGenerator(KeyPairAlgorithm.DSA);
        keyPairGenerator.initialize(512);
        keyPair = keyPairGenerator.genKeyPair();
        Assert.assert( keyPair.getPublic() instanceof DSAPublicKey);
        dsaPubKey = (DSAPublicKey) keyPair.getPublic();
        System.out.println("Generated 512-bit DSA KeyPair!");
        dsaParams = dsaPubKey.getParams();
        System.out.println("P: "+dsaParams.getP());
        System.out.println("Q: "+dsaParams.getQ());
        System.out.println("G: "+dsaParams.getG());
        System.out.println("Y: "+dsaPubKey.getY());

        // 1024-bit DSA
        keyPairGenerator.initialize(1024);
        keyPair = keyPairGenerator.genKeyPair();
        Assert.assert( keyPair.getPublic() instanceof DSAPublicKey);
        dsaPubKey = (DSAPublicKey) keyPair.getPublic();
        System.out.println("Generated 1024-bit DSA KeyPair!");
        dsaParams = dsaPubKey.getParams();
        System.out.println("P: "+dsaParams.getP());
        System.out.println("Q: "+dsaParams.getQ());
        System.out.println("G: "+dsaParams.getG());
        System.out.println("Y: "+dsaPubKey.getY());

        System.out.println("TestKeyGen passed");
        System.exit(0);
      } catch (Exception e) {
            e.printStackTrace();
            System.exit(1);
      }
    }
}
