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
 * An interface for secure random numbers. This should be replaced with
 * java.security.SecureRandom when we move to JDK 1.2.  In JDK 1.1,
 * SecureRandom is implemented by a Sun class. In JDK 1.2, it uses a
 * provider architecture.
 */
public interface JSSSecureRandom {

    /**
     * Seed the RNG with the given seed bytes.
     */
    public void setSeed(byte[] seed);

    /**
     * Seed the RNG with the eight bytes contained in <code>seed</code>.
     */
    public void setSeed(long seed);

    /**
     * Retrieves random bytes and stores them in the given array.
     */
    public void nextBytes(byte bytes[]);
}
