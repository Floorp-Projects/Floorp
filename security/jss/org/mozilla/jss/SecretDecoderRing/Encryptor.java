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
 * Encrypts data with the SecretDecoderRing.
 */
public class Encryptor {

    private CryptoToken token;
    private byte[] keyID;
    private SecretKey key;
    private EncryptionAlgorithm alg;
    private KeyManager keyManager;

    /**
     * The default encryption algorithm, currently DES3_CBC.
     */
    public static final EncryptionAlgorithm DEFAULT_ENCRYPTION_ALG
        = EncryptionAlgorithm.DES3_CBC;

    static final String PROVIDER = "Mozilla-JSS";
    static final String RNG_ALG = "pkcs11prng";

    /**
     * Creates an Encryptor on the given CryptoToken, using the key with
     * the given keyID and algorithm
     * @param token The CryptoToken to use for encryption. The key must
     *  reside on this token.
     * @param keyID The keyID of the key to use for encryption. This key
     *  must have been generated on this token with KeyManager.
     * @param alg The EncryptionAlgorithm this key will be used for.
     * @throws InvalidKeyException If no key exists on this token with this
     *  keyID.
     */
    public Encryptor(CryptoToken token, byte[] keyID, EncryptionAlgorithm alg)
            throws TokenException, InvalidKeyException
    {
        this.token = token;
        this.keyID = keyID;
        this.alg = alg;
        this.keyManager = new KeyManager(token);

        // make sure this key exists on the token
        key = keyManager.lookupKey(alg, keyID);

        // make sure key matches algorithm
        // !!! not sure how to do this
    }

    /**
     * Encrypts a byte array.
     * @param plaintext The plaintext bytes to be encrypted.
     * @return The ciphertext. This is actually a DER-encoded Encoding
     *  object. It contains the keyID, AlgorithmIdentifier, and the encrypted
     *  plaintext. It is compatible with the SDRResult created by NSS's
     *  SecretDecoderRing.
     */
    public byte[] encrypt(byte[] plaintext) throws
            CryptoManager.NotInitializedException,
            GeneralSecurityException,
            InvalidBERException
    {
        CryptoManager cm = CryptoManager.getInstance();

        CryptoToken savedToken = cm.getThreadToken();

        try {
            cm.setThreadToken(token);

            //
            // generate an IV
            //
            byte[] iv = new byte[alg.getIVLength()];
            SecureRandom rng = SecureRandom.getInstance(RNG_ALG,
                PROVIDER);
            rng.nextBytes(iv);
            IvParameterSpec ivSpec = new IvParameterSpec(iv);

            //
            // do the encryption
            //
            Cipher cipher = Cipher.getInstance(alg.toString(),PROVIDER);
            cipher.init(Cipher.ENCRYPT_MODE, key, ivSpec);
            byte[] paddedPtext = 
                org.mozilla.jss.crypto.Cipher.pad(
                    plaintext, alg.getBlockSize() );
            byte[] rawCtext = cipher.doFinal(paddedPtext);

            //
            // package the encrypted content and IV
            //
            Encoding encoding =
                new Encoding(keyID, iv, alg.toOID(), rawCtext);

            return ASN1Util.encode(encoding);

        } catch(IllegalStateException ise ) {
            throw new GeneralSecurityException(ise.toString());
        } finally {
            cm.setThreadToken(savedToken);
        }
    }
}
