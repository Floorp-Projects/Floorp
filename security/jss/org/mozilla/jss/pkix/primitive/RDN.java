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
package org.mozilla.jss.pkix.primitive;

import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import org.mozilla.jss.asn1.*;

/**
 * A RelativeDistinguishedName, whose ASN.1 is:
 * <pre>
 * RelativeDistinguishedName ::= SET SIZE(1..MAX) OF AttributeValueAssertion
 * </pre>
 */
public class RDN implements ASN1Value {

    private SET avas;

    private RDN() { }

    /**
     * An RDN must have at least one element at all times, so an initial
     *  element must be provided.
     */
    public RDN(AVA initialElement) {
        avas = new SET();
        avas.addElement(initialElement);
    }

    // This is for private use only, so we can be constructed from our
    // template.
    RDN(SET avas) {
        this.avas = avas;
    }

    public void add( AVA ava ) {
        avas.addElement( ava );
    }

    public AVA at( int idx ) {
        return (AVA) avas.elementAt( idx );
    }

    /**
     * @exception TooFewElementsException If removing this element would
     *  result in the RDN being empty.
     */
    public void removeAt( int idx ) throws TooFewElementsException {
        if( avas.size() <= 1 ) {
            throw new TooFewElementsException();
        }
        avas.removeElementAt( idx );
    }

    public int size() {
        return avas.size();
    }

    public static final Tag TAG = SET.TAG;
    public Tag getTag() {
        return TAG;
    }

    public void encode(OutputStream ostream) throws IOException {
        avas.encode(ostream);
    }

    public void encode(Tag implicit, OutputStream ostream)
        throws IOException
    {
        avas.encode(implicit, ostream);
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
        AVA.Template avatemp = new AVA.Template();
        SET.OF_Template sett = new SET.OF_Template( avatemp );

        SET set =  (SET) sett.decode(implicit, istream);

        if(set.size() < 1) {
            throw new InvalidBERException("RDN with zero elements; "+
                "an RDN must have at least one element");
        }

        return new RDN(set);
    }
}

}
