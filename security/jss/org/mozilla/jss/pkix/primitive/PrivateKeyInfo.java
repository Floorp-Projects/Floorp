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

package org.mozilla.jss.pkix.primitive;

import org.mozilla.jss.asn1.*;
import java.io.*;
import org.mozilla.jss.crypto.PrivateKey;
import java.security.NoSuchAlgorithmException;

public class PrivateKeyInfo
    implements ASN1Value, java.security.PrivateKey {

    ///////////////////////////////////////////////////////////////////////
    // members and member access
    ///////////////////////////////////////////////////////////////////////
    private INTEGER version;
    private AlgorithmIdentifier privateKeyAlgorithm;
    private OCTET_STRING privateKey;
    private SET attributes; // may be null
    private SEQUENCE sequence;

    public INTEGER getVersion() {
        return version;
    }

    public AlgorithmIdentifier getPrivateKeyAlgorithm() {
        return privateKeyAlgorithm;
    }

    public String getAlgorithm() {
        try {
            return PrivateKey.Type.fromOID(privateKeyAlgorithm.getOID())
                        .toString();
        } catch( NoSuchAlgorithmException e ) {
            return null;
        }
    }

    public String getFormat() {
        return "PKCS#8";
    }

    public byte[] getEncoded() {
        return privateKey.toByteArray();
    }

    public OCTET_STRING getPrivateKey() {
        return privateKey;
    }

    /**
     * May return null if no attributes are present.
     */
    public SET getAttributes() {
        return attributes;
    }

    ///////////////////////////////////////////////////////////////////////
    // Constructors
    ///////////////////////////////////////////////////////////////////////

    private PrivateKeyInfo() { }

    /**
     * Create a PrivateKeyInfo from its components.
     *
     * @param attributes May be null if there are no attributes, in which
     *      case the attributes field will be omitted from the DER encoding.
     *      Each element must be a org.mozilla.jss.pkix.primitive.Attribute.
     */
    public PrivateKeyInfo(INTEGER version,
                AlgorithmIdentifier privateKeyAlgorithm,
                OCTET_STRING privateKey, SET attributes)
    {
        if( version==null || privateKeyAlgorithm==null || privateKey==null ) {
            throw new IllegalArgumentException(
                    "PrivateKeyInfo parameter is null");
        }

        this.version = version;
        this.privateKeyAlgorithm = privateKeyAlgorithm;
        this.privateKey = privateKey;
        this.attributes = attributes;

        sequence = new SEQUENCE();
        sequence.addElement(version);
        sequence.addElement(privateKeyAlgorithm);
        sequence.addElement(privateKey);

        if(attributes!=null) {
            sequence.addElement( new Tag(0), attributes );

            // make sure all the attributes are Attributes
            int size = attributes.size();
            for(int i=0; i < size; i++) {
                if( ! (attributes.elementAt(i) instanceof Attribute) ) {
                    throw new IllegalArgumentException("element "+i+
                        " of attributes is not an Attribute");
                }
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////
    // DER encoding
    ///////////////////////////////////////////////////////////////////////

    private static final Tag TAG = SEQUENCE.TAG;

    public Tag getTag() {
        return TAG;
    }

    public void encode(OutputStream ostream) throws IOException {
        sequence.encode(ostream);
    }

    public void encode(Tag implicitTag, OutputStream ostream)
        throws IOException
    {
        sequence.encode(implicitTag, ostream);
    }

    private static final Template templateInstance = new Template();
    public static Template getTemplate() {
        return templateInstance;
    }

    /**
     * A template class for decoding PrivateKeyInfos from BER.
     */
    public static class Template implements ASN1Template {

        private SEQUENCE.Template seqt;

        public Template() {
            seqt = new SEQUENCE.Template();

            seqt.addElement( INTEGER.getTemplate() );
            seqt.addElement( AlgorithmIdentifier.getTemplate() );
            seqt.addElement( OCTET_STRING.getTemplate() );
            seqt.addOptionalElement( new Tag(0),
                        new SET.OF_Template( Attribute.getTemplate() ) );
        }

        public boolean tagMatch(Tag tag) {
            return TAG.equals(tag);
        }

        public ASN1Value decode(InputStream istream)
            throws InvalidBERException, IOException
        {
            return decode(TAG, istream);
        }

        public ASN1Value decode(Tag implicitTag, InputStream istream)
            throws InvalidBERException, IOException
        {
            SEQUENCE seq = (SEQUENCE) seqt.decode(implicitTag, istream);

            return new PrivateKeyInfo( (INTEGER) seq.elementAt(0),
                                        (AlgorithmIdentifier) seq.elementAt(1),
                                        (OCTET_STRING) seq.elementAt(2),
                                        (SET) seq.elementAt(3) );
        }
    }
}
