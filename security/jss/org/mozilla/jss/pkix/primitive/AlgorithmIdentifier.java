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
package org.mozilla.jss.pkix.primitive;

import org.mozilla.jss.asn1.*;
import org.mozilla.jss.util.Assert;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;

public class AlgorithmIdentifier implements ASN1Value {

    private OBJECT_IDENTIFIER oid;
    private ASN1Value parameters=null;
    private SEQUENCE sequence = new SEQUENCE();

    public static final Tag TAG = SEQUENCE.TAG;
    public Tag getTag() {
        return TAG;
    }

    /**
     * Creates an <i>AlgorithmIdentifier</i> with no parameter.
     */
    public AlgorithmIdentifier(OBJECT_IDENTIFIER oid) {
        this.oid = oid;
        sequence.addElement( oid );
    }

    /**
     * Creates an <i>AlgorithmIdentifier</i>.
     * 
     * @param parameters The algorithm parameters. A value of <code>null</code>
     *      will be encoded with an ASN.1 <code>NULL</code>.
     */
    public AlgorithmIdentifier(OBJECT_IDENTIFIER oid, ASN1Value parameters) {
        this.oid = oid;
        sequence.addElement( oid );
        this.parameters = parameters;
        if( parameters != null ) {
            sequence.addElement(parameters);
        } else {
            sequence.addElement(new NULL());
        }
    }

    public OBJECT_IDENTIFIER getOID() {
        return oid;
    }

    /**
     * If this instance was constructed, returns the
     * parameter passed in to the constructer.  If this instance was
     * decoded from a template, returns an ANY that was read from the
     * BER stream. In either case, it will return null if no parameters
     * were supplied.
     */
    public ASN1Value getParameters() {
        return parameters;
    }

    private static final AlgorithmIdentifier.Template templateInstance =
                                new AlgorithmIdentifier.Template();
    public static AlgorithmIdentifier.Template getTemplate() {
        return templateInstance;
    }

    public void encode(OutputStream ostream) throws IOException {
        encode(TAG, ostream);
    }

    public void encode(Tag implicit, OutputStream ostream)
        throws IOException
    {
        sequence.encode(implicit, ostream);
    }

public static class Template implements ASN1Template {

    public boolean tagMatch(Tag tag) {
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
        SEQUENCE.Template seqt = new SEQUENCE.Template();
        seqt.addElement( new OBJECT_IDENTIFIER.Template() );
        seqt.addOptionalElement( new ANY.Template() );

        SEQUENCE seq = (SEQUENCE) seqt.decode(implicit, istream);

        // the template should have enforced this
        Assert._assert( seq.size() == 2 );

        return new AlgorithmIdentifier(
            (OBJECT_IDENTIFIER)seq.elementAt(0),  // OID
            seq.elementAt(1)                      // parameters
        );
    }
} // end of Template

}
