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

package org.mozilla.jss.pkix.cmmf;

import org.mozilla.jss.asn1.*;
import org.mozilla.jss.util.Assert;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class CertResponse implements ASN1Value {

    private INTEGER certReqId;
    private PKIStatusInfo status;
    private CertifiedKeyPair certifiedKeyPair;

    private CertResponse() { }

    public CertResponse(INTEGER certReqId, PKIStatusInfo status) {
        this.certReqId = certReqId;
        this.status = status;
    }

    public CertResponse(INTEGER certReqId, PKIStatusInfo status,
            CertifiedKeyPair certifiedKeyPair)
    {
        this(certReqId, status);
        this.certifiedKeyPair = certifiedKeyPair;
    }

    public INTEGER getCertReqId() {
        return certReqId;
    }

    public PKIStatusInfo getPKIStatusInfo() {
        return status;
    }

    /**
     * Returns <code>true</code> if the certifiedKeyPair field is present.
     */
    public boolean hasCertifiedKeyPair() {
        return (certifiedKeyPair != null);
    }

    /**
     * Returns the optional certified key pair. Should only be called if
     * the certifiedKeyPair field is present.
     */
    public CertifiedKeyPair getCertifiedKeyPair() {
        Assert._assert(certifiedKeyPair!=null);
        return certifiedKeyPair;
    }

    public static final Tag TAG = SEQUENCE.TAG;
    public Tag getTag() {
        return TAG;
    }

    public void encode(OutputStream ostream) throws IOException {
        encode(TAG, ostream);
    }

    public void encode(Tag implicitTag, OutputStream ostream)
        throws IOException
    {
        SEQUENCE seq = new SEQUENCE();
        seq.addElement( certReqId );
        seq.addElement( status );
        if( certifiedKeyPair != null ) {
            seq.addElement( certifiedKeyPair );
        }

        seq.encode(implicitTag, ostream);
    }
}
