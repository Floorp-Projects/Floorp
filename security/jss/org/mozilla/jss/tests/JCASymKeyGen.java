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

import java.security.GeneralSecurityException;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.NoSuchProviderException;
import org.mozilla.jss.CertDatabaseException;
import org.mozilla.jss.KeyDatabaseException;
import org.mozilla.jss.crypto.*;
import org.mozilla.jss.CryptoManager;
import java.security.Provider;
import java.security.Security;

import java.security.AlgorithmParameters;
import java.security.spec.AlgorithmParameterSpec;
import javax.crypto.spec.RC2ParameterSpec;

import javax.crypto.KeyGenerator;
import javax.crypto.Cipher;
import javax.crypto.SecretKey;
import javax.crypto.SecretKeyFactory;
import javax.crypto.spec.PBEKeySpec;
import java.security.SecureRandom;

/**
 * 
 */
public class JCASymKeyGen {
    static final String MOZ_PROVIDER_NAME = "Mozilla-JSS";
    byte[] plainText16Bytes = "Firefox   rules!".getBytes(); /* 16 bytes */
    byte[] plainText18Bytes = "Thunderbird rules!".getBytes(); /* 18 bytes */
    /**
     * Default constructor
     */
    public JCASymKeyGen( String certDbLoc) {
        try {
            CryptoManager.initialize(certDbLoc);
            CryptoManager cm  = CryptoManager.getInstance();
            CryptoToken token = cm.getInternalCryptoToken();
        } catch (AlreadyInitializedException ex) {
            ex.printStackTrace();
            System.exit(1);
        } catch (CertDatabaseException ex) {
            ex.printStackTrace();
            System.exit(1);
        } catch (CryptoManager.NotInitializedException ex) {
            ex.printStackTrace();
            System.exit(1);
        } catch (GeneralSecurityException ex) {
            ex.printStackTrace();
            System.exit(1);
        } catch (KeyDatabaseException ex) {
            ex.printStackTrace();
            System.exit(1);
        }
    }
    /**
     * 
     * @param  
     */
    public javax.crypto.SecretKey genSecretKey(String keyType, String provider){
        javax.crypto.SecretKey key = null;
        javax.crypto.KeyGenerator kg = null;
        try {
            
            kg = KeyGenerator.getInstance(keyType,
                    provider);
            if (keyType.equals("AES") || keyType.equals("RC2")) {
                kg.init(128); //JDK 1.4 and 1.5 only supports 128 keys for AES
            }
            
            System.out.println("Key " + keyType + " generation done by "
                    + kg.getProvider().toString());
            key = kg.generateKey();
            if( !checkAlgorithm(key, keyType) ) {
                throw new Exception("Error: " + key.getAlgorithm() +
                        "  algorithm");
            }
            //System.out.println("The length of the generated key in bits: " +
            //    (key.getEncoded().length * 8) +
            //    " " + key.getAlgorithm() );
        } catch (NoSuchProviderException ex) {
            ex.printStackTrace();
        } catch (NoSuchAlgorithmException ex) {
            ex.printStackTrace();
        } catch (Exception ex) {
            ex.printStackTrace();
        }
        return key;
    }
    
    /**
     * 
     * @param keyType 
     * @param provider 
     * @return 
     */
    public javax.crypto.SecretKey genPBESecretKey(String keyType,
            String provider){
        javax.crypto.SecretKey key = null;
        javax.crypto.SecretKeyFactory kf = null;
        try {
            char[] pw = "thunderbird".toCharArray();
            byte[] salt = new byte[8];
            SecureRandom random = SecureRandom.getInstance("pkcs11prng",
                    MOZ_PROVIDER_NAME);
            random.nextBytes(salt);
            int iterationCount = 2;
            
            kf = SecretKeyFactory.getInstance(keyType,
                    provider);
            PBEKeySpec keySpec = new PBEKeySpec(pw, salt, iterationCount);
            key = (SecretKeyFacade) kf.generateSecret(keySpec);
            
            //todo this should work as well
            //PBEKeySpec pbeKeySpec = new PBEKeySpec(pw));
            // key = kf.generateSecret(pbeKeySpec);
            System.out.println("Key " + keyType + " generation done by "
                    + kf.getProvider().toString());
            //System.out.println("The length of the generated key in bits: " +
            //    (key.getEncoded().length * 8) +
            //    " " + key.getAlgorithm() );
        } catch (NoSuchProviderException ex) {
            ex.printStackTrace();
        } catch (NoSuchAlgorithmException ex) {
            ex.printStackTrace();
        } catch (Exception ex) {
            ex.printStackTrace();
        }
        return key;
    }
    
    /**
     *
     * @param sKey
     * @param AlgType
     * @param provider
     */
    public void testCipher(javax.crypto.SecretKey sKey, String algFamily,
            String algType, String providerForEncrypt, String providerForDecrypt)
            throws Exception {
        try {
            
            // if no padding is used plainText needs to be fixed length
            // block divisable by 8 bytes
            byte[] plaintext = plainText16Bytes;
            if (algType.endsWith("PKCS5Padding")) {
                plaintext = plainText18Bytes;
            }
            
            //encypt
            Cipher cipher = Cipher.getInstance(algType, providerForEncrypt);
            AlgorithmParameters ap = null;
            byte[] encodedAlgParams = null;
            AlgorithmParameterSpec RC2ParSpec = null;
            
            if (algFamily.compareToIgnoreCase("RC2")==0) {
                //JDK 1.4 requires you to pass in generated algorithm
                //parameters for RC2 (JDK 1.5 does not).
                byte[] iv = new byte[8];
                SecureRandom random = SecureRandom.getInstance("pkcs11prng",
                        MOZ_PROVIDER_NAME);
                random.nextBytes(iv);
                RC2ParSpec = new RC2ParameterSpec(128, iv);
                cipher.init(Cipher.ENCRYPT_MODE, sKey, RC2ParSpec);
                
            } else {
                cipher.init(Cipher.ENCRYPT_MODE, sKey);
                //generate the algorithm Parameters; they need to be
                //the same for encrypt/decrypt if they are needed.
                ap = cipher.getParameters();
                if (ap != null) {
                    //get parameters to store away as example.
                    encodedAlgParams = ap.getEncoded();
                }
            }
            
            byte[] ciphertext = cipher.doFinal(plaintext);
            
            //decrypt
            
            cipher = Cipher.getInstance(algType, providerForDecrypt);
            if (encodedAlgParams == null)
                if (RC2ParSpec != null)
                    // JDK 1.4 RC2
                    cipher.init(Cipher.DECRYPT_MODE, sKey, RC2ParSpec);
                else
                    cipher.init(Cipher.DECRYPT_MODE, sKey);
            else {
                //retrieve the algorithmParameters from the encoded array
                AlgorithmParameters aps =
                        AlgorithmParameters.getInstance(algFamily);
                aps.init(encodedAlgParams);
                cipher.init(Cipher.DECRYPT_MODE, sKey, aps);
            }
            byte[] recovered = cipher.doFinal(ciphertext);
            
            if (java.util.Arrays.equals(plaintext, recovered) ) {
                // System.out.println(providerForEncrypt + " encrypted & " +
                //        providerForDecrypt + " decrypted using " +
                //        algType + " successful.");
            } else {
                throw new Exception("ERROR: " + providerForEncrypt +
                        " and " + providerForDecrypt + " failed for "
                        + algType );
            }
        } catch (InvalidKeyException ex) {
            ex.printStackTrace();
        } catch (javax.crypto.BadPaddingException ex) {
            ex.printStackTrace();
        } catch (NoSuchProviderException ex) {
            ex.printStackTrace();
        } catch (javax.crypto.NoSuchPaddingException ex) {
            ex.printStackTrace();
        } catch (javax.crypto.IllegalBlockSizeException ex) {
            ex.printStackTrace();
        } catch (NoSuchAlgorithmException ex) {
            ex.printStackTrace();
        }
    }
    
    public static void main(String args[]) {
        
        String certDbLoc             = ".";
        
        // Mozilla supported symmetric key ciphers and algorithms
        // Note JCE supports algorthm/ECB/PKCS5Padding and JSS does
        // not support algorithms in ECB mode with PKCS5Padding
        String [][] symKeyTable = {
            {"DES",  "DES/ECB/NoPadding", "DES/CBC/PKCS5Padding",
                     "DES/CBC/NoPadding" },
            {"DESede", "DESede/ECB/NoPadding", "DESede/CBC/PKCS5Padding",
                              "DESede/CBC/NoPadding" },
            {"AES", "AES/ECB/NoPadding",  "AES/CBC/NoPadding",
                                 "AES/CBC/PKCS5Padding"},
            {"RC2", "RC2/CBC/NoPadding", "RC2/CBC/PKCS5Padding"},
            //{"RC4", "RC4"}, todo
            //{PBAHmacSHA1"}, todo
            {"PBEWithMD5AndDES", "DES/ECB/NoPadding"},
            //todo "DES/CBC/PKCS5Padding",  "DES/CBC/NoPadding" },
            {"PBEWithSHA1AndDES"},
            {"PBEWithSHA1AndDESede", "DESede/ECB/NoPadding"},
            //{"PBEWithSHA1And128RC4"}, todo
        };
        
        
        
        if ( args.length == 1 ) {
            certDbLoc  = args[0];
        } else {
            System.out.println(
                    "USAGE: java org.mozilla.jss.tests.JCASymKeyGen" +
                    " <certDbPath> ");
            System.exit(1);
        }
        
        //If the IBMJCE provider exists tests with it otherwise
        //use the SunJCE provider.
        String otherProvider = new String("IBMJCE");
        Provider p = null;
        p = Security.getProvider(otherProvider);
        if (p == null) {
            otherProvider = new String("SunJCE");
            p = Security.getProvider(otherProvider);
            if (p == null){
                System.out.println("unable to find IBMJCE or SunJCE providers");
                System.exit(1);
            }
        }
        
        JCASymKeyGen skg = new JCASymKeyGen(certDbLoc);
        System.out.println(otherProvider + ": " + p.getInfo());
        p = Security.getProvider(MOZ_PROVIDER_NAME);
        System.out.println(MOZ_PROVIDER_NAME + ": " + p.getInfo());
        
        javax.crypto.SecretKey mozKey = null;
        
        try {
            
            for (int i = 0 ; i < symKeyTable.length; i++) {
                try {
                    //generate the key using mozilla
                    if (symKeyTable[i][0].startsWith("PBE") == true) {
                        mozKey = skg.genPBESecretKey(symKeyTable[i][0],
                                MOZ_PROVIDER_NAME);
                    } else {
                        mozKey = skg.genSecretKey(symKeyTable[i][0],
                                MOZ_PROVIDER_NAME);
                    }
                } catch(Exception e) {
                    System.out.println("unable to generate key: " +
                            symKeyTable[i][0] + " " + e.getMessage());
                }
                //test the cihper algorithms for this keyType
                for (int a = 1 ;  a < symKeyTable[i].length; a++){
                    //encrypt/decrypt with Mozilla Provider
                    skg.testCipher(mozKey, symKeyTable[i][0], symKeyTable[i][a],
                            MOZ_PROVIDER_NAME, MOZ_PROVIDER_NAME);
                    try {
                        //check to see if the otherProvider we are testing
                        //against supports the algorithm.
                        Cipher cipher = Cipher.getInstance(symKeyTable[i][a],
                                otherProvider);
                    } catch (Exception e) {
                        System.out.println(MOZ_PROVIDER_NAME + " only supports "
                                + symKeyTable[i][a]);
                        //therefore don't try comparison
                        continue;
                    }
                    //encrypt with Mozilla, and Decrypt with otherProvider
                    skg.testCipher(mozKey, symKeyTable[i][0], symKeyTable[i][a],
                            MOZ_PROVIDER_NAME, otherProvider);
                    
                    //encrypt with default otherProvider and decrypt with Mozilla
                    skg.testCipher(mozKey, symKeyTable[i][0], symKeyTable[i][a],
                            otherProvider, MOZ_PROVIDER_NAME);
                    System.out.println(MOZ_PROVIDER_NAME + " and  " + otherProvider
                            + " tested " + symKeyTable[i][a]);
                    
                }
            }
        } catch(Exception e) {
            e.printStackTrace();
            System.exit(1);
        }
        //end of main
        System.exit(0);
    }
    
    /**
     * Validate if the key algorithm of a given SecretKey
     * is the same as expected.
     * @param SecretKey k
     * @param String algorithm
     * @return boolean status
     */
    private boolean checkAlgorithm(SecretKey k, String alg) {
        boolean status = false;
        if( k.getAlgorithm().equals(alg) ) {
            status = true;
        }
        return status;
    }
    
    /**
     * Validate if the key length of a given SecretKey
     * is the same as expected.
     * @param SecretKey k
     * @param int key length
     * @return boolean status
     */
    private boolean checkKeyLength(SecretKey k, int len) {
        boolean status = false;
        byte[] keyData = k.getEncoded();
        if( keyData.length == len ) {
            status = true;
        }
        return status;
    }
    /**
     * Turns array of bytes into string
     *
     * @param buf Array of bytes to convert to hex string
     * @return Generated hex string
     */
    private String asHex(byte buf[]) {
        StringBuffer strbuf = new StringBuffer(buf.length * 2);
        int i;
        
        for (i = 0; i < buf.length; i++) {
            if (((int) buf[i] & 0xff) < 0x10)
                strbuf.append("0");
            strbuf.append(Long.toString((int) buf[i] & 0xff, 16));
        }
        return strbuf.toString();
    }
}
