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
import org.mozilla.jss.asn1.*;

/**
 * An algorithm for performing symmetric encryption.
 */
public class EncryptionAlgorithm extends Algorithm {
    protected EncryptionAlgorithm(int oidTag, String name, Class paramClass,
        int blockSize, boolean padded, OBJECT_IDENTIFIER oid)
    {
        super(oidTag, name, oid, paramClass);
        this.blockSize = blockSize;
        this.padded = padded;
        if(oid!=null) {
            oidMap.put(oid, this);
        }
    }

    private int blockSize;
    private boolean padded;

    ///////////////////////////////////////////////////////////////////////
    // OID mapping
    ///////////////////////////////////////////////////////////////////////
    private static Hashtable oidMap = new Hashtable();

    public static EncryptionAlgorithm fromOID(OBJECT_IDENTIFIER oid)
        throws NoSuchAlgorithmException
    {
        Object alg = oidMap.get(oid);
        if( alg == null ) {
            throw new NoSuchAlgorithmException();
        } else {
            return (EncryptionAlgorithm) alg;
        }
    }

    /** 
     * The blocksize of the algorithm in bytes. Stream algorithms (such as
     * RC4) have a blocksize of 1.
     */
    public int getBlockSize() {
        return blockSize;
    }

    /**
     * Returns <code>true</code> if this algorithm performs padding.
     */
    public boolean isPadded() {
        return padded;
    }

    /**
     * Returns the number of bytes that this algorithm expects in
     * its initialization vector.
     *
     * @return The size in bytes of the IV for this algorithm.  A size of
     *      0 means this algorithm does not take an IV.
     */
    public native int getIVLength();

    public static final EncryptionAlgorithm
    RC4 = new EncryptionAlgorithm(SEC_OID_RC4, "RC4", null, 1, false,
            OBJECT_IDENTIFIER.RSA_CIPHER.subBranch(4) );

    public static final EncryptionAlgorithm
    DES_ECB = new EncryptionAlgorithm(SEC_OID_DES_ECB, "DES/ECB", null,
                    8, false, OBJECT_IDENTIFIER.ALGORITHM.subBranch(6) );

    public static final EncryptionAlgorithm
    DES_CBC = new EncryptionAlgorithm(SEC_OID_DES_CBC, "DES/CBC",
        IVParameterSpec.class, 8, false,
        OBJECT_IDENTIFIER.ALGORITHM.subBranch(7) );

    public static final EncryptionAlgorithm
    DES_CBC_PAD = new EncryptionAlgorithm(CKM_DES_CBC_PAD, "DES/CBC/Pad",
        IVParameterSpec.class, 8, true, null); // no oid

    public static final EncryptionAlgorithm
    DES3_ECB = new EncryptionAlgorithm(CKM_DES3_ECB, "DES3/ECB", null, 8,
                    false, null); // no oid

    public static final EncryptionAlgorithm
    DES3_CBC = new EncryptionAlgorithm(SEC_OID_DES_EDE3_CBC, "DES3/CBC",
        IVParameterSpec.class, 8, false,
        OBJECT_IDENTIFIER.RSA_CIPHER.subBranch(7) );

    public static final EncryptionAlgorithm
    DES3_CBC_PAD = new EncryptionAlgorithm(CKM_DES3_CBC_PAD, "DES3/CBC/Pad",
        IVParameterSpec.class, 8, true, null); // no oid

    public static final EncryptionAlgorithm
    RC2_CBC = new EncryptionAlgorithm(SEC_OID_RC2_CBC, "RC2/CBC",
        IVParameterSpec.class, 8, false,
        OBJECT_IDENTIFIER.RSA_CIPHER.subBranch(2) );

    ///////////////////////////////////////////////////////////////////////
	// Export control code
    ///////////////////////////////////////////////////////////////////////

    private static final int NUMBER_OF_ALGORITHMS = 8;

	public EncryptionAlgorithm[] getAllAlgorithms( Usage usage ) {
		EncryptionAlgorithm[] algs = new
									EncryptionAlgorithm[NUMBER_OF_ALGORITHMS];

		long[] indices = getAllAlgorithmIndices( usage );

		int j = 0;
		for( int i = 0; i <= indices.length; i++ ) {
			switch( ( int ) indices[i] ) {
				case SEC_OID_RC4:
					algs[j] = RC4;
					j++;
					break;
				case SEC_OID_DES_ECB:
					algs[j] = DES_ECB;
					j++;
					break;
				case SEC_OID_DES_CBC:
					algs[j] = DES_CBC;
					j++;
					break;
				case CKM_DES_CBC_PAD:
					algs[j] = DES_CBC_PAD;
					j++;
					break;
				case CKM_DES3_ECB:
					algs[j] = DES3_ECB;
					j++;
					break;
				case SEC_OID_DES_EDE3_CBC:
					algs[j] = DES3_CBC;
					j++;
					break;
				case CKM_DES3_CBC_PAD:
					algs[j] = DES3_CBC_PAD;
					j++;
					break;
				case SEC_OID_RC2_CBC:
					algs[j] = RC2_CBC;
					j++;
					break;
				default:
					continue;
			}
		}

		return algs;
	}
}
