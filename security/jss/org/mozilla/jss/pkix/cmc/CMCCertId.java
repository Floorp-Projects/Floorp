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
 * Portions created by the Initial Developer are Copyright (C) 2004
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

package org.mozilla.jss.pkix.cmc;

import org.mozilla.jss.asn1.*;
import java.io.*;

/**
 * CMC <i>CMCCertId</i>.
 * <pre>
 * The definition of IssuerSerial comes from RFC 3281.
 * CMCCertId ::= SEQUENCE {
 *      issuer      GeneralNames,
 *      serial      INTEGER 
 *      issuerUID   UniqueIdentifier OPTIONAL}
 * </pre>
 */
public class CMCCertId implements ASN1Value {

    ///////////////////////////////////////////////////////////////////////
    // Members and member access
    ///////////////////////////////////////////////////////////////////////
    private SEQUENCE issuer;
    private INTEGER serial;
    private BIT_STRING issuerUID;
    private SEQUENCE sequence;

    /**
     * Returns the <code>issuer</code> field as an <code>SEQUENCE of
     * ANY</code>. The actual type of the field is <i>GeneralNames</i>.
     */
    public SEQUENCE getIssuer() {
        return issuer;
    }

    /**
     * Returns the <code>serial</code> field.
     */
    public INTEGER getSerial() {
        return serial;
    }

    /**
     * Returns the <code>issuerUID</code> field.
     */
    public BIT_STRING getIssuerUID() {
        return issuerUID;
    }

    ///////////////////////////////////////////////////////////////////////
    // Constructors
    ///////////////////////////////////////////////////////////////////////
    private CMCCertId() { }

    /**
     * Constructs a new <code>CMCCertId</code> from its components. The
     * uniqueIdentifier component may be <code>null</code>.
     */
    public CMCCertId(SEQUENCE issuer, INTEGER serial, BIT_STRING issuerUID) {
        if (issuer == null || serial == null) {
            throw new IllegalArgumentException(
                "parameter to CMCCertId constructor is null");
        }
        if (issuer.size() == 0) {
            throw new IllegalArgumentException(
                "issuer parameter to CMCCertId constructor is empty");
        }
        sequence = new SEQUENCE();

        this.issuer = issuer;
        sequence.addElement(issuer);

        this.serial = serial;
        sequence.addElement(serial);

        if (issuerUID != null) {
            sequence.addElement(issuerUID);
        }
    }

    /**
     * Constructs a new <code>CMCCertId</code> from its components. The
     * issuerUID component may be <code>null</code>.
     */
    public CMCCertId(ANY issuer, INTEGER serial, BIT_STRING issuerUID) {
        if (issuer == null || serial == null) {
            throw new IllegalArgumentException(
                "parameter to CMCCertId constructor is null");
        }
        sequence = new SEQUENCE();
        this.issuer = new SEQUENCE();
        this.issuer.addElement(issuer);
        sequence.addElement(this.issuer);

        this.serial = serial;
        sequence.addElement(serial);

        if (issuerUID != null) {
            sequence.addElement(issuerUID);
        }
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

    private static final Template templateInstance = new Template();
    public static Template getTemplate() {
        return templateInstance;
    }

    /**
     * A Template for decoding a <code>CMCCertId</code>.
     */
    public static class Template implements ASN1Template {
        private SEQUENCE.Template seqt;

        public Template() {
            seqt = new SEQUENCE.Template();
            seqt.addElement(new SEQUENCE.OF_Template(ANY.getTemplate()));
            seqt.addElement( INTEGER.getTemplate() );
            seqt.addOptionalElement(BIT_STRING.getTemplate());
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

            return new CMCCertId((SEQUENCE)seq.elementAt(0),
                                 (INTEGER)seq.elementAt(1),
                                 (BIT_STRING)seq.elementAt(2));
        }
    }
}
