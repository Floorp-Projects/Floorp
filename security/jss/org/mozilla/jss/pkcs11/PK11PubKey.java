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

import org.mozilla.jss.crypto.Algorithm;
import org.mozilla.jss.util.*;
import org.mozilla.jss.crypto.PrivateKey;
import org.mozilla.jss.crypto.InvalidKeyFormatException;

public class PK11PubKey extends org.mozilla.jss.pkcs11.PK11Key
	implements java.security.PublicKey {

    protected PK11PubKey(byte[] pointer) {
        Assert._assert(pointer!=null);
        keyProxy = new PublicKeyProxy(pointer);
    }

	/**
	 * Make sure this key lives on the given token.
	 */
	public native void verifyKeyIsOnToken(PK11Token token)
		throws org.mozilla.jss.crypto.NoSuchItemOnTokenException;

    public native KeyType getKeyType();

    public String getAlgorithm() {
        return getKeyType().toString();
    }

    /**
     * Creates a PK11PubKey from its raw form. The raw form is a DER encoding
     * of the public key.  For example, this is what is stored in a
     * SubjectPublicKeyInfo.
     *
     * @param type The type of private key to be decoded.
     * @param rawKey The bytes of the raw key.
     * @exception InvalidKeyFormatException If the raw key could not be
     *      decoded.
     * @deprecated This method works for RSA keys but not DSA keys. Use
     *      fromSPKI() instead.
     * @see #fromSPKI(byte[])
     */
    public static PK11PubKey fromRaw(PrivateKey.Type type, byte[] rawKey)
        throws InvalidKeyFormatException
    {
        if( type == PrivateKey.Type.DSA ) {
            throw new InvalidKeyFormatException(
                "fromRaw() is broken for DSA keys. Use fromSPKI() instead.");
        }
        return fromRawNative( type.getPKCS11Type(), rawKey );
    }

    /**
     * param type The PKCS #11 type of the key (CKK_).
     */
    private static native PK11PubKey fromRawNative(int type, byte[] rawKey)
        throws InvalidKeyFormatException;

    /**
     * Creates a PK11PubKey from a SubjectPublicKeyInfo.
     *
     * @param spki The BER-encoded SubjectPublicKeyInfo.
     * @exception InvalidKeyFormatException If the SPKI could not be
     *      decoded.
     */
    public static native PK11PubKey fromSPKI(byte[] spki)
        throws InvalidKeyFormatException;

    /**
     * deprecated Use fromRawNative instead.
     */
    private static native PK11PubKey RSAFromRaw(byte[] rawKey);

    /**
     * deprecated Use fromRawNative instead.
     */
    private static native PK11PubKey DSAFromRaw(byte[] rawKey);

    /**
     * Returns a DER-encoded SubjectPublicKeyInfo representing this key.
     */
    public native byte[] getEncoded();

    /**
     *  The name of the primary encoding format of this key.  The primary
     *  encoding format is X.509 <i>SubjectPublicKeyInfo</i>, and the name
     *  is "X.509".
     */
    public String getFormat() {
        return "X.509";
    }
}

class PublicKeyProxy extends KeyProxy {

    public PublicKeyProxy(byte[] pointer) {
        super(pointer);
    }

    protected native void releaseNativeResources();

    protected void finalize() throws Throwable {
        super.finalize();
		Debug.trace(Debug.OBNOXIOUS, "Releasing a PublicKeyProxy");
    }
}
