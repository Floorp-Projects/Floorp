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
import org.mozilla.jss.crypto.PrivateKey;
import org.mozilla.jss.crypto.CryptoToken;
import org.mozilla.jss.crypto.TokenException;
import java.security.spec.PKCS8EncodedKeySpec;
import java.security.spec.DSAParameterSpec;
import java.security.interfaces.DSAParams;
import org.mozilla.jss.util.*;
import java.math.BigInteger;

public class PK11PrivKey extends org.mozilla.jss.pkcs11.PK11Key
	implements PrivateKey {

    private PK11PrivKey() { }

    protected PK11PrivKey(byte[] pointer) {
        Assert._assert(pointer!=null);
        keyProxy = new PrivateKeyProxy(pointer);
    }

	/**
	 * Make sure this key lives on the given token.
	 */
	public native void verifyKeyIsOnToken(PK11Token token)
		throws org.mozilla.jss.crypto.NoSuchItemOnTokenException;

    /**
     * Returns a new CryptoToken where this key resides.
     *
     * @return The PK11Token that owns this key.
     */
    public native CryptoToken getOwningToken();

    public native byte[] getUniqueID() throws TokenException;

    public native KeyType getKeyType();

    public PrivateKey.Type getType() {
        KeyType kt = getKeyType();

        if( kt == KeyType.RSA ) {
            return PrivateKey.Type.RSA;
        } else {
            Assert._assert(kt == KeyType.DSA);
            return PrivateKey.Type.DSA;
        }
    }

    public String getAlgorithm() {
        return getType().toString();
    }

    /**
     * Returns the size in bits of the modulus of an RSA Private key.
     * Returns -1 for other types of keys.
     */
    public native int getStrength();

    /**
     * Imports a PrivateKeyInfo, storing it as a temporary PrivateKey
     * on the given token.
     * The key will be a temporary (session) key until it is imported
     * into a KeyStore, at which point it will be made a permanent (token)
     * object.
     */
    public static PK11PrivKey
    fromPrivateKeyInfo(PKCS8EncodedKeySpec spec, CryptoToken token)
        throws TokenException
    {
        return fromPrivateKeyInfo(spec.getEncoded(), token);
    }

    /**
     * Imports a PrivateKeyInfo, storing it as a temporary PrivateKey
     * on the given token.
     * The key will be a temporary (session) key until it is imported
     * into a KeyStore, at which point it will be made a permanent (token)
     * object.
     */
    public static PK11PrivKey
    fromPrivateKeyInfo(byte[] pki, CryptoToken token) throws TokenException {
        return fromPrivateKeyInfo(pki, token, null);
    }

    /**
     * Imports a PrivateKeyInfo, storing it as a temporary PrivateKey
     * on the given token.
     * The key will be a temporary (session) key until it is imported
     * into a KeyStore, at which point it will be made a permanent (token)
     * object.
     * @param publicValue An encoding of the public key, as used by the NSS
     *  pk11wrap code. Don't use this unless you know what you're doing.
     */
    public static native PK11PrivKey
    fromPrivateKeyInfo(byte[] pki, CryptoToken token, byte[] publicValue)
        throws TokenException;

    protected DSAParameterSpec
    getDSAParams() throws TokenException {
        byte[][] pqgArray = getDSAParamsNative();

        return new DSAParameterSpec(
            new BigInteger(pqgArray[0]),
            new BigInteger(pqgArray[1]),
            new BigInteger(pqgArray[2])
        );
    }

    private native byte[][]
    getDSAParamsNative() throws TokenException;
    
        
}

class PrivateKeyProxy extends KeyProxy {

    public PrivateKeyProxy(byte[] pointer) {
        super(pointer);
    }

    protected native void releaseNativeResources();

    protected void finalize() throws Throwable {
        super.finalize();
		Debug.trace(Debug.OBNOXIOUS, "Finalizing a PrivateKeyProxy");
    }
}
