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

import java.io.*;
import org.mozilla.jss.asn1.*;
import org.mozilla.jss.crypto.*;
import org.mozilla.jss.CryptoManager;
import org.mozilla.jss.pkcs7.*;
import org.mozilla.jss.util.*;
import java.security.*;
import org.mozilla.jss.pkix.primitive.AlgorithmIdentifier;

public class MacData implements ASN1Value {

    private DigestInfo mac;
    private OCTET_STRING macSalt;
    private INTEGER macIterationCount;

    private static final int DEFAULT_ITERATIONS = 1;

    // 20 is the length of SHA-1 hash output
    private static final int SALT_LENGTH = 20;

    public DigestInfo getMac() {
        return mac;
    }

    public OCTET_STRING getMacSalt() {
        return macSalt;
    }

    public INTEGER getMacIterationCount() {
        return macIterationCount;
    }

    public MacData() { }

    /**
     * Creates a MacData from the given parameters.
     *
     * @param macIterationCount 1 is the default and should be used for
     *      maximum compatibility. null can also be used, in which case
     *      the macIterationCount will be omitted from the structure
     *      (and the default value of 1 will be implied).
     */
    public MacData(DigestInfo mac, OCTET_STRING macSalt,
                INTEGER macIterationCount)
    {
        if( mac==null || macSalt==null || macIterationCount==null ) {
             throw new IllegalArgumentException("null parameter");
        }

        this.mac = mac;
        this.macSalt = macSalt;
        this.macIterationCount = macIterationCount;
    }

    /**
     * Creates a MacData by computing a HMAC on the given bytes. An HMAC
	 *	is a message authentication code, which is a keyed digest. It proves
	 *	not only that data has not been tampered with, but also that the
	 *	entity that created the HMAC possessed the symmetric key.
	 *
	 * @param password The password used to generate a key using a
	 *		PBE mechanism.
	 * @param macSalt The salt used as input to the PBE key generation
	 *		mechanism. If null is passed in, new random salt will be created.
     * @param iterations The iteration count for creating the PBE key.
	 * @param toBeMACed The data on which the HMAC will be computed.
     * @exception CryptoManager.NotInitializedException If the crypto subsystem
     *      has not been initialized yet.
     * @exception TokenException If an error occurs on a crypto token.
	 */
    public MacData( Password password, byte[] macSalt,
                    int iterations, byte[] toBeMACed )
        throws CryptoManager.NotInitializedException,
            DigestException, TokenException, CharConversionException
    {
      try {

        CryptoManager cm = CryptoManager.getInstance();
        CryptoToken token = cm.getInternalCryptoToken();

        if(macSalt == null) {
            JSSSecureRandom rand = cm.createPseudoRandomNumberGenerator();
            macSalt = new byte[ SALT_LENGTH ];
            rand.nextBytes(macSalt);
        }

        // generate key from password and salt
        KeyGenerator kg = token.getKeyGenerator( KeyGenAlgorithm.PBA_SHA1_HMAC);
        PBEKeyGenParams params = new PBEKeyGenParams(password, macSalt,
            iterations);
        kg.setCharToByteConverter(new PasswordConverter());
        kg.initialize(params);
        SymmetricKey key = kg.generate();


        // perform the digesting
        JSSMessageDigest digest = token.getDigestContext(HMACAlgorithm.SHA1);
        digest.initHMAC(key);
        byte[] digestBytes = digest.digest(toBeMACed);


        // put everything into a DigestInfo
        AlgorithmIdentifier algID = new AlgorithmIdentifier(
                            DigestAlgorithm.SHA1.toOID() );
        this.mac = new DigestInfo( algID, new OCTET_STRING(digestBytes));
        this.macSalt = new OCTET_STRING(macSalt);
        this.macIterationCount = new INTEGER(iterations);

      } catch( NoSuchAlgorithmException e ) {
        Assert.notReached("SHA-1 HMAC algorithm not found on internal "+
            " token ("+ e.toString() + ")");
      } catch( InvalidAlgorithmParameterException e ) {
        Assert.notReached("Invalid PBE algorithm parameters");
      } catch( java.lang.IllegalStateException e ) {
        Assert.notReached("IllegalStateException");
      } catch( InvalidKeyException e ) {
        Assert.notReached("Invalid key exception");
      }
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

        seq.addElement(mac);
        seq.addElement(macSalt);
        if( ! macIterationCount.equals(new INTEGER(DEFAULT_ITERATIONS)) ) {
            // 1 is the default, only include this element if it is not
            // the default
            seq.addElement(macIterationCount);
        }

        seq.encode(implicitTag, ostream);
    }

    private static final Template templateInstance = new Template();
    public static final Template getTemplate() {
        return templateInstance;
    }

    /**
     * A Template for decoding a MacData from its BER encoding.
     */
    public static class Template implements ASN1Template {

        private SEQUENCE.Template seqt;

        public Template() {
            seqt = new SEQUENCE.Template();

            seqt.addElement( DigestInfo.getTemplate() );
            seqt.addElement( OCTET_STRING.getTemplate() );
            seqt.addElement( INTEGER.getTemplate(),
                                new INTEGER(DEFAULT_ITERATIONS) );
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

            return new MacData( (DigestInfo) seq.elementAt(0),
                                (OCTET_STRING) seq.elementAt(1),
                                (INTEGER) seq.elementAt(2) );
        }
    }
}
