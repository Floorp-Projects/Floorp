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
package org.mozilla.jss.pkcs7;

import org.mozilla.jss.asn1.*;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import org.mozilla.jss.util.Assert;

/**
 * An Attribute, which has the following ASN.1
 *      definition (roughly):
 * <pre>
 *      Attribute ::= SEQUENCE {
 *          type        OBJECT IDENTIFIER,
 *          value       SET }
 * </pre>
 */
public class Attribute implements ASN1Value {

    private OBJECT_IDENTIFIER type;
    private SET values;

    public static final Tag TAG = SEQUENCE.TAG;
    public Tag getTag() {
        return TAG;
    }

    private Attribute() { }

    public Attribute(OBJECT_IDENTIFIER type, SET values) {
        this.type = type;
        this.values = values;
    }

    public Attribute(OBJECT_IDENTIFIER type, ASN1Value value) {
        this.type = type;
        this.values = new SET();
        values.addElement(value);
    }

    public OBJECT_IDENTIFIER getType() {
        return type;
    }

    /**
     * If this AVA was constructed, returns the SET of ASN1Values passed to the
     * constructor.  If this Atrribute was decoded with an Attribute.Template,
     * returns a SET of ANYs.
     */
    public SET getValues() {
        return values;
    }

    public void encode(OutputStream ostream) throws IOException {
        encode(TAG, ostream);
    }

    public void encode(Tag implicit, OutputStream ostream)
        throws IOException
    {
        SEQUENCE seq = new SEQUENCE();
        seq.addElement(type);
        seq.addElement(values);

        seq.encode(implicit, ostream);
    }

    public static Template getTemplate() {
        return templateInstance;
    }
    private static Template templateInstance = new Template();

/**
 * A Template for decoding an Attribute.
 */
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

        seqt.addElement( new OBJECT_IDENTIFIER.Template()   );
        seqt.addElement( new SET.OF_Template(new ANY.Template()));

        SEQUENCE seq = (SEQUENCE) seqt.decode(implicit, istream);

        // The template should have enforced this
        Assert._assert(seq.size() == 2);

        return new Attribute( (OBJECT_IDENTIFIER) seq.elementAt(0),
                              (SET)               seq.elementAt(1));
    }
}

}
