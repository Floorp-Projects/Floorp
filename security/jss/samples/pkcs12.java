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
/**
 * This program reads in a PKCS #12 file and dumps its contents to the screen.
 * At the same time, it creates a new PKCS #12 file.  As it reads through the
 * old file, it writes its contents into the new file.  Then the new file
 * is encrypted and MACed with a new, different password.
 */

/* note that this code still has some problems reading PKCS12 file 
 * generated with Communicator.
 */


import java.io.*;
import org.mozilla.jss.*;
import org.mozilla.jss.pkcs12.*;
import org.mozilla.jss.crypto.*;
import org.mozilla.jss.asn1.*;
import org.mozilla.jss.util.*;
import org.mozilla.jss.pkix.primitive.*;
import org.mozilla.jss.pkix.cert.*;


public class pkcs12 {

  public static void main(String []args) {

    try {

        // Read arguments
        if( args.length != 3 ) {
            System.out.println("Usage: PFX <dbdir> <infile> <outfile>");
            System.exit(-1);
        }

        // open input file for reading
       	FileInputStream infile=null;
		try {
        	infile = new FileInputStream(args[1]);
		}
		catch (FileNotFoundException f) {
			System.out.println("Cannot open file "+args[1]+
					" for reading: "+f.getMessage());
			return;
		}
        int certfile = 0;

        // initialize CryptoManager. This is necessary because there is
        // crypto involved with decoding a PKCS #12 file
        CryptoManager.initialize( args[0] );
		CryptoManager manager = CryptoManager.getInstance();

        // Decode the P12 file
        PFX.Template pfxt = new PFX.Template();
        PFX pfx = (PFX) pfxt.decode(new BufferedInputStream(infile, 2048));
        System.out.println("Decoded PFX");

        // print out information about the top-level PFX structure
        System.out.println("Version: "+pfx.getVersion());
        AuthenticatedSafes authSafes = pfx.getAuthSafes();
        SEQUENCE safeContentsSequence = authSafes.getSequence();
        System.out.println("AuthSafes has "+
            safeContentsSequence.size()+" SafeContents");

        // Get the password for the old file
        System.out.println("Enter password: ");
        Password pass = Password.readPasswordFromConsole();

        // get new password, which will be used for the new file we create
        // later
        System.out.println("Enter new password:");
        Password newPass = Password.readPasswordFromConsole();


        // Verify the MAC on the PFX.  This is important to be sure
        // it hasn't been tampered with.
        StringBuffer sb = new StringBuffer();
        if( pfx.verifyAuthSafes(pass, sb) ) {
            System.out.println("AuthSafes verifies correctly.");
        } else {
            System.out.println("AuthSafes failed to verify because: "+
                sb);
        }

        // Create a new AuthenticatedSafes. As we read the contents of the
        // old authSafes, we will store them into the new one.  After we have
        // cycled through all the contents, they will all have been copied into
        // the new authSafes.
        AuthenticatedSafes newAuthSafes = new AuthenticatedSafes();

        // Loop over contents of the old authenticated safes
        //for(int i=0; i < asSeq.size(); i++) {
        for(int i=0; i < safeContentsSequence.size(); i++) {

            // The safeContents may or may not be encrypted.  We always send
            // the password in.  It will get used if it is needed.  If the
            // decryption of the safeContents fails for some reason (like
            // a bad password), then this method will throw an exception
            SEQUENCE safeContents = authSafes.getSafeContentsAt(pass, i);

            System.out.println("\n\nSafeContents #"+i+" has "+
                safeContents.size()+" bags");

            // Go through all the bags in this SafeContents
            for(int j=0; j < safeContents.size(); j++) {
                SafeBag safeBag = (SafeBag) safeContents.elementAt(j);

                // The type of the bag is an OID
                System.out.println("\nBag "+j+" has type "+
                    safeBag.getBagType() );


                // look for bag attributes
                SET attribs = safeBag.getBagAttributes();
                if( attribs == null ) {
                    System.out.println("Bag has no attributes");
                } else {
                    for(int b=0; b < attribs.size(); b++) {
                        Attribute a = (Attribute) attribs.elementAt(b);
                        if( a.getType().equals(SafeBag.FRIENDLY_NAME)) {
                            // the friendly name attribute is a nickname
                            BMPString bs = (BMPString) ((ANY)a.getValues().
                                elementAt(0)).decodeWith(
                                    BMPString.getTemplate());
                            System.out.println("Friendly Name: "+bs);
                        } else if(a.getType().equals(SafeBag.LOCAL_KEY_ID)){
                            // the local key id is used to match a key
                            // to its cert.  The key id is the SHA-1 hash of
                            // the DER-encoded cert.
                            OCTET_STRING os =(OCTET_STRING)
                                ((ANY)a.getValues().
                                elementAt(0)).decodeWith(
                                OCTET_STRING.getTemplate());
                            System.out.println("LocalKeyID:");
							/*
                            AuthenticatedSafes.
                                print_byte_array(os.toByteArray());
							*/
                        } else {
                            System.out.println("Unknown attribute type: "+
                                a.getType().toString());
                        }
                    }
                }

                // now look at the contents of the bag
                ASN1Value val = safeBag.getInterpretedBagContent();

                if( val instanceof PrivateKeyInfo ) {
                    // A PrivateKeyInfo contains an unencrypted private key
                    System.out.println("content is PrivateKeyInfo");
                } else if( val instanceof EncryptedPrivateKeyInfo ) {
                    // An EncryptedPrivateKeyInfo is, well, an encrypted
                    // PrivateKeyInfo. Usually, strong crypto is used in
                    // an EncryptedPrivateKeyInfo.
                    EncryptedPrivateKeyInfo epki =
                        ((EncryptedPrivateKeyInfo)val);
                    System.out.println(
                        "content is EncryptedPrivateKeyInfo, algoid:"
                        + epki.getEncryptionAlgorithm().getOID());

                    // Because we are in a PKCS #12 file, the passwords are
                    // char-to-byte converted in a special way.  We have to
                    // use the special converter class instead of the default.
                    PrivateKeyInfo pki = epki.decrypt(pass,
                        new org.mozilla.jss.pkcs12.PasswordConverter() );

					// import the key into the key3.db
					CryptoToken tok = manager.getTokenByName("Internal Key Storage Token");
					CryptoStore store = tok.getCryptoStore();
                    tok.login( new ConsolePasswordCallback());
					ByteArrayOutputStream baos = new ByteArrayOutputStream();
					pki.encode(baos);
					store.importPrivateKey(baos.toByteArray(),
									PrivateKey.RSA);


                    // re-encrypt the PrivateKeyInfo with the new password
                    // and random salt
                    byte[] salt = new byte[
                            PBEAlgorithm.PBE_SHA1_DES3_CBC.getSaltLength()];
                    JSSSecureRandom rand = CryptoManager.getInstance().
                        getSecureRNG();
                    rand.nextBytes(salt);
                    epki = EncryptedPrivateKeyInfo.createPBE(
                        PBEAlgorithm.PBE_SHA1_DES3_CBC, newPass,
                        salt, 1, new PasswordConverter(), pki);

                    // Overwrite the previous EncryptedPrivateKeyInfo with
                    // this new one we just created using the new password.
                    // This is what will get put in the new PKCS #12 file 
                    // we are creating.
                    safeContents.insertElementAt(
                        new SafeBag( safeBag.getBagType(),
                            epki, safeBag.getBagAttributes()), i);
                    safeContents.removeElementAt(i+1);

                } else if( val instanceof CertBag ) {
                    System.out.println("content is CertBag");
                    CertBag cb = (CertBag) val;
                    if( cb.getCertType().equals(CertBag.X509_CERT_TYPE)) {
                        // this is an X.509 certificate
                        OCTET_STRING os = (OCTET_STRING)cb.getInterpretedCert();
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

            // Add the new safe contents to the new authsafes
            if( authSafes.safeContentsIsEncrypted(i) ) {
                newAuthSafes.addEncryptedSafeContents(
                    authSafes.DEFAULT_KEY_GEN_ALG, newPass,
                    null, authSafes.DEFAULT_ITERATIONS, safeContents);
            } else {
                newAuthSafes.addSafeContents( safeContents );
            }
        }

        // Create new PFX from the new authsafes
        PFX newPfx = new PFX(newAuthSafes);

        // Add a MAC to the new PFX
        newPfx.computeMacData(newPass, null, PFX.DEFAULT_ITERATIONS);

        // write the new PFX out to a file
        FileOutputStream fos = new FileOutputStream(args[2]);
        newPfx.encode(fos);
        fos.close();

        
     } catch( Exception e ) {
        e.printStackTrace();
     }
  }
	


}
