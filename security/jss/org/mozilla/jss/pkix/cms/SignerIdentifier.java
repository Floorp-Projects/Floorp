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

package org.mozilla.jss.pkix.cms;

import org.mozilla.jss.asn1.*;
import org.mozilla.jss.pkix.cert.*;
import java.io.*;
import org.mozilla.jss.util.Assert;

/**
 * CMS <i>SignerIdentifier</i>:
 * <pre>
 * SignerIdentifier ::= CHOICE {
 *      issuerAndSerialNumber IssuerAndSerialNumber,
 *      subjectKeyIdentifier [0] SubjectKeyIdentifier }
 * </pre>
 */
public class SignerIdentifier implements ASN1Value {
    /**
     * The type of SignerIdentifier.
     */
    public static class Type {
        private Type() { }

        static Type ISSUER_AND_SERIALNUMBER = new Type();
        static Type SUBJECT_KEY_IDENTIFIER = new Type();
    }
    public static Type ISSUER_AND_SERIALNUMBER = Type.ISSUER_AND_SERIALNUMBER;
    public static Type SUBJECT_KEY_IDENTIFIER = Type.SUBJECT_KEY_IDENTIFIER;

    ///////////////////////////////////////////////////////////////////////
    // members and member access
    ///////////////////////////////////////////////////////////////////////

    private Type type;
    private IssuerAndSerialNumber issuerAndSerialNumber = null; // if type == ISSUER_AND_SERIALNUMBER
    private OCTET_STRING subjectKeyIdentifier = null; // if type == SUBJECT_KEY_IDENTIFIER

    /**
     * Returns the type of SignerIdentifier: <ul>
     * <li><code>ISSUER_AND_SERIALNUMBER</code>
     * <li><code>SUBJECT_KEY_IDENTIFIER</code>
     * </ul>
     */
    public Type getType() {
        return type;
    }

    /**
     * If type == ISSUER_AND_SERIALNUMBER, returns the IssuerAndSerialNumber
	 * field. Otherwise, returns null.
     */
    public IssuerAndSerialNumber getIssuerAndSerialNumber() {
        return issuerAndSerialNumber;
    }

    /**
     * If type == SUBJECT_KEY_IDENTIFIER, returns the SubjectKeyIdentifier
	 * field. Otherwise, returns null.
     */
    public OCTET_STRING getSubjectKeyIdentifier() {
        return subjectKeyIdentifier;
    }

    ///////////////////////////////////////////////////////////////////////
    // Constructors
    ///////////////////////////////////////////////////////////////////////

    private SignerIdentifier() { }

    public SignerIdentifier(Type type, IssuerAndSerialNumber
							 issuerAndSerialNumber,
							 OCTET_STRING subjectKeyIdentifier) {
        this.type = type;
        this.issuerAndSerialNumber = issuerAndSerialNumber;
        this.subjectKeyIdentifier = subjectKeyIdentifier;
    }

    /**
     * Creates a new SignerIdentifier with the given IssuerAndSerialNumber field.
     */
    public static SignerIdentifier
    createIssuerAndSerialNumber(IssuerAndSerialNumber ias) {
        return new SignerIdentifier( ISSUER_AND_SERIALNUMBER, ias, null );
    }

    /**
     * Creates a new SignerIdentifier with the given SubjectKeyIdentifier field.
     */
    public static SignerIdentifier
    createSubjectKeyIdentifier(OCTET_STRING ski) {
        return new SignerIdentifier(SUBJECT_KEY_IDENTIFIER , null, ski );
    }

    ///////////////////////////////////////////////////////////////////////
    // decoding/encoding
    ///////////////////////////////////////////////////////////////////////


    public Tag getTag() {
        if( type == SUBJECT_KEY_IDENTIFIER ) {
            return Tag.get(0);
        } else {
            Assert._assert( type == ISSUER_AND_SERIALNUMBER );
            return IssuerAndSerialNumber.TAG;
        }
    }

    public void encode(OutputStream ostream) throws IOException {

        if( type == SUBJECT_KEY_IDENTIFIER ) {
            // a CHOICE must be explicitly tagged
            //EXPLICIT e = new EXPLICIT( Tag.get(0), subjectKeyIdentifier );
            //e.encode(ostream);
            subjectKeyIdentifier.encode(Tag.get(0), ostream);
        } else {
            Assert._assert( type == ISSUER_AND_SERIALNUMBER );
            issuerAndSerialNumber.encode(ostream);
        }
    }

    public void encode(Tag implicitTag, OutputStream ostream)
            throws IOException {
				//Assert.notReached("A CHOICE cannot be implicitly tagged");
        encode(ostream);
    }

    public static Template getTemplate() {
        return templateInstance;
    }
    private static Template templateInstance = new Template();

    /**
     * A Template for decoding a SignerIdentifier.
     */
    public static class Template implements ASN1Template {

        private CHOICE.Template choicet;

        public Template() {
            choicet = new CHOICE.Template();

            //EXPLICIT.Template et = new EXPLICIT.Template(
            //    Tag.get(0), OCTET_STRING.getTemplate() );
		    //choicet.addElement( et );
            choicet.addElement( Tag.get(0), OCTET_STRING.getTemplate() );
            choicet.addElement(IssuerAndSerialNumber.getTemplate() );
        }

        public boolean tagMatch(Tag tag) {
            return choicet.tagMatch(tag);
        }

        public ASN1Value decode(InputStream istream)
                throws InvalidBERException, IOException {
            CHOICE c = (CHOICE) choicet.decode(istream);

            if( c.getTag() == SEQUENCE.TAG ) {
                return createIssuerAndSerialNumber( (IssuerAndSerialNumber) c.getValue() );
            } else {
                Assert._assert( c.getTag().equals(Tag.get(0)) );
                //EXPLICIT e = (EXPLICIT) c.getValue();
				//ASN1Value dski =  e.getContent();
				//OCTET_STRING ski = (OCTET_STRING) e.getContent();
				OCTET_STRING ski = (OCTET_STRING) c.getValue();
				return createSubjectKeyIdentifier(ski);
            }
        }

        public ASN1Value decode(Tag implicitTag, InputStream istream)
                throws InvalidBERException, IOException {
					//Assert.notReached("A CHOICE cannot be implicitly tagged");
            return decode(istream);
        }
    }
}





