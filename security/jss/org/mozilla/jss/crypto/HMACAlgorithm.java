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

import java.util.Hashtable;
import org.mozilla.jss.asn1.*;
import java.security.NoSuchAlgorithmException;

/**
 * Algorithms for performing HMACs. These can be used to create
 * MessageDigests.
 */
public class HMACAlgorithm extends DigestAlgorithm {

    protected HMACAlgorithm(int oidIndex, String name, OBJECT_IDENTIFIER oid,
                int outputSize) {
        super(oidIndex, name, oid, outputSize);

        if( oid!=null && oidMap.get(oid)==null) {
            oidMap.put(oid, this);
        }
    }

    ///////////////////////////////////////////////////////////////////////
    // OID mapping
    ///////////////////////////////////////////////////////////////////////
    private static Hashtable oidMap = new Hashtable();

    /**
     * Looks up the HMAC algorithm with the given OID.
     * 
     * @exception NoSuchAlgorithmException If no registered HMAC algorithm
     *  has the given OID.
     */
    public static DigestAlgorithm fromOID(OBJECT_IDENTIFIER oid)
        throws NoSuchAlgorithmException
    {
        Object alg = oidMap.get(oid);
        if( alg == null ) {
            throw new NoSuchAlgorithmException();
        } else {
            return (HMACAlgorithm) alg;
        }
    }

    /**
     * SHA-1 HMAC.  This is a Message Authentication Code that uses a
     * symmetric key together with SHA-1 digesting to create a form of
     * signature.
     */
    public static final HMACAlgorithm SHA1 = new HMACAlgorithm
        (CKM_SHA_1_HMAC, "SHA-1-HMAC",
             OBJECT_IDENTIFIER.ALGORITHM.subBranch(26), 20);
}
