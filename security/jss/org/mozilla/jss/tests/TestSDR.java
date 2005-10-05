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
 * The Original Code is Network Security Services for Java.
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
import java.security.cert.Certificate;
import org.mozilla.jss.CryptoManager;
import org.mozilla.jss.crypto.*;
import org.mozilla.jss.SecretDecoderRing.*;
import java.io.*;
import javax.crypto.*;
import org.mozilla.jss.asn1.ASN1Util;

public class TestSDR {

    public static final EncryptionAlgorithm encAlg =
        EncryptionAlgorithm.DES3_CBC;
    public static final KeyGenAlgorithm keyGenAlg = KeyGenAlgorithm.DES3;

    public static void main(String[] args) throws Exception {
        if( args.length != 2 ) {
            throw new Exception("Usage: java TestSDR <dbdir> <pwfile>");
        }
        CryptoManager.initialize(args[0]);
        CryptoManager cm = CryptoManager.getInstance();
        cm.setPasswordCallback( new FilePasswordCallback(args[1]) );

        CryptoToken ksToken = cm.getInternalKeyStorageToken();

        //
        // test key management
        //
        KeyManager km = new KeyManager(ksToken);

        byte[] keyID = km.generateKey(keyGenAlg, 0);
        System.out.println("Successfully generated key");

        SecretKey key = km.lookupKey(encAlg, keyID);
        if( key == null ) {
            throw new Exception("Error: generated key not found");
        }
        System.out.println("Successfully looked up key");

        km.deleteKey(keyID);
        System.out.println("Successfully deleted key");

        key = km.lookupKey(encAlg, keyID);
        if( key != null ) {
            throw new Exception("Deleted key still found");
        }
        System.out.println("Good: deleted key not found");

        //
        // test encryption/decryption
        //
        keyID = km.generateKey(keyGenAlg, 0);

        Encryptor encryptor = new Encryptor(ksToken, keyID, encAlg);

        byte[] plaintext = "Hello, world!".getBytes("UTF-8");

        byte[] ciphertext = encryptor.encrypt(plaintext);

        System.out.println("Successfully encrypted plaintext");

        Decryptor decryptor = new Decryptor(ksToken);

        byte[] recovered = decryptor.decrypt(ciphertext);
        System.out.println("Decrypted ciphertext");

        if( plaintext.length != recovered.length ) {
            throw new Exception(
                "Recovered plaintext does not match original plaintext");
        }
        for(int i=0; i < plaintext.length; ++i) {
            if( plaintext[i] != recovered[i] ) {
                throw new Exception(
                    "Recovered plaintext does not match original plaintext");
            }
        }
        System.out.println("Decrypted ciphertext matches original plaintext");

        // delete the key and try to decrypt. Decryption should fail.
        km.deleteKey(keyID);
        try {
            recovered = decryptor.decrypt(ciphertext);
            throw new Exception(
                "Error: successfully decrypted with deleted key");
        } catch (InvalidKeyException ike) { }
        System.out.println(
            "Good: failed to decrypt plaintext with deleted key");

        System.out.println("TestSDR: Success");
        System.exit(0);
    }
}
