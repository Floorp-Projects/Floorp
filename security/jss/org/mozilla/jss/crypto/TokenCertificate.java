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
 * An X509 Certificate that lives on a PKCS #11 token.
 * Many of the X509Certificates returned by JSS calls are actually
 * TokenCertificates.
 * To find out if an X509Certificate is a TokenCertificate, use 
 *  <code>instanceof</code>.
 */
public interface TokenCertificate extends X509Certificate {

    /**
     * Returns the unique ID of this key.  Unique IDs can be used to match
     * certificates to keys.
     *
     * @see com.netscape.jss.crypto.PrivateKey#getUniqueID
     */
    public abstract byte[] getUniqueID();

    /**
     * Returns the CryptoToken that owns this certificate. Cryptographic
     * operations with this key may only be performed on the token that
     * owns the key.
     */
    public abstract CryptoToken getOwningToken();
}
