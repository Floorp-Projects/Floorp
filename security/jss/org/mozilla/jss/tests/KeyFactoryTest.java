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

import java.security.*;
import java.security.spec.*;
import org.mozilla.jss.CryptoManager;
import org.mozilla.jss.crypto.CryptoToken;
import java.util.Iterator;
import org.mozilla.jss.util.PasswordCallback;

abstract class TestValues {
    protected TestValues(String keyGenAlg, String sigAlg,
        Class privateKeySpecClass, Class publicKeySpecClass,
        String provider)
    {
        this.keyGenAlg = keyGenAlg;
        this.sigAlg = sigAlg;
        this.privateKeySpecClass = privateKeySpecClass;
        this.publicKeySpecClass = publicKeySpecClass;
        this.provider = provider;
    }

    public final String keyGenAlg;
    public final String sigAlg;
    public final Class privateKeySpecClass;
    public final Class publicKeySpecClass;
    public final String provider;
}

class RSATestValues extends TestValues {
    public RSATestValues() {
        super("RSA", "SHA1withRSA", RSAPrivateCrtKeySpec.class,
            RSAPublicKeySpec.class, "SunRsaSign");
    }
}

class DSATestValues extends TestValues {
    public DSATestValues() {
        super("DSA", "SHA1withDSA", DSAPrivateKeySpec.class,
            DSAPublicKeySpec.class, "SUN");
    }
}

public class KeyFactoryTest {


    public static void main(String argv[]) {
      try {

        if( argv.length < 2 ) {
	    System.out.println(
		"Usage: java org.mozilla.jss.tests.KeyFactoryTest " +
		 "<dbdir> <passwordFile>");
            System.exit(1);
        }
        CryptoManager.initialize(argv[0]);
        CryptoToken tok = CryptoManager.getInstance().getInternalKeyStorageToken();
	PasswordCallback cb = new FilePasswordCallback(argv[1]);
        tok.login(cb);

        Provider []provs = Security.getProviders();
        for( int i=0; i < provs.length; ++i) {
            System.out.println("======");
            System.out.println(provs[i].getName());
            provs[i].list(System.out);
            System.out.println("======");
        }

        (new KeyFactoryTest()).doTest();          
        
      } catch(Throwable e) {
            e.printStackTrace();
            System.exit(1);
      }
      System.exit(0);
    }

    public void doTest() throws Throwable {
        RSATestValues rsa = new RSATestValues();
        DSATestValues dsa = new DSATestValues();

        // Generate RSA private key from spec
        genPrivKeyFromSpec(rsa);

        // Generate DSA private key from spec
        genPrivKeyFromSpec(dsa);

        // translate RSA key
        genPubKeyFromSpec(rsa);

        // translate key
	genPubKeyFromSpec(dsa);

	System.exit(0);
    }

    public void genPrivKeyFromSpec(TestValues vals) throws Throwable {

        // generate the key pair
        KeyPairGenerator kpg =
            KeyPairGenerator.getInstance(vals.keyGenAlg, vals.provider);
        kpg.initialize(512);
        KeyPair pair = kpg.generateKeyPair();

        // get the private key spec
        KeyFactory sunFact = KeyFactory.getInstance(vals.keyGenAlg,
            vals.provider);
        KeySpec keySpec = 
            sunFact.getKeySpec(pair.getPrivate(), vals.privateKeySpecClass);

        // import it into JSS
        KeyFactory jssFact = KeyFactory.getInstance(vals.keyGenAlg,
            "Mozilla-JSS");
        PrivateKey jssPrivk = jssFact.generatePrivate(keySpec);

        signVerify(vals.sigAlg, jssPrivk, "Mozilla-JSS",
            pair.getPublic(), vals.provider);

        System.out.println("Successfully generated a " + vals.keyGenAlg +
            " private key from a " + vals.privateKeySpecClass.getName());
    }

    public void signVerify(String sigAlg, PrivateKey privk, String signProv,
        PublicKey pubk, String verifyProv) throws Throwable
    {
        Signature signSig = Signature.getInstance(sigAlg, signProv);
        signSig.initSign(privk);
        String toBeSigned = "blah blah blah sign me";
        signSig.update(toBeSigned.getBytes("UTF-8"));
        byte[] signature = signSig.sign();

        Signature verSig = Signature.getInstance(sigAlg, verifyProv);
        verSig.initVerify(pubk);
        verSig.update(toBeSigned.getBytes("UTF-8"));
        if( ! verSig.verify(signature) ) {
            throw new Exception(
                "Private/public key mismatch: signing alg=" + sigAlg +
                ", signing provider=" + signProv + ", verifying provider = " +
                verifyProv);
        }
    }

    public void genPubKeyFromSpec(TestValues vals) throws Throwable {
        // generate a key pair
        KeyPairGenerator kpg = KeyPairGenerator.getInstance(vals.keyGenAlg,
            vals.provider);
        kpg.initialize(512);
        KeyPair pair = kpg.generateKeyPair();

        // get the public key spec
        KeyFactory sunFact = KeyFactory.getInstance(vals.keyGenAlg,
            vals.provider);
        KeySpec keySpec = 
            sunFact.getKeySpec(pair.getPublic(), vals.publicKeySpecClass);

        // import it into JSS
        KeyFactory jssFact = KeyFactory.getInstance(vals.keyGenAlg,
            "Mozilla-JSS");
        PublicKey jssPubk = jssFact.generatePublic(keySpec);

        signVerify(vals.sigAlg, pair.getPrivate(), vals.provider,
            jssPubk, "Mozilla-JSS");

        System.out.println("Successfully generated a " + vals.keyGenAlg +
            " public key from a " + vals.publicKeySpecClass.getName());
    }
}
