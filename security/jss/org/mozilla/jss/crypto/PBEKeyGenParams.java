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

import java.security.spec.AlgorithmParameterSpec;
import java.security.spec.KeySpec;
import org.mozilla.jss.util.Password;

public class PBEKeyGenParams implements AlgorithmParameterSpec, KeySpec {

    private Password pass;
    private byte[] salt;
    private int iterations;

    private PBEKeyGenParams() { }

    static private final int DEFAULT_SALT_LENGTH = 8;
    static private final int DEFAULT_ITERATIONS = 1;

    /**
     * Creates PBE parameters.
     *
     * @param pass The password. It will be cloned, so the
     *      caller is still responsible for clearing it. It must not be null.
     * @param salt The salt for the PBE algorithm. Will <b>not</b> be cloned.
     *      Must not be null. It is the responsibility of the caller to
     *      use the right salt length for the algorithm. Most algorithms
     *      use 8 bytes of salt.
     * @param The iteration count for the PBE algorithm.
     */
    public PBEKeyGenParams(Password pass, byte[] salt, int iterations) {
        if(pass==null || salt==null) {
            throw new NullPointerException();
        }
        this.pass = (Password) pass.clone();
        this.salt = salt;
        this.iterations = iterations;
    }

    /**
     * Creates PBE parameters.
     *
     * @param pass The password. It will be cloned, so the
     *      caller is still responsible for clearing it. It must not be null.
     * @param salt The salt for the PBE algorithm. Will <b>not</b> be cloned.
     *      Must not be null. It is the responsibility of the caller to
     *      use the right salt length for the algorithm. Most algorithms
     *      use 8 bytes of salt.
     * @param The iteration count for the PBE algorithm.
     */
    public PBEKeyGenParams(char[] pass, byte[] salt, int iterations) {
        if(pass==null || salt==null) {
            throw new NullPointerException();
        }
        this.pass = new Password( (char[]) pass.clone() );
        this.salt = salt;
        this.iterations = iterations;
    }

    /**
     * Returns a <b>reference</b> to the password, not a copy.
     */
    public Password getPassword() {
        return pass;
    }

    /**
     * Returns a <b>reference</b> to the salt.
     */
    public byte[] getSalt() {
        return salt;
    }

    /**
     * Returns the iteration count.
     */
    public int getIterations() {
        return iterations;
    }

    /**
     * Clears the password. This should be called when this object is no
     * longer needed so the password is not left around in memory.
     */
    public void clear() {
        pass.clear();
    }

    protected void finalize() throws Throwable {
        pass.clear();
    }
}
