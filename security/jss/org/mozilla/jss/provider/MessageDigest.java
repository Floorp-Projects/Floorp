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

package org.mozilla.jss.provider;

import org.mozilla.jss.crypto.*;
import org.mozilla.jss.util.Debug;
import org.mozilla.jss.util.Assert;
import java.security.DigestException;
import java.security.NoSuchAlgorithmException;

/**
 * A JCA provider of message digesting implemented with
 * Netscape Security Services. One big problem is the Sun interface doesn't
 * thrown any exceptions, so I can't thrown any exceptions, even though
 * lots can go wrong.
 */
abstract class MessageDigest extends java.security.MessageDigest {

    private org.mozilla.jss.crypto.JSSMessageDigest md;

    /**
     * Returns the particular algorithm that the subclass is implementing.
     */
    protected abstract DigestAlgorithm getAlg();

    /**
     * Creates a JSS MessageDigest provider object.
     */
    protected MessageDigest(String algName) {
        super( algName );

      try {
        Debug.trace(Debug.OBNOXIOUS, "In JSS JCA message digest constructor");
        CryptoToken internal = TokenSupplierManager.getTokenSupplier().
                getInternalCryptoToken();
        md = internal.getDigestContext( getAlg() );
      } catch( java.security.NoSuchAlgorithmException e ) {
        Debug.trace(Debug.ERROR, "Unknown message digesting algorithm: "+
                algName);
        Assert.notReached("Unknown message digesting algorithm");
      } catch( DigestException e ) {
        Debug.trace(Debug.ERROR, "Digest Exception while creating "+
                algName + " digest" );
        Assert.notReached("Digest exception creating message digest");
      }
    }

    protected void engineUpdate(byte input) {
      try {
        Debug.trace(Debug.OBNOXIOUS, "In JSS JCA message digest engineUpdate");
        md.update(input);
      } catch( DigestException e ) {
        Debug.trace(Debug.ERROR, "Digest Exception while updating digest");
        Assert.notReached("Digest exception while updating digest");
      }
    }

    protected void engineUpdate(byte[] input, int offset, int len) {
      try {

        Debug.trace(Debug.OBNOXIOUS, "In JSS JCA message digest engineUpdate");
        md.update(input, offset, len);

      } catch( DigestException e ) {
        Debug.trace(Debug.ERROR, "Digest Exception while updating digest");
        Assert.notReached("Digest exception while updating digest");
      }
    }

    protected byte[] engineDigest() {
      try {

        Debug.trace(Debug.OBNOXIOUS, "In JSS JCA message digest engineDigest");
        return md.digest();

      } catch( DigestException e ) {
        Debug.trace(Debug.ERROR, "Digest Exception while finalizing digest");
        Assert.notReached("Digest exception while finalizing digest");
        return null;
      }
    }

    protected void engineReset() {
      try {

        Debug.trace(Debug.OBNOXIOUS, "In JSS JCA message digest engineReset");
        md.reset();

      } catch( DigestException e ) {
        Debug.trace(Debug.ERROR, "Digest Exception while resetting digest");
        Assert.notReached("Digest exception while resetting digest");
      }
    }

    /**
     * Throws CloneNotSupportedException, because this implementation is
     * not clonable.
     */
    public Object clone() throws CloneNotSupportedException {
        throw new CloneNotSupportedException();
    }
}
