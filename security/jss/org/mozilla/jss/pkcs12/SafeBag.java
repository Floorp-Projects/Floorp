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

package org.mozilla.jss.pkcs12;

import org.mozilla.jss.asn1.*;
import java.io.*;
import org.mozilla.jss.pkix.primitive.*;
import org.mozilla.jss.util.*;
import java.security.*;
import java.security.MessageDigest;
import org.mozilla.jss.crypto.*;
import org.mozilla.jss.CryptoManager;

/**
 * A PKCS #12 <i>SafeBag</i> structure.
 */
public final class SafeBag implements ASN1Value {

    ///////////////////////////////////////////////////////////////////////
    // members and member access
    ///////////////////////////////////////////////////////////////////////
    private OBJECT_IDENTIFIER bagType;
    private ANY bagContent;
    private SET bagAttributes; // may be null

    public OBJECT_IDENTIFIER getBagType() {
        return bagType;
    }

    /**
     * Returns the contents of this bag as an ANY.
     */
    public ANY getBagContent() {
        return bagContent;
    }

    /**
     * Returns the bagContent interpreted by type.
     * @return If type is KeyBag, a PrivateKeyInfo.
     *      <br>If type is PKCS-8ShroudedKeyBag, an EncryptedPrivateKeyInfo.
     *      <br>If type is CertBag, a CertBag.
     *      <br>For any other type, returns an ANY.
     */
    public ASN1Value getInterpretedBagContent() throws InvalidBERException {

        if( bagType.equals(KEY_BAG) ) {
            return bagContent.decodeWith(PrivateKeyInfo.getTemplate());
        } else if( bagType.equals(PKCS8_SHROUDED_KEY_BAG)) {
            return bagContent.decodeWith(EncryptedPrivateKeyInfo.getTemplate());
        } else if( bagType.equals(CERT_BAG) ) {
            return bagContent.decodeWith(CertBag.getTemplate());
        } else {
            return bagContent;
        }
    }

    /**
     * Returns the attributes of this bag. May return null if this bag
     * has no attributes.  Each element of the set is a
     * <code>org.mozilla.jss.pkix.primitive.Attribute</code>.
     */
    public SET getBagAttributes() {
       return bagAttributes;
    } 

    ///////////////////////////////////////////////////////////////////////
    // OIDs
    ///////////////////////////////////////////////////////////////////////

    /**
     * The OID branch for PKCS #12, version 1.0.
     */
    public static final OBJECT_IDENTIFIER PKCS12_VERSION_1=
                OBJECT_IDENTIFIER.PKCS12.subBranch(10);

    /**
     * The OID branch for the PKCS #12 bag types.
     */
    public static final OBJECT_IDENTIFIER PKCS12_BAG_IDS =
                PKCS12_VERSION_1.subBranch(1);

    /** 
     * A bag containing a private key.  The bag content is a <i>KeyBag</i>,
     * which is equivalent to a PKCS #8 <i>PrivateKeyInfo</i>
     */
    public static final OBJECT_IDENTIFIER KEY_BAG = PKCS12_BAG_IDS.subBranch(1);

    /**
     * A bag containing a private key encrypted à la PKCS #8.  The bag
     * content is a PKCS #8 <i>EncryptedPrivateKeyInfo</i>.
     */
    public static final OBJECT_IDENTIFIER PKCS8_SHROUDED_KEY_BAG =
            PKCS12_BAG_IDS.subBranch(2);

    /**
     * A bag containing a certificate.  The bag content is <code>CertBag</code>.
     */
    public static final OBJECT_IDENTIFIER CERT_BAG =
            PKCS12_BAG_IDS.subBranch(3);

    /**
     * A bag containing a certificate revocation list.
     * The bag content is <code>CRLBag</code>.
     */
    public static final OBJECT_IDENTIFIER CRL_BAG =
            PKCS12_BAG_IDS.subBranch(4);

    /**
     * A bag containing an arbitrary secret.  The bag content is
     * <code>SecretBag</code>.
     */
    public static final OBJECT_IDENTIFIER SECRET_BAG =
            PKCS12_BAG_IDS.subBranch(5);

    /**
     * A bag containing a nested SafeContent .  The bag content is
     * <i>SafeContents</i>, which is merely a SEQUENCE of SafeBag.
     */
    public static final OBJECT_IDENTIFIER SAFE_CONTENTS_BAG =
            PKCS12_BAG_IDS.subBranch(6);

    /**
     * A FriendlyName attribute. The value is a BMPString.
     */
    public static final OBJECT_IDENTIFIER FRIENDLY_NAME = 
            OBJECT_IDENTIFIER.PKCS9.subBranch(20);

    /**
     * A LocalKeyID attribute.  The value is an octet string.
     */
    public static final OBJECT_IDENTIFIER LOCAL_KEY_ID =
            OBJECT_IDENTIFIER.PKCS9.subBranch(21);

    ///////////////////////////////////////////////////////////////////////
    // Constructors
    ///////////////////////////////////////////////////////////////////////

    private SafeBag() { }

    /**
     * Creates a new SafeBag from its components.
     *
     * @param bagType The type of this bag.  For compatibility, it should
     *  be one of the constants defined in this class.
     * @param bagContent The contents of the bag. The type of this parameter
     *      is defined by the <code>bagType</code> parameter.
     * @param bagAttributes A SET of Attributes for this SafeBag.  Since
     *      attributes are optional, this parameter may be null.
     */
    public SafeBag(OBJECT_IDENTIFIER bagType, ASN1Value bagContent, 
                SET bagAttributes)
    {
        if( bagType==null || bagContent==null ) {
            throw new IllegalArgumentException("bagType or bagContent is null");
        }
        this.bagType = bagType;
        try {
            if( bagContent instanceof ANY ) {
                this.bagContent = (ANY) bagContent;
            } else {
                byte[] encoded = ASN1Util.encode(bagContent);
                this.bagContent = (ANY) ASN1Util.decode(ANY.getTemplate(),
                                                        encoded);
            }
        } catch( InvalidBERException e ) {
            Assert.notReached("failed to convert ASN1Value to ANY");
        }
        this.bagAttributes = bagAttributes;
    }

    /**
     * Creates a SafeBag that contains an X.509 Certificate.
     * The SafeBag will have a <i>localKeyID</i> attribute equal
     *  to the SHA-1 hash of the certificate, and a <i>friendlyName</i>
     *  attribute equal to the supplied string.  This is the way Communicator
     *  makes a CertBag.  The same <i>localKeyID</i> attribute should be stored
     *  in the matching private key bag.
     *
     * @param cert A DER-encoded X.509 certificate.
     * @param friendlyName Will be stored in the <i>friendlyName</i>
     *      attribute of the SafeBag.  Should be the nickname of the cert.
     */
    public static SafeBag
    createCertBag(byte[] cert, String friendlyName)
            throws DigestException, NoSuchAlgorithmException,
            InvalidBERException {
        return createCertBag(cert, friendlyName, getLocalKeyIDFromCert(cert));
    }

    /**
     * Creates a SafeBag that contains an X.509 Certificate.
     * The SafeBag will have the given <i>localKeyID</i> attribute,
     *  and a <i>friendlyName</i>
     *  attribute equal to the supplied string.  This is the way Communicator
     *  makes a CertBag.  The same <i>localKeyID</i> attribute should be stored
     *  in the matching private key bag.
     *
     * @param cert A DER-encoded X.509 certificate.
     * @param friendlyName Will be stored in the <i>friendlyName</i>
     *      attribute of the SafeBag.  Should be the nickname of the cert.
     * @param localKeyID The bytes to used for the localKeyID.  These should
     *      be obtained from the <code>getLocalKeyIDFromCert</code> method.
     * @exception InvalidBERException If the cert is not a valid DER encoding.
     * @see #getLocalKeyIDFromCert
     */
    public static SafeBag
    createCertBag(byte[] cert, String friendlyName, byte[] localKeyID)
            throws InvalidBERException {
      try {

        // create CertBag
        CertBag cb = new CertBag(CertBag.X509_CERT_TYPE, new ANY(cert) );

        // setup attributes
        SET attributes = new SET();
        // friendly name should be cert nickname
        attributes.addElement(new Attribute(
                    FRIENDLY_NAME,
                    new BMPString(friendlyName) ));
        attributes.addElement( new Attribute(
                    LOCAL_KEY_ID,
                    new OCTET_STRING(localKeyID) ));

        return new SafeBag(CERT_BAG, cb, attributes);
      } catch( CharConversionException e ) {
        throw new AssertionException("CharConversionException converting"+
            " Unicode to BMPString");
      }
    }

    /**
     * Computes the LocalKeyID attribute that should be stored with a key
     *  and certificate.
     *
     * @param derCert A DER-encoded X.509 certificate.
     * @return The SHA-1 hash of the cert, which should be used as the
     *      localKeyID attribute for the cert's SafeBag.
     */
    public static final byte[]
    getLocalKeyIDFromCert(byte[] derCert)
            throws DigestException, NoSuchAlgorithmException {
        MessageDigest digester = MessageDigest.getInstance("SHA-1");
        return digester.digest(derCert);
    }
        

    /**
     * Creates a SafeBag containing a PKCS-8ShroudedKeyBag, which is
     * an EncryptedPrivateKeyInfo. The key will be encrypted using
     *  a triple-DES PBE algorithm, using the supplied password.
     *
     * @param privk The PrivateKeyInfo containing the private key.
     * @param friendlyName The nickname for the key; should be the same
     *      as the nickname of the associated cert.
     * @param localKeyID The localKeyID for the key; should be the same as
     *      the localKeyID of the associated cert.
     * @param The password used to encrypt the private key.
     */
    public static SafeBag
    createEncryptedPrivateKeyBag(PrivateKeyInfo privk, String friendlyName,
            byte[] localKeyID, Password password)
            throws CryptoManager.NotInitializedException, TokenException
    {
      try {

        PBEAlgorithm pbeAlg = PBEAlgorithm.PBE_SHA1_DES3_CBC;
        final int DEFAULT_ITERATIONS = 1;
        byte[] salt = new byte[pbeAlg.getSaltLength()];

        JSSSecureRandom rand = CryptoManager.getInstance().getSecureRNG();
        rand.nextBytes(salt);

        EncryptedPrivateKeyInfo epki= EncryptedPrivateKeyInfo.createPBE(
                PBEAlgorithm.PBE_SHA1_DES3_CBC, password, salt,
                DEFAULT_ITERATIONS, new PasswordConverter(), privk);
        
        SET attributes = new SET();
        attributes.addElement(new Attribute(
                    FRIENDLY_NAME,
                    new BMPString(friendlyName) ));
        attributes.addElement( new Attribute(
                    LOCAL_KEY_ID,
                    new OCTET_STRING(localKeyID) ));

        return new SafeBag(PKCS8_SHROUDED_KEY_BAG , epki, attributes);
      } catch(java.security.NoSuchAlgorithmException e) {
            throw new AssertionException(
                "Unable to find PBE algorithm: "+e);
      } catch(java.security.InvalidKeyException e) {
            throw new AssertionException(
                "InvalidKeyException while creating EncryptedContentInfo: "+e);
      } catch(java.security.InvalidAlgorithmParameterException e) {
            throw new AssertionException(
                "InvalidAlgorithmParameterException while creating"+
                " EncryptedContentInfo: "+e);
      } catch(java.io.CharConversionException e) {
            throw new AssertionException(
                "CharConversionException while creating EncryptedContentInfo: "+
                e);
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
        encode(TAG, ostream);
    }

    public void encode(Tag implicitTag, OutputStream ostream)
        throws IOException
    {
        SEQUENCE seq = new SEQUENCE();

        seq.addElement( bagType );
        seq.addElement( new EXPLICIT(new Tag(0), bagContent) );
        if( bagAttributes!=null ) {
            seq.addElement( bagAttributes );
        }

        seq.encode(implicitTag, ostream);
    }

    private static final Template templateInstance = new Template();
    public static Template getTemplate() {
        return templateInstance;
    }

    /**
     * A template for decoding SafeBags.
     */
    public static class Template implements ASN1Template {

        private SEQUENCE.Template seqt;

        public Template() {
            seqt = new SEQUENCE.Template();

            seqt.addElement( OBJECT_IDENTIFIER.getTemplate() );
            seqt.addElement( new EXPLICIT.Template(
                                    new Tag(0),
                                    ANY.getTemplate() )
                            );
            seqt.addOptionalElement( new SET.OF_Template(
                                            Attribute.getTemplate() ) );
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
          try {

            SEQUENCE seq = (SEQUENCE) seqt.decode(implicitTag, istream);

            return new SafeBag( (OBJECT_IDENTIFIER) seq.elementAt(0),
                                ((EXPLICIT)seq.elementAt(1)).getContent(),
                                (SET) seq.elementAt(2) );

          } catch( InvalidBERException e ) {
            throw new InvalidBERException(e, "SafeBag");
          }
        }
    }
}
