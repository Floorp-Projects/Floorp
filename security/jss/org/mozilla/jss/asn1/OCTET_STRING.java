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

import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.io.ByteArrayOutputStream;

public class OCTET_STRING implements ASN1Value {

    public static final Tag TAG = new Tag(Tag.Class.UNIVERSAL, 4);
    public Tag getTag() {
        return TAG;
    }
    public static final Form FORM = Form.PRIMITIVE;

    byte[] data;

    private OCTET_STRING() { }

    public OCTET_STRING( byte[] data ) {
        this.data = data;
    }

    public byte[] toByteArray() {
        return data;
    }

    public void encode(OutputStream ostream) throws IOException {
        // use getTag() so we can be subclassed
        encode(getTag(), ostream);
    }

    public void encode(Tag implicitTag, OutputStream ostream)
        throws IOException
    {
        ASN1Header head = new ASN1Header(implicitTag, FORM, data.length);

        head.encode(ostream);

        ostream.write(data);
    }

    private static final Template templateInstance = new Template();
    public static Template getTemplate() {
        return templateInstance;
    }

public static class Template implements ASN1Template {

    public Tag getTag() {
        return TAG;
    }

    public boolean tagMatch(Tag tag) {
        return( TAG.equals(tag) );
    }

    public ASN1Value decode(InputStream istream)
        throws IOException, InvalidBERException
    {   
        return decode(getTag(), istream);
    }

    // this can be overridden by subclasses
    protected ASN1Value generateInstance(byte[] bytes) {
        return new OCTET_STRING( bytes );
    }

    // this can be overridden by subclasses
    protected String getName() {
        return "OCTET_STRING";
    }

    public ASN1Value decode(Tag implicitTag, InputStream istream)
        throws IOException, InvalidBERException
    {
      try {
        ASN1Header head = new ASN1Header(istream);

        head.validate(implicitTag);

        byte[] data;

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

            data = bos.toByteArray();
        } else {
            data = new byte[ (int) head.getContentLength() ];
            ASN1Util.readFully(data, istream);
        }

        return generateInstance(data);

      } catch( InvalidBERException e ) {
        throw new InvalidBERException(e, getName());
      }
    }

} // end of Template

}
