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
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.
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

package org.mozilla.jss.crypto;

import java.io.UnsupportedEncodingException;

/**
 * This is a proprietary NSS interface. It is used for encrypting
 * data with a secret key stored in the NSS key database (which is in turn
 * protected with a password). It thus provides a quick, convenient way
 * to encrypt stuff your application wants to keep around for its own use:
 * for example, the list of web passwords stored in the web browser.
 *
 * <p>A dedicated key is used to encrypt all SecretDecoderRing data.
 * The same key is used for all SDR data, and not for any other data.
 * This key will be generated the first time it is needed.
 *
 * <p>The cipher used is DES3-EDE (Triple-DES) in CBC mode. The ciphertext
 * is DER-encoded in the following ASN.1 data structure:
 * <pre>
 *    SEQUENCE {
 *      keyid       OCTET STRING,
 *      alg         AlgorithmIdentifier,
 *      ciphertext  OCTET STRING }
 * </pre>
 *
 * <p>You must set the password on the Internal Key Storage Token
 *   (aka software token, key3.db) before you use the SecretDecoderRing.
 */
public class SecretDecoderRing {

    public static final String encodingFormat = "UTF-8";

    /**
     * Encrypts the given plaintext with the Secret Decoder Ring key stored
     * in the NSS key database.
     */
    public native byte[] encrypt(byte[] plaintext)
        throws TokenException;

    /**
     * Encrypts the given plaintext string with the Secret Decoder Ring key
     * stored in the NSS key database.
     */
    public byte[] encrypt(String plaintext) throws TokenException {
      try {
        return encrypt(plaintext.getBytes(encodingFormat));
      } catch(UnsupportedEncodingException e) {
        // this shouldn't happen, because we use a universally-supported
        // charset
        throw new RuntimeException(e.getMessage());
      }
    }

    /**
     * Decrypts the given ciphertext with the Secret Decoder Ring key stored
     * in the NSS key database.
     */
    public native byte[] decrypt(byte[] ciphertext)
        throws TokenException;

    /**
     * Decrypts the given ciphertext with the Secret Decoder Ring key stored
     * in the NSS key database, returning the original plaintext string.
     */
    public String decryptToString(byte[] ciphertext)
            throws TokenException {
      try {
        return new String(decrypt(ciphertext), encodingFormat);
      } catch(UnsupportedEncodingException e) {
        // this shouldn't happen, because we use a universally-supported
        // charset
        throw new RuntimeException(e.getMessage());
      }
    }
}
