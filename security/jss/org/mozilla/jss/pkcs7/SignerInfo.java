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

package org.mozilla.jss.pkcs7;

import java.io.*;
import org.mozilla.jss.asn1.*;
import org.mozilla.jss.util.Assert;
import org.mozilla.jss.pkix.primitive.*;
import org.mozilla.jss.crypto.*;
import java.util.Vector;
import java.math.BigInteger;
import java.io.ByteArrayInputStream;
import java.security.InvalidKeyException;
import java.security.SignatureException;
import java.security.NoSuchAlgorithmException;
import java.security.MessageDigest;
import org.mozilla.jss.crypto.*;
import org.mozilla.jss.*;
import java.security.PublicKey;

/*
 * The high-level interface takes care of all
 * the logic and cryptography.  If you need to use the low-level interface,
 * please let us know what you are using it for. Perhaps it should be part
 * of the high-level interface.
 */

/**
 * A PKCS #7 <i>SignerInfo</i>.
 */
public class SignerInfo implements ASN1Value {

    ////////////////////////////////////////////////
    // PKCS #9 attributes
    ////////////////////////////////////////////////
    private static final OBJECT_IDENTIFIER
        CONTENT_TYPE = OBJECT_IDENTIFIER.PKCS.subBranch(9).subBranch(3);
    private static final OBJECT_IDENTIFIER
        MESSAGE_DIGEST = OBJECT_IDENTIFIER.PKCS.subBranch(9).subBranch(4);

    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////
    // Members
    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////

    private INTEGER version;
    private IssuerAndSerialNumber issuerAndSerialNumber;
    private AlgorithmIdentifier digestAlgorithm;
    private SET authenticatedAttributes; // [0] OPTIONAL
    private AlgorithmIdentifier digestEncryptionAlgorithm;
    private OCTET_STRING encryptedDigest;
    private SET unauthenticatedAttributes; // [1] OPTIONAL

    // we only do version 1 of PKCS #7
    private static final INTEGER VERSION = new INTEGER(1);

    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////
    // Accessor methods
    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////

    /**
     * Retrieves the version number of this SignerInfo.
     */
    public INTEGER getVersion() {
        return version;
    }

    /**
     * Low-level method to set the version.
     * It is not normally necessary to call this.  Use it at your own risk.
    public void setVersion(INTEGER version) {
        this.version = version;
    }
     */

    /**
     * Retrieves the issuer and serial number of the certificate whose
     * private key was used to sign the SignerInfo.
     */
    public IssuerAndSerialNumber getIssuerAndSerialNumber() {
        return issuerAndSerialNumber;
    }

    /**
     * Low-level method to set the issuerAndSerialNumber.
     * It is not normally necessary to call this.  Use it at your own risk.
    public void setIssuerAndSerialNumber( IssuerAndSerialNumber iasn ) {
        this.issuerAndSerialNumber = iasn;
    }
     */

    /**
     * Retrieves the DigestAlgorithm used in this SignerInfo.
     *
     * @exception NoSuchAlgorithm If the algorithm is not recognized by JSS.
     */
    public DigestAlgorithm getDigestAlgorithm()
        throws NoSuchAlgorithmException
    {
        return DigestAlgorithm.fromOID( digestAlgorithm.getOID() );
    }


    /**
     * Retrieves the DigestAlgorithmIdentifier used in this SignerInfo.
     */
    public AlgorithmIdentifier getDigestAlgorithmIdentifer() {
        return digestAlgorithm;
    }

    /**
     * Low-level method to set the digest AlgorithmIdentifier.
     * It is not normally necessary to call this.  Use it at your own risk.
    public void setDigestAlgorithmIdentifier(AlgorithmIdentifier algid) {
        this.digestAlgorithm = algid;
    }
     */

    /**
     * Retrieves the authenticated attributes, if they exist.
     *
     */
    public SET getAuthenticatedAttributes() {
        return authenticatedAttributes;
    }

    /**
     * Returns true if the authenticatedAttributes field is present.
     */
    public boolean hasAuthenticatedAttributes() {
        return (authenticatedAttributes != null);
    }

    /**
     * Low-level method to set the authenticatedAttributes field.
     * It is not normally necessary to call this.  Use it at your own risk.
    public void setAuthenticatedAttributes(SET authAttrib) {
        this.authenticatedAttributes = authAttrib;
    }
     */

    /**
     * Returns the raw signature (digest encryption) algorithm used in this
     * SignerInfo.
     *
     * @exception NoSuchAlgorithmException If the algorithm is not recognized
     *  by JSS.
     */
    public SignatureAlgorithm getDigestEncryptionAlgorithm()
        throws NoSuchAlgorithmException
    {
        return SignatureAlgorithm.fromOID(
                    digestEncryptionAlgorithm.getOID() );
    }

    /**
     * Returns the DigestEncryptionAlgorithmIdentifier used in this SignerInfo.
     */
    public AlgorithmIdentifier getDigestEncryptionAlgorithmIdentifier() {
        return digestEncryptionAlgorithm;
    }

    /**
     * Low-level method to set the digestEncryptionAlgorithm field.
     * It is not normally necessary to call this.  Use it at your own risk.
    public void
    setDigestEncryptionAlgorithmIdentifier(AlgorithmIdentifier algid) {
        this.digestEncryptionAlgorithm= algid;
    }
     */

    /**
     * Retrieves the encrypted digest.
     */
    public byte[] getEncryptedDigest() {
        return encryptedDigest.toByteArray();
    }

    /**
     * Low-level method to set the encryptedDigest field.
     * It is not normally necessary to call this.  Use it at your own risk.
    public void setEncryptedDigest(byte[] ed) {
        this.encryptedDigest = new OCTET_STRING(ed);
    }
     */

    /**
     * Retrieves the unauthenticated attributes, if they exist.
     *
     */
    public SET getUnauthenticatedAttributes() {
        return unauthenticatedAttributes;
    }

    /**
     * Returns true if the unauthenticatedAttributes field is present.
     */
    public boolean hasUnauthenticatedAttributes() {
        return (unauthenticatedAttributes!=null);
    }

    /**
     * Low-level method to set the unauthenticatedAttributes field.
     * It is not normally necessary to call this.  Use it at your own risk.
    public void setUnauthenticatedAttributes(SET unauthAttrib) {
        this.unauthenticatedAttributes = unauthAttrib;
    }
     */

    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////
    // Constructors
    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////

    /**
     * Low-level default constructor.  All fields are initialized to null.
     * Before this SignerInfo can be processed or used in any way, all of
     * the fields except <code>authenticatedAttributes</code> and
     * <code>unauthenticatedAttributes</code> must be non-null.
     * <p>It is not normally necessary to call this constructor.Use it at
     * your own risk.
    public SignerInfo() {
    }
     */

    /**
     * A constructor for creating a new SignerInfo from scratch.
     *
     * @param issuerAndSerialNumber The issuer and serial number of the
     *  certificate from which the public key was extracted to create
     *  this SignerInfo.
     * @param signingAlg The algorithm to be used to sign the content.
     *  This should be a composite algorithm, such as
	 *  RSASignatureWithMD5Digest, instead of a raw algorithm, such as
     *  RSASignature.
     *  Note that the digest portion of this algorithm must be the same
     *  algorithm as was used to digest the message content.
     * @param authenticatedAttributes An optional set of Attributes, which
     *  will be signed along with the message content.  This parameter may
     *  be null, or the SET may be empty. DO NOT insert
     *  the PKCS #9 content-type or message-digest attributes.  They will
     *  be added automatically if they are necessary.
     * @param unauthenticatedAttributes An optional set of Attributes, which
     *  will be included in the SignerInfo but not signed.  This parameter
     *  may be null, or the SET may be empty.
     * @param messageDigest The digest of the message contents.  The digest
     *  must have been created with the digest algorithm specified by
     *  the signingAlg parameter.
     * @param contentType The type of the ContentInfo that is being signed.
     *  If it is not <code>data</code>, then the PKCS #9 attributes
     *  content-type and message-digest will be automatically computed and
     *  added to the authenticated attributes.
     */
    public SignerInfo(  IssuerAndSerialNumber issuerAndSerialNumber,
                        SET authenticatedAttributes,
                        SET unauthenticatedAttributes,
                        OBJECT_IDENTIFIER contentType,
                        byte[] messageDigest,
                        SignatureAlgorithm signingAlg,
                        PrivateKey signingKey )
        throws InvalidKeyException, NoSuchAlgorithmException,
        CryptoManager.NotInitializedException, SignatureException,
        TokenException
    {
        version = VERSION;
        this.issuerAndSerialNumber = issuerAndSerialNumber;
        this.digestAlgorithm =
                new AlgorithmIdentifier(signingAlg.getDigestAlg().toOID(),null);

        // if content-type is not data, add message-digest and content-type
        // to authenticated attributes.
        if( ! contentType.equals(ContentInfo.DATA) ) {
            if( authenticatedAttributes == null ) {
                authenticatedAttributes = new SET();
            }
            Attribute attrib = new Attribute(CONTENT_TYPE, ContentInfo.DATA);
            authenticatedAttributes.addElement(attrib);
            attrib = new Attribute(MESSAGE_DIGEST,
                            new OCTET_STRING(messageDigest) );
            authenticatedAttributes.addElement(attrib);
        }

        digestEncryptionAlgorithm = new AlgorithmIdentifier(
            signingAlg.getRawAlg().toOID(),null );


        if( authenticatedAttributes != null ) 
        {
            Assert._assert( authenticatedAttributes.size() >= 2 );
            this.authenticatedAttributes = authenticatedAttributes;
        }

        //////////////////////////////////////////////////
        // create encrypted digest (signature)
        //////////////////////////////////////////////////

        // compute the digest
        byte[] digest=null;
        DigestAlgorithm digestAlg = signingAlg.getDigestAlg();
        if( authenticatedAttributes == null ) {
            // just use the message digest of the content
            digest = messageDigest;
        } else {
            // digest the contents octets of the authenticated attributes
            byte[] enc = ASN1Util.encode(authenticatedAttributes);
            MessageDigest md =
                        MessageDigest.getInstance(digestAlg.toString());
            digest = md.digest( enc );
        }

        byte[] toBeSigned;
        if( signingAlg.getRawAlg() == SignatureAlgorithm.RSASignature ) {
            // put the digest in a DigestInfo
            SEQUENCE digestInfo = new SEQUENCE();
            AlgorithmIdentifier digestAlgId =
                    new AlgorithmIdentifier( digestAlg.toOID(),null );
            digestInfo.addElement( digestAlgId );
            digestInfo.addElement( new OCTET_STRING( digest ) );
            toBeSigned = ASN1Util.encode(digestInfo);
        } else {
            toBeSigned = digest;
        }

        // encrypt the DER-encoded DigestInfo with the private key
        CryptoToken token = signingKey.getOwningToken();
        Signature sig;
        sig = token.getSignatureContext( signingAlg.getRawAlg() );
        sig.initSign(signingKey);
        sig.update(toBeSigned);
        encryptedDigest = new OCTET_STRING(sig.sign());

        if( unauthenticatedAttributes != null )
        {
            this.unauthenticatedAttributes = unauthenticatedAttributes;
        }
    }

    /**
     * A constructor for creating a new SignerInfo from its decoding.
     */
    SignerInfo( INTEGER version,
            IssuerAndSerialNumber issuerAndSerialNumber,
            AlgorithmIdentifier digestAlgorithm,
            SET authenticatedAttributes,
            AlgorithmIdentifier digestEncryptionAlgorithm,
            byte[] encryptedDigest,
            SET unauthenticatedAttributes)
    {
        this.version = version;
        this.issuerAndSerialNumber = issuerAndSerialNumber;
        this.digestAlgorithm = digestAlgorithm;
        this.authenticatedAttributes = authenticatedAttributes;
        this.digestEncryptionAlgorithm = digestEncryptionAlgorithm;
        this.encryptedDigest = new OCTET_STRING(encryptedDigest);
        this.unauthenticatedAttributes = unauthenticatedAttributes;
    }

    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////
    // Verification
    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////

    /**
     * Verifies that this SignerInfo contains a valid signature of the
     * given message digest.  If any authenticated attributes are present,
     * they are also validated. The verification algorithm is as follows:<ul>
     * <p>Note that this does <b>not</b> verify the validity of the
     *  the certificate itself, only the signature.
     *
     * <li>If no authenticated attributes are present, the content type is 
     *  verified to be <i>data</i>. Then it is verified that the message
     *  digest passed
     *  in, when encrypted with the given public key, matches the encrypted
     *  digest in the SignerInfo.
     *
     * <li>If authenticated attributes are present,
     *  two particular attributes must be present: <ul>
     *  <li>PKCS #9 Content-Type, the type of content that is being signed.
     *      This must match the contentType parameter.
     *  <li>PKCS #9 Message-Digest, the digest of the content that is being
     *      signed. This must match the messageDigest parameter.
     * </ul>
     * After these two attributes are verified to be both present and correct,
     * the encryptedDigest field of the SignerInfo is verified to be the
     * signature of the contents octets of the DER encoding of the
     * authenticatedAttributes field.
     *
     * </ul>
     *
     * @param messageDigest The hash of the content that is signed by this
     *  SignerInfo.
     * @param contentType The type of the content that is signed by this
     *  SignerInfo.
     * @param pubkey The public key to use to verify the signature.
     * @exception NoSuchObjectException If no certificate matching the
     *      the issuer name and serial number can be found.
     */
    public void verify(byte[] messageDigest, OBJECT_IDENTIFIER contentType)
        throws CryptoManager.NotInitializedException, NoSuchAlgorithmException,
        InvalidKeyException, TokenException, SignatureException,
        ObjectNotFoundException
    {
        CryptoManager cm = CryptoManager.getInstance();
        X509Certificate cert = cm.findCertByIssuerAndSerialNumber(
            ASN1Util.encode(issuerAndSerialNumber.getIssuer()),
            issuerAndSerialNumber.getSerialNumber()  );
        verify(messageDigest, contentType, cert.getPublicKey());
    }
    

    /**
     * Verifies that this SignerInfo contains a valid signature of the
     * given message digest.  If any authenticated attributes are present,
     * they are also validated. The verification algorithm is as follows:<ul>
     *
     * <li>If no authenticated attributes are present, the content type is 
     *  verified to be <i>data</i>. Then it is verified that the message
     *  digest passed
     *  in, when encrypted with the given public key, matches the encrypted
     *  digest in the SignerInfo.
     *
     * <li>If authenticated attributes are present,
     *  two particular attributes must be present: <ul>
     *  <li>PKCS #9 Content-Type, the type of content that is being signed.
     *      This must match the contentType parameter.
     *  <li>PKCS #9 Message-Digest, the digest of the content that is being
     *      signed. This must match the messageDigest parameter.
     * </ul>
     * After these two attributes are verified to be both present and correct,
     * the encryptedDigest field of the SignerInfo is verified to be the
     * signature of the contents octets of the DER encoding of the
     * authenticatedAttributes field.
     *
     * </ul>
     *
     * @param messageDigest The hash of the content that is signed by this
     *  SignerInfo.
     * @param contentType The type of the content that is signed by this
     *  SignerInfo.
     * @param pubkey The public key to use to verify the signature.
     */
    public void verify(byte[] messageDigest, OBJECT_IDENTIFIER contentType,
        PublicKey pubkey)
        throws CryptoManager.NotInitializedException, NoSuchAlgorithmException,
        InvalidKeyException, TokenException, SignatureException
    {

        if( authenticatedAttributes == null ) {
            verifyWithoutAuthenticatedAttributes(messageDigest, contentType,
                pubkey);
        } else {
            verifyWithAuthenticatedAttributes(messageDigest, contentType,
                pubkey);
        }
    }

    /**
     * Verifies that the message digest passed in, when encrypted with the
     * given public key, matches the encrypted digest in the SignerInfo.
     */
    private void verifyWithoutAuthenticatedAttributes
        (byte[] messageDigest, OBJECT_IDENTIFIER contentType,
        PublicKey pubkey)
        throws CryptoManager.NotInitializedException, NoSuchAlgorithmException,
        InvalidKeyException, TokenException, SignatureException
    {
        if( ! contentType.equals(ContentInfo.DATA) ) {
            // only data can be signed this way.  Everything else has
            // to go into authenticatedAttributes.
            throw new SignatureException(
                "Content-Type is not DATA, but there are"+
                " no authenticated attributes");
        }

        SignatureAlgorithm sigAlg =
            SignatureAlgorithm.fromOID(
                digestEncryptionAlgorithm.getOID()
            );

        byte[] toBeVerified;
        if( sigAlg.getRawAlg() == SignatureAlgorithm.RSASignature ) {
            // create DigestInfo structure
            SEQUENCE digestInfo = new SEQUENCE();
            digestInfo.addElement(
                new AlgorithmIdentifier(digestAlgorithm.getOID(), null) );
            digestInfo.addElement( new OCTET_STRING(messageDigest) );
            toBeVerified = ASN1Util.encode(digestInfo);
        } else {
            toBeVerified = messageDigest;
        }

        CryptoToken token = CryptoManager.getInstance()
                                .getInternalCryptoToken();
        Signature sig = token.getSignatureContext(sigAlg);
        sig.initVerify(pubkey);
        sig.update(toBeVerified);
        if( sig.verify(encryptedDigest.toByteArray()) ) {
            return; // success
        } else {
            throw new SignatureException(
                "Encrypted message digest parameter does not "+
                "match encrypted digest in SignerInfo");
        }
    }


    /**
     * Verifies a SignerInfo with authenticated attributes.  If authenticated
     * attributes are present, then two particular attributes must
     * be present: <ul>
     * <li>PKCS #9 Content-Type, the type of content that is being signed.
     *      This must match the contentType parameter.
     * <li>PKCS #9 Message-Digest, the digest of the content that is being
     *      signed. This must match the messageDigest parameter.
     * </ul>
     * After these two attributes are verified to be both present and correct,
     * the encryptedDigest field of the SignerInfo is verified to be the
     * signature of the contents octets of the DER encoding of the
     * authenticatedAttributes field.
     */
    private void verifyWithAuthenticatedAttributes
        (byte[] messageDigest, OBJECT_IDENTIFIER contentType,
        PublicKey pubkey)
        throws CryptoManager.NotInitializedException, NoSuchAlgorithmException,
        InvalidKeyException, TokenException, SignatureException
    {

        int numAttrib = authenticatedAttributes.size();

        if(numAttrib < 2) {
            throw new SignatureException(
                "At least two authenticated attributes must be present:"+
                " content-type and message-digest");
        }

        // go through the authenticated attributes, verifying the
        // interesting ones
        boolean foundContentType = false;
        boolean foundMessageDigest = false;
        for( int i = 0; i < numAttrib; i++ ) {

            if( ! (authenticatedAttributes.elementAt(i) instanceof Attribute)) {
                throw new SignatureException(
                    "Element of authenticatedAttributes is not an Attribute");
            }

            Attribute attrib = (Attribute)
                    authenticatedAttributes.elementAt(i);

            if( attrib.getType().equals(CONTENT_TYPE) ) {
                // content-type.  Compare with what was passed in.

                SET vals = attrib.getValues();
                if( vals.size() != 1 ) {
                    throw new SignatureException(
                        "Content-Type attribute "+
                        " does not have exactly one value");
                }

                ASN1Value val = vals.elementAt(0);
                OBJECT_IDENTIFIER ctype;
                try {
                    if( val instanceof OBJECT_IDENTIFIER ) {
                        ctype = (OBJECT_IDENTIFIER) val;
                    } else if( val instanceof ANY ) {
                        ctype = (OBJECT_IDENTIFIER) ((ANY)val).decodeWith(
                                    OBJECT_IDENTIFIER.getTemplate() );
                    } else {
                        // what the heck is it? not what it's supposed to be
                        throw new InvalidBERException(
                        "Content-Type authenticated attribute has unexpected"+
                        " content type");
                    }
                } catch( InvalidBERException e ) {
                    throw new SignatureException(
                        "Content-Type authenticated attribute does not have "+
                        "OBJECT IDENTIFIER value");
                }

                // compare the content type in the attribute to the
                // contentType parameter
                if( ! ctype.equals( contentType ) ) {
                    throw new SignatureException(
                        "Content-type in authenticated attributes does not "+
                        "match content-type being verified");
                }

                // content type is A-OK
                foundContentType = true;

            } else if( attrib.getType().equals(MESSAGE_DIGEST) ) {

                SET vals = attrib.getValues();
                if( vals.size() != 1 ) {
                    throw new SignatureException(
                        "Message-digest attribute does not have"+
                        " exactly one value");
                }

                ASN1Value val = vals.elementAt(0);
                byte[] mdigest;
                try {
                    if( val instanceof OCTET_STRING ) {
                        mdigest = ((OCTET_STRING)val).toByteArray();
                    } else if( val instanceof ANY ) {
                        OCTET_STRING os;
                        os = (OCTET_STRING) ((ANY)val).decodeWith(
                            OCTET_STRING.getTemplate() );
                        mdigest = os.toByteArray();
                    } else {
                        // what the heck is it? not what it's supposed to be
                        throw new InvalidBERException(
                        "Content-Type authenticated attribute has unexpected"+
                        " content type");
                    }
                } catch( InvalidBERException e ) {
                    throw new SignatureException(
                        "Message-digest attribute does not"+
                        " have OCTET STRING value");
                }

                // compare the message digest in the attribute to the
                // message digest being verified
                if( ! byteArraysAreSame(mdigest, messageDigest) ) {
                    throw new SignatureException(
                        "Message-digest attribute does not"+
                        " match message digest being verified");
                }

                // message digest is A-OK
                foundMessageDigest = true;
            }

            // we don't care about other attributes

        }

        if( !foundContentType ) {
            throw new SignatureException(
                "Authenticated attributes does not contain"+
                " PKCS #9 content-type attribute");
        }

        if( !foundMessageDigest ) {
            throw new SignatureException(
                "Authenticate attributes does not contain"+
                " PKCS #9 message-digest attribute");
        }

        SignatureAlgorithm sigAlg =
            SignatureAlgorithm.fromOID(digestEncryptionAlgorithm.getOID());

        // All the authenticated attributes are present and correct.
        // Now verify the signature.
        CryptoToken token =
                    CryptoManager.getInstance().getInternalCryptoToken();
        Signature sig = token.getSignatureContext( sigAlg );
        sig.initVerify(pubkey);

        // verify the contents octets of the DER encoded authenticated attribs
        byte[] toBeDigested;
        toBeDigested = ASN1Util.encode(authenticatedAttributes);
    
        MessageDigest md = MessageDigest.getInstance(
                DigestAlgorithm.fromOID(digestAlgorithm.getOID()).toString() );
        byte[] digest = md.digest(toBeDigested);

        byte[] toBeVerified;
        if( sigAlg.getRawAlg() == SignatureAlgorithm.RSASignature ) {
            // create DigestInfo structure
            SEQUENCE digestInfo = new SEQUENCE();
            digestInfo.addElement(
                new AlgorithmIdentifier(digestAlgorithm.getOID(),null) );
            digestInfo.addElement( new OCTET_STRING(digest) );
            toBeVerified = ASN1Util.encode(digestInfo);
        } else {
            toBeVerified = digest;
        }

        sig.update( toBeVerified );

        if( ! sig.verify(encryptedDigest.toByteArray()) ) {
            // signature is invalid
            throw new SignatureException("encryptedDigest was not the correct"+
                " signature of the contents octets of the DER-encoded"+
                " authenticated attributes");
        }

        // SUCCESSFULLY VERIFIED

    }

    /**
     * Compares two non-null byte arrays.  Returns true if they are identical,
     * false otherwise.
     */
    private static boolean byteArraysAreSame(byte[] left, byte[] right) {

        Assert._assert(left!=null && right!=null);

        if( left.length != right.length ) {
            return false;
        }

        for(int i = 0 ; i < left.length ; i++ ) {
            if( left[i] != right[i] ) {
                return false;
            }
        }

        return true;
    } 

    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////
    // DER-encoding
    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////

    private static final Tag TAG = SEQUENCE.TAG;
    public Tag getTag() {
        return TAG;
    }

    public void encode(OutputStream ostream) throws IOException {
        encode(getTag(),ostream);
    }

    public void encode(Tag tag, OutputStream ostream) throws IOException {
        SEQUENCE sequence = new SEQUENCE();

        sequence.addElement( version );
        sequence.addElement( issuerAndSerialNumber );
        sequence.addElement( digestAlgorithm );
        if( authenticatedAttributes != null ) {
            sequence.addElement( new Tag(0), authenticatedAttributes );
        }
        sequence.addElement( digestEncryptionAlgorithm );
        sequence.addElement( encryptedDigest );
        if( unauthenticatedAttributes != null ) {
            sequence.addElement( new Tag(1), unauthenticatedAttributes );
        }

        sequence.encode(tag,ostream);
    }

    public static Template getTemplate() {
        return templateInstance;
    }
    private static Template templateInstance = new Template();

    /**
     * A template for decoding a SignerInfo blob
     *
     */
    public static class Template implements ASN1Template {

        SEQUENCE.Template seqt;

        public Template() {
            seqt = new SEQUENCE.Template();

            // serial number
            seqt.addElement(INTEGER.getTemplate()); // vn

            // issuerAndSerialNumber
            seqt.addElement(IssuerAndSerialNumber.getTemplate()); // issuer + sernum

            // digestAlgorithm
            seqt.addElement(AlgorithmIdentifier.getTemplate()); // digest alg

            // authenticatedAttributes
            seqt.addOptionalElement(
                        new Tag(0),
                        new SET.OF_Template(Attribute.getTemplate()));
          
            // digestEncryptionAlgorithm
            seqt.addElement(AlgorithmIdentifier.getTemplate());  // dig encr alg

            // encryptedDigest
            seqt.addElement(OCTET_STRING.getTemplate()); // encr digest

            // unauthenticated attributes
            seqt.addOptionalElement(
                    new Tag(1),
                    new SET.OF_Template(Attribute.getTemplate()));

        }
        
        public boolean tagMatch(Tag tag) {
            return TAG.equals(tag);
        }

        public ASN1Value decode(InputStream istream) 
            throws IOException, InvalidBERException
            {
                return decode(TAG,istream);
            }

        public ASN1Value decode(Tag implicitTag, InputStream istream)
            throws IOException, InvalidBERException
            {
                SEQUENCE seq = (SEQUENCE) seqt.decode(implicitTag,istream);
                Assert._assert(seq.size() == 7);

                return new SignerInfo(
                    (INTEGER)                   seq.elementAt(0),
                    (IssuerAndSerialNumber)     seq.elementAt(1),
                    (AlgorithmIdentifier)       seq.elementAt(2),
                    (SET)                       seq.elementAt(3),
                    (AlgorithmIdentifier)       seq.elementAt(4),
                    ((OCTET_STRING)             seq.elementAt(5)).toByteArray(),
                    (SET)                       seq.elementAt(6)
                    );
            }
    } // end of template

}
