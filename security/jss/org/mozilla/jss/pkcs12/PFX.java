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

package org.mozilla.jss.pkcs12;

import org.mozilla.jss.asn1.*;
import org.mozilla.jss.pkcs7.*;
import org.mozilla.jss.pkix.cert.*;
import java.io.*;
import org.mozilla.jss.util.Password;
import org.mozilla.jss.CryptoManager;
import org.mozilla.jss.pkix.primitive.*;
import org.mozilla.jss.pkix.primitive.Attribute;
import org.mozilla.jss.crypto.*;
import java.security.*;
import org.mozilla.jss.pkix.cert.Certificate;

/**
 * The top level ASN.1 structure for a PKCS #12 blob.
 *
 * <p>The general procedure for creating a PFX blob is as follows:<ul>
 *
 * <li>Create instances of <code>SafeBag</code> containing things such as
 *      private keys, certificates, or arbitrary secrets.
 * <li>Store the SafeBags in one or more SEQUENCEs.  Each SEQUENCE is
 *      called a <i>SafeContents</i>.
 * <li>Create an AuthenticatedSafes.  Store each SafeContents into the
 *      AuthenticatedSafes with <code>addEncryptedSafeContents</code> or
 *      <code>addSafeContents</code>.
 * <p>Standard procedure for browsers is for the AuthenticatedSafes to contain
 *      two instances of SafeContents, one encrypted and the other not.
 *      Anything you want encrypted can go in the encrypted SafeContents,
 *      and anything you want in plaintext can go in the regular SafeContents.
 *      Keep in mind that private key SafeBags usually consist of an
 *      EncryptedPrivateKeyInfo, which has its own (strong) encryption,
 *      in which case it is not essential that the SafeContents containing
 *      the private key also be encrypted.
 * <li>Create a PFX containing the AuthenticatedSafes instance, using the
 *      <code>PFX(AuthenticatedSafes)</code> constructor.
 * <li>Add a MAC to the PFX so it can be verified, using
 *      <code>PFX.computeMacData</code>.
 * </ul>
 *
 * To decode a PFX, <ul>
 *
 * <li>Use a <code>PFX.Template</code> to decode the ASN.1 into a
 *      <code>PFX</code> object.
 * <li>Check the output of <code>PFX.verifyAuthSafes</code> to verify
 *      the MAC on the PFX.
 * <li>Use <code>PFX.getAuthSafes</code> to extract the AuthenticatedSafes
 *      instance.
 * <li>Use <code>AuthenticatedSafes.getSafeContentsAt</code> to grab the
 *      SafeContents objects in the AuthenticatedSafes.
 * <li>Each SafeContents is a SEQUENCE of SafeBags, each of which may
 *      contain a private key, cert, or arbitrary secret.
 * </ul>
 */
public class PFX implements ASN1Value {

    ///////////////////////////////////////////////////////////////////////
    // Members
    ///////////////////////////////////////////////////////////////////////
    private INTEGER version;
    private AuthenticatedSafes authSafes;
    private MacData macData; // may be null
    private byte[] encodedAuthSafes; // may be null

    // currently we are on version 3 of the standard
    private static final INTEGER VERSION = new INTEGER(3);

    /**
     * The default number of iterations to use when generating the MAC.
     * Currently, it is 1.
     */
    public static final int DEFAULT_ITERATIONS = 1;


    public INTEGER getVersion() {
        return version;
    }

    public AuthenticatedSafes getAuthSafes() {
        return authSafes;
    }

    /**
     * Returns the MacData of this PFX, which is used to verify the contents.
     * This field is optional.  If it is not present, null is returned.
     */
    public MacData getMacData() {
        return macData;
    }

    private void setEncodedAuthSafes(byte[] encodedAuthSafes) {
        this.encodedAuthSafes = encodedAuthSafes;
    }

    /**
     * Verifies the HMAC on the authenticated safes, using the password
     * provided.
     *
     * @param password The password to use to compute the HMAC.
     * @param reason If supplied, the reason for the verification failure
     *      will be appended to this StringBuffer.
     * @return true if the MAC verifies correctly, false otherwise. If
     *      this PFX does not contain a MacData, returns false.
     */
    public boolean verifyAuthSafes(Password password, StringBuffer reason)
        throws CryptoManager.NotInitializedException
    {
      try {

        if(reason == null) {
            // this is just so we don't get a null pointer exception
            reason = new StringBuffer();
        }

        if( macData == null ) {
            reason.append("No MAC present in PFX");
            return false;
        }

        if( encodedAuthSafes == null ) {
            // We weren't decoded from a template, we were constructed,
            // so just verify the encoding of the AuthSafes provided to
            // the constructor.
            encodedAuthSafes = ASN1Util.encode(authSafes);
        }

        // create a new MacData based on the encoded Auth Safes
        DigestInfo macDataMac = macData.getMac();
        MacData testMac = new MacData( password,
                            macData.getMacSalt().toByteArray(),
                            macData.getMacIterationCount().intValue(),
                            encodedAuthSafes );

        if( testMac.getMac().equals(macDataMac) ) {
            return true;
        } else {
            reason.append("Digests do not match");
            return false;
        }

      } catch( java.security.DigestException e ) {
        e.printStackTrace();
        reason.append("A DigestException occurred");
        return false;
      } catch( TokenException e ) {
        reason.append("A TokenException occurred");
        return false;
      } catch( CharConversionException e ) {
        reason.append("An exception occurred converting the password from"+
            " chars to bytes");
        return false;
      }
    }

    ///////////////////////////////////////////////////////////////////////
    // Constructors
    ///////////////////////////////////////////////////////////////////////
    private PFX() { }

    /**
     * Creates a PFX with the given parameters.
     */
    public PFX( INTEGER version, AuthenticatedSafes authSafes,
                MacData macData) {
        if( version==null || authSafes==null ) {
            throw new IllegalArgumentException("null parameter");
        }

        this.version = version;
        this.authSafes = authSafes;
        this.macData = macData;
    }

    /**
     * Creates a PFX with the default version.
     */
    public PFX( AuthenticatedSafes authSafes, MacData macData ) {
        this( VERSION, authSafes, macData );
    }

    /**
     * Creates a PFX with the default version and no MacData.  The MacData
     * can be added later with <code>computeMacData</code>.
     * @see #computeMacData
     */
    public PFX( AuthenticatedSafes authSafes ) {
        this( VERSION, authSafes, null );
    }

    /**
     * Computes the macData field and adds it to the PFX. The macData field
     *   is a Message Authentication Code of the AuthenticatedSafes, and
     *   is used to prove the authenticity of the PFX.
     * 
     * @param password The password to be used to create the password-based MAC.
     * @param salt The salt to be used.  If null is passed in, a new salt
     *      will be created from a random source.
     * @param iterationCount The iteration count for the key generation.
     *      Use DEFAULT_ITERATIONS unless there's a need to be clever.
     */
    public void computeMacData(Password password,
            byte[] salt, int iterationCount)
        throws CryptoManager.NotInitializedException, DigestException,
        TokenException, CharConversionException
    {
        macData = new MacData( password, salt, iterationCount,
                ASN1Util.encode(authSafes) );
    }


    ///////////////////////////////////////////////////////////////////////
    // DER encoding
    ///////////////////////////////////////////////////////////////////////

    public Tag getTag() {
        return TAG;
    }
    private static final Tag TAG = SEQUENCE.TAG;


    public void encode(OutputStream ostream) throws IOException {
        encode(TAG, ostream);
    }


    public void encode(Tag implicitTag, OutputStream ostream)
        throws IOException
    {
        SEQUENCE seq = new SEQUENCE();

        seq.addElement(version);
        seq.addElement( new ContentInfo( ASN1Util.encode(authSafes) ) );
        if(macData != null) {
            seq.addElement(macData);
        }
        seq.encode(implicitTag, ostream);
    }

    /**
     * A Template for decoding a BER-encoded PFX.
     */
    public static class Template implements ASN1Template {

        private SEQUENCE.Template seqt;
        public Template() {
            seqt = SEQUENCE.getTemplate();

            seqt.addElement( INTEGER.getTemplate() );
            seqt.addElement( ContentInfo.getTemplate() );
            seqt.addOptionalElement( MacData.getTemplate() );
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

            ContentInfo authSafesCI = (ContentInfo) seq.elementAt(1);
            if( ! authSafesCI.getContentType().equals(ContentInfo.DATA) ) {
                throw new InvalidBERException(
                    "ContentInfo containing AuthenticatedSafes does not have"+
                    " content-type DATA");
            }
            OCTET_STRING authSafesOS = (OCTET_STRING)
                                            authSafesCI.getInterpretedContent();
            AuthenticatedSafes authSafes = (AuthenticatedSafes)
                    ASN1Util.decode( AuthenticatedSafes.getTemplate(),
                                    authSafesOS.toByteArray() );

            PFX pfx = new PFX( (INTEGER) seq.elementAt(0),
                            authSafes,
                            (MacData) seq.elementAt(2) );

            // record the encoding of the auth safes so we can verify the
            // MAC later.  We can't just re-encode the AuthSafes because
            // it is BER, and the re-encoding might be different from
            // the original encoding.
            pfx.setEncodedAuthSafes(authSafesOS.toByteArray());

            return pfx;
        }
    }

    public static void main(String []args) {

      try {

            if( args.length != 2 ) {
                System.out.println("Usage: PFX <dbdir> <infile>");
                System.exit(-1);
            }
            FileInputStream fis = new FileInputStream(args[1]);
            int certfile = 0;

            CryptoManager.initialize( args[0] );

            // Decode the P12 file
            PFX.Template pfxt = new PFX.Template();
            PFX pfx = (PFX) pfxt.decode(new BufferedInputStream(fis, 2048));
            System.out.println("Decoded PFX");

            // now peruse it for interesting info
            System.out.println("Version: "+pfx.getVersion());
            AuthenticatedSafes authSafes = pfx.getAuthSafes();
            SEQUENCE asSeq = authSafes.getSequence();
            System.out.println("AuthSafes has "+
                asSeq.size()+" SafeContents");
            System.out.println("Enter password: ");
            Password pass = Password.readPasswordFromConsole();

            // get new password
            System.out.println("Enter new password:");
            Password newPass = Password.readPasswordFromConsole();


            // verify the PFX
            StringBuffer sb = new StringBuffer();
            if( pfx.verifyAuthSafes(pass, sb) ) {
                System.out.println("AuthSafes verifies correctly");
            } else {
                System.out.println("AuthSafes failed to verify because: "+
                    sb);
            }

            // get new AuthSafes ready
            AuthenticatedSafes newAuthSafes = new AuthenticatedSafes();

            for(int i=0; i < asSeq.size(); i++) {
                SEQUENCE safeContents = authSafes.getSafeContentsAt(pass,i);

                System.out.println("\n\nSafeContents #"+i+" has "+
                    safeContents.size()+" bags");
                for(int j=0; j < safeContents.size(); j++) {
                    SafeBag safeBag = (SafeBag) safeContents.elementAt(j);
                    System.out.println("\nBag "+j+" has type "+
                        safeBag.getBagType() );
                    SET attribs = safeBag.getBagAttributes();
                    if( attribs == null ) {
                        System.out.println("Bag has no attributes");
                    } else {
                        for(int b=0; b < attribs.size(); b++) {
                            Attribute a = (Attribute) attribs.elementAt(b);
                            if( a.getType().equals(SafeBag.FRIENDLY_NAME)) {
                                BMPString bs = (BMPString) ((ANY)a.getValues().
                                    elementAt(0)).decodeWith(
                                        BMPString.getTemplate());
                                System.out.println("Friendly Name: "+bs);
                            } else if(a.getType().equals(SafeBag.LOCAL_KEY_ID)){
                                OCTET_STRING os =(OCTET_STRING)
                                    ((ANY)a.getValues().
                                    elementAt(0)).decodeWith(
                                    OCTET_STRING.getTemplate());
                                System.out.println("LocalKeyID:");
                                AuthenticatedSafes.
                                    print_byte_array(os.toByteArray());
                            } else {
                                System.out.println("Unknown attribute type");
                            }
                        }
                    }
                    ASN1Value val = safeBag.getInterpretedBagContent();
                    if( val instanceof PrivateKeyInfo ) {
                        System.out.println("content is PrivateKeyInfo");
                    } else if( val instanceof EncryptedPrivateKeyInfo ) {
                        EncryptedPrivateKeyInfo epki =
                            ((EncryptedPrivateKeyInfo)val);
                        System.out.println(
                            "content is EncryptedPrivateKeyInfo, algoid:"
                            + epki.getEncryptionAlgorithm().getOID());
                        PrivateKeyInfo pki = epki.decrypt(pass,
                            new PasswordConverter() );
                        byte[] salt = new byte[20];
                        JSSSecureRandom rand = CryptoManager.getInstance().
                            getSecureRNG();
                        rand.nextBytes(salt);
                        epki = EncryptedPrivateKeyInfo.createPBE(
                            PBEAlgorithm.PBE_SHA1_DES3_CBC, newPass,
                            salt, 1, new PasswordConverter(), pki);

                        // replace the old safe bag with the new
                        safeContents.insertElementAt(
                            new SafeBag( safeBag.getBagType(),
                                epki, safeBag.getBagAttributes()), j);
                        safeContents.removeElementAt(j+1);
                    } else if( val instanceof CertBag ) {
                        System.out.println("   content is CertBag");
                        CertBag cb = (CertBag) val;
                        if( cb.getCertType().equals(CertBag.X509_CERT_TYPE)) {
                            OCTET_STRING os =
                                (OCTET_STRING)cb.getInterpretedCert();
                            FileOutputStream fos = new FileOutputStream(
                                "cert"+(certfile++)+".der");
                            os.encode(fos);
                            fos.close();
                            Certificate cert = (Certificate)
                                ASN1Util.decode(Certificate.getTemplate(),
                                                os.toByteArray());
                            cert.getInfo().print(System.out);
                        } else {
                            System.out.println("Unrecognized cert type");
                        }
                    } else {
                        System.out.println("content is ANY");
                    }
                }

                // Add the new safe contents to the authsafes
                if( authSafes.safeContentsIsEncrypted(i) ) {
                    newAuthSafes.addEncryptedSafeContents(
                        authSafes.DEFAULT_KEY_GEN_ALG, newPass,
                        null, authSafes.DEFAULT_ITERATIONS, safeContents);
                } else {
                    newAuthSafes.addSafeContents( safeContents );
                }
            }

            // Create new PFX from new authsafes
            PFX newPfx = new PFX(newAuthSafes);
            newPfx.computeMacData(newPass, null, DEFAULT_ITERATIONS);

            FileOutputStream fos = new FileOutputStream("newjss.p12");
            newPfx.encode(fos);
            fos.close();

            
        } catch( Exception e ) {
            e.printStackTrace();
        }
    }

}
