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
package org.mozilla.jss.pkix.cmmf;

import org.mozilla.jss.asn1.*;
import java.io.*;
import org.mozilla.jss.pkix.crmf.CertId;

/**
 * CMMF <i>RevRepContent</i>.
 * <pre>
 * RevRepContent ::= SEQUENCE {
 *    status          SEQUENCE SIZE (1..MAX) OF PKIStatusInfo,
 *      -- in same order as was sent in RevReqContent
 *    revCerts        [0] SEQUENCE SIZE (1..MAX) OF CertId OPTIONAL,
 *      -- IDs for which revocation was requested (same order as status)
 *    crls            [1] SEQUENCE SIZE (1..MAX) OF CertificateList OPTIONAL
 *      -- the resulting CRLs (there may be more than one) }
 * </pre>
 */
public class RevRepContent implements ASN1Value {

    ///////////////////////////////////////////////////////////////////////
    // members and member access
    ///////////////////////////////////////////////////////////////////////
    private SEQUENCE status;
    private SEQUENCE revCerts; // may be null
    private SEQUENCE crls; // may be null
    private SEQUENCE sequence; // for encoding

    /**
     * The <code>status</code> field, which is a <code>SEQUENCE</code>
     * of <code>PKIStatusInfo</code>.
     *
     * @see org.mozilla.jss.pkix.cmmf.PKIStatusInfo
     */
    public SEQUENCE getStatus() {
        return status;
    }

    /**
     * The <code>revCerts</code> field, which is a <code>SEQUENCE</code>
     *  of <code>CertId</code>.  Returns <code>null</code> if this
     *  field is not present.
     *
     * @see org.mozilla.jss.pkix.crmf.CertId
     */
    public SEQUENCE getRevCerts() {
        return revCerts;
    }

    /**
     * The <code>crls</code> field, which is a <code>SEQUENCE</code> of
     *  <code>ANY</code>.  Returns <code>null</code> if this field 
     *  is not present.
     */
    public SEQUENCE getCrls() {
        return crls;
    }

    ///////////////////////////////////////////////////////////////////////
    // Constructors
    ///////////////////////////////////////////////////////////////////////

    private RevRepContent() { }

    /**
     * Creates a new <code>RevRepContent</code> from its components.
     *
     * @param status A <code>SEQUENCE</code> of <code>PKIStatusInfo</code>.
     * @param revCerts A <code>SEQUENCE</code> of <code>CertId</code>. This
     *      field is optional, so <code>null</code> may be used.
     * @param crls A <code>SEQUENCE</code> of <code>ANY</code>.  This field
     *      is optional, so <code>null</code> may be used.
     * @see org.mozilla.jss.pkix.cmmf.PKIStatusInfo
     */
    public RevRepContent(SEQUENCE status, SEQUENCE revCerts, SEQUENCE crls) {

        sequence = new SEQUENCE();

        this.status = status;
        sequence.addElement(status);

        this.revCerts = revCerts;
        sequence.addElement(Tag.get(0), revCerts);

        this.crls = crls;
        sequence.addElement(Tag.get(1), crls);

    }

    ///////////////////////////////////////////////////////////////////////
    // encoding/decoding
    ///////////////////////////////////////////////////////////////////////

    private static final Tag TAG = SEQUENCE.TAG;
    public Tag getTag() {
        return TAG;
    }

    public void encode(OutputStream ostream) throws IOException {
        sequence.encode(ostream);
    }

    public void encode(Tag implicitTag, OutputStream ostream)
            throws IOException {
        sequence.encode(implicitTag, ostream);
    }


    /**
     * A Template for decoding a <code>RevRepContent</code>.
     */
    public static class Template implements ASN1Template {
        private SEQUENCE.Template seqt;

        public Template() {
            seqt = new SEQUENCE.Template();
            seqt.addElement( new SEQUENCE.OF_Template(
                                    PKIStatusInfo.getTemplate() ) );
            seqt.addOptionalElement( new SEQUENCE.OF_Template(
                                        CertId.getTemplate() ) );
            seqt.addOptionalElement( new SEQUENCE.OF_Template(
                                        ANY.getTemplate() ) );
        }

        public boolean tagMatch(Tag tag) {
            return TAG.equals(tag);
        }

        public ASN1Value decode(InputStream istream)
                throws InvalidBERException, IOException {
            return decode(TAG, istream);
        }

        public ASN1Value decode(Tag implicitTag, InputStream istream)
                throws InvalidBERException, IOException {

            SEQUENCE seq = (SEQUENCE) seqt.decode(implicitTag, istream);   

            return new RevRepContent(   (SEQUENCE) seq.elementAt(0),
                                        (SEQUENCE) seq.elementAt(1),
                                        (SEQUENCE) seq.elementAt(2)   );
        }
    }
}
