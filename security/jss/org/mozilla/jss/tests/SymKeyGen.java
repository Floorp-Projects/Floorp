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

public class SymKeyGen {

    public static void main(String args[]) {

      try {

        CryptoManager.initialize(".");
        CryptoManager cm = CryptoManager.getInstance();
        CryptoToken token = cm.getInternalCryptoToken();
        byte[] keyData;

        // DES key
        KeyGenerator kg = token.getKeyGenerator(KeyGenAlgorithm.DES);
        SymmetricKey key = kg.generate();
        if( key.getType() != SymmetricKey.DES ) {
            throw new Exception("wrong algorithm");
        }
        if( ! key.getOwningToken().equals( token ) ) {
            throw new Exception("wrong token");
        }
        if( key.getStrength() != 56 ) {
            throw new Exception("wrong strength");
        }
        keyData = key.getKeyData();
        if( keyData.length != 8 ) {
            throw new Exception("key data wrong length: " + keyData.length);
        }
        System.out.println("DES key is correct");

        IVParameterSpec iv = new IVParameterSpec(
            new byte[]{ (byte)0xff, (byte)0x00, (byte)0xff, (byte)0x00,
                        (byte)0xff, (byte)0x00, (byte)0xff, (byte)0x00 } );
        Cipher cipher = token.getCipherContext(EncryptionAlgorithm.DES_CBC_PAD);
        cipher.initEncrypt(key, iv);
        byte[] plaintext = new byte[] { (byte)0xff, (byte)0x00 };
        byte[] ciphertext = cipher.doFinal(plaintext);
        cipher.initDecrypt(key, iv);
        byte[] recovered = cipher.doFinal(ciphertext);
        if( recovered.length != plaintext.length ) {
            throw new Exception("Recovered plaintext has different length ("+
                recovered.length+") than original ("+plaintext.length+")");
        }
        for(int i=0; i < recovered.length; i++) {
            if( plaintext[i] != recovered[i] ) {
                throw new Exception("Recovered plaintext differs from original"
                    + " at position "+i);
            }
        }
        System.out.println("DES encryption succeeded");
        

        // DES3 key
        kg = token.getKeyGenerator(KeyGenAlgorithm.DES3);
        key = kg.generate();
        if( key.getType() != SymmetricKey.DES3 ) {
            throw new Exception("wrong algorithm");
        }
        if( ! key.getOwningToken().equals( token ) ) {
            throw new Exception("wrong token");
        }
        if( key.getStrength() != 168 ) {
            throw new Exception("wrong strength");
        }
        keyData = key.getKeyData();
        if( keyData.length != 24 ) {
            throw new Exception("key data wrong length: " + keyData.length);
        }
        System.out.println("DES3 key is correct");

        // RC4 key
        kg = token.getKeyGenerator(KeyGenAlgorithm.RC4);
        kg.initialize(128);
        key = kg.generate();
        if( key.getType() != SymmetricKey.RC4 ) {
            throw new Exception("wrong algorithm");
        }
        if( ! key.getOwningToken().equals( token ) ) {
            throw new Exception("wrong token");
        }
        if( key.getStrength() != 128 ) {
            throw new Exception("wrong strength");
        }
        keyData = key.getKeyData();
        if( keyData.length != 16 ) {
            throw new Exception("key data wrong length: " + keyData.length);
        }
        System.out.println("RC4 key is correct");
        
        // PBE MD5 DES CBC
        kg = token.getKeyGenerator(PBEAlgorithm.PBE_MD5_DES_CBC);
        try {
            kg.initialize(56);
            throw new Exception("ERROR: Initializing PBE key with strength "+
                "succeeded");
        } catch( InvalidAlgorithmParameterException e) { }
        Password pass = new Password( ("passwd").toCharArray() );
        byte[] salt = new byte[] { (byte)0xff, (byte)0x00, (byte)0xff };
        PBEKeyGenParams kgp = new PBEKeyGenParams(pass, salt, 2);
        kg.initialize(kgp);
        key = kg.generate();
        if( key.getType() != SymmetricKey.DES ) {
            throw new Exception("Wrong key type: "+key.getType());
        }
        if( ! key.getOwningToken().equals( token ) ) {
            throw new Exception("wrong token");
        }
        if( key.getStrength() != 56 && key.getStrength() != 64) {
            throw new Exception("wrong strength: "+key.getStrength());
        }
        keyData = key.getKeyData();
        if( keyData.length != 8 ) {
            throw new Exception("key data wrong length: " + keyData.length);
        }
        System.out.println("PBE key is correct");

      } catch(Exception e) {
        e.printStackTrace();
      }
    }
}
