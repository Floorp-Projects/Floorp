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
    }
}
