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

package org.mozilla.jss.pkix.crmf;

import org.mozilla.jss.asn1.*;
import java.io.*;
import org.mozilla.jss.util.Assert;

public class PKIArchiveOptions implements ASN1Value {

    /**
     * A type of PKIArchiveOption.
     */
    public static class Type {
        private Type() { }

        static final Type ENCRYPTED_PRIV_KEY = new Type();
        static final Type KEY_GEN_PARAMETERS = new Type();
        static final Type ARCHIVE_REM_GEN_PRIV_KEY = new Type();
    }

    public static final Type ENCRYPTED_PRIV_KEY = Type.ENCRYPTED_PRIV_KEY;
    public static final Type KEY_GEN_PARAMETERS = Type.KEY_GEN_PARAMETERS;
    public static final Type ARCHIVE_REM_GEN_PRIV_KEY =
        Type.ARCHIVE_REM_GEN_PRIV_KEY;

    ///////////////////////////////////////////////////////////////////////
    // members and member access
    ///////////////////////////////////////////////////////////////////////
    private EncryptedKey encryptedPrivKey;
    private OCTET_STRING keyGenParameters;
    private boolean archiveRemGenPrivKey;
    private Type type;


    /**
     * Returns the type of PKIArchiveOptions.
     */
    public Type getType() {
        return type;
    }

    /**
     * Returns the encrypted key. Should only be called if the type
     * is <code>ENCRYPTED_PRIV_KEY</code>.
     */
    public EncryptedKey getEncryptedKey( ) {
        Assert._assert(type == ENCRYPTED_PRIV_KEY);
        return encryptedPrivKey;
    }

    /**
     * Returns the key gen parameters. Should only be called if the type
     * is <code>KEY_GEN_PARAMETERS</code>.
    public byte[] getKeyGenParameters( ) {
        Assert._assert(type == KEY_GEN_PARAMETERS);
        return keyGenParameters;
    }

    /**
     * Returns the archiveRemGenPrivKey field, which indicates that 
     * the sender wishes the receiver to generate and archive a key pair.
     * Should only be called if the type is
     * <code>ARCHIVE_REM_GEN_PRIV_KEY</code>.
     */
    public boolean getArchiveRemGenPrivKey( ) {
        Assert._assert( type == ARCHIVE_REM_GEN_PRIV_KEY );
        return archiveRemGenPrivKey;
    }

    ///////////////////////////////////////////////////////////////////////
    // constructors
    ///////////////////////////////////////////////////////////////////////
    private PKIArchiveOptions() { }

    public PKIArchiveOptions( EncryptedKey eKey ) {
        encryptedPrivKey = eKey;
        type = ENCRYPTED_PRIV_KEY;
        tag = new Tag(0);
    }

    public PKIArchiveOptions( byte[] keyGenParameters ) {
        this.keyGenParameters = new OCTET_STRING(keyGenParameters);
        type = KEY_GEN_PARAMETERS;
        tag = new Tag(1);
    }

    public PKIArchiveOptions( boolean archiveRemGenPrivKey ) {
        this.archiveRemGenPrivKey = archiveRemGenPrivKey;
        type = ARCHIVE_REM_GEN_PRIV_KEY;
        tag = new Tag(2);
    }

    ///////////////////////////////////////////////////////////////////////
    // encoding/decoding
    ///////////////////////////////////////////////////////////////////////
    private Tag tag; // set by the constructor depending on the type
    public Tag getTag() {
        return tag;
    }

    /**
     * DER-encodes a PKIArchiveOptions.
     */
    public void encode(OutputStream ostream) throws IOException {
        encode( getTag(), ostream );
    }

    /**
     * DER-encodes a PKIArchiveOptions.
     * @param implicitTag <b>This parameter is ignored.</b> A CHOICE cannot
     *      have an implicit tag.
     */
    public void encode(Tag implicitTag, OutputStream ostream)
        throws IOException
    {
        // no implicit tags on a CHOICE
        Assert._assert( implicitTag.equals(tag) );

        if( type == ENCRYPTED_PRIV_KEY ) {
            // CHOICEs are always EXPLICITly tagged
            EXPLICIT explicit = new EXPLICIT( new Tag(0), encryptedPrivKey );
            explicit.encode(tag, ostream);
        } else if( type == KEY_GEN_PARAMETERS ) {
            keyGenParameters.encode(tag, ostream);
        } else {
            Assert._assert( type == ARCHIVE_REM_GEN_PRIV_KEY );
            (new BOOLEAN(archiveRemGenPrivKey)).encode(tag, ostream);
        }
    }

    private static final Template templateInstance = new Template();

    public static Template getTemplate() {
        return templateInstance;
    }

    /**
     * A template for decoding PKIArchiveOptions.
     */
    public static class Template implements ASN1Template {

        CHOICE.Template template;

        public Template() {
            template = new CHOICE.Template();

            // CHOICEs are always EXPLICIT
            template.addElement( new EXPLICIT.Template(
                                        new Tag(0),
                                        new EncryptedKey.Template() ));

            template.addElement( new Tag(1), new OCTET_STRING.Template() );
            template.addElement( new Tag(2), new BOOLEAN.Template()      );
        }

        /**
         * Returns true if the given tag can satisfy this CHOICE.
         */
        public boolean tagMatch(Tag tag) {
            return template.tagMatch(tag);
        }

        /**
         * Decodes a PKIArchiveOptions.
         * @return A PKIArchiveOptions object.
         */
        public ASN1Value decode(InputStream istream)
            throws IOException, InvalidBERException
        {
            CHOICE choice = (CHOICE) template.decode(istream);

            if( choice.getTag().getNum() == 0 ) {
                EncryptedKey ekey = (EncryptedKey) ((EXPLICIT)choice.getValue()).getContent();
                return new PKIArchiveOptions(ekey);
            } else if( choice.getTag().getNum() == 1 ) {
                OCTET_STRING kgp = (OCTET_STRING) choice.getValue();
                return new PKIArchiveOptions(kgp.toByteArray());
            } else if( choice.getTag().getNum() == 2 ) {
                BOOLEAN arckey = (BOOLEAN) choice.getValue();
                return new PKIArchiveOptions(arckey.toBoolean());
            } else {
                String s = "Unrecognized tag in PKIArchiveOptions";
                Assert.notReached(s);
                throw new InvalidBERException(s);
            }
        }

        /**
         * Decodes a PKIArchiveOptions.
         * @param implicitTag <b>This parameter is ignored.</b> Since
         *  PKIArchiveOptions is a CHOICE, it cannot have an implicit tag.
         * @return A PKIArchiveOptions object.
         */
        public ASN1Value decode(Tag implicitTag, InputStream istream)
            throws IOException, InvalidBERException
        {
            return decode(istream);
        }
    }
}
