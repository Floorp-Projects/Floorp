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
import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.io.ByteArrayOutputStream;

/**
 * An abstract base class for all character string types in ASN.1.
 */
public abstract class CharacterString implements ASN1Value {

    abstract CharConverter getCharConverter();

    public abstract Tag getTag();
    static final Form FORM = Form.PRIMITIVE;

    private char[] chars;

    /**
     * Converts this ASN.1 character string to a Java String.
     */
    public String toString() {
        return new String(chars);
    }

    /**
     * Converts this ASN.1 character string to an array of Java characters.
     */
    public char[] toCharArray() {
        return chars;
    }

    protected CharacterString(char[] chars) throws CharConversionException {
        this.chars = chars;
        cachedContents = computeContents();
    }

    protected CharacterString(String s) throws CharConversionException {
        this.chars = s.toCharArray();
        cachedContents = computeContents();
    }

    private byte[] cachedContents;

    private byte[] getEncodedContents() {
        return cachedContents;
    }

    private byte[] computeContents() throws CharConversionException {
        CharConverter converter = getCharConverter();

        byte[] contents = converter.charToByte(chars, 0, chars.length);

        return contents;
    }

    public void encode(OutputStream ostream) throws IOException {
        encode( getTag(), ostream );
    }

    public void encode( Tag implicitTag, OutputStream ostream )
        throws IOException
    {
        byte[] contents = getEncodedContents();
        ASN1Header head = new ASN1Header( implicitTag, FORM, contents.length);

        head.encode(ostream);

        ostream.write( contents );
    }

public abstract static class Template implements ASN1Template {

    /**
     * Must be overridden to return the tag for the subclass.
     */
    protected abstract Tag getTag();

    public abstract boolean tagMatch(Tag tag);

    /**
     * Must be overridden to return the correct character converter
     * for the subclass.
     */
    protected abstract CharConverter getCharConverter();

    /**
     * Must be overridden to create an instance of the subclass given
     * a char array.
     */
    protected abstract CharacterString generateInstance(char[] chars)
        throws CharConversionException;

    /**
     * Must be overridden to provide the name of the subclass, for including
     * into error messages.
     */
    protected abstract String typeName();

    public ASN1Value decode(InputStream istream)
        throws IOException, InvalidBERException
    {
        return decode(getTag(), istream);
    }

    public ASN1Value decode(Tag implicitTag, InputStream istream)
        throws IOException, InvalidBERException
    {
      try {
        ASN1Header head = new ASN1Header(istream);

        head.validate(implicitTag);

        byte[] raw; // raw bytes, not translated to chars yet

        if( head.getContentLength() == -1 ) {
            // indefinite length encoding
            ASN1Header ahead;
            ByteArrayOutputStream bos = new ByteArrayOutputStream();
            do {
                ahead = ASN1Header.lookAhead( istream );
                if( ! ahead.isEOC() ) {
                    OCTET_STRING.Template ot = new OCTET_STRING.Template();
                    OCTET_STRING os = (OCTET_STRING) ot.decode(istream);
                    bos.write( os.toByteArray() );
                }
            } while( ! ahead.isEOC() );

            // consume EOC
            ahead = new ASN1Header(istream);

            raw = bos.toByteArray();
        } else {
            // definite length
            raw = new byte[ (int) head.getContentLength() ];
            ASN1Util.readFully(raw, istream);
        }

        char[] chars = getCharConverter().byteToChar(raw, 0, raw.length);

        return generateInstance(chars);

      } catch( CharConversionException e ) {
        throw new InvalidBERException(e.getMessage());
      } catch( InvalidBERException e ) {
        throw new InvalidBERException(e, typeName());
      }
    }
}

}
