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

import org.mozilla.jss.util.Debug;
import java.io.*;
import org.mozilla.jss.asn1.*;
import org.mozilla.jss.pkcs7.*;
import org.mozilla.jss.crypto.*;
import org.mozilla.jss.pkix.primitive.*;
import org.mozilla.jss.util.*;
import org.mozilla.jss.CryptoManager;
import java.security.*;
import java.security.spec.AlgorithmParameterSpec;

/**
 * An <i>AuthenticatedSafes</i>, which is a <code>SEQUENCE</code> of
 * <i>SafeContents</i>.
 */
public class AuthenticatedSafes implements ASN1Value {

    private SEQUENCE sequence;

    /**
     * The default number of hash iterations (1) when performing PBE keygen.
     */
    public static final int DEFAULT_ITERATIONS = 1;

    /**
     * Salt length is variable with PKCS #12.  NSS uses 16 bytes, MSIE
     * uses 20.  We'll use 20 to get the 4 extra bytes of security.
     */
    private static final int SALT_LENGTH = 20;

    /**
     * The default PBE key generation algorithm: SHA-1 with RC2 40-bit CBC.
     */
    public static final PBEAlgorithm DEFAULT_KEY_GEN_ALG =
                PBEAlgorithm.PBE_SHA1_RC2_40_CBC;

    // security dynamics has a weird way of packaging things, we'll
    // work with it for debugging
    private static final boolean ACCEPT_SECURITY_DYNAMICS = false;

    /**
     * Default constructor, creates an empty AuthenticatedSafes.
     */
    public AuthenticatedSafes() {
        sequence = new SEQUENCE();
    }

    /**
     * Creates an AuthenticatedSafes from a SEQUENCE of ContentInfo.
     * @param sequence A non-null sequence of ContentInfo.
     */
    public AuthenticatedSafes(SEQUENCE sequence) {
        if( sequence==null) {
            throw new IllegalArgumentException("parameter is null");
        }
        if( Debug.DEBUG ) {
            for( int i = 0; i < sequence.size(); i++ ) {
                if( ! (sequence.elementAt(i) instanceof ContentInfo) ) {
                    throw new IllegalArgumentException(
                        "element "+i+" of sequence is not a ContentInfo");
                }
            }
        }
        this.sequence = sequence;
    }

    /**
     * Returns the raw SEQUENCE which constitutes this
     * <i>AuthenticatedSafes</i>.  The elements of this sequence are some
     * form of <i>SafeContents</i>, wrapped in a ContentInfo or 
     * an EncryptedData.
     */
    public SEQUENCE getSequence() {
        return sequence;
    }

    /**
     * Returns the size of the sequence, which is the number of SafeContents
     * in this AuthenticatedSafes.
     */
    public int getSize() {
        return sequence.size();
    }

    /**
     * Returns true if the SafeContents at the given index in the
     * AuthenticatedSafes is encrypted.  If it is encrypted, a password
     * must be supplied to <code>getSafeContentsAt</code> when accessing
     * this SafeContents.
     */
    public boolean safeContentsIsEncrypted(int index) {
        ContentInfo ci = (ContentInfo) sequence.elementAt(index);
        return ci.getContentType().equals(ContentInfo.ENCRYPTED_DATA);
    }

    /**
     * Returns the SafeContents at the given index in the AuthenticatedSafes,
     * decrypting it if necessary.
     *
     * <p>The algorithm used to extract encrypted SafeContents does not
     *  conform to version 1.0 of the spec. Instead, it conforms to the
     *  draft 1.0 spec, because this is what Communicator and MSIE seem
     *  to conform to.  This looks like an implementation error that has
     *  become firmly entrenched to preserve interoperability. The draft
     *  spec dictates that the encrypted content in the EncryptedContentInfo
     *  is the DER encoding of a SafeContents.  This is simple enough.  The
     *  1.0 final spec says that the SafeContents is wrapped in a ContentInfo,
     *  then the ContentInfo is BER encoded, then the value octets (not the
     *  tag or length) are encrypted. No wonder people stayed with the old way.
     *
     * @param password The password to use to decrypt the SafeContents if
     *  it is encrypted.  If the SafeContents is known to not be encrypted,
     *  this parameter can be null. If the password is incorrect, the
     *  decoding will fail somehow, probably with an InvalidBERException,
     *  BadPaddingException, or IllegalBlockSizeException.
     * @param index The index of the SafeContents to extract.
     * @return A SafeContents object, which is merely a
     *      SEQUENCE of SafeBags.
     * @exception IllegalArgumentException If no password was provided,
     *      but the SafeContents is encrypted.
     */
    public SEQUENCE getSafeContentsAt(Password password, int index)
        throws IllegalStateException, CryptoManager.NotInitializedException,
        NoSuchAlgorithmException, InvalidBERException, IOException,
        InvalidKeyException, InvalidAlgorithmParameterException, TokenException,
        IllegalBlockSizeException, BadPaddingException
    {

        ContentInfo ci = (ContentInfo) sequence.elementAt(index);

        if( ci.getContentType().equals(ContentInfo.ENCRYPTED_DATA) ) {
            // SafeContents is encrypted

            if( password == null ) {
                // can't decrypt if we don't have a password
                throw new IllegalStateException("No password to decode "+
                    "encrypted SafeContents");
            }

            EncryptedContentInfo encCI = (EncryptedContentInfo)
                    ((EncryptedData)ci.getInterpretedContent()).
                        getEncryptedContentInfo();

            // this should be a BER-encoded SafeContents
            byte[] decrypted = encCI.decrypt(password,
                                    new PasswordConverter());

            //print_byte_array(decrypted);

            try {
                SEQUENCE.OF_Template seqt = new SEQUENCE.OF_Template(
                                                SafeBag.getTemplate() );
                return (SEQUENCE) ASN1Util.decode(seqt, decrypted);
            } catch(InvalidBERException e) {
              if( ACCEPT_SECURITY_DYNAMICS ) {
                // try the security dynamics approach
                ContentInfo.Template cit = ContentInfo.getTemplate();
                ci = (ContentInfo) ASN1Util.decode(cit, decrypted);
                if( ! ci.getContentType().equals(ContentInfo.DATA) ) {
                    throw new InvalidBERException("");
                }
                OCTET_STRING os = (OCTET_STRING) ci.getInterpretedContent();
                SEQUENCE.OF_Template seqt = new SEQUENCE.OF_Template(
                                                    SafeBag.getTemplate() );
                return (SEQUENCE) ASN1Util.decode(seqt, os.toByteArray());
              } else {
                throw e;
              }
            }

        } else if( ci.getContentType().equals(ContentInfo.DATA) )  {
            // This SafeContents is not encrypted

            SEQUENCE.OF_Template seqt = new SEQUENCE.OF_Template(
                                                SafeBag.getTemplate() );
            return (SEQUENCE) ASN1Util.decode(seqt,
                        ((OCTET_STRING)ci.getInterpretedContent()).
                            toByteArray() );
        } else {
            throw new InvalidBERException("AuthenticatedSafes element is"+
                " neither a Data or an EncryptedData");
        }
    }

    static void print_byte_array(byte[] bytes) {
        int online=0;
        for(int i=0; i < bytes.length; i++, online++) {
            if( online > 25 ) {
                System.out.println("");
                online=0;
            }
            System.out.print(Integer.toHexString(bytes[i]&0xff) + " ");
        }
        System.out.println("");
    }

    /**
     * Returns the decrypted content from the encrypted content info.
    private static byte[]
    decryptEncryptedContentInfo(EncryptedContentInfo eci, Password pass)
        throws IllegalStateException,CryptoManager.NotInitializedException,
        NoSuchAlgorithmException, InvalidBERException, IOException,
        InvalidKeyException, InvalidAlgorithmParameterException, TokenException,
        IllegalBlockSizeException, BadPaddingException
    {
        OCTET_STRING encryptedContent = eci.getEncryptedContent();
        if( encryptedContent == null ) {
            return null;
        }

        // get the key gen parameters
        AlgorithmIdentifier algid = eci.getContentEncryptionAlgorithm();
        KeyGenAlgorithm kgAlg = KeyGenAlgorithm.fromOID( algid.getOID() );
        ASN1Value params = algid.getParameters();
        if( params == null ) {
            throw new InvalidAlgorithmParameterException(
                "PBE algorithms require parameters");
        }
        byte[] encodedParams = ASN1Util.encode(params);
        PBEParameter pbeParams = (PBEParameter)
                ASN1Util.decode( PBEParameter.getTemplate(), encodedParams );
        PBEKeyGenParams kgp = new PBEKeyGenParams(pass,
                    pbeParams.getSalt(), pbeParams.getIterations() );


        // compute the key and IV
        CryptoToken token =
            CryptoManager.getInstance().getInternalCryptoToken();
        KeyGenerator kg = token.getKeyGenerator( kgAlg );
        kg.setCharToByteConverter( new PasswordConverter() );
        kg.initialize( kgp );
        SymmetricKey key = kg.generate();

        // compute algorithm parameters
        EncryptionAlgorithm encAlg = keyGenAlgToEncryptionAlg(kgAlg);
        AlgorithmParameterSpec algParams;
        if( encAlg.getParameterClass().equals( IVParameterSpec.class ) ) {
            algParams = new IVParameterSpec( kg.generatePBE_IV() );
        } else {
            algParams = null;
        }

        // perform the decryption
        Cipher cipher = token.getCipherContext( encAlg );
        cipher.initDecrypt(key,  algParams );
        return cipher.doFinal( encryptedContent.toByteArray() );
    }
     */

    /**
     * Appends an unencrypted SafeContents to the end of the AuthenticatedSafes.
     */
    public void addSafeContents(SEQUENCE safeContents) {
        checkSafeContents(safeContents);

        ContentInfo ci = new ContentInfo( ASN1Util.encode(safeContents) );

        sequence.addElement(ci);
    }

    /**
     * Verifies that each element is a SafeBag.  Throws an
     * IllegalArgumentException otherwise.
     */
    private static void checkSafeContents(SEQUENCE safeContents) {
        int size = safeContents.size();
        for( int i = 0; i < size; i++) {
            if( ! (safeContents.elementAt(i) instanceof SafeBag) ) {
                throw new IllegalArgumentException(
                    "Element "+i+" of SafeContents is not a SafeBag");
            }
        }
    }

    /**
     * Encrypts a SafeContents and adds it to the AuthenticatedSafes.
     *
     * @param keyGenAlg The algorithm used to generate a key from the password.
     *      Must be a PBE algorithm. <code>DEFAULT_KEY_GEN_ALG</code> is
     *      usually fine here. It only provides 40-bit security, but if the
     *      private key material is packaged in its own
     *      <i>EncryptedPrivateKeyInfo</i>, the security of the SafeContents
     *      is not as important.
     * @param password The password to use to generate the encryption key
     *      and IV.
     * @param salt The salt to use to generate the key and IV. If null is
     *      passed in, the salt will be generated randomly, which is usually
     *      the right thing to do.
     * @param iterationCount The number of hash iterations to perform when
     *      generating the key and IV.  Use DEFAULT_ITERATIONS unless
     *      you want to be clever.
     * @param safeContents A SafeContents, which is a SEQUENCE of SafeBags.
     *      Each element of the sequence must in fact be an instance of
     *      <code>SafeBag</code>.
     */
    public void addEncryptedSafeContents(PBEAlgorithm keyGenAlg,
                Password password, byte[] salt, int iterationCount,
                SEQUENCE safeContents)
        throws CryptoManager.NotInitializedException, InvalidKeyException,
            InvalidAlgorithmParameterException, TokenException,
            NoSuchAlgorithmException, BadPaddingException,
            IllegalBlockSizeException
    {
      try {

        // generate salt if necessary
        if( salt == null ) {
            // generate random salt
            JSSSecureRandom rand = CryptoManager.getInstance().
                                        createPseudoRandomNumberGenerator();
            salt = new byte[SALT_LENGTH];
            rand.nextBytes(salt);
        }

        EncryptedContentInfo encCI = 
                EncryptedContentInfo.createPBE(keyGenAlg, password, salt,
                    iterationCount, new PasswordConverter(),
                    ASN1Util.encode(safeContents));

        EncryptedData encData = new EncryptedData(encCI);

        ContentInfo ci = new ContentInfo(encData);

        sequence.addElement( ci );
      } catch( CharConversionException e ) {
        Assert.notReached("CharConversionException while converting password");
      }
    }

    /*
    private static EncryptedContentInfo
    createEncryptedContentInfo(Password password, byte[] salt,
        int iterationCount, KeyGenAlgorithm keyGenAlg,
        SEQUENCE safeContents)
        throws CryptoManager.NotInitializedException, NoSuchAlgorithmException,
        InvalidKeyException, InvalidAlgorithmParameterException, TokenException,
        BadPaddingException, IllegalBlockSizeException
    {

      try {

        // check key gen algorithm
        if( ! keyGenAlg.isPBEAlg() ) {
            throw new NoSuchAlgorithmException("Key generation algorithm"+
                " is not a PBE algorithm");
        }

        CryptoManager cman = CryptoManager.getInstance();

        // generate salt if necessary
        if( salt == null ) {
            // generate random salt
            JSSSecureRandom rand = cman.createPseudoRandomNumberGenerator();
            salt = new byte[SALT_LENGTH];
            rand.nextBytes(salt);
        }

        // generate key
        CryptoToken token = cman.getInternalCryptoToken();
        KeyGenerator kg = token.getKeyGenerator( keyGenAlg );
        PBEKeyGenParams pbekgParams = new PBEKeyGenParams(
            password, salt, iterationCount);
        kg.setCharToByteConverter( new PasswordConverter() );
        kg.initialize(pbekgParams);
        SymmetricKey key = kg.generate();

        // generate IV
        EncryptionAlgorithm encAlg = keyGenAlgToEncryptionAlg(keyGenAlg);
        AlgorithmParameterSpec params=null;
        if( encAlg.getParameterClass().equals( IVParameterSpec.class ) ) {
            params = new IVParameterSpec( kg.generatePBE_IV() );
        }

        // perform encryption
        Cipher cipher = token.getCipherContext( encAlg );
        cipher.initEncrypt( key, params );
        byte[] encrypted = cipher.doFinal( Cipher.pad(
                ASN1Util.encode(safeContents), encAlg.getBlockSize()) );
        
        // make encryption algorithm identifier
        PBEParameter pbeParam = new PBEParameter( salt, iterationCount );
        AlgorithmIdentifier encAlgID = new AlgorithmIdentifier(
                keyGenAlg.toOID(), pbeParam);

        // create EncryptedContentInfo
        EncryptedContentInfo encCI = new EncryptedContentInfo(
                ContentInfo.DATA,
                encAlgID,
                new OCTET_STRING(encrypted) );

        return encCI;

      } catch( CharConversionException e ) {
        Assert.notReached("Unable to convert password characters");
        return null;
      }
    }
    */


    /*
    private static EncryptionAlgorithm
    keyGenAlgToEncryptionAlg(KeyGenAlgorithm kgAlg)
        throws NoSuchAlgorithmException
    {

        if( kgAlg==KeyGenAlgorithm.PBE_MD2_DES_CBC ||
            kgAlg==KeyGenAlgorithm.PBE_MD5_DES_CBC ||
            kgAlg==KeyGenAlgorithm.PBE_SHA1_DES_CBC) {
            return EncryptionAlgorithm.DES_CBC;
        } else if(  kgAlg==KeyGenAlgorithm.PBE_SHA1_RC4_128 ||
                    kgAlg==KeyGenAlgorithm.PBE_SHA1_RC4_40 ) {
            return EncryptionAlgorithm.RC4;
        } else if( kgAlg==KeyGenAlgorithm.PBE_SHA1_DES3_CBC ) {
            return EncryptionAlgorithm.DES3_CBC;
        } else if(  kgAlg==KeyGenAlgorithm.PBE_SHA1_RC2_128 ||
                    kgAlg==KeyGenAlgorithm.PBE_SHA1_RC2_40 ) {
            return EncryptionAlgorithm.RC2_CBC;
        } else {
            throw new NoSuchAlgorithmException(kgAlg.toString());
        }
    }
    */

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
        sequence.encode(implicitTag, ostream);
    }

    private static final Template templateInstance = new Template();

    public static Template getTemplate() {
        return templateInstance;
    }


    /**
     * A Template class for decoding an AuthenticatedSafes from its
     * BER encoding.
     */
    public static class Template implements ASN1Template {

        private SEQUENCE.OF_Template seqt;

        public Template() {
            seqt = new SEQUENCE.OF_Template( ContentInfo.getTemplate() );
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

            return new AuthenticatedSafes(seq);
        }
    }
}
