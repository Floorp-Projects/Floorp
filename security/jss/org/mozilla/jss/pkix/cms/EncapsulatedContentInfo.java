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
package org.mozilla.jss.pkix.cms;

import java.io.*;
import org.mozilla.jss.asn1.*;
import java.util.Vector;
import org.mozilla.jss.util.Assert;
import java.math.BigInteger;
import java.io.ByteArrayInputStream;

/**
 * A CMS EncapsulatedContentInfo structure.
 */
public class EncapsulatedContentInfo implements ASN1Value {

    public static final Tag TAG = SEQUENCE.TAG; // XXX is this right?


    private OBJECT_IDENTIFIER contentType;
    private OCTET_STRING content;
    private SEQUENCE sequence = new SEQUENCE();

    private EncapsulatedContentInfo() {}

    /**
     * Creates a EncapsulatedContentInfo with the given type and content.
     *
     * @param contentType The contentType of the EncapsulatedContentInfo.
     * @param content The content of the EncapsulatedContentInfo. May be <code>null</code>
     *      to signify that the optional content field is not present.
     */
    public EncapsulatedContentInfo(OBJECT_IDENTIFIER contentType, ASN1Value content) {
        this.contentType = contentType;
        sequence.addElement(contentType);
        if (content != null) {
            if( content instanceof OCTET_STRING) {
                this.content = (OCTET_STRING) content;
            } else {
                // convert content to OCTET_STRING
                this.content = (OCTET_STRING) new OCTET_STRING(
                                    ASN1Util.encode(content) );
            }
            sequence.addElement(new EXPLICIT(new Tag(0), this.content) );
        }
    }

    /**
     * Returns the contentType field, which determines what kind of content
     * is contained in this EncapsulatedContentInfo.  
     */
    public OBJECT_IDENTIFIER getContentType() {
        return contentType;
    }

    /**
     * Returns <code>true</code> if the content field is present.
     */
    public boolean hasContent() {
        return (content != null);
    }

    /**
     * Returns the content encoded as an OCTET_STRING. If there is no content,
     * <code>null</code> is returned.
     */
    public OCTET_STRING getContent() {
        return content;
    }

    public void encode(OutputStream ostream) throws IOException {
        encode(getTag(),ostream);
    }

    public void encode(Tag implicitTag, OutputStream ostream)
        throws IOException
    {
        sequence.encode(implicitTag,ostream);
    }

    public Tag getTag() {
        return EncapsulatedContentInfo.TAG;
    }

    /**
     * Returns a singleton instance of a decoding template for EncapsulatedContentInfo.
     */
    public static Template getTemplate() {
        return templateInstance;
    }
    private static Template templateInstance = new Template();

    /**
     * A template for decoding a EncapsulatedContentInfo blob
     *
     */
    public static class Template implements ASN1Template {
        public boolean tagMatch(Tag tag) {
            return (tag.equals(EncapsulatedContentInfo.TAG));
        }

        private SEQUENCE.Template seqt;

        public Template() {
            seqt = new SEQUENCE.Template();
            seqt.addElement(new OBJECT_IDENTIFIER.Template());
            seqt.addOptionalElement(
               new EXPLICIT.Template(
                         new Tag(0), new OCTET_STRING.Template()
                        ));
        }

        public ASN1Value decode(InputStream istream) 
            throws IOException, InvalidBERException
            {
                return decode(EncapsulatedContentInfo.TAG,istream);
            }


        public ASN1Value decode(Tag implicitTag, InputStream istream )
            throws IOException, InvalidBERException
            {
                SEQUENCE seq = (SEQUENCE) seqt.decode(implicitTag,istream);
                Assert._assert(seq.size() == 2);
                ASN1Value content;

                if( seq.elementAt(1) == null ) {
                    content = null;
                } else {
                    content = ((EXPLICIT)seq.elementAt(1)).getContent();
                }

                return new EncapsulatedContentInfo(
                    (OBJECT_IDENTIFIER) seq.elementAt(0),
                    content
                    );
            }
    } // end of template

}



