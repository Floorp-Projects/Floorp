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

package org.mozilla.jss.pkcs10;

import org.mozilla.jss.asn1.*;
import org.mozilla.jss.pkix.primitive.*;
import org.mozilla.jss.crypto.*;
import org.mozilla.jss.CryptoManager;
import java.security.cert.CertificateException;
import java.security.NoSuchAlgorithmException;
import java.security.InvalidKeyException;
import java.security.SignatureException;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.security.PublicKey;
import java.security.KeyPair;
import java.util.Date;
import java.util.Calendar;

import java.io.BufferedInputStream;
import java.io.FileInputStream;

/**
 * A pkcs10 signed CertificationRequest.
 */
public class CertificationRequest implements ASN1Value {

    private CertificationRequestInfo info;
    private byte[] infoEncoding;
    private byte[] signature;
    private AlgorithmIdentifier algId;
    SEQUENCE sequence;

    private CertificationRequest() { }

    CertificationRequest(CertificationRequestInfo info, 
						 //byte[] infoEncoding,
            AlgorithmIdentifier algId, byte[] signature) throws IOException
    {
        this.info = info;
        //this.infoEncoding = infoEncoding;
        this.algId = algId;
        this.signature = signature;

        // bundle everything into a SEQUENCE
        sequence = new SEQUENCE();
        sequence.addElement( info );
        sequence.addElement( algId );
        sequence.addElement( new BIT_STRING( signature, 0 ) );
    }

    /**
     * Creates and signs an X.509 CertificationRequest.
     * @param info A CertificationRequestInfo (TBSCertificationRequest), 
     *      which specifies
     *      the actual information of the CertificationRequest.
     * @param privKey The private key with which to sign the certificat.
     * @param signingAlg The algorithm to use to sign the CertificationRequest.
     *      It must match the algorithm specified in the CertificationRequestInfo.
     * @exception IOException If an error occurred while encoding the
     *      CertificationRequest.
     * @exception CryptoManager.NotInitializedException Because this
     *      operation involves cryptography (signing), CryptoManager must
     *      be initialized before calling it.
     * @exception TokenException If an error occurs on a PKCS #11 token.
     * @exception NoSuchAlgorithmException If the OID for the signing algorithm
     *      cannot be located.
     * @exception CertificateException If the signing algorithm specified
     *      as a parameter does not match the one in the CertificationRequest info.
     * @exception InvalidKeyException If the key does not match the signing
     *      algorithm.
     * @exception SignatureException If an error occurs while signing the
     *      CertificationRequest.
     */
    public CertificationRequest(CertificationRequestInfo info, java.security.PrivateKey privKey,
                SignatureAlgorithm signingAlg)
        throws IOException, CryptoManager.NotInitializedException,
            TokenException, NoSuchAlgorithmException, CertificateException,
            InvalidKeyException, SignatureException
    {
        // make sure key is a Ninja private key
        if( !(privKey instanceof PrivateKey) ) {
            throw new InvalidKeyException("Private Key is does not belong to"+
                " this provider");
        }
        PrivateKey priv = (PrivateKey)privKey;

        // create algId 
        if(signingAlg.getSigningAlg() == SignatureAlgorithm.RSASignature) {
            algId = new AlgorithmIdentifier( signingAlg.toOID(), null );
        } else {
            algId = new AlgorithmIdentifier( signingAlg.toOID() );
        }

        // encode the cert info
        this.info = info;
        infoEncoding = ASN1Util.encode(info);
        
        // sign the info encoding
        CryptoManager cm = CryptoManager.getInstance();
        CryptoToken token = priv.getOwningToken();
        Signature sig = token.getSignatureContext(signingAlg);
        sig.initSign(priv);
        sig.update(infoEncoding);
        signature = sig.sign();

        // bundle everything into a SEQUENCE
        sequence = new SEQUENCE();
        sequence.addElement( info );
        sequence.addElement( algId );
        sequence.addElement( new BIT_STRING( signature, 0 ) );
    }

    /**
     * Verifies the signature on this CertificationRequest.  Does not indicate
     * that the CertificationRequest is valid at any specific time.
     */
    public void verify()
        throws InvalidKeyException, CryptoManager.NotInitializedException,
        NoSuchAlgorithmException, CertificateException, TokenException,
        SignatureException, InvalidKeyFormatException
    {
        verify( info.getSubjectPublicKeyInfo().toPublicKey() );
    }

    /**
     * Verifies the signature on this CertificationRequest, using the given public key.
     * Does not indicate the CertificationRequest is valid at any specific time.
     */
    public void verify(PublicKey key)
        throws InvalidKeyException, CryptoManager.NotInitializedException,
        NoSuchAlgorithmException, CertificateException, TokenException,
        SignatureException
    {
        CryptoManager cm = CryptoManager.getInstance();
        verify(key, cm.getInternalCryptoToken());
    }

    /**
     * Verifies the signature on this CertificationRequest, using the given public
     * key and CryptoToken. Does not indicate the CertificationRequest is valid at
     * any specific time.
     */
    public void verify(PublicKey key, CryptoToken token)
        throws NoSuchAlgorithmException, CertificateException, TokenException,
            SignatureException, InvalidKeyException
    {
        Signature sig = token.getSignatureContext(
            SignatureAlgorithm.fromOID( algId.getOID() ) );

        sig.initVerify(key);
        sig.update(infoEncoding);
        if( ! sig.verify(signature) ) {
            throw new CertificateException("Signature is invalid");
        }
    }


    /**
     * Returns the information (TBSCertificationRequest) contained in this CertificationRequest.
     */
    public CertificationRequestInfo getInfo() {
        return info;
    }

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

    public static class Template implements ASN1Template {

        private SEQUENCE.Template seqt;

        public Template() {
            seqt = new SEQUENCE.Template();
            seqt.addElement( CertificationRequestInfo.getTemplate() );
            //seqt.addElement( new ANY.Template() );
            seqt.addElement( AlgorithmIdentifier.getTemplate() );
            seqt.addElement( BIT_STRING.getTemplate() );
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

            //ANY infoAny = (ANY)seq.elementAt(0);
            //byte[] infoEncoding = infoAny.getEncoded();
            /*CertificationRequestInfo info = (CertificationRequestInfo) infoAny.decodeWith(
                                        CertificationRequestInfo.getTemplate() );
										*/
            CertificationRequestInfo info = (CertificationRequestInfo) seq.elementAt(0);
            // although signature is a bit string, all algorithms we use
            // will produce an octet string.
            BIT_STRING bs = (BIT_STRING) seq.elementAt(2);
            if( bs.getPadCount() != 0 ) {
                throw new InvalidBERException("signature does not fall into"+
                    " an integral number of bytes");
            }
            byte[] signature = bs.getBits();

            return new CertificationRequest( info,
											//infoEncoding,
                                    (AlgorithmIdentifier) seq.elementAt(1),
                                    signature
                        );
        }
    }

    public static void main(String argv[]) {

      try {

        if(argv.length > 2 || argv.length < 1) {
            System.out.println("Usage: CertificationRequest <dbdir> [<certfile>]");
            System.exit(0);
        }

        CryptoManager.initialize( argv[0] );
        CryptoManager cm = CryptoManager.getInstance();

        // read in a cert
        BufferedInputStream bis = new BufferedInputStream(
                new FileInputStream(argv[1]) );

        CertificationRequest cert = (CertificationRequest)
                CertificationRequest.getTemplate().decode(bis);

        CertificationRequestInfo info = cert.getInfo();

        info.print(System.out);

        //X509CertificationRequest hardcore = cm.findCertByNickname("Hardcore");
        //PublicKey key = hardcore.getPublicKey();

        cert.verify();
        System.out.println("verified");

        FileOutputStream fos = new FileOutputStream("certinfo.der");
        info.encode(fos);
        fos.close();

        // make a new public key
        CryptoToken token = cm.getInternalKeyStorageToken();
        KeyPairGenerator kpg = token.getKeyPairGenerator(KeyPairAlgorithm.RSA);
        kpg.initialize(512);
        System.out.println("Generating a new key pair...");
        KeyPair kp = kpg.genKeyPair();
        System.out.println("Generated key pair");

        // set the CertificationRequest's public key
        info.setSubjectPublicKeyInfo(kp.getPublic());

        // make new Name
        Name name = new Name();
        name.addCommonName("asldkj");
        name.addCountryName("US");
        name.addOrganizationName("Some Corp");
        name.addOrganizationalUnitName("Some Org Unit");
        name.addLocalityName("Silicon Valley");
        name.addStateOrProvinceName("California");
        info.setSubject(name);
        
        System.out.println("About to create a new cert request...");
        // create a new cert requestfrom this certReqinfo
        CertificationRequest genCert = new CertificationRequest(info, kp.getPrivate(),
                SignatureAlgorithm.RSASignatureWithMD5Digest);
        System.out.println("Created new cert request");

        genCert.verify();
        System.out.println("Cert verifies!");

        fos = new FileOutputStream("gencert.der");
        genCert.encode(fos);
        fos.close();

      } catch( Exception e ) {
        e.printStackTrace();
      }
    }
}
