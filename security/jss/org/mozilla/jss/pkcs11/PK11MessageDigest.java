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

package org.mozilla.jss.pkcs11;

import org.mozilla.jss.crypto.*;
import java.security.DigestException;
import java.security.NoSuchAlgorithmException;
import java.security.InvalidKeyException;

/**
 * Message Digesting with PKCS #11.
 */
public final class PK11MessageDigest extends JSSMessageDigest {

    private PK11Token token;
    private CipherContextProxy digestProxy;
    private PK11SymKey hmacKey;
    private DigestAlgorithm alg;

    PK11MessageDigest(PK11Token token, DigestAlgorithm alg)
        throws NoSuchAlgorithmException, DigestException
    {
        this.token = token;
        this.alg = alg;

        if( ! token.doesAlgorithm(alg) ) {
            throw new NoSuchAlgorithmException();
        }

        reset();
    }

    public void initHMAC(SymmetricKey key)
        throws DigestException, InvalidKeyException
    {

        if( ! (alg instanceof HMACAlgorithm) ) {
            throw new DigestException("Digest is not an HMAC digest");
        }

        reset();

        if( ! (key instanceof PK11SymKey) ) {
            throw new InvalidKeyException("HMAC key is not a PKCS #11 key");
        }

        hmacKey = (PK11SymKey) key;

        if( ! key.getOwningToken().equals(token) ) {
            hmacKey = null;
            throw new InvalidKeyException(
                "HMAC key does not live on the same token as this digest");
        }

        this.digestProxy = initHMAC(token, alg, hmacKey);
    }

    public void update(byte[] input, int offset, int len)
        throws DigestException
    {
        if( digestProxy == null ) {
            throw new DigestException("Digest not correctly initialized");
        }
        if( input.length < offset+len ) {
            throw new IllegalArgumentException(
                "Input buffer is not large enough for offset and length");
        }

        update(digestProxy, input, offset, len);
    }

    public int digest(byte[] outbuf, int offset, int len)
        throws DigestException
    {
        if( digestProxy == null ) {
            throw new DigestException("Digest not correctly initialized");
        }
        if( outbuf.length < offset+len ) {
            throw new IllegalArgumentException(
                "Output buffer is not large enough for offset and length");
        }

        int retval = digest(digestProxy, outbuf, offset, len);

        reset();

        return retval;
    }

    public void reset() throws DigestException {
        if( ! (alg instanceof HMACAlgorithm) ) {
            // This is a regular digest, so we have enough information
            // to inialize the context
            this.digestProxy = initDigest(alg);
        } else if( hmacKey != null ) {
            // This is an HMAC digest, and we have a key
            this.digestProxy = initHMAC(token, alg, hmacKey);
        } else {
            // this is an HMAC digest for which we don't have the key yet,
            // we have to wait to construct the context
            this.digestProxy = null;
        }
    }

    public DigestAlgorithm getAlgorithm() {
        return alg;
    }

    private static native CipherContextProxy
    initDigest(DigestAlgorithm alg)
        throws DigestException;

    private static native CipherContextProxy
    initHMAC(PK11Token token, DigestAlgorithm alg, PK11SymKey key)
        throws DigestException;

    private static native void
    update(CipherContextProxy proxy, byte[] inbuf, int offset, int len);

    private static native int
    digest(CipherContextProxy proxy, byte[] outbuf, int offset, int len);

}
