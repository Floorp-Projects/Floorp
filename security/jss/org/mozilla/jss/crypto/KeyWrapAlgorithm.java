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

package org.mozilla.jss.crypto;

import java.util.Hashtable;
import java.security.NoSuchAlgorithmException;

/**
 *
 */
public class KeyWrapAlgorithm extends Algorithm {
    protected KeyWrapAlgorithm(int oidTag, String name, Class paramClass,
        boolean padded, int blockSize) {
        super(oidTag, name, null, paramClass);
        this.padded = padded;
        this.blockSize = blockSize;
        if( name != null ) {
            nameMap.put(name.toLowerCase(), this);
        }
    }

    private boolean padded;
    private int blockSize;

    private static Hashtable nameMap = new Hashtable();

    public static KeyWrapAlgorithm fromString(String name)
            throws NoSuchAlgorithmException
    {
        Object alg = nameMap.get( name.toLowerCase() );
        if( alg == null ) {
            throw new NoSuchAlgorithmException();
        } else {
            return (KeyWrapAlgorithm) alg;
        }
    }

    public boolean isPadded() {
        return padded;
    }

    public int getBlockSize() {
        return blockSize;
    }

    public static final KeyWrapAlgorithm
    DES_ECB = new KeyWrapAlgorithm(SEC_OID_DES_ECB, "DES/ECB", null, false, 8);

    public static final KeyWrapAlgorithm
    DES_CBC = new KeyWrapAlgorithm(SEC_OID_DES_CBC, "DES/CBC",
                        IVParameterSpec.class, false, 8);

    public static final KeyWrapAlgorithm
    DES_CBC_PAD = new KeyWrapAlgorithm(CKM_DES_CBC_PAD, "DES/CBC/Pad",
                        IVParameterSpec.class, true, 8);

    public static final KeyWrapAlgorithm
    DES3_ECB = new KeyWrapAlgorithm(CKM_DES3_ECB, "DES3/ECB", null, false, 8);

    public static final KeyWrapAlgorithm
    DES3_CBC = new KeyWrapAlgorithm(SEC_OID_DES_EDE3_CBC, "DES3/CBC",
                        IVParameterSpec.class, false, 8);

    public static final KeyWrapAlgorithm
    DES3_CBC_PAD = new KeyWrapAlgorithm(CKM_DES3_CBC_PAD, "DES3/CBC/Pad",
                        IVParameterSpec.class, true, 8);

    public static final KeyWrapAlgorithm
    RSA = new KeyWrapAlgorithm(SEC_OID_PKCS1_RSA_ENCRYPTION, "RSA", null,
            false, 0);

    public static final KeyWrapAlgorithm
    PLAINTEXT = new KeyWrapAlgorithm(0, "Plaintext", null,
            false, 0);

    public static final KeyWrapAlgorithm
    AES_ECB = new KeyWrapAlgorithm(CKM_AES_ECB, "AES/ECB/NoPadding", null,
                    false, 16);

    public static final KeyWrapAlgorithm
    AES_CBC = new KeyWrapAlgorithm(CKM_AES_CBC, "AES/CBC/NoPadding",
                        IVParameterSpec.class, false, 16);

    public static final KeyWrapAlgorithm
    AES_CBC_PAD = new KeyWrapAlgorithm(CKM_AES_CBC_PAD, "AES/CBC/PKCS5Padding",
                        IVParameterSpec.class, true, 16);
}
