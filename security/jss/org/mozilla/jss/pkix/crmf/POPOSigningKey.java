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

package org.mozilla.jss.pkix.crmf;

import org.mozilla.jss.asn1.*;
import org.mozilla.jss.pkix.primitive.*;
import java.io.*;

/**
 * CRMF <i>POPOSigningKey</i>:
 * <pre>
 * POPOSigningKey ::= SEQUENCE {
 *      poposkInput         [0] POPOSigningKeyInput OPTIONAL,
 *      algorithmIdentifier AlgorithmIdentifier,
 *      signature           BIT STRING }
 * </pre>
 */
public class POPOSigningKey implements ASN1Value {

    ///////////////////////////////////////////////////////////////////////
    // members and member access
    ///////////////////////////////////////////////////////////////////////
    private ANY poposkInput; // may be null
    private AlgorithmIdentifier algorithmIdentifier;
    private BIT_STRING signature;
    private SEQUENCE sequence;

    /**
     * Retrieves the input to the Proof-of-Possession of the signing key.
     * May return null, because this field is optional. Returns an ANY
     * because this type is not currently parsed.
     */
    public ANY getPoposkInput() {
        return poposkInput;
    }

    /**
     * Retrieves the algorithm identifier for the signature.
     */
    public AlgorithmIdentifier getAlgorithmIdentifier() {
        return algorithmIdentifier;
    }

    /**
     * Retrieves the signature.
     */
    public BIT_STRING getSignature() {
        return signature;
    }

    ///////////////////////////////////////////////////////////////////////
    // Constructors
    ///////////////////////////////////////////////////////////////////////

    private POPOSigningKey() { }

    /**
     * Creates a POPOSigningKey.
     * @param poposkInput May be null.
     */
    public POPOSigningKey(ANY poposkInput,
                AlgorithmIdentifier algorithmIdentifier, BIT_STRING signature) {

        if(algorithmIdentifier==null || signature==null) {
            throw new IllegalArgumentException("parameter to POPOSigningKey"+
                " constructor is null");
        }

        this.poposkInput = poposkInput;
        this.algorithmIdentifier = algorithmIdentifier;
        this.signature = signature;

        sequence = new SEQUENCE();
        sequence.addElement( Tag.get(0), poposkInput );
        sequence.addElement( algorithmIdentifier );
        sequence.addElement( signature );
    }


    ///////////////////////////////////////////////////////////////////////
    // encoding/decoding
    ///////////////////////////////////////////////////////////////////////

    private static final Tag TAG = SEQUENCE.TAG;

    public Tag getTag() {
        return TAG;
    }

    public void encode(OutputStream ostream) throws IOException {
        sequence.encode(TAG, ostream);
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
     * A Template for decoding POPOSigningKey.
     */
    public static class Template implements ASN1Template {

        private SEQUENCE.Template seqt;

        public Template() {
            seqt = new SEQUENCE.Template();
            seqt.addOptionalElement( new EXPLICIT.Template(
                    Tag.get(0), ANY.getTemplate()) );
            seqt.addElement( AlgorithmIdentifier.getTemplate());
            seqt.addElement( BIT_STRING.getTemplate() );
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

            return new POPOSigningKey(
                        (ANY) seq.elementAt(0),
                        (AlgorithmIdentifier) seq.elementAt(1),
                        (BIT_STRING)          seq.elementAt(2) );
        }
    }
}
