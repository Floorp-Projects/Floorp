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

package com.netscape.jss.crypto;

/**
 *
 */
public class KeyWrapAlgorithm extends Algorithm {
    protected KeyWrapAlgorithm(int oidTag, String name, Class paramClass,
        boolean padded) {
        super(oidTag, name);
        parameterClass = paramClass;
        this.padded = padded;
    }

    private Class parameterClass;
    private boolean padded;

    /**
     * The type of parameter that this algorithm expects.  Returns
     *   <code>null</code> if this algorithm does not take any parameters.
     */
    public Class getParameterClass() {
        return parameterClass;
    }

    public boolean isPadded() {
        return padded;
    }

    public static final KeyWrapAlgorithm
    DES_ECB = new KeyWrapAlgorithm(SEC_OID_DES_ECB, "DES/ECB", null, false);

    public static final KeyWrapAlgorithm
    DES_CBC = new KeyWrapAlgorithm(SEC_OID_DES_CBC, "DES/CBC",
                        IVParameterSpec.class, false);

    public static final KeyWrapAlgorithm
    DES_CBC_PAD = new KeyWrapAlgorithm(CKM_DES_CBC_PAD, "DES/CBC/Pad",
                        IVParameterSpec.class, true);

    public static final KeyWrapAlgorithm
    DES3_ECB = new KeyWrapAlgorithm(CKM_DES3_ECB, "DES3/ECB", null, false);

    public static final KeyWrapAlgorithm
    DES3_CBC = new KeyWrapAlgorithm(SEC_OID_DES_EDE3_CBC, "DES3/CBC",
                        IVParameterSpec.class, false);

    public static final KeyWrapAlgorithm
    DES3_CBC_PAD = new KeyWrapAlgorithm(CKM_DES3_CBC_PAD, "DES3/CBC/Pad",
                        IVParameterSpec.class, true);

    public static final KeyWrapAlgorithm
    RSA = new KeyWrapAlgorithm(SEC_OID_PKCS1_RSA_ENCRYPTION, "RSA", null,
            false);

    ///////////////////////////////////////////////////////////////////////
	// Export control code
    ///////////////////////////////////////////////////////////////////////

    private static final int NUMBER_OF_ALGORITHMS = 7;

	public KeyWrapAlgorithm[] getAllAlgorithms( Usage usage ) {
		KeyWrapAlgorithm[] algs = new KeyWrapAlgorithm[NUMBER_OF_ALGORITHMS];

		long[] indices = getAllAlgorithmIndices( usage );

		int j = 0;
		for( int i = 0; i <= indices.length; i++ ) {
			switch( ( int ) indices[i] ) {
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
				case SEC_OID_PKCS1_RSA_ENCRYPTION:
					algs[j] = RSA;
					j++;
					break;
				default:
					continue;
			}
		}

		return algs;
	}
}
