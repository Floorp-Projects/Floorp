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

package org.mozilla.jss.crypto;

import org.mozilla.jss.util.*;
import java.security.*;
import java.security.cert.CertificateEncodingException;
import java.io.Serializable;

/**
 * This is an interface for a permanent repository of cryptographic objects,
 * such as keys, certs, and passwords.
 */
public interface CryptoStore {

    ////////////////////////////////////////////////////////////
    // Private Keys
    ////////////////////////////////////////////////////////////

    /**
     * Imports a raw private key into this token.
     *
     * @param key The private key.
     * @exception TokenException If the key cannot be imported to this token.
     * @exception KeyAlreadyImportedException If the key already exists on this token.
     */
    public void
    importPrivateKey(  byte[] key,
                       PrivateKey.Type type       )
        throws TokenException, KeyAlreadyImportedException;


    /**
     * Returns all private keys stored on this token.
     *
     * @return An array of all private keys stored on this token.
     * @exception TokenException If an error occurs on the token while
     *      gathering the keys.
     */
    public PrivateKey[]
    getPrivateKeys() throws TokenException;

    /**
     * Deletes the given PrivateKey from the CryptoToken.
     * This is a very dangerous call: it deletes the key from the underlying
     * token. After calling this, the PrivateKey passed in must no longer
     * be used, or a TokenException will occur.
     *
     * @param key A PrivateKey to be permanently deleted.  It must reside
     *      on this token.
     * @exception NoSuchItemOnTokenException If the given privae key does 
     *      not reside on this token.
     * @exception TokenException If an error occurs on the token while
     *      deleting the key.
     */
    public void deletePrivateKey(org.mozilla.jss.crypto.PrivateKey key)
        throws NoSuchItemOnTokenException, TokenException;


    public byte[] getEncryptedPrivateKeyInfo(X509Certificate cert,
        PBEAlgorithm pbeAlg, Password pw, int iteration);

    ////////////////////////////////////////////////////////////
    // Certs
    ////////////////////////////////////////////////////////////
    /**
     * Returns all user certificates stored on this token. A user certificate
     *      is one that has a matching private key.
     *
     * @return An array of all user certificates present on this token.
     * @exception TokenException If an error occurs on the token while
     *      gathering the certificates.
     */
    public X509Certificate[]
    getCertificates() throws TokenException;

    /**
     * Deletes a certificate from a token.
     *
     * @param cert A certificate to be deleted from this token. The cert
     *      must actually reside on this token.
     * @exception NoSuchItemOnTokenException If the given cert does not
     *      reside on this token.
     * @exception TokenException If an error occurred on the token while
     *      deleting the certificate.
     */
    public void deleteCert(X509Certificate cert)
        throws NoSuchItemOnTokenException, TokenException;
}
