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
import java.io.UnsupportedEncodingException;
import org.mozilla.jss.util.Assert;

public class UTF8String extends CharacterString implements ASN1Value {

    public UTF8String(char[] chars) throws CharConversionException {
        super(chars);
    }

    public UTF8String(String s) throws CharConversionException {
        super(s);
    }

    CharConverter getCharConverter() {
        return new UTF8Converter();
    }

    public static final Tag TAG = new Tag( Tag.UNIVERSAL, 12 );
    public static final Form FORM = Form.PRIMITIVE;

    public Tag getTag() {
        return TAG;
    }

    private static final Template templateInstance = new Template();
    /**
     * Returns a singleton instance of UTF8String.Template. This is more
     * efficient than creating a new UTF8String.Template.
     */
    public static Template getTemplate() {
        return templateInstance;
    }

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
        return new UTF8Converter();
    }

    protected CharacterString generateInstance(char[] chars)
        throws CharConversionException
    {
        return new UTF8String(chars);
    }

    protected String typeName() {
        return "UTF8String";
    }
}

private static class UTF8Converter implements CharConverter {

    public char[] byteToChar(byte[] bytes, int offset, int len)
        throws CharConversionException
    {
        try {

            String s = new String(bytes, offset, len, "UTF8");
            return s.toCharArray();

        } catch( UnsupportedEncodingException e ) {
            String err = "Unable to find UTF8 encoding mechanism";
            Assert.notReached(err);
            throw new CharConversionException(err);
        }
    }

    public byte[] charToByte(char[] chars, int offset, int len)
        throws CharConversionException
    {
        try {

            String s = new String(chars, offset, len);
            return s.getBytes("UTF8");

        } catch( UnsupportedEncodingException e ) {
            String err = "Unable to find UTF8 encoding mechanism";
            Assert.notReached(err);
            throw new CharConversionException(err);
        }
    }
} // end of char converter

}
