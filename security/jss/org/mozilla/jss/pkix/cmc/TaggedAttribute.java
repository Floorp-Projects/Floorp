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

package org.mozilla.jss.pkix.cmc;

import org.mozilla.jss.asn1.*;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import org.mozilla.jss.util.Assert;

/**
 * A tagged attribute, which has the following ASN.1
 *      definition :
 * <pre>
 *   TaggedAttribute ::= SEQUENCE {
 *      bodyPartID         BodyPartId, 
 *      attrType           OBJECT IDENTIFIER, 
 *      attrValues         SET OF AttributeValue 
 *   bodyIdMax INTEGER ::= 4294967295
 *
 *   BodyPartID ::= INTEGER(0..bodyIdMax)
 * </pre>
 */
public class TaggedAttribute implements ASN1Value {
	public static final INTEGER BODYIDMAX = new INTEGER("4294967295");

    ///////////////////////////////////////////////////////////////////////
    // Members
    ///////////////////////////////////////////////////////////////////////

    private SEQUENCE sequence;
    private INTEGER bodyPartID;
    private OBJECT_IDENTIFIER type;
    private SET values;

    ///////////////////////////////////////////////////////////////////////
    // Construction
    ///////////////////////////////////////////////////////////////////////

    private TaggedAttribute() { }

    public TaggedAttribute(INTEGER bodyPartID, OBJECT_IDENTIFIER type, SET values) {
        sequence = new SEQUENCE();
        Assert._assert(bodyPartID.compareTo(BODYIDMAX) <= 0);
        this.bodyPartID = bodyPartID;
        sequence.addElement(bodyPartID);
        this.type = type;
        sequence.addElement(type);
        this.values = values;
        sequence.addElement(values);
    }

    public TaggedAttribute(INTEGER bodyPartID, OBJECT_IDENTIFIER type, ASN1Value value) {
        sequence = new SEQUENCE();
        Assert._assert(bodyPartID.compareTo(BODYIDMAX) <= 0);
        this.bodyPartID = bodyPartID;
        sequence.addElement(bodyPartID);
        this.type = type;
        sequence.addElement(type);
        this.values = new SET();
        values.addElement(value);
        sequence.addElement(values);
    }

    ///////////////////////////////////////////////////////////////////////
    // accessors
    ///////////////////////////////////////////////////////////////////////

    public INTEGER getBodyPartID() {
        return bodyPartID;
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

    public void encode(Tag implicit, OutputStream ostream)
        throws IOException
    {
        sequence.encode(implicit, ostream);
    }

    public static Template getTemplate() {
        return templateInstance;
    }
    private static Template templateInstance = new Template();

	/**
	 * A Template for decoding an Attribute.
	 */
	public static class Template implements ASN1Template {
        private SEQUENCE.Template seqt;

        public Template() {
            seqt = new SEQUENCE.Template();
            seqt.addElement( INTEGER.getTemplate() );
			seqt.addElement( new OBJECT_IDENTIFIER.Template()   );
			seqt.addElement( new SET.OF_Template(new ANY.Template()));
        }


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
			SEQUENCE seq = (SEQUENCE) seqt.decode(implicit, istream);

			// The template should have enforced this
			Assert._assert(seq.size() == 3);

			return new TaggedAttribute(
                            (INTEGER)      seq.elementAt(0),
							(OBJECT_IDENTIFIER) seq.elementAt(1),
                            (SET)               seq.elementAt(2));
		}
	}
}



