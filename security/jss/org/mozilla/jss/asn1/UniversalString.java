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

import java.io.CharConversionException;
import java.io.CharArrayWriter;
import java.io.ByteArrayOutputStream;

/**
 * A UCS4 string.
 */
public class UniversalString extends CharacterString implements ASN1Value {

    public static final Tag TAG = new Tag(Tag.UNIVERSAL, 28);
    public Tag getTag() {
        return TAG;
    }

    public UniversalString(char[] chars) throws CharConversionException {
        super(chars);
    }

    public UniversalString(String s) throws CharConversionException {
        super(s);
    }

    CharConverter getCharConverter() {
        return new UniversalConverter();
    }

    /**
     * Returns a singleton instance of the decoding template for this class.
     */
    public static Template getTemplate() {
        return templateInstance;
    }
    private static final Template templateInstance = new Template();

// nested class
public static class Template
    extends CharacterString.Template implements ASN1Template
{
    protected Tag getTag() {
        return TAG;
    }

    public boolean tagMatch(Tag tag) {
        return TAG.equals(tag);
    }

    protected CharConverter getCharConverter() {
        return new UniversalConverter();
    }

    protected CharacterString generateInstance(char[] chars)
        throws CharConversionException
    {
        return new UniversalString( chars );
    }

    protected String typeName() {
        return "UniversalString";
    }
} // end of Template

    /**
     * A class for converting between Unicode and UCS4.
     */
    private static class UniversalConverter implements CharConverter {

        // This is the maximum a UCS4 character can be if it has
        // straight Unicode inside it.
        public static final int MAX_UNICODE = 0x0000ffff;

        // This is the maximum a UCS4 character can be if it is UTF-16
        // encoded. UTF-16 encoding allows UCS4 chars to be stored across
        // two Unicode chars.
        public static final int MAX_UTF16 = 0x0010ffff;

        // This Unicode character is used to represent an unknown character
        // in some other encoding.  We use it for UCS4 characters that
        // are not a part of normal Unicode and also cannot be encoded
        // across two Unicode chars with UTF-16.
        public static final char REPLACEMENT_CHAR = 0xfffd;

        // This is the base for UCS4 characters that can be mapped with UTF16.
        public static final int UTF16_BASE = 0x00100000;


        // In UTF16 encoding, each Unicode character has 10 bits of
        // information.
        public static final int HALF_SHIFT = 10;

        // The lowest 10 bits
        public static final int HALF_MASK = 0x3ff;

        public static final int UTF16_HIGH_START = 0xd800;
        public static final int UTF16_HIGH_END = 0xdcff;
        public static final int UTF16_LOW_START = 0xdc00;
        public static final int UTF16_LOW_END = 0xdfff;

        /**
         * Turns big-endian UCS4 characters into Unicode Java characters
         */
        public char[] byteToChar(byte[] bytes, int offset, int len)
            throws CharConversionException
        {
            // Each UCS4 character is 4 bytes. Most UCS4 characters will
            // map to one Unicode character. The exception is UTF-16
            // characters, which map to two Unicode characters.
            CharArrayWriter out = new CharArrayWriter( len / 4 );

            int end = offset + len;

            while( offset < end ) {
                // eat 4 bytes and make a UCS4 char
                if( end - offset < 4 ) {
                    throw new CharConversionException("input exhausted");
                }
                int ucs4 = (bytes[offset++] & 0xff) << 24;
                ucs4 += (bytes[offset++] & 0xff) << 16;
                ucs4 += (bytes[offset++] & 0xff) << 8;
                ucs4 += bytes[offset++] & 0xff;

                // convert UCS4 to Unicode
                if( ucs4 <= MAX_UNICODE ) {
                    // Unicode is a subset of UCS4, and this char is
                    // in the common subset.  Just chop off the unused top
                    // two bytes.

                    out.write( ucs4 & 0xffff );

                } else if( ucs4 <= MAX_UTF16 ) {
                    // This UCS4 char is not in Unicode, but can be encoded
                    // into two Unicode chars using UTF16.

                    ucs4 -= UTF16_BASE;
                    out.write( (ucs4 >>> HALF_SHIFT) + UTF16_HIGH_START );
                    out.write( (ucs4 & HALF_MASK) + UTF16_LOW_START );

                } else {
                    // This character is not in Unicode or UTF16.  We can't
                    // provide a suitable translation, so use the Unicode
                    // replacement char.

                    out.write( REPLACEMENT_CHAR );

                }
            }

            return out.toCharArray();
        }

        // Convert Unicode chars to UCS4 chars
        public byte[] charToByte(char[] chars, int offset, int len)
            throws CharConversionException
        {
            ByteArrayOutputStream out = new ByteArrayOutputStream(len * 4);

            int end = offset + len;

            while( offset < end ) {

                char c = chars[offset++];
                int ucs4;

                if( c >= UTF16_HIGH_START && c <= UTF16_HIGH_END ) {
                    // This is the beginning of a UTF16 char
                    if( offset == end ) {
                        throw new CharConversionException("input exhausted");
                    }
                    char low = chars[offset++];

                    // make sure the next char is the low half of a UTF16 char
                    if( low < UTF16_LOW_START || low > UTF16_LOW_END ) {
                        throw new CharConversionException("UTF16 high "+
                            "character not followed by a UTF16 low character");
                    }

                    ucs4 = UTF16_BASE;
                    ucs4 += (c - UTF16_HIGH_START) << HALF_SHIFT;
                    ucs4 += low - UTF16_LOW_START;

                } else {
                    // this is a normal Unicode char
                    ucs4 = (c & 0x0000ffff);
                }

                out.write( (ucs4 & 0xff000000) >>> 24 );
                out.write( (ucs4 & 0x00ff0000) >>> 16 );
                out.write( (ucs4 & 0x0000ff00) >>> 8  );
                out.write( (ucs4 & 0x000000ff)        );
            }

            return out.toByteArray();
        }

    }
}
