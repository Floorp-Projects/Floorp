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

import java.security.NoSuchAlgorithmException;
import java.util.Hashtable;

/**
 * Algorithms that can be used for keypair generation.
 */
public class KeyPairAlgorithm extends Algorithm {

    protected KeyPairAlgorithm(int oidIndex, String name, Algorithm algFamily) {
        super(oidIndex, name);
        this.algFamily = algFamily;
        nameMap.put(name, this);
    }

    /**
     * Returns the algorithm family for a given key pair generation algorithm.
     * If a token supports a family and is writable, we can do keypair gen
     * on the token even if it doesn't support the keypair gen algorithm.
     * We do this by doing the keypair gen on the internal module and then
     * moving the key out to the other token.
     */
    public Algorithm
    getAlgFamily()
    {
        return algFamily;
    }

    private static Hashtable nameMap = new Hashtable();

    /**
     * Looks up a key pair generation algorithm from its name. The names
     * are those specified in the JCA spec. For example, "RSA" and "DSA".
     *
     * @throws NoSuchAlgorithmException If the name of the algorithm is not
     *  recognized as a supported algorithm.
     */
    public static KeyPairAlgorithm fromString(String algName)
        throws NoSuchAlgorithmException
    {
        KeyPairAlgorithm alg = (KeyPairAlgorithm)nameMap.get(algName);
        if( alg == null ) {
            throw new NoSuchAlgorithmException();
        }
        return alg;
    }

    protected Algorithm algFamily;

    ////////////////////////////////////////////////////////////////
    // Key-Pair Generation Algorithms
    ////////////////////////////////////////////////////////////////
    public static final Algorithm
    RSAFamily = new Algorithm(SEC_OID_PKCS1_RSA_ENCRYPTION, "RSA");

    public static final Algorithm
    DSAFamily = new Algorithm(SEC_OID_ANSIX9_DSA_SIGNATURE, "DSA");

    public static final KeyPairAlgorithm
    RSA = new KeyPairAlgorithm(CKM_RSA_PKCS_KEY_PAIR_GEN, "RSA", RSAFamily);

    public static final KeyPairAlgorithm
    DSA = new KeyPairAlgorithm(CKM_DSA_KEY_PAIR_GEN, "DSA", DSAFamily);
}
