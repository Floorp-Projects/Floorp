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
import org.mozilla.jss.crypto.*;
import org.mozilla.jss.util.Assert;
import java.security.*;
import org.mozilla.jss.CryptoManager;
import org.mozilla.jss.util.Password;
import java.security.spec.AlgorithmParameterSpec;

/**
 * PKCS #8 <i>EncryptedPrivateKeyInfo</i>.
 * <pre>
 * EncryptedPrivateKeyInfo ::= SEQUENCE {
 *      encryptionAlgorithm     AlgorithmIdentifier,
 *      encryptedData           OCTET STRING }
 * </pre>
 */
public class EncryptedPrivateKeyInfo implements ASN1Value {

    ///////////////////////////////////////////////////////////////////////
    // members and member access
    ///////////////////////////////////////////////////////////////////////
    private AlgorithmIdentifier encryptionAlgorithm;
    private OCTET_STRING encryptedData;
    private SEQUENCE sequence;

    public AlgorithmIdentifier getEncryptionAlgorithm() {
        return encryptionAlgorithm;
    }

    public OCTET_STRING getEncryptedData() {
        return encryptedData;
    }

    ///////////////////////////////////////////////////////////////////////
    // Constructors
    ///////////////////////////////////////////////////////////////////////

    private EncryptedPrivateKeyInfo() { }

    /**
     * Creates an EncryptedPrivateKeyInfo from its components.
     *
     */
    public EncryptedPrivateKeyInfo( AlgorithmIdentifier encryptionAlgorithm,
                OCTET_STRING encryptedData)
    {
        if( encryptionAlgorithm==null || encryptedData==null ) {
            throw new IllegalArgumentException(
                    "EncryptedPrivateKeyInfo parameter is null");
        }

        this.encryptionAlgorithm = encryptionAlgorithm;
        this.encryptedData = encryptedData;

        sequence = new SEQUENCE();
        sequence.addElement(encryptionAlgorithm);
        sequence.addElement(encryptedData);

    }

    ///////////////////////////////////////////////////////////////////////
    // crypto shortcuts
    ///////////////////////////////////////////////////////////////////////

    /**
     * Creates a new EncryptedPrivateKeyInfo, where the data is encrypted
     * with a password-based key.
     *
     * @param keyGenAlg The algorithm for generating a symmetric key from
     *      a password, salt, and iteration count.
     * @param password The password to use in generating the key.
     * @param salt The salt to use in generating the key.
     * @param iterationCount The number of hashing iterations to perform
     *      while generating the key.
     * @param charToByteConverter The mechanism for converting the characters
     *      in the password into bytes.  If null, the default mechanism
     *      will be used, which is UTF8.
     * @param pki The PrivateKeyInfo to be encrypted and stored in the
     *      EncryptedContentInfo. Before they are encrypted, they will be
     *      padded using PKCS padding.
     */
    public static EncryptedPrivateKeyInfo
    createPBE(PBEAlgorithm keyGenAlg, Password password, byte[] salt,
            int iterationCount,
            KeyGenerator.CharToByteConverter charToByteConverter,
            PrivateKeyInfo pki)
        throws CryptoManager.NotInitializedException, NoSuchAlgorithmException,
        InvalidKeyException, InvalidAlgorithmParameterException, TokenException,
        CharConversionException
    {
      try {

        // check key gen algorithm
        if( ! (keyGenAlg instanceof PBEAlgorithm) ) {
            throw new NoSuchAlgorithmException("Key generation algorithm"+
                " is not a PBE algorithm");
        }
        PBEAlgorithm pbeAlg = (PBEAlgorithm) keyGenAlg;

        CryptoManager cman = CryptoManager.getInstance();

        // generate key
        CryptoToken token = cman.getInternalCryptoToken();
        KeyGenerator kg = token.getKeyGenerator( keyGenAlg );
        PBEKeyGenParams pbekgParams = new PBEKeyGenParams(
            password, salt, iterationCount);
        if( charToByteConverter != null ) {
            kg.setCharToByteConverter( charToByteConverter );
        }
        kg.initialize(pbekgParams);
        SymmetricKey key = kg.generate();

        // generate IV
        EncryptionAlgorithm encAlg = pbeAlg.getEncryptionAlg();
        AlgorithmParameterSpec params=null;
        if( encAlg.getParameterClass().equals( IVParameterSpec.class ) ) {
            params = new IVParameterSpec( kg.generatePBE_IV() );
        }

        // perform encryption
        Cipher cipher = token.getCipherContext( encAlg );
        cipher.initEncrypt( key, params );
        byte[] encrypted = cipher.doFinal( Cipher.pad(
                ASN1Util.encode(pki), encAlg.getBlockSize()) );

        // make encryption algorithm identifier
        PBEParameter pbeParam = new PBEParameter( salt, iterationCount );
        AlgorithmIdentifier encAlgID = new AlgorithmIdentifier(
                keyGenAlg.toOID(), pbeParam);

        // create EncryptedPrivateKeyInfo
        EncryptedPrivateKeyInfo epki = new EncryptedPrivateKeyInfo (
                encAlgID,
                new OCTET_STRING(encrypted) );

        return epki;

      } catch( IllegalBlockSizeException e ) {
        Assert.notReached("IllegalBlockSizeException in EncryptedContentInfo"
            +".createPBE");
      } catch( BadPaddingException e ) {
        Assert.notReached("BadPaddingException in EncryptedContentInfo"
            +".createPBE");
      }
      return null;
    }

    /**
     * Decrypts an EncryptedPrivateKeyInfo that was encrypted with a PBE
     *  algorithm.  The algorithm and its parameters are extracted from
     *  the EncryptedPrivateKeyInfo.
     *
     * @param pass The password to use to generate the PBE key.
     * @param charToByteConverter The converter to change the password
     *      characters to bytes.  If null, the default conversion is used.
     */
    public PrivateKeyInfo
    decrypt(Password pass, KeyGenerator.CharToByteConverter charToByteConverter)
        throws CryptoManager.NotInitializedException, NoSuchAlgorithmException,
        InvalidBERException, InvalidKeyException,
        InvalidAlgorithmParameterException, TokenException,
        IllegalBlockSizeException, BadPaddingException, CharConversionException
    {
        // get the key gen parameters
        AlgorithmIdentifier algid = encryptionAlgorithm;
        KeyGenAlgorithm kgAlg = KeyGenAlgorithm.fromOID(algid.getOID());
        if( !(kgAlg instanceof PBEAlgorithm)) {
            throw new NoSuchAlgorithmException("KeyGenAlgorithm is not a "+
                "PBE algorithm");
        }
        ASN1Value params = algid.getParameters();
        if( params == null ) {
            throw new InvalidAlgorithmParameterException(
                "PBE algorithms require parameters");
        }
        PBEParameter pbeParams;
        if( params instanceof PBEParameter ) {
            pbeParams = (PBEParameter) params;
        } else {
            byte[] encodedParams = ASN1Util.encode(params);
            pbeParams = (PBEParameter)
                ASN1Util.decode(PBEParameter.getTemplate(), encodedParams);
        }
        PBEKeyGenParams kgp = new PBEKeyGenParams(pass,
            pbeParams.getSalt(), pbeParams.getIterations() );

        // compute the key and IV
        CryptoToken token =
            CryptoManager.getInstance().getInternalCryptoToken();
        KeyGenerator kg = token.getKeyGenerator( kgAlg );
        if( charToByteConverter != null ) {
            kg.setCharToByteConverter( charToByteConverter );
        }
        kg.initialize(kgp);
        SymmetricKey key = kg.generate();

        // compute algorithm parameters
        EncryptionAlgorithm encAlg = ((PBEAlgorithm)kgAlg).getEncryptionAlg();
        AlgorithmParameterSpec algParams;
        if( encAlg.getParameterClass().equals( IVParameterSpec.class ) ) {
            algParams = new IVParameterSpec( kg.generatePBE_IV() );
        } else {
            algParams = null;
        }

        // perform the decryption
        Cipher cipher = token.getCipherContext( encAlg );
        cipher.initDecrypt(key, algParams);
        byte[] decrypted = Cipher.unPad( cipher.doFinal(
                                            encryptedData.toByteArray() ) );

        return (PrivateKeyInfo)
            ASN1Util.decode(PrivateKeyInfo.getTemplate(), decrypted);

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
     * A template class for decoding EncryptedPrivateKeyInfos from BER.
     */
    public static class Template implements ASN1Template {

        private SEQUENCE.Template seqt;

        public Template() {
            seqt = new SEQUENCE.Template();

            seqt.addElement( AlgorithmIdentifier.getTemplate() );
            seqt.addElement( OCTET_STRING.getTemplate() );
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

            return new EncryptedPrivateKeyInfo(
                    (AlgorithmIdentifier) seq.elementAt(0),
                    (OCTET_STRING) seq.elementAt(1) );
        }
    }
}
