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

import java.io.IOException;
import java.io.InputStream;

/**
 * Represents an ASN.1 <code>ENUMERATED</code> value. This has the same
 * interface as <code>INTEGER</code>
 */
public class ENUMERATED extends INTEGER implements ASN1Value {

    public static final Tag TAG = new Tag(Tag.Class.UNIVERSAL, 10);
    public Tag getTag() {
        return TAG;
    }

    /**
     * Creates a new ENUMERATED value from a long.
     */
    public ENUMERATED( long val ) {
        super( val );
    }

    ENUMERATED( byte[] valBytes ) {
        super( valBytes );
    }

    /**
     * Returns the value as a long.
     */
    public long getValue() {
        return longValue();
    }

    private static final ENUMERATED.Template templateInstance =
                                            new ENUMERATED.Template();
    public static ASN1Template getTemplate() {
        return templateInstance;
    }


/**
 * A template for decoding ENUMERATED values from their BER encodings.
 * The template reads the value as an INTEGER.  It does not check that it
 * is a valid value for the ENUMERATED type.
 */
public static class Template
    extends INTEGER.Template implements ASN1Template
{
    Tag getTag() {
        return ENUMERATED.TAG;
    }
    public boolean tagMatch(Tag tag) {
        return( tag.equals(ENUMERATED.TAG) );
    }

    public ASN1Value
    decode(Tag tag, InputStream derStream)
        throws InvalidBERException, IOException
    {
      try {
        ASN1Header wrapper = new ASN1Header(derStream);

        wrapper.validate(tag, FORM);

        // Is length < 1 ?
        if( wrapper.getContentLength() < 1 ) {
            throw new InvalidBERException("Invalid 0 length for ENUMERATED");
        }

        byte[] valBytes = new byte[ (int) wrapper.getContentLength() ];
        ASN1Util.readFully(valBytes, derStream);
        return new ENUMERATED( valBytes );

      } catch(InvalidBERException e) {
        throw new InvalidBERException(e, "ENUMERATED");
      }
    }

} // end of Template

}
