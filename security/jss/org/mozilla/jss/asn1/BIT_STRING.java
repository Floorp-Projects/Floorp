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

import org.mozilla.jss.util.Assert;
import java.math.BigInteger;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.ByteArrayOutputStream;
import java.util.BitSet;

/**
 * An ASN.1 <code>BIT STRING</code>, which is an ordered sequence of bits.
 * The bits are stored the same way they are encoded in BER: as an array
 * of bytes with 0-7 unused bits at the end.
 */
public class BIT_STRING implements ASN1Value {

    private BIT_STRING() { }

    private byte[] bits;
    private int padCount;
    private boolean removeTrailingZeroes = false;

    /**
     * @param bits The bits packed into an array of bytes, with padding
     *      at the end. The array may be empty (but not null), in which case
     *      <code>padCount</code> must be zero. The array is referenced,
     *      not cloned.
     * @param padCount The number of padding bits at the end of the array.
     *      Must be in the range <code>[0,7]</code>.
     * @exception NumberFormatException If <code>padCount</code> is not in
     *      the range <code>[0,7]</code>, or <code>bits</code> is
     *      empty and <code>padCount</code> is non-zero.
     */
    public BIT_STRING(byte[] bits, int padCount)
        throws NumberFormatException
    {
        if(padCount < 0 || padCount > 7) {
            throw new NumberFormatException();
        }
        if(bits.length == 0 && padCount != 0) {
            throw new NumberFormatException();
        }
        this.bits = bits;
        this.padCount = padCount;
    }

    /**
     * Constructs a BIT_STRING from a BitSet.
     * @param bs A BitSet.
     * @param numBits The number of bits to copy from the BitSet.
     *      This is necessary because the size of a BitSet is always padded
     *      up to a multiple of 64, but not all of these bits may
     *      be significant.
     * @exception NumberFormatException If <code>numBits</code> is larger
     *      than <code>bs.size()</code> or less than zero.
     */
    public BIT_STRING(BitSet bs, int numBits)
        throws NumberFormatException
    {
        if( numBits < 0 || numBits > bs.size() ) {
            throw new NumberFormatException();
        }
        // allocate enough bytes to hold all the bits
        bits = new byte[(numBits+7) / 8];
        padCount = (bits.length * 8) - numBits;
        Assert._assert( padCount >= 0 && padCount <= 7);

        for(int i=0; i < numBits; i++) {
            if( bs.get(i) ) {
                bits[i/8] |= 0x80 >>> (i%8);
            }
        }
    }

    /**
     * Determines whether the DER-encoding of this bitstring will have
     * its trailing zeroes removed. Generally, DER requires that trailing
     * zeroes be removed when the bitstring is used to hold flags, but
     * not when it is used to hold binary data (such as a public key).
     * The default is <tt>false</tt>.
     */
    public boolean getRemoveTrailingZeroes() {
        return this.removeTrailingZeroes;
    }

    /**
     * Determines whether the DER-encoding of this bitstring will have
     * its trailing zeroes removed. Generally, DER requires that trailing
     * zeroes be removed when the bitstring is used to hold flags, but
     * not when it is used to hold binary data (such as a public key).
     * The default is <tt>false</tt>. If this bit string is used to hold
     * flags, you should set this to <tt>true</tt>.
     */
    public void setRemoveTrailingZeroes(boolean removeTrailingZeroes) {
        this.removeTrailingZeroes = removeTrailingZeroes;
    }

    /**
     * Returns the bits packed into an array of bytes, with padding
     *      at the end. The array may be empty (but not null), in which case
     *      <code>padCount</code> must be zero. The array is referenced,
     *      not cloned.
     */
    public byte[] getBits() {
        return bits;
    }

    /**
     * Copies this BIT STRING into a Java BitSet.  Note that BitSet.size()
     * will not accurately reflect the number of bits in the BIT STRING,
     * because the size of a BitSet is always rounded up to the next multiple
     * of 64. The extra bits will be set to 0.
     */
    public BitSet toBitSet() {
        BitSet bs = new BitSet();
        int numBits = (bits.length * 8) - padCount;
        for( int i=0; i < numBits; i++) {
            if( (bits[i/8] & (0x80 >>> (i%8))) != 0 ) {
                bs.set(i);
            } else {
                bs.clear(i);
            }
        }
        return bs;
    }

    /**
     * Copies this BIT STRING into a boolean array.  Each element of the array
     * represents one bit with <code>true</code> for 1 and <code>false</code>
     * for 0.
     */
    public boolean[] toBooleanArray() {
        boolean[] array = new boolean[(bits.length*8) - padCount];
        // all elements are set to false by default

        for(int i=0; i < array.length; i++) {
            if( (bits[i/8] & (0x80 >>> (i%8))) != 0 ) {
                array[i] = true;
            }
        }
        return array;
    }

    /**
     * Returns the number of padding bits at the end of the array.
     *      Must be in the range <code>[0,7]</code>.
     */
    public int getPadCount() {
        return padCount;
    }

    public static final Tag TAG = new Tag(Tag.UNIVERSAL, 3);
    public static final Form FORM = Form.PRIMITIVE;

    public Tag getTag() {
        return TAG;
    }

    public void encode(OutputStream ostream) throws IOException {
        encode(TAG, ostream);
    }

    public void encode(Tag implicitTag, OutputStream ostream)
        throws IOException
    {
        // force all unused bits to be zero, in support of DER standard.
        if( bits.length > 0 ) {
            bits[bits.length-1] &= (0xff << padCount);
        }
        int padBits;
        int numBytes;

        if( removeTrailingZeroes ) {
            // first pare off empty bytes
            numBytes = bits.length;
            for( ; numBytes > 0; --numBytes) {
                if( bits[numBytes-1] != 0 ) {
                    break;
                }
            }

            // Now compute the number of unused bits. This includes any
            // trailing zeroes, whether they are significant or not.
            if( numBytes == 0 ) {
                padBits = 0;
            } else {
                for( padBits=0; padBits < 8; ++padBits ) {
                    if( (bits[numBytes-1] & (1 << padBits)) != 0 ) {
                        break;
                    }
                }
                Assert._assert(padBits >=0 && padBits <= 7);
            }
        } else {
            // Don't remove trailing zeroes. Just write the bits out as-is.
            padBits = padCount;
            numBytes = bits.length;

        }

        ASN1Header head = new ASN1Header(implicitTag, FORM, numBytes+1);

        head.encode(ostream);

        ostream.write(padBits);
        ostream.write(bits, 0, numBytes);
    }

    private static final Template templateInstance = new Template();
    public static Template getTemplate() {
        return templateInstance;
    }

/**
 * A class for decoding a <code>BIT_STRING</code> from its BER encoding.
 */
public static class Template implements ASN1Template {


    public boolean tagMatch(Tag tag) {
        return( TAG.equals(tag) );
    }

    public ASN1Value decode(InputStream istream)
        throws IOException, InvalidBERException
    {
        return decode(TAG, istream);
    }

    public ASN1Value decode(Tag implicitTag, InputStream istream)
        throws IOException, InvalidBERException
    {
      try {
        ASN1Header head = new ASN1Header( istream );
        head.validate( implicitTag );

        if( head.getContentLength() == -1 ) {
            // indefinite length encoding
            ByteArrayOutputStream bos = new ByteArrayOutputStream();
            int padCount=0;
            ASN1Header ahead;
            do {
                ahead = ASN1Header.lookAhead(istream);
                if( ! ahead.isEOC() ) {
                    if(padCount != 0 ) {
                      throw new InvalidBERException("Element of constructed "+
                        "BIT STRING has nonzero unused bits, but is not\n"+
                        "the last element of the construction.");
                    }
                    BIT_STRING.Template bst = new BIT_STRING.Template();
                    BIT_STRING bs = (BIT_STRING) bst.decode(istream);
                    bos.write( bs.getBits() );
                    padCount = bs.getPadCount();
                }
            } while( ! ahead.isEOC() );

            // consume the EOC
            ahead = new ASN1Header(istream);

            return new BIT_STRING( bos.toByteArray(), padCount );
        }   

        // First octet is the number of unused bits in last octet
        int padCount = istream.read();
        if( padCount == -1 ) {
            throw new InvalidBERException.EOF();
        } else if( padCount < 0 || padCount > 7 ) {
            throw new InvalidBERException("Unused bits not in range [0,7]");
        }

        // get the rest of the octets
        byte[] bits = new byte[ (int) head.getContentLength() - 1];
        ASN1Util.readFully(bits, istream);

        return new BIT_STRING(bits, padCount);

      } catch(InvalidBERException e) {
        throw new InvalidBERException(e, "BIT STRING");
      }
    }
} // end of Template

}
