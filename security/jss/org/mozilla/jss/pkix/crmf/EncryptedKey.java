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

package org.mozilla.jss.pkix.crmf;

import org.mozilla.jss.asn1.*;
import org.mozilla.jss.util.Assert;
import java.io.*;

public class EncryptedKey implements ASN1Value {

    /**
     * The type of EncryptedKey.
     */
    public static class Type {
        private Type() { }

        static final Type ENCRYPTED_VALUE = new Type();
        static final Type ENVELOPED_DATA = new Type();
    }
    public static final Type ENCRYPTED_VALUE = Type.ENCRYPTED_VALUE;
    public static final Type ENVELOPED_DATA = Type.ENVELOPED_DATA;


    ///////////////////////////////////////////////////////////////////////
    // member and member access
    ///////////////////////////////////////////////////////////////////////
    private Type type;
    private EncryptedValue encryptedValue;
    private ANY envelopedData;

    public Type getType() {
        return type;
    }

    /**
     * Should only be called if <code>getType</code> returns
     * <code>ENCRYPTED_VALUE</code>.
     */
    public EncryptedValue getEncryptedValue() {
        return encryptedValue;
    }

    /**
     * Should only be called if <code>getType</code> returns
     * <code>ENVELOPED_DATA</code>. ANY is returned to prevent a circular
     * dependency between the org.mozilla.jss.pkcs7 package and the
     * org.mozilla.jss.pkix hierarchy.
     */
    public ANY getEnvelopedData() {
        return envelopedData;
    }

    ///////////////////////////////////////////////////////////////////////
    // Constructors
    ///////////////////////////////////////////////////////////////////////

    private EncryptedKey() { }

    public EncryptedKey(EncryptedValue encryptedValue) {
        this.type = ENCRYPTED_VALUE;
        this.encryptedValue = encryptedValue;
        this.tag = SEQUENCE.TAG;
    }

    public EncryptedKey(ANY envelopedData) {
        this.type = ENVELOPED_DATA;
        this.envelopedData = envelopedData;
        this.tag = new Tag(0);
    }

    ///////////////////////////////////////////////////////////////////////
    // encoding/decoding
    ///////////////////////////////////////////////////////////////////////

    private Tag tag; // set by constructor based on type
    public Tag getTag() {
        return tag;
    }

    public void encode(OutputStream ostream) throws IOException {
        encode(getTag(), ostream);
    }

    public void encode(Tag implicitTag, OutputStream ostream)
            throws IOException {

        // no IMPLICIT tags allowed on ANY
        Assert._assert( getTag().equals(implicitTag));

        if( type == ENCRYPTED_VALUE ) {
            Assert._assert( encryptedValue != null );
            encryptedValue.encode(implicitTag, ostream);
        } else {
            Assert._assert(type == ENVELOPED_DATA);
            Assert._assert(envelopedData != null);
            envelopedData.encode(implicitTag, ostream);
        }
    }

    private static final Template templateInstance = new Template();
    public static Template getTemplate() {
        return templateInstance;
    }

    /**
     * A Template for decoding BER-encoded EncryptedKeys.
     */
    public static class Template implements ASN1Template {

        private CHOICE.Template choicet;

        public Template() {
            choicet = new CHOICE.Template();

            choicet.addElement( EncryptedValue.getTemplate() );
            choicet.addElement( new Tag(0), ANY.getTemplate() );
        }

        public boolean tagMatch(Tag tag) {
            return choicet.tagMatch(tag);
        }

        public ASN1Value decode(InputStream istream)
                throws InvalidBERException, IOException {
          try {

            CHOICE choice = (CHOICE) choicet.decode(istream);

            if( choice.getTag().equals(SEQUENCE.TAG) ) {
                return new EncryptedKey( (EncryptedValue) choice.getValue() );
            } else {
                Assert._assert( choice.getTag().equals(new Tag(0)) );
                return new EncryptedKey( (ANY) choice.getValue() );
            }

          } catch(InvalidBERException e) {
                throw new InvalidBERException(e, "EncryptedKey");
          }
        }

        /**
         * @param implicitTag This parameter is ignored, because a CHOICE
         *      cannot have an implicitTag.
         */
        public ASN1Value decode(Tag implicitTag, InputStream istream)
                throws InvalidBERException, IOException {
            Assert.notReached("EncryptedKey, being a CHOICE, cannot be"+
                " implicitly tagged");
            return decode(istream);
        }
    }
}
