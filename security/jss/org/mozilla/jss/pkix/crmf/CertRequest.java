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
package org.mozilla.jss.pkix.crmf;

import java.util.Date;
import org.mozilla.jss.asn1.*;
import org.mozilla.jss.util.Assert;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import org.mozilla.jss.pkix.primitive.*;

/**
 * A PKIX <i>CertRequest</i>.  Currently can only be decoded from its BER
 *  encoding. There are no methods for constructing one.
 */
public class CertRequest implements ASN1Value {

    private INTEGER certReqId;
    private CertTemplate certTemplate;
    private SEQUENCE controls; // may be null

    private CertRequest() { }

    /**
     * @param certReqId May NOT be null.
     * @param certTemplate May NOT be null.
     * @param controls May be null.
     */
    public CertRequest(INTEGER certReqId, CertTemplate certTemplate,
            SEQUENCE controls)
    {
        if( certReqId == null ) {
            throw new NullPointerException("certReqId is null");
        }
        this.certReqId = certReqId;
        if( certTemplate == null ) {
            throw new NullPointerException("certTemplate is null");
        }
        this.certTemplate = certTemplate;
        this.controls = controls;
    }

    /**
     * Returns the <i>certReqId</i> (certificate request ID) field.
     */
    public INTEGER getCertReqId() {
        return certReqId;
    }

    /**
     * Returns the <i>CertTemplate</i> field.
     */
    public CertTemplate getCertTemplate() {
        return certTemplate;
    }

    /**
     * Returns the number of optional Controls in the cert request.
     * The number may be zero.
     */
    public int numControls() {
        if(controls == null) {
            return 0;
        } else {
            return controls.size();
        }

    }

    /**
     * Returns the <i>i</i>th Control.  <code>i</code> must be in the
     * range [0..numControls-1].
     */
    public AVA controlAt(int i) {
        if( controls == null ) {
            throw new ArrayIndexOutOfBoundsException();
        }
        return (AVA) controls.elementAt(i);
    }

    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////
    // DER-encoding
    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////

    public static final Tag TAG = SEQUENCE.TAG;
    public Tag getTag() {
        return TAG;
    }

    /**
     * This method is not yet supported.
     */
    public void encode(OutputStream ostream) throws IOException {
        //Assert.notYetImplemented("CertRequest encoding");
        encode(getTag(),ostream);
    }

    /**
     * This method is not yet supported.
     */
    public void encode(Tag implicit, OutputStream ostream) throws IOException {
        //Assert.notYetImplemented("CertRequest encoding");
        SEQUENCE sequence = new SEQUENCE();

        sequence.addElement( certReqId );
        sequence.addElement( certTemplate );
		if (controls != null)
			sequence.addElement( controls );

        sequence.encode(implicit,ostream);
    }

    /**
     * A Template class for constructing <i>CertRequest</i>s from their
     * BER encoding.
     */
    public static class Template implements ASN1Template {
        private SEQUENCE.Template seqTemplate;

        public Template() {
            seqTemplate = new SEQUENCE.Template();
            seqTemplate.addElement( new INTEGER.Template() );
            seqTemplate.addElement( new CertTemplate.Template() );
            seqTemplate.addOptionalElement( new
                SEQUENCE.OF_Template( new AVA.Template() ));
        }

        public boolean tagMatch( Tag tag ) {
            return TAG.equals(tag);
        }

        public ASN1Value decode(InputStream istream)
            throws IOException, InvalidBERException
        {
            return decode(TAG, istream);
        }

        public ASN1Value decode(Tag implicit, InputStream istream)
            throws IOException, InvalidBERException
        {

            SEQUENCE seq = (SEQUENCE) seqTemplate.decode(implicit, istream);
            return new CertRequest(
                    (INTEGER) seq.elementAt(0),
                    (CertTemplate) seq.elementAt(1),
                    (SEQUENCE) seq.elementAt(2) );
        }
    }
}
