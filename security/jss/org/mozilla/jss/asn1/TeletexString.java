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

import java.io.CharConversionException;

/**
 * The ASN.1 type <i>TeletexString</i>.
 */
public class TeletexString extends CharacterString implements ASN1Value {

    public static final Tag TAG = new Tag(Tag.UNIVERSAL, 20);
    public Tag getTag() {
        return TAG;
    }

    public TeletexString(char[] chars) throws CharConversionException {
        super(chars);
    }

    public TeletexString(String s) throws CharConversionException {
        super(s);
    }

    CharConverter getCharConverter() {
        return new TeletexConverter();
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
        return new TeletexConverter();
    }

    protected CharacterString generateInstance(char[] bytes)
        throws CharConversionException
    {
        return new TeletexString( bytes );
    }

    protected String typeName() {
        return "TeletexString";
    }
} // end of Template

    private static class TeletexConverter implements CharConverter {

        public char[] byteToChar(byte[] bytes, int offset, int len)
            throws CharConversionException
        {
            char[] chars = new char[len];

            int b;
            int c;
            for(b=offset, c=0; c < len; b++, c++) {
                chars[c] = (char) (bytes[b] & 0xff);
            }
            return chars;
        }

        public byte[] charToByte(char[] chars, int offset, int len)
            throws CharConversionException
        {
            byte[] bytes = new byte[len];

            int b;
            int c;
            for(b=0, c=offset; b < len; b++, c++) {
                if( (chars[c]&0xff00) != 0 ) {
                    throw new CharConversionException("Invalid character for"+
                        " TeletexString");
                }
                bytes[b] = (byte) (chars[c] & 0xff);
            }
            return bytes;
        }
    }
}
