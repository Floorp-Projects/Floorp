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

import java.security.DigestException;
import java.security.InvalidKeyException;

/**
 * A class for performing message digesting (hashing) and MAC operations.
 * @deprecated Use the JCA interface instead ({@link java.security.MessageDigest})
 */
public abstract class JSSMessageDigest {

    /**
     * Initializes an HMAC digest with the given symmetric key. This also
     *  has the effect of resetting the digest.
     *
     * @exception DigestException If this algorithm is not an HMAC algorithm.
     * @exception InvalidKeyException If the given key is not valid.
     */
    public abstract void initHMAC(SymmetricKey key)
        throws DigestException, InvalidKeyException;

    /**
     * Updates the digest with a single byte of input.
     */
    public void update(byte input) throws DigestException {
        byte[] in = { input };
        update(in, 0, 1);
    }

    /**
     * Updates the digest with a portion of an array.
     *
     * @param input An array from which to update the digest.
     * @param offset The index in the array at which to start digesting.
     * @param len The number of bytes to digest.
     * @exception DigestException If an error occurs while digesting.
     */
    public abstract void update(byte[] input, int offset, int len)
        throws DigestException;

    /**
     * Updates the digest with an array.
     *
     * @param input An array to feed to the digest.
     * @exception DigestException If an error occurs while digesting.
     */
    public void update(byte[] input) throws DigestException {
        update(input, 0, input.length);
    }

    /**
     * Completes digestion.
     * 
     * @return The, ahem, output of the digest operation.
     * @param If an error occurs while digesting.
     */
    public byte[] digest() throws DigestException {
        byte[] output = new byte[getOutputSize()];
        digest(output, 0, output.length);
        return output;
    }

    /**
     * Completes digesting, storing the result into the provided array.
     *
     * @param buf The buffer in which to place the digest output.
     * @param offset The offset in the buffer at which to store the output.
     * @param len The amount of space available in the buffer for the
     *      digest output.
     * @return The number of bytes actually stored into buf.
     * @exception DigestException If the provided space is too small for
     *      the digest, or an error occurs with the digest.
     */
    public abstract int digest(byte[] buf, int offset, int len)
        throws DigestException;

    /**
     * Provides final data to the digest, then completes it and returns the
     * output.
     *
     * @param input The digest's last meal.
     * @return The completed digest.
     * @exception DigestException If an error occurs while digesting.
     */
    public byte[] digest(byte[] input) throws DigestException {
        update(input);
        return digest();
    }

    /**
     * Resets this digest for further use.  This clears all input and
     * output streams. If this is an HMAC digest, the HMAC key is not
     * cleared.
     */
    public abstract void reset() throws DigestException;

    /**
     * Returns the algorithm that this digest uses.
     */
    public abstract DigestAlgorithm getAlgorithm();

    /**
     * Returns the length of the digest created by this digest's
     * digest algorithm.
     *
     * @return The size in bytes of the output of this digest.
     */
    public int getOutputSize() {
        return getAlgorithm().getOutputSize();
    }
}
