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

import java.math.BigInteger;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.ByteArrayOutputStream;
import java.util.Vector;
import org.mozilla.jss.util.Assert;

/**
 * The portion of a BER encoding that precedes the contents octets.  Consists
 * of the tag, form, and length octets.
 */
public class ASN1Header {

    // This is set by the the decoding constructor, and by the encode()
    // method.  If it is set by the decoding constructor, it is supposed
    // to represent what was actually read from the input stream, so it
    // must not be overwritten later by the output of encode(), which could
    // be a different encoding (DER vs. BER, for example).
    private byte[] cachedEncoding = null;

    /**
     * Returns the length of the header plus the length of the contents;
     *  the total length of the DER encoding of an ASN1 value. Returns
     *  -1 if indefinite length encoding was used.
     */
    public long getTotalLength() {
        if( contentLength == -1 ) {
            return -1;
        } else {
            return encode().length + contentLength;
        }
    }

    private Tag tag;
    public Tag getTag() {
        return tag;
    }

    // -1 means indefinite length encoding
    private long contentLength;
    /**
     * Returns -1 for indefinite length encoding.
     */
    public long getContentLength() {
        return contentLength;
    }

    // PRIMITIVE or CONSTRUCTED
    public static final Form PRIMITIVE = Form.PRIMITIVE;
    public static final Form CONSTRUCTED = Form.CONSTRUCTED;
    private Form form;

    /**
     * Returns the Form, PRIMITIVE or CONSTRUCTED.
     */
    public Form getForm() {
        return form;
    }

    // This is the maximum size of ASN1 Header we support.
    // 32 bytes is pretty huge, I've never seen anything bigger than 7.
    private static final int MAX_LOOK_AHEAD = 32;

    /**
     * Returns information about the next item in the stream, but does not
     *  consume any octets.
     * @exception IOException If the input stream does not support look ahead.
     */
    public static ASN1Header lookAhead(InputStream derStream)
        throws IOException, InvalidBERException
    {
        if( ! derStream.markSupported() ) {
            throw new IOException("Mark not supported on this input stream");
        }

        derStream.mark(MAX_LOOK_AHEAD);
        ASN1Header info = new ASN1Header(derStream);
        derStream.reset();

        return info;
    }

    /**
     * Gets info about the next item in the DER stream, consuming the
     * identifier and length octets.
     */
    public ASN1Header(InputStream istream)
        throws InvalidBERException, IOException
    {
        // default BAOS size is 32 bytes, which is plenty
        ByteArrayOutputStream encoding = new ByteArrayOutputStream();
        int inInt = istream.read();
        if( inInt == -1 ) {
            throw new InvalidBERException("End-of-file reached while "+
                "decoding ASN.1 header");
        }
        encoding.write(inInt);
        byte byte1 = (byte) inInt;
        Tag.Class tagClass;

        //
        // Get Tag Class
        //
        tagClass = Tag.Class.fromInt( (byte1 & 0xff) >>> 6 );
        
        //
        // Get form
        //
        if( (byte1 & 0x20) == 0x20 ) {
            form = CONSTRUCTED;
        } else {
            form = PRIMITIVE;
        }

        //
        // Get Tag Number
        //
        long tagNum;
        if( (byte1 & 0x1f) == 0x1f ) {
            // long form

            //
            // read all octets into a Vector of Bytes
            //
            byte next;
            Vector bV = new Vector();

            // last byte has MSB == 0.
            do {
                inInt = istream.read();
                if( inInt == -1 ) {
                    throw new InvalidBERException("End-of-file reached while"
                        +" decoding ASN.1 header");
                }
                encoding.write(inInt);
                next = (byte) inInt;
                bV.addElement( new Byte(next) );
            } while( (next & 0x80) == 0x80 );
            Assert._assert( bV.size() > 0 );

            //
            // Copy Vector of 7-bit bytes into array of 8-bit bytes.
            //
            byte[] bA = new byte[ ( (bV.size()*7) + 7 ) / 8 ];
            int v; // vector index
            int a; // array index

            // clear the target array
            for( a = 0; a < bA.length; a++ ) {
                bA[a] = 0;
            }
            int shift = 0; // the amount the Vector is shifted from the array

            // copy bits from the Vector to the array, going from the
            // end (LSB) to the beginning (MSB).
            a = bA.length - 1;
            for( v=bV.size()-1 ; v >= 0; v--) {
                Assert._assert( v >= 0 );
                Assert._assert( v < bV.size() );
                Assert._assert( a >= 0 );
                Assert._assert( a < bA.length );

                // MSB is not part of the number
                byte b = (byte) ( ((Byte)bV.elementAt(v)).byteValue() & 0x7f );
                bA[a] |= b << shift;
                if( shift > 1 ) {
                    // The byte from the Vector falls across a byte boundary
                    // in the array.  We've already got the less-significant
                    // bits, now copy the more-significant bits into
                    // the next element of the array.
                    Assert._assert( a > 0 );
                    --a;
                    bA[a] |= b >>> (8-shift);
                }

                shift = (shift+7)%8; // update shift
            }

            // Create a new unsigned BigInteger from the byte array
            tagNum = (new BigInteger( 1, bA )).longValue();

        } else {
            // short form
            tagNum = byte1 & 0x1f;
        }

        tag = new Tag(tagClass, tagNum);

        //
        // Get Length
        //
        inInt = istream.read();
        if(inInt == -1) {
            throw new InvalidBERException("End-of-file reached while "+
                "decoding ASN.1 header");
        }
        encoding.write(inInt);
        byte lenByte = (byte) inInt;

        if( (lenByte & 0x80) == 0 ) {
            // short form
            contentLength = lenByte;
        } else {
            // long form
            if( (lenByte & 0x7f) == 0 ) {
                // indefinite
                contentLength = -1;
            } else {
                // definite 
                byte[] lenBytes = new byte[ lenByte & 0x7f ];
                ASN1Util.readFully(lenBytes, istream);
                encoding.write( lenBytes );
                contentLength = (new BigInteger( 1, lenBytes )).longValue();
            }
        }

        // save our encoding so we don't have to recompute it later
        cachedEncoding = encoding.toByteArray();
    }

    /**
     * This constructor is to be called when we are constructing an ASN1Value
     * rather than decoding it.
     * @param contentLength Must be >=0. Although indefinite length
     *      <i>decoding</i> is supported, indefinite length <i>encoding</i>
     *      is not.
     */
    public ASN1Header( Tag tag, Form form, long contentLength)
    {
        this.tag = tag;
        this.form = form;
        Assert._assert(contentLength >= 0);
        this.contentLength = contentLength;
    }

    public void encode( OutputStream ostream )
        throws IOException
    {
        ostream.write( encode() );
    }

    public byte[] encode() {
        // It's important that we not recompute the encoding if it was
        // set by ASN1Header(InputStream), since in that case it represents
        // the encoding that was actually read from the InputStream.
        if( cachedEncoding != null ) {
            return cachedEncoding;
        }

        ByteArrayOutputStream cache = new ByteArrayOutputStream();
        
        //
        // Identifier octet(s)
        //

        byte idOctet = 0;
        idOctet |= tag.getTagClass().toInt() << 6;

        if( form == CONSTRUCTED ) {
            idOctet |= 0x20;
        }

        if( tag.getNum() <= 30 ) {
            // short form
            idOctet |= (tag.getNum() & 0x1f );

            cache.write( idOctet );
        }  else {
            // long form
            idOctet |= 0x1f;
            BigInteger tagNum = BigInteger.valueOf(tag.getNum());

            cache.write( idOctet );

            int bitlength = tagNum.bitLength();
            int reps = (bitlength+6)/7;

            for( reps = reps-1; reps > 0 ; reps--) {
                long shifted = tag.getNum() >>> ( 7*reps );
                cache.write( (((byte)shifted) & 0x7f) | 0x80 );
            }

            cache.write( ((byte)tag.getNum()) & 0x7f );
        }

        //
        // Length Octets
        //
        if( contentLength == -1 ) {
            // indefinite form
            cache.write( (byte) 0x80 );
        } else if( contentLength <= 127 ) {
            // short form
            cache.write( (byte) contentLength );
        } else {
            // long form
            byte[] val = unsignedBigIntToByteArray(
                            BigInteger.valueOf(contentLength) );
            cache.write( ((byte)val.length) | 0x80 );
            cache.write( val, 0, val.length );
        }

        cachedEncoding = cache.toByteArray();

        return cachedEncoding;
    }

    /**
     * Converts an unsigned BigInteger to a minimal-length byte array.
     * This is necessary because BigInteger.toByteArray() attaches an extra
     * sign bit, which could cause the size of the byte representation to
     * be bumped up by an extra byte.
     */
    public static byte[] unsignedBigIntToByteArray(BigInteger bi) {
        // make sure it is not negative
        Assert._assert( bi.compareTo(BigInteger.valueOf(0)) != -1 );

        // find minimal number of bytes to hold this value
        int bitlen = bi.bitLength(); // minimal number of bits, without sign
        int bytelen;
        if( bitlen == 0 ) {
            // special case, since bitLength() returns 0
            bytelen = 1;
        } else {
            bytelen = (bitlen + 7) / 8;
        }

        byte[] withSign = bi.toByteArray();

        if( bytelen == withSign.length ) {
            return withSign;
        } else {
            // trim off extra byte at the beginning
            Assert._assert( bytelen == withSign.length - 1 );
            Assert._assert( withSign[0] == 0 );
            byte[] without = new byte[bytelen];
            System.arraycopy(withSign,1, without, 0, bytelen);
            return without;
        }
    }

    /**
     * Verifies that this header has the given tag and form.
     * @exception InvalidBERException If the header's tag or form
     *  differ from those passed in.
     */
    public void validate(Tag expectedTag, Form expectedForm)
        throws InvalidBERException
    {
        validate(expectedTag);
        if( getForm() != expectedForm ) {
            throw new InvalidBERException("Incorrect form: expected ["+
                expectedForm+"], found ["+getForm());
        }
    }

    /**
     * Verifies that this head has the given tag.
     * @exception InvalidBERException If the header's tag differs from that
     *      passed in.
     */
    public void validate(Tag expectedTag) throws InvalidBERException {
        if( ! getTag().equals( expectedTag ) ) {
            throw new InvalidBERException("Incorrect tag: expected ["+
                expectedTag+"], found ["+getTag()+"]");
        }
    }

    /**
     * Returns <code>true</code> if this is a BER end-of-contents marker.
     */
    public boolean isEOC() {
        return( tag.equals(Tag.EOC) );
    }

}
