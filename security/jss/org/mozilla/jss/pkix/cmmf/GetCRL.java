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

/**
 * CMMF <i>GetCRL</i>.
 * <pre>
 * GetCRL ::= SEQUENCE {
 *      issuerName      Name,
 *      cRLName         GeneralName OPTIONAL,
 *      time            GeneralizedTime OPTIONAL,
 *      reasons         ReasonFlags OPTIONAL }
 * </pre>
 */
public class GetCRL implements ASN1Value {

    ///////////////////////////////////////////////////////////////////////
    // constants
    ///////////////////////////////////////////////////////////////////////
    /**
     * A bit position in a ReasonFlags bit string.
     */
    public static final int unused = 0;
    /**
     * A bit position in a ReasonFlags bit string.
     */
    public static final int keyCompromise = 1;
    /**
     * A bit position in a ReasonFlags bit string.
     */
    public static final int cACompromise = 2;
    /**
     * A bit position in a ReasonFlags bit string.
     */
    public static final int affiliationChanged = 3;
    /**
     * A bit position in a ReasonFlags bit string.
     */
    public static final int superseded = 4;
    /**
     * A bit position in a ReasonFlags bit string.
     */
    public static final int cessationOfOperation = 5;
    /**
     * A bit position in a ReasonFlags bit string.
     */
    public static final int certificateHold = 6;

    ///////////////////////////////////////////////////////////////////////
    // members and member access
    ///////////////////////////////////////////////////////////////////////
    private ANY issuerName;
    private ANY cRLName; // may be null
    private GeneralizedTime time; // may be null
    private BIT_STRING reasons; // may be null
    private SEQUENCE sequence;

    /**
     * Returns the <code>issuerName</code> field.
     */
    public ANY getIssuerName() {
        return issuerName;
    }

    /**
     * Returns the <code>cRLName</code> field, which may be <code>null</code>.
     */
    public ANY getCRLName() {
        return cRLName;
    }

    /**
     * Returns the <code>time</code> field, which may be <code>null</code>.
     */
    public GeneralizedTime getTime() {
        return time;
    }

    /**
     * Returns the <code>reasons</code> field, which may be <code>null</code>.
     */
    public BIT_STRING getReasons() {
        return reasons;
    }

    ///////////////////////////////////////////////////////////////////////
    // Constructors
    ///////////////////////////////////////////////////////////////////////

    private GetCRL() { }

    /**
     * Constructs a <code>GetCRL</code> from its components.
     *
     * @param issuerName The issuer name of the CRL.  This should be an ASN.1
     *      <i>Name</i>.
     * @param cRLName The name of the CRL, which may be <code>null</code>.
     *      This should be an ASN.1 <i>GeneralName</i>.
     * @param time The time of the CRL, which may be <code>null</code>.
     * @param reasons Can be used to specify from among CRLs partitioned
     *      by revocation reason.  The BIT_STRING can be created from a
     *      Java BitSet.  The positions in the BitSet should be set or cleared
     *      using the constants provided in this class.
     */
    public GetCRL( ANY issuerName, ANY cRLName, GeneralizedTime time,
                    BIT_STRING reasons ) {
        if( issuerName == null ) {
            throw new IllegalArgumentException(
                "issuerName parameter to GetCRL constructor is null");
        }

        sequence = new SEQUENCE();

        this.issuerName = issuerName;
        sequence.addElement(issuerName);
        this.cRLName = cRLName;
        sequence.addElement(cRLName);
        this.time = time;
        sequence.addElement(time);

        // Remove trailing zeroes in this bit string because it contains
        // flags.
        this.reasons = reasons;
        reasons.setRemoveTrailingZeroes(true);
        sequence.addElement(reasons);
    }

    ///////////////////////////////////////////////////////////////////////
    // encoding / decoding
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

    private static final Template templateInstance = new Template();
    public static Template getTemplate() {
        return templateInstance;
    }

    /**
     * A Template for decoding a <code>GetCRL</code>.
     */
    public static class Template implements ASN1Template {

        private SEQUENCE.Template seqt;

        public Template() {
            seqt = new SEQUENCE.Template();

            seqt.addElement( ANY.getTemplate() );
            seqt.addOptionalElement( ANY.getTemplate() );
            seqt.addOptionalElement( GeneralizedTime.getTemplate() );
            seqt.addOptionalElement( BIT_STRING.getTemplate() );
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

            return new GetCRL(  (ANY)               seq.elementAt(0),
                                (ANY)               seq.elementAt(1),
                                (GeneralizedTime)   seq.elementAt(2),
                                (BIT_STRING)        seq.elementAt(3)   );
        }
    }
}
