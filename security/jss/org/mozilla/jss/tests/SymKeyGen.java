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

import org.mozilla.jss.crypto.*;
import org.mozilla.jss.CryptoManager;
import org.mozilla.jss.util.Assert;
import org.mozilla.jss.util.Password;
import java.security.InvalidAlgorithmParameterException;
import java.security.spec.AlgorithmParameterSpec;
import org.mozilla.jss.CertDatabaseException;
import org.mozilla.jss.KeyDatabaseException;
import java.security.GeneralSecurityException;
import org.mozilla.jss.pkcs11.PK11SecureRandom;
import javax.crypto.spec.RC2ParameterSpec;
import java.util.*;

/**
 * Most of this code is Deprecated look at JCASymKeyGen.java for sample.
 */
public class SymKeyGen {
    private CryptoToken token = null;
    byte[] plainText16Bytes = "Firefox   rules!".getBytes(); /* 16 bytes */
    byte[] plainText18Bytes = "Thunderbird rules!".getBytes(); /* 18 bytes */
            
    public SymmetricKey genPBEKey(PBEAlgorithm alg, 
                    SymmetricKey.Type keyType, int keyStrength)    
        throws Exception {

        SymmetricKey key = null;
        byte[] keyData;    
        KeyGenerator kg = token.getKeyGenerator(alg);

        try {
            //this is debug code you don't initialize 
            //PBE algs with key Strength doing this should throw 
            //InvaldAlgortithmParameterException
            kg.initialize(keyStrength);
            throw new Exception("ERROR: Initializing PBE key with strength " +
                keyStrength + " succeeded");

        } catch( InvalidAlgorithmParameterException e) {
        }
        
        Password pass = new Password( ("passwd1").toCharArray() );
        byte[] salt = genSalt(alg.getSaltLength());
        PBEKeyGenParams kgp = new PBEKeyGenParams(pass, salt, 2);
        kg.initialize(kgp);
        key = kg.generate();
        
        if( key.getType() != keyType ) {
            throw new Exception("Wrong key type: "+key.getType());
        }
        if( ! key.getOwningToken().equals( token ) ) {
            throw new Exception("wrong token");
        }
        if( key.getStrength() != keyStrength ) {
            throw new Exception("wrong strength: "+key.getStrength());
        }
        return key;
    }

    public SymmetricKey genPBAKey(KeyGenAlgorithm alg, 
                    SymmetricKey.Type keyType, int keyStrength)    
        throws Exception {

        SymmetricKey key = null;
        byte[] keyData;    
        KeyGenerator kg = token.getKeyGenerator(alg);

        try {
            //this is debug code you don't initialize 
            //PBE algs with key Strength doing this should throw 
            //InvaldAlgortithmParameterException
            kg.initialize(keyStrength);
            throw new Exception("ERROR: Initializing PBE key with strength "+
                keyStrength + " succeeded");

        } catch( InvalidAlgorithmParameterException e) {
        }
        
        Password pass = new Password( ("passwd1").toCharArray() );
        byte[] salt = genSalt(8);
        PBEKeyGenParams kgp = new PBEKeyGenParams(pass, salt, 2);
        kg.initialize(kgp);
        key = kg.generate();
        
        if( key.getType() != keyType ) {
            throw new Exception("Wrong key type: "+key.getType());
        }
        if( ! key.getOwningToken().equals( token ) ) {
            throw new Exception("wrong token");
        }
        if( key.getStrength() != keyStrength ) {
            throw new Exception("wrong strength: "+key.getStrength());
        }
        return key;
    }
    
    public SymmetricKey genSymKey(KeyGenAlgorithm alg, SymmetricKey.Type keyType
                                  , int keyStrength, int keyLength) 
        throws Exception {
        SymmetricKey key = null;
        byte[] keyData;    
        KeyGenerator kg = token.getKeyGenerator(alg);
        
        if (alg == KeyGenAlgorithm.AES || alg == KeyGenAlgorithm.RC4 
                || alg == KeyGenAlgorithm.RC2) {
            kg.initialize (keyStrength);
        }

        key = kg.generate();
        if( key.getType() != keyType ) {
            throw new Exception("wrong algorithm");
        }
        if( ! key.getOwningToken().equals( token ) ) {
            throw new Exception("wrong token");
        }
        if( key.getStrength() != keyStrength ) {
            throw new Exception("wrong strength");
        }
        keyData = key.getKeyData();
        if( keyData.length != keyLength ) {
            throw new Exception("key data wrong length: " + keyData.length);
        }

        return key;
    }
    
    public boolean cipherTest(SymmetricKey key, EncryptionAlgorithm eAlg 
                              ) throws Exception {
        boolean bStatus = false;   
        int ivLength = 0; 
        AlgorithmParameterSpec algParSpec = null;

        Cipher cipher = null;
        cipher = token.getCipherContext(eAlg);
        
        // if no padding is used plainText needs to be fixed length 
        // block divisable by 8 bytes
        byte[] plaintext = plainText18Bytes;
        if ((eAlg.getMode() == EncryptionAlgorithm.Mode.CBC || 
             eAlg.getMode() == EncryptionAlgorithm.Mode.ECB ) &&               
             eAlg.getPadding() == EncryptionAlgorithm.Padding.NONE) {
            plaintext = plainText16Bytes;
            
        }
        // size 0 means this algorithm does not take an IV.
        // you need to use the same IV for Encrypt/Decrypt
        ivLength = eAlg.getIVLength();
        if (ivLength != 0 ) {
            algParSpec = genIV(ivLength);
        } 
        
        if (key.getType() == (SymmetricKey.Type) SymmetricKey.RC2) {
            byte[] iv = new byte[ivLength];
            PK11SecureRandom rng = new PK11SecureRandom();
            rng.nextBytes(iv);
            algParSpec = new RC2ParameterSpec(40, iv);  
        }  

        if (algParSpec == null) {
            cipher.initEncrypt(key);  
        } else {
            cipher.initEncrypt(key, algParSpec);
        }
        
        byte[] ciphertext = cipher.doFinal(plaintext);
        if (ivLength == 0) {
            cipher.initDecrypt(key);  
        } else {
            cipher.initDecrypt(key, algParSpec);
        }

        byte[] recovered = cipher.doFinal(ciphertext);
        
        if( recovered.length != plaintext.length ) {
            throw new Exception("Recovered plaintext has different length ("+
                recovered.length+") than original ("+plaintext.length+")");
        }
        
        if (java.util.Arrays.equals(plaintext, recovered) ) {
            bStatus = true;
        } else {
            throw new Exception("ERROR: unable to recover plaintext");
        }
        
        return bStatus; // no exception was thrown. 
    }
    
    private SymKeyGen( String certDbLoc) {
        try {
            CryptoManager.initialize(certDbLoc);
            CryptoManager cm  = CryptoManager.getInstance();
            token = cm.getInternalCryptoToken();
        } catch (AlreadyInitializedException ex) {
            ex.printStackTrace();
        } catch (CertDatabaseException ex) {
            ex.printStackTrace();
        } catch (CryptoManager.NotInitializedException ex) {
            ex.printStackTrace();
        } catch (GeneralSecurityException ex) {
            ex.printStackTrace();
        } catch (KeyDatabaseException ex) {
            ex.printStackTrace();
        }
    }

    public IVParameterSpec genIV(int blockSize) throws Exception {
        // generate an IV
        byte[] iv = new byte[blockSize];
        
        PK11SecureRandom rng = new PK11SecureRandom();
        rng.nextBytes(iv);
        
        return new IVParameterSpec(iv);
    }
    
    public byte[] genSalt(int saltSize) throws Exception {
        byte[] salt = new byte[saltSize];
        
        PK11SecureRandom rng = new PK11SecureRandom();
        rng.nextBytes(salt);
        
        return salt;
    }

    class alg {
        public KeyGenAlgorithm sAlg;
        public SymmetricKey.Type keyType;
        public int size;
        public int blkSize;
        List ciphers = new LinkedList();
        
        public alg (KeyGenAlgorithm alg, SymmetricKey.Type kType, int sz, int bSize) {
            sAlg = alg;
            keyType = kType;
            size = sz;
            blkSize = bSize;
        } 
        
        public void setEncAlgs(List c) {
            ciphers = c;
        }
       
    } 
    
    public static void main(String args[]) {

      try {

         if ( args.length < 1 ) {
             System.out.println("Usage: java org.mozilla.jss.tests." +
                                "SymKeyGen <dbdir>");
             System.exit(1);
         }

        SymKeyGen skg = new SymKeyGen(args[0]);
        SymmetricKey key = null;
        
        //DES Key
        key = skg.genSymKey(KeyGenAlgorithm.DES, SymmetricKey.DES, 56, 8);
        skg.cipherTest(key, EncryptionAlgorithm.DES_CBC_PAD);
        skg.cipherTest(key, EncryptionAlgorithm.DES_CBC);
        skg.cipherTest(key, EncryptionAlgorithm.DES_ECB);        
        System.out.println("DES key and cipher tests correct");
        
        // DES3 key
        key = skg.genSymKey(KeyGenAlgorithm.DES3, SymmetricKey.DES3, 168, 24);
        skg.cipherTest(key, EncryptionAlgorithm.DES3_CBC_PAD);
        skg.cipherTest(key, EncryptionAlgorithm.DES3_CBC);
        skg.cipherTest(key, EncryptionAlgorithm.DES3_ECB);        
        System.out.println("DESede key and cipher tests correct");
        
        // AES 128 key
        key = skg.genSymKey(KeyGenAlgorithm.AES, SymmetricKey.AES, 128, 128/8);
        skg.cipherTest(key, EncryptionAlgorithm.AES_128_CBC);
        skg.cipherTest(key, EncryptionAlgorithm.AES_128_ECB);        
        skg.cipherTest(key, EncryptionAlgorithm.AES_128_CBC_PAD);
        System.out.println("AES 128 key and cipher tests correct");
        
        // AES 192 key
        key = skg.genSymKey(KeyGenAlgorithm.AES, SymmetricKey.AES, 192, 192/8);
        skg.cipherTest(key, EncryptionAlgorithm.AES_192_CBC);
        skg.cipherTest(key, EncryptionAlgorithm.AES_192_ECB);        
        skg.cipherTest(key, EncryptionAlgorithm.AES_192_CBC_PAD);
        System.out.println("AES 192 key and cipher tests correct");
        
        // AES 256 key
        key = skg.genSymKey(KeyGenAlgorithm.AES, SymmetricKey.AES, 256, 256/8);
        skg.cipherTest(key, EncryptionAlgorithm.AES_256_CBC);
        skg.cipherTest(key, EncryptionAlgorithm.AES_256_ECB);        
        skg.cipherTest(key, EncryptionAlgorithm.AES_256_CBC_PAD);
        System.out.println("AES 256 key and cipher tests correct");
        
        // RC2 Key 
        key = skg.genSymKey(KeyGenAlgorithm.RC2, SymmetricKey.RC2, 40, 5);
        skg.cipherTest(key, EncryptionAlgorithm.RC2_CBC);
        skg.cipherTest(key, EncryptionAlgorithm.RC2_CBC_PAD);
        System.out.println("RC2 key and cipher tests correct");
        
        // RC4 key
        key = skg.genSymKey(KeyGenAlgorithm.RC4, SymmetricKey.RC4, 128, 128/8);
        skg.cipherTest(key, EncryptionAlgorithm.RC4);
        System.out.println("RC4 key and cipher tests correct");
        
        //Todo 
        //KeyGenAlgorithm.PBA_SHA1_HMAC, SymmetricKey.SHA1_HMAC, 160);

        //PBE key gen test. 
        // PBEAlgorithm.PBE_MD2_DES_CBC
        key = skg.genPBEKey(PBEAlgorithm.PBE_MD2_DES_CBC, SymmetricKey.DES, 
                      56);
        skg.cipherTest(key, EncryptionAlgorithm.DES_CBC_PAD);
        skg.cipherTest(key, EncryptionAlgorithm.DES_CBC);
        skg.cipherTest(key, EncryptionAlgorithm.DES_ECB);        
        
        //PBEAlgorithm.PBE_MD5_DES_CBC
        key = skg.genPBEKey(PBEAlgorithm.PBE_MD5_DES_CBC, SymmetricKey.DES, 
                      56);
        skg.cipherTest(key, EncryptionAlgorithm.DES_CBC_PAD);
        skg.cipherTest(key, EncryptionAlgorithm.DES_CBC);
        skg.cipherTest(key, EncryptionAlgorithm.DES_ECB);        
        
        //PBEAlgorithm.PBE_SHA1_DES_CBC
        key = skg.genPBEKey(PBEAlgorithm.PBE_SHA1_DES_CBC, SymmetricKey.DES, 
                      64);  
        skg.cipherTest(key, EncryptionAlgorithm.DES_CBC_PAD);
        skg.cipherTest(key, EncryptionAlgorithm.DES_CBC);
        skg.cipherTest(key, EncryptionAlgorithm.DES_ECB);        
        
        //PBEAlgorithm.PBE_SHA1_DES3_CBC
        key = skg.genPBEKey(PBEAlgorithm.PBE_SHA1_DES3_CBC, SymmetricKey.DES3, 
                      168);
        skg.cipherTest(key, EncryptionAlgorithm.DES3_CBC_PAD);
        skg.cipherTest(key, EncryptionAlgorithm.DES3_CBC);
        skg.cipherTest(key, EncryptionAlgorithm.DES3_ECB);        
        
        //PBEAlgorithm.PBE_SHA1_RC2_40_CBC
        key = skg.genPBEKey(PBEAlgorithm.PBE_SHA1_RC2_40_CBC, SymmetricKey.RC2, 
                      40);
        skg.cipherTest(key, EncryptionAlgorithm.RC2_CBC);
        skg.cipherTest(key, EncryptionAlgorithm.RC2_CBC_PAD);
        
        //PBEAlgorithm.PBE_SHA1_RC2_128_CBC
        key = skg.genPBEKey(PBEAlgorithm.PBE_SHA1_RC2_128_CBC, SymmetricKey.RC2, 
                      128);
        skg.cipherTest(key, EncryptionAlgorithm.RC2_CBC);
        skg.cipherTest(key, EncryptionAlgorithm.RC2_CBC_PAD);
                
        //PBEAlgorithm.PBE_SHA1_RC4_40
        key = skg.genPBEKey(PBEAlgorithm.PBE_SHA1_RC4_40, SymmetricKey.RC4, 
                      40);
        skg.cipherTest(key, EncryptionAlgorithm.RC4);        
        
        //PBEAlgorithm.PBE_SHA1_RC4_128
        key = skg.genPBEKey(PBEAlgorithm.PBE_SHA1_RC4_128, SymmetricKey.RC4, 
                      128);
        skg.cipherTest(key, EncryptionAlgorithm.RC4);
        
        System.out.println("Password Based key generation tests correct");         
        
        
      } catch(Exception e) {
        e.printStackTrace();
      }
    }
}
