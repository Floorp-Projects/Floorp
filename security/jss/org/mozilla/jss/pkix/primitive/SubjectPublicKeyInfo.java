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
import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import org.mozilla.jss.util.Assert;
import java.security.PublicKey;
import java.security.NoSuchAlgorithmException;
import org.mozilla.jss.crypto.PrivateKey;
import org.mozilla.jss.crypto.InvalidKeyFormatException;
import org.mozilla.jss.pkcs11.PK11PubKey;

/**
 * A <i>SubjectPublicKeyInfo</i>, which stores information about a public key.
 * This class implements <code>java.security.PublicKey</code>.
 */
public class SubjectPublicKeyInfo extends java.security.spec.X509EncodedKeySpec
    implements ASN1Value, java.security.PublicKey {

    private AlgorithmIdentifier algorithm;
    private BIT_STRING subjectPublicKey;

    public String getAlgorithm() {
        try {
            return PrivateKey.Type.fromOID(algorithm.getOID()).toString();
        } catch( NoSuchAlgorithmException e ) {
            // unknown algorithm
            return null;
        }
    }

    public byte[] getEncoded() {
        return ASN1Util.encode(this);
    }
        

    public AlgorithmIdentifier getAlgorithmIdentifier() {
        return algorithm;
    }

    public BIT_STRING getSubjectPublicKey() {
        return subjectPublicKey;
    }

    private SubjectPublicKeyInfo() { super(new byte[] {0});}

    public SubjectPublicKeyInfo(AlgorithmIdentifier algorithm,
        BIT_STRING subjectPublicKey)
    {
        super( new byte[] {0} ); // super constructor can't handle null
        this.algorithm = algorithm;
        this.subjectPublicKey = subjectPublicKey;
    }

    public SubjectPublicKeyInfo(PublicKey pubk)
            throws InvalidBERException, IOException
    {
        super( new byte[] {0});
        SubjectPublicKeyInfo spki = (SubjectPublicKeyInfo)
            ASN1Util.decode( getTemplate(), pubk.getEncoded() );
        algorithm = spki.algorithm;
        subjectPublicKey = spki.subjectPublicKey;
    }

    public static final Tag TAG = SEQUENCE.TAG;

    public Tag getTag() {
        return TAG;
    }

    public void encode(OutputStream ostream) throws IOException {
        encode(TAG, ostream);
    }

    public void encode(Tag implicit, OutputStream ostream)
        throws IOException
    {
        SEQUENCE seq = new SEQUENCE();
        seq.addElement( algorithm );
        seq.addElement( subjectPublicKey );
        seq.encode( implicit, ostream );
    }

    private static final Template templateInstance = new Template();
    public static Template getTemplate() {
        return templateInstance;
    }

    /**
     * Creates a PublicKey from the public key information. Currently
     *      only RSA and DSA keys can be converted.
     *
     * @exception NoSuchAlgorithmException If the cryptographic provider
     *      does not recognize the algorithm for this public key.
     * @exception InvalidKeyFormatException If the subjectPublicKey could
     *      not be decoded correctly.
     */
    public PublicKey toPublicKey() throws NoSuchAlgorithmException,
            InvalidKeyFormatException
    {
        PrivateKey.Type type = PrivateKey.Type.fromOID( algorithm.getOID() );

        if( subjectPublicKey.getPadCount() != 0 ) {
            throw new InvalidKeyFormatException();
        }

        return PK11PubKey.fromRaw(type, subjectPublicKey.getBits() );
    }

    public static class Template implements ASN1Template {

        private SEQUENCE.Template seqt;

        public Template() {
            seqt = new SEQUENCE.Template();

            seqt.addElement( AlgorithmIdentifier.getTemplate() );
            seqt.addElement( BIT_STRING.getTemplate() );
        }

        public boolean tagMatch(Tag tag) {
            return TAG.equals(tag);
        }

        public ASN1Value decode(InputStream istream)
            throws IOException, InvalidBERException
        {
            return decode(TAG, istream);
        }

        public ASN1Value decode(Tag implicit, InputStream istream)
            throws IOException, InvalidBERException
        {
            SEQUENCE seq = (SEQUENCE) seqt.decode(implicit, istream);

            return new SubjectPublicKeyInfo(
                    (AlgorithmIdentifier) seq.elementAt(0),
                    (BIT_STRING) seq.elementAt(1)
            );
        }
    }
}
