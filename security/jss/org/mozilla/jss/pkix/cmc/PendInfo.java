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

package org.mozilla.jss.pkix.cmc;

import org.mozilla.jss.asn1.*;
import java.io.*;
import org.mozilla.jss.util.Assert;
import java.util.Date;

/**
 * CMC <i>PendInfo</i>:
 * <pre>
 *   PendInfo ::= SEQUENCE { 
 *       pendToken            OCTET STRING, 
 *       pendTime             GeneralizedTime 
 *   } 
 * </pre>
 */
public class PendInfo implements ASN1Value {
    ///////////////////////////////////////////////////////////////////////
    // Members
    ///////////////////////////////////////////////////////////////////////
    private SEQUENCE sequence;
    private OCTET_STRING pendToken;
	private GeneralizedTime pendTime; 

    ///////////////////////////////////////////////////////////////////////
    // Construction
    ///////////////////////////////////////////////////////////////////////

    // no default constructor
    private PendInfo() { }

    /** 
     * Constructs a PendInfo from its components.
     *
     * @param pendToken the identifier.
     * @param pendTime the suggested time for the client to query the status.
     */
    public PendInfo(OCTET_STRING pendToken, GeneralizedTime pendTime) {
        sequence = new SEQUENCE();
        this.pendToken = pendToken;
        sequence.addElement(pendToken);
        this.pendTime = pendTime;
        sequence.addElement(pendTime);
	}

    /** 
     * Constructs a PendInfo from requestId and date.
     *
     * @param reqId the request Id
     * @param date the suggested time for the client to query the status.
     */
    public PendInfo(String reqId, Date date) {
        sequence = new SEQUENCE();
        this.pendToken = new OCTET_STRING(reqId.getBytes());
        sequence.addElement(new OCTET_STRING(reqId.getBytes()));
        this.pendTime = new GeneralizedTime(date);
        sequence.addElement(new GeneralizedTime(date));
	}
    ///////////////////////////////////////////////////////////////////////
    // accessors
    ///////////////////////////////////////////////////////////////////////

    public GeneralizedTime getPendTime() {
        return pendTime;
    }

    public OCTET_STRING getPendToken() {
        return pendToken;
    }

    ///////////////////////////////////////////////////////////////////////
    // DER encoding/decoding
    ///////////////////////////////////////////////////////////////////////
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
        sequence.encode(implicitTag, ostream);
    }

    private static final Template templateInstance = new Template();
    public static Template getTemplate() {
        return templateInstance;
    }

    /**
     * A template for decoding an PendInfo from its BER encoding.
     */
    public static class Template implements ASN1Template {
        private SEQUENCE.Template seqt;

        public Template() {
            seqt = new SEQUENCE.Template();
            seqt.addElement( OCTET_STRING.getTemplate() );
            seqt.addElement( GeneralizedTime.getTemplate() );
        }

        public boolean tagMatch(Tag tag) {
            return TAG.equals(tag);
        }

        public ASN1Value decode(InputStream istream)
            throws InvalidBERException, IOException
        {
            return decode(TAG, istream);
        }

        public ASN1Value decode(Tag implicitTag, InputStream istream)
            throws InvalidBERException, IOException
        {
            SEQUENCE seq = (SEQUENCE) seqt.decode(implicitTag, istream);

            Assert._assert(seq.size() == 2);

            return new PendInfo(
                            (OCTET_STRING)      seq.elementAt(0),
                            (GeneralizedTime)      seq.elementAt(1));
        }
    }
}
