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

import org.mozilla.jss.util.Assert;
import java.util.Enumeration;
import java.util.Vector;

public final class PK11Module {

    private PK11Module() {
        Assert.notReached("PK11Module default constructor");
    }

    /**
     * This constructor should only be called from native code.
     */
    private PK11Module(byte[] pointer) {
        Assert._assert(pointer!=null);
        moduleProxy = new ModuleProxy(pointer);
        reloadTokens();
    }

    /**
     * Returns the common name of this module.
     */
    public native String getName();

    /**
     * Returns the name of the shared library implementing this module.
     */
    public native String getLibraryName();

    /**
     * Get the CryptoTokens provided by this module.
     *
     * @return An enumeration of CryptoTokens that come from this module.
     */
    public synchronized Enumeration getTokens() {
        return tokenVector.elements();
    }

    /**
     * Re-load the list of this module's tokens. This function is private
     * to JSS.
     */
    public synchronized void reloadTokens() {
        tokenVector = new Vector();
        putTokensInVector(tokenVector);
    }

    private native void putTokensInVector(Vector tokens);

    private Vector tokenVector;
    private ModuleProxy moduleProxy;
}
