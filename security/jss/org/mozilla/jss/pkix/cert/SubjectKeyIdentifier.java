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

package org.mozilla.jss.pkix.cert;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import org.mozilla.jss.asn1.*;
import org.mozilla.jss.util.Assert;

/**
 * Represent the Subject Key Identifier Extension.
 *
 * This extension, if present, provides a means of identifying the particular
 * public key used in an application.  This extension by default is marked
 * non-critical.
 *
 * <p>Extensions are addiitonal attributes which can be inserted in a X509
 * v3 certificate. For example a "Driving License Certificate" could have
 * the driving license number as a extension.
 *
 * <p>Extensions are represented as a sequence of the extension identifier
 * (Object Identifier), a boolean flag stating whether the extension is to
 * be treated as being critical and the extension value itself (this is again
 * a DER encoding of the extension value).
 *
 * @author Michelle Zhao
 * @version 1.0
 * @see Extension
 */
public class SubjectKeyIdentifier extends Extension {

    ///////////////////////////////////////////////////////////////////////
    // Members
    ///////////////////////////////////////////////////////////////////////
    private OCTET_STRING keyIdentifier;
	private static OBJECT_IDENTIFIER OID = new
	OBJECT_IDENTIFIER("2.5.29.14");

    ///////////////////////////////////////////////////////////////////////
    // Construction
    ///////////////////////////////////////////////////////////////////////

    /** 
     * Constructs an SubjectKeyIdentifier from its components.
     *
     * @param keyIdentifier must not be null.
     */
    public SubjectKeyIdentifier(OCTET_STRING keyIdentifier) {
		super(OID,false,keyIdentifier);
    }

    public SubjectKeyIdentifier(boolean critical, OCTET_STRING keyIdentifier) {
		super(OID,critical,keyIdentifier);
    }

    public static class Template implements ASN1Template {

        private SEQUENCE.Template seqt;

        public Template() {
            seqt = new SEQUENCE.Template();
            seqt.addElement( OBJECT_IDENTIFIER.getTemplate() );
            seqt.addElement( BOOLEAN.getTemplate(), new BOOLEAN(false) );
            seqt.addElement( OCTET_STRING.getTemplate() );
        }

        public boolean tagMatch(Tag t) {
            return TAG.equals(t);
        }

        public ASN1Value decode(InputStream istream)
            throws IOException, InvalidBERException
        {
            return decode(TAG, istream);
        }

        public ASN1Value decode(Tag implicit, InputStream istream)
            throws IOException, InvalidBERException
        {
            SEQUENCE seq = (SEQUENCE) seqt.decode(implicit, istream);
            Assert._assert( ((OBJECT_IDENTIFIER) seq.elementAt(0)).equals(OID) );

            return new SubjectKeyIdentifier(
                ((BOOLEAN) seq.elementAt(1)).toBoolean(),
                (OCTET_STRING) seq.elementAt(2)
            );
        }
    }
}
