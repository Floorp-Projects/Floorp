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

package org.mozilla.jss.SecretDecoderRing;

import java.security.*;
import javax.crypto.*;
import javax.crypto.spec.*;
import org.mozilla.jss.asn1.*;
import org.mozilla.jss.pkix.primitive.*;
import org.mozilla.jss.CryptoManager;
import org.mozilla.jss.crypto.CryptoToken;
import org.mozilla.jss.crypto.EncryptionAlgorithm;
import org.mozilla.jss.crypto.TokenException;
import java.io.*;

/**
 * Decrypts data with the SecretDecoderRing.
 */
public class Decryptor {
    private CryptoToken token;
    private KeyManager keyManager;

    /**
     * Creates a Decryptor for use with the given CryptoToken.
     */
    public Decryptor(CryptoToken token) {
        this.token = token;
        this.keyManager = new KeyManager(token);
    }

    /**
     * Decrypts the given ciphertext. It must have been created previously
     * with the SecretDecoderRing, either the JSS version or the NSS version.
     * The key used for decryption must exist on the token that was passed
     * into the constructor. The token will be searched for a key whose keyID
     * matches the keyID in the encoded SecretDecoderRing result.
     *
     * @param ciphertext A DER-encoded Encoding object, created from a previous
     *  call to Encryptor.encrypt(), or with the NSS SecretDecoderRing.
     * @return The decrypted plaintext.
     * @throws InvalidKeyException If no key can be found with the matching
     *  keyID.
     */
    public byte[] decrypt(byte[] ciphertext)
        throws CryptoManager.NotInitializedException,
        GeneralSecurityException, TokenException
    {
        CryptoManager cm = CryptoManager.getInstance();
        CryptoToken savedToken = cm.getThreadToken();

        try {
            cm.setThreadToken(token);

            //
            // decode ASN1
            //
            Encoding encoding = (Encoding)
                ASN1Util.decode(Encoding.getTemplate(), ciphertext);

            //
            // lookup the algorithm
            //
            EncryptionAlgorithm alg = EncryptionAlgorithm.fromOID(
                encoding.getEncryptionOID() );

            //
            // Lookup the key
            //
            SecretKey key = keyManager.lookupKey(alg, encoding.getKeyID());
            if( key == null ) {
                throw new InvalidKeyException("No matching key found");
            }

            //
            // do the decryption
            //
            IvParameterSpec ivSpec = new IvParameterSpec(encoding.getIv());

            Cipher cipher = Cipher.getInstance(alg.toString(),
                Encryptor.PROVIDER);
            cipher.init(Cipher.DECRYPT_MODE, key, ivSpec);

            byte[] paddedPtext = cipher.doFinal(encoding.getCiphertext());
            return org.mozilla.jss.crypto.Cipher.unPad(paddedPtext,
                alg.getBlockSize() );
        } catch(InvalidBERException ibe) {
            throw new GeneralSecurityException(ibe.toString());
        } catch(IllegalStateException ise) {
            throw new GeneralSecurityException(ise.toString());
        } catch(org.mozilla.jss.crypto.BadPaddingException bpe) {
            throw new javax.crypto.BadPaddingException(bpe.getMessage());
        } finally {
            cm.setThreadToken(savedToken);
        }
    }

}
