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
package org.mozilla.jss.asn1;

import org.mozilla.jss.util.Assert;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;

/**
 * Represents an ASN.1 <code>ANY</code> value. An ANY is just an arbitrary
 * ASN.1 value. It can be thought of as the simplest implementation of the
 * <code>ASN1Value</code> interface. Although they can be created
 * from scratch (from raw BER), instances of <code>ANY</code> are usually
 * found after decoding
 * with a template that has an <code>ANY</code> field.
 *
 * <p>An <code>ANY</code> supports extracting the BER encoding, or decoding
 *  with a different template.
 */
public class ANY implements ASN1Value {

    private ANY() { }

    // The complete encoding of header + contents
    private byte[] encoded;
    private Tag tag;

    /**
     * Creates an ANY value, which is just a generic ASN.1 value.
     * This method is provided for efficiency if the tag is already known,
     * so that we don't have to parse the encoding for it.
     * @param tag The tag of this value. It must be the same as the actual tag
     *  contained in the encoding.
     * @param encoded The complete BER encoding of this value, including
     *      tag, form, length, and contents.
     */
    public ANY(Tag tag, byte[] encoded) {
        this.encoded = encoded;
        this.tag = tag;
    }

    /**
     * Creates an ANY value, which is just a generic ASN.1 value.
     * @param encoded The complete BER encoding of this value, including
     *      tag, form, length, and contents.
     */
    public ANY(byte[] encoded) throws InvalidBERException {
      try {
        this.encoded = encoded;

        ByteArrayInputStream bis = new ByteArrayInputStream(encoded);
        ASN1Header head = new ASN1Header(bis);
        this.tag = head.getTag();
      } catch(IOException e) {
            throw new org.mozilla.jss.util.AssertionException(
                "IOException while creating ANY: "+e);
      }
    }

    /**
     * Returns the tag of this value.
     */
    public Tag getTag() {
        return tag;
    }

    /**
     * Returns the complete encoding of header and contents, as passed into
     * the constructor or read from a BER input stream.
     */
    public byte[] getEncoded() {
        return encoded;
    }

    /**
     * Returns the ASN.1 header from the encoding.
     */
    public ASN1Header getHeader() throws InvalidBERException, IOException {
        if( header == null ) {
            ByteArrayInputStream bis = new ByteArrayInputStream(encoded);
            header = new ASN1Header(bis);
        }
        return header;
    }
    private ASN1Header header=null;

    /**
     * Strips out the header and returns just the contents octets of the
     * encoding.
     */
    private byte[] contents=null;
    public byte[] getContents() throws InvalidBERException {
      try {
        if( contents==null ) {
            ByteArrayInputStream bis = new ByteArrayInputStream(encoded);
            header = new ASN1Header(bis);
            contents = new byte[ bis.available() ];
            if( (contents.length != header.getContentLength()) &&
                ( header.getContentLength() != -1 ) ) {
                throw new InvalidBERException("Length of contents was not the "+
                    "same as the header predicted");
            }
            ASN1Util.readFully(contents, bis);
        }

        return contents;

      } catch( IOException e ) {
            Assert.notReached("IOException reading from byte array");
            return null;
      }
    }
    
    public void encode(OutputStream ostream) throws IOException {
        ostream.write(encoded);
    }

    /**
     * Decodes this ANY using the given template.  This is useful if you
     * originally decoded something as an ANY because you didn't know
     * what it was, but now you know what it is supposed to be.
     *
     * @param template The template to use to decode this ANY.
     * @return The output of the given template when it is fed the
     *      encoding of this ANY.
     */
    public ASN1Value decodeWith(ASN1Template template)
        throws InvalidBERException
    {
      try {
        ByteArrayInputStream bis = new ByteArrayInputStream(encoded);
        return template.decode(bis);
      } catch( IOException e ) {
        Assert.notReached("IOException while reading from byte array input"+
            " stream");
        return null;
      }
    }

    /**
     * Decodes this ANY using the given template.  This is useful if you
     * originally decoded something as an ANY because you didn't know
     * what it was, but now you know what it is supposed to be.
     *
     * @param implicitTag The implicit tag for the encoding.
     * @param template The template to use to decode this ANY.
     * @return The output of the given template when it is fed the
     *      encoding of this ANY.
     */
    public ASN1Value decodeWith(Tag implicitTag, ASN1Template template)
        throws IOException, InvalidBERException
    {
        ByteArrayInputStream bis = new ByteArrayInputStream(encoded);
        return template.decode(implicitTag, bis);
    }

    /**
     * @param implicitTag <b>This parameter is ignored</b>, because
     * ANY values cannot have implicit tags.
     */
    public void encode(Tag implicitTag, OutputStream ostream)
        throws IOException
    {
        if( ! implicitTag.equals(tag) ) {
            Assert.notReached("No implicit tags allowed for ANY");
        }
        ostream.write(encoded);
    }

    /**
     * Extracts the contents from the ANY and encodes them with
     * the provided tag.
     */
    public void encodeWithAlternateTag(Tag alternateTag, OutputStream ostream)
        throws IOException, InvalidBERException
    {
        byte[] contents = getContents();
        ASN1Header oldHead = getHeader();
        Assert._assert( contents.length == oldHead.getContentLength() );

        ASN1Header newHead = new ASN1Header( alternateTag, oldHead.getForm(),
                                contents.length);
        newHead.encode(ostream);
        ostream.write(contents);
    }

    /**
     * Returns a singleton instance of a decoding template.
     */
    public static Template getTemplate() {
        return templateInstance;
    }
    private static Template templateInstance = new Template();

/**
 * A class for decoding <code>ANY</code> values from BER.
 */
public static class Template implements ASN1Template {

    public boolean tagMatch(Tag tag) {
        return true; // wheeeeee...it's ANY!

    }

    public ASN1Value decode(InputStream istream)
        throws IOException, InvalidBERException
    {
      try {

        ASN1Header head = ASN1Header.lookAhead(istream);

        if( head.getContentLength() == -1 ) {
            // indefinite length encoding
            ByteArrayOutputStream recording = new ByteArrayOutputStream();

            // eat the header off the input stream
            head = new ASN1Header(istream);
            
            // write the header to the recording stream
            recording.write( head.encode() );

            // write all objects from the input stream to the recording
            // stream, until we hit an END-OF-CONTENTS tag
            ANY any;
            ANY.Template anyt = new ANY.Template();
            int count=0;
            do {
                any = (ANY) anyt.decode(istream);
                recording.write( any.getEncoded() );
            } while( ! any.getTag().equals(Tag.EOC) );

            return new ANY( head.getTag(), recording.toByteArray() );
                
        } else {
            // definite length encoding
            byte[] data = new byte[ (int) head.getTotalLength() ];

            ASN1Util.readFully(data, istream);
            return new ANY(head.getTag(), data);
        }

      } catch( InvalidBERException e ) {
            throw new InvalidBERException(e, "ANY");
      }
    }

    public ASN1Value decode(Tag implicitTag, InputStream istream)
        throws IOException, InvalidBERException
    {
        throw new InvalidBERException("Implicit tag on ANY");
    }
} // End of Template

}
