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

import org.mozilla.jss.crypto.*;
import org.mozilla.jss.util.*;
import java.security.*;
import java.security.cert.*;
import java.util.*;
import java.math.BigInteger;

/**
 * A certificate that lives in the internal cert database.
 */
public class PK11InternalCert extends PK11Cert
        implements InternalCertificate
{
    ///////////////////////////////////////////////////////////////////////
    // Trust Management.  This is all package stuff because it can only
    // be called from PK11InternalCert.
    ///////////////////////////////////////////////////////////////////////
    public static final int SSL = 0;
    public static final int EMAIL = 1;
    public static final int OBJECT_SIGNING=2;

    /**
     * Set the SSL trust flags for this certificate.
     *
     * @param trust A bitwise OR of the trust flags VALID_PEER, VALID_CA,
     *      TRUSTED_CA, USER, and TRUSTED_CLIENT_CA.
     */
    public void setSSLTrust(int trust)
    {
        super.setTrust(SSL, trust);
    }

    /**
     * Set the email (S/MIME) trust flags for this certificate.
     *
     * @param trust A bitwise OR of the trust flags VALID_PEER, VALID_CA,
     *      TRUSTED_CA, USER, and TRUSTED_CLIENT_CA.
     */
    public void setEmailTrust(int trust)
    {
        super.setTrust(EMAIL, trust);
    }

    /**
     * Set the object signing trust flags for this certificate.
     *
     * @param trust A bitwise OR of the trust flags VALID_PEER, VALID_CA,
     *      TRUSTED_CA, USER, and TRUSTED_CLIENT_CA.
     */
    public void setObjectSigningTrust(int trust)
    {
        super.setTrust(OBJECT_SIGNING, trust);
    }

    /**
     * Get the SSL trust flags for this certificate.
     *
     * @return A bitwise OR of the trust flags VALID_PEER, VALID_CA,
     *      TRUSTED_CA, USER, and TRUSTED_CLIENT_CA.
     */
    public int getSSLTrust()
    {
        return super.getTrust(SSL);
    }

    /**
     * Get the email (S/MIME) trust flags for this certificate.
     *
     * @return A bitwise OR of the trust flags VALID_PEER, VALID_CA,
     *      TRUSTED_CA, USER, and TRUSTED_CLIENT_CA.
     */
    public int getEmailTrust()
    {
        return super.getTrust(EMAIL);
    }

    /**
     * Get the object signing trust flags for this certificate.
     *
     * @return A bitwise OR of the trust flags VALID_PEER, VALID_CA,
     *      TRUSTED_CA, USER, and TRUSTED_CLIENT_CA.
     */
    public int getObjectSigningTrust()
    {
        return super.getTrust(OBJECT_SIGNING);
    }

	/////////////////////////////////////////////////////////////
	// Construction
	/////////////////////////////////////////////////////////////
    PK11InternalCert(byte[] certPtr, byte[] slotPtr) {
        super(certPtr, slotPtr);
    }
}
