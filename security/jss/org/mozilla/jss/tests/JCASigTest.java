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

import java.security.*;
import java.security.cert.X509Certificate;
import java.io.*;
import org.mozilla.jss.util.*;
import org.mozilla.jss.*;


public class JCASigTest {

    public static void usage() {
        System.out.println(
        "Usage: java org.mozilla.jss.tests.JCASigTest <dbdir> <passwordFile>");
    }

    public static void sigTest(String alg, KeyPair keyPair) {
        byte[] data = new byte[] {1,2,3,4,5,6,7,8,9};
        byte[] signature;
        Signature signer;

        try {
            signer = Signature.getInstance(alg);

            System.out.println("Created a signing context");
            Provider provider = signer.getProvider();
            System.out.println("The provider used for the signer " 
                 + provider.getName() + " and the algorithm was " + alg);

            signer.initSign(
                   (org.mozilla.jss.crypto.PrivateKey)keyPair.getPrivate());
            System.out.println("initialized the signing operation");

            signer.update(data);
            System.out.println("updated signature with data");
            signature = signer.sign();
            System.out.println("Successfully signed!");

            signer.initVerify(keyPair.getPublic());
            System.out.println("initialized verification");
            signer.update(data);
            System.out.println("updated verification with data");
            if ( signer.verify(signature) ) {
                System.out.println("Signature Verified Successfully!");
            } else {
                System.out.println("ERROR: Signature failed to verify.");
            }
        } catch ( Exception e ) {
            e.printStackTrace();
        }
    }

    public static void main(String args[]) {
        CryptoManager manager;
        KeyPairGenerator kpgen;
        KeyPair keyPair;

        if ( args.length != 2 ) {
            usage();
            return;
        }
        String dbdir = args[0];
        String file = args[1];
        try {
            CryptoManager.InitializationValues vals = new
                                CryptoManager.InitializationValues (dbdir );
            vals.removeSunProvider = true;
            CryptoManager.initialize(vals);
            manager = CryptoManager.getInstance();
            manager.setPasswordCallback( new FilePasswordCallback(file) );

            Debug.setLevel(Debug.OBNOXIOUS);

            Provider[] providers = Security.getProviders();
            for ( int i=0; i < providers.length; i++ ) {
                System.out.println("Provider "+i+": "+providers[i].getName());
            }

            // Generate an RSA keypair
            kpgen = KeyPairGenerator.getInstance("RSA");
            kpgen.initialize(1024);
            keyPair = kpgen.generateKeyPair();
            Provider  provider = kpgen.getProvider();

            System.out.println("The provider used to Generate the Keys was " 
                                + provider.getName() );
            System.out.println("provider info " + provider.getInfo() );

            sigTest("MD5/RSA", keyPair);
            sigTest("MD2/RSA", keyPair);
            sigTest("SHA-1/RSA", keyPair);
            sigTest("SHA-256/RSA", keyPair);
            sigTest("SHA-384/RSA", keyPair);
            sigTest("SHA-512/RSA", keyPair);


        } catch ( Exception e ) {
            e.printStackTrace();
        }
    }
}
