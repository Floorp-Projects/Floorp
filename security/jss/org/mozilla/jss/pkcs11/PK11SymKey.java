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

import org.mozilla.jss.crypto.*;
import org.mozilla.jss.util.Assert;

public final class PK11SymKey implements SymmetricKey {

    protected PK11SymKey(byte[] pointer) {
        Assert._assert(pointer!=null);
        keyProxy  = new SymKeyProxy(pointer);
    }

    private SymKeyProxy keyProxy;

    public SymmetricKey.Type getType() {
        KeyType kt = getKeyType();
        if(kt == KeyType.DES) {
            return DES;
        } else if(kt == KeyType.DES3) {
            return DES3;
        } else if(kt == KeyType.RC4) {
            return RC4;
        } else if(kt == KeyType.RC2) {
            return RC2;
        } else {
            Assert.notReached("Unrecognized key type");
            return DES;
        }
    }

    public native CryptoToken getOwningToken();

    /**
     * Returns key strength, measured as the number of bits of secret material.
     * <b>NOTE:</b> Due to a bug in the security library (333440), this
     *  may return a wrong answer for PBE keys that have embedded parity
     *  (like DES).  A DES key is 56 bits of information plus
     *  8 bits of parity, so it takes up 64 bits.  For a normal DES key,
     * this method will correctly return 56, but for a PBE-generated DES key,
     * the security library bug causes it to return 64.
     */
    public native int getStrength();


    /**
     * Returns the length of the key in bytes, as returned by
     * PK11_GetKeyLength().
     */
    public native int getLength();

    public native byte[] getKeyData()
        throws SymmetricKey.NotExtractableException;

    public native KeyType getKeyType();

    public String getAlgorithm() {
        return getKeyType().toString();
    }

    public byte[] getEncoded() {
        try {
            return getKeyData();
        } catch(SymmetricKey.NotExtractableException nee) {
            return null;
        }
    }

    public String getFormat() {
        return "RAW";
    }
}

class SymKeyProxy extends KeyProxy {

    public SymKeyProxy(byte[] pointer) {
        super(pointer);
    }

    protected native void releaseNativeResources();

    protected void finalize() throws Throwable {
        super.finalize();
    }
}
