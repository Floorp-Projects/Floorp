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
package org.mozilla.jss.asn1;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.ByteArrayOutputStream;
import org.mozilla.jss.util.Assert;

/**
 * An explicit tag.
 */
public class EXPLICIT implements ASN1Value {

    public static final Form FORM = Form.CONSTRUCTED;

    private ASN1Value content;
    private Tag tag;

    private EXPLICIT() { }

    /**
     * Creates an EXPLICIT tag wrapping some other ASN1Value.  For example,
     * for the following ASN.1 snippet:
     * <pre>
     * MyType [3] EXPLICIT INTEGER
     * </pre>
     * assuming a sample value of 5 for the INTEGER, a MyType could be
     * created with:
     * <pre>
     *  EXPLICIT myValue = new EXPLICIT( new Tag(3), new INTEGER(5) );
     * </pre>
     */
    public EXPLICIT( Tag tag, ASN1Value content ) {
        Assert._assert(tag!=null && content!=null);
        this.content = content;
        this.tag = tag;
    }

    /**
     * Returns the ASN1Value that is wrapped by this EXPLICIT tag.
     */
    public ASN1Value getContent() {
        return content;
    }

    /**
     * Returns the Tag of this EXPLICIT tag.
     */
    public Tag getTag() {
        return tag;
    }

    public void encode(OutputStream ostream) throws IOException {
        encode(tag, ostream);
    }

    public void encode(Tag implicitTag, OutputStream ostream)
        throws IOException
    {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        content.encode(bos);
        byte[] contentBytes = bos.toByteArray();
        ASN1Header head = new ASN1Header(implicitTag, FORM,
            contentBytes.length );
        head.encode(ostream);
        ostream.write(contentBytes);
    }

    public static Template getTemplate( Tag tag, ASN1Template content) {
        return new Template(tag, content);
    }

/**
 * A template for decoding an object wrapped in an EXPLICIT tag.
 */
public static class Template implements ASN1Template {

    private ASN1Template content;
    private Tag tag;

    private Template() { }

    /**
     * Creates a template for unwrapping an object wrapped in an explicit tag.
     * For example, to decode:
     * <pre>
     * MyValue ::= [3] EXPLICIT INTEGER
     * </pre>
     * use:
     * <pre>
     * EXPLICIT.Template myTemplate = new EXPLICIT.Template( new Tag(3),
     *      new INTEGER.Template() );
     * </pre>
     *
     * @param tag The tag value of the EXPLICIT tag.
     * @param content The template for decoding the object that is wrapped
     *      in the explicit tag.
     */
    public Template(Tag tag, ASN1Template content) {
        this.content = content;
        this.tag = tag;
    }

    public boolean tagMatch(Tag tag) {
        return( this.tag.equals(tag) );
    }

    public ASN1Value decode(InputStream istream)
        throws IOException, InvalidBERException
    {
        return decode(tag, istream);
    }

    public ASN1Value decode(Tag implicitTag, InputStream istream)
        throws IOException, InvalidBERException
    {
      try {
        ASN1Header head = new ASN1Header(istream);

        head.validate( implicitTag, FORM.CONSTRUCTED );

        ASN1Value val = content.decode(istream);

        EXPLICIT e = new EXPLICIT(tag, val);

        // if indefinite content length, consume the end-of-content marker
        if( head.getContentLength() == -1 ) {
            head = new ASN1Header(istream);

            if( ! head.isEOC() ) {
                throw new InvalidBERException("No end-of-contents marker");
            }
        }

        return e;

      } catch(InvalidBERException e) {
        throw new InvalidBERException(e, "EXPLICIT");
      }
    }
} // end of Template

}
