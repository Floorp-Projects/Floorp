/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Security Services for Java.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

package org.mozilla.jss.pkcs11;

import org.mozilla.jss.crypto.Algorithm;
import org.mozilla.jss.crypto.PrivateKey;
import org.mozilla.jss.crypto.CryptoToken;
import org.mozilla.jss.crypto.TokenException;
import org.mozilla.jss.util.*;

final class PK11PrivKey extends org.mozilla.jss.pkcs11.PK11Key
	implements PrivateKey {

    protected PK11PrivKey(byte[] pointer) {
        Assert.assert(pointer!=null);
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
            Assert.assert(kt == KeyType.DSA);
            return PrivateKey.Type.DSA;
        }
    }

    /**
     * Returns the size in bits of the modulus of an RSA Private key.
     * Returns -1 for other types of keys.
     */
    public native int getStrength();
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
