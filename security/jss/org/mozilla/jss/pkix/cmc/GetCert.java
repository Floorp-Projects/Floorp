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
 * CMC <i>GetCert</i>.
 * <pre>
 * GetCert ::= SEQUENCE {
 *      issuerName      GeneralName,
 *      serialNumber    INTEGER }
 * </pre>
 */
public class GetCert implements ASN1Value {

    ///////////////////////////////////////////////////////////////////////
    // Members and member access
    ///////////////////////////////////////////////////////////////////////
    private ANY issuerName;
    private INTEGER serialNumber;
    private SEQUENCE sequence;

    /**
     * Returns the <code>issuerName</code> field as an <code>ANY</code>.
     * The actual type of the field is <i>GeneralName</i>.
     */
    public ANY getIssuer() {
        return issuerName;
    }

    /**
     * Returns the <code>serialNumber</code> field.
     */
    public INTEGER getSerialNumber() {
        return serialNumber;
    }

    ///////////////////////////////////////////////////////////////////////
    // Constructors
    ///////////////////////////////////////////////////////////////////////
    private GetCert() { }

    /**
     * Constructs a new <code>GetCert</code> from its components.  Neither
     * component may be <code>null</code>.
     */
    public GetCert(ANY issuerName, INTEGER serialNumber) {
        if( issuerName == null || serialNumber == null ) {
            throw new IllegalArgumentException(
                "parameter to GetCert constructor is null");
        }
        sequence = new SEQUENCE();

        this.issuerName = issuerName;
        sequence.addElement(issuerName);

        this.serialNumber = serialNumber;
        sequence.addElement(serialNumber);
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
     * A Template for decoding a <code>GetCert</code>.
     */
    public static class Template implements ASN1Template {
        private SEQUENCE.Template seqt;

        public Template() {
            seqt = new SEQUENCE.Template();
            seqt.addElement( ANY.getTemplate() );
            seqt.addElement( INTEGER.getTemplate() );
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

            return new GetCert( (ANY)       seq.elementAt(0),
                                (INTEGER)   seq.elementAt(1) );
        }
    }
}
