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
package org.mozilla.jss.pkix.crmf;

import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.FileInputStream;
import org.mozilla.jss.asn1.*;
import org.mozilla.jss.pkix.primitive.*;
import org.mozilla.jss.util.Assert;
import java.math.BigInteger;
import java.text.DateFormat;
import java.security.SignatureException;
import java.security.NoSuchAlgorithmException;
import org.mozilla.jss.*;
import org.mozilla.jss.crypto.*;
import org.mozilla.jss.crypto.*;
import java.security.PublicKey;
import java.io.FileOutputStream;

/**
 * This class models a CRMF <i>CertReqMsg</i> structure.
 */
public class CertReqMsg implements ASN1Value {

    public static final Tag TAG = SEQUENCE.TAG;
    public Tag getTag() {
        return TAG;
    }

    /**
     * Retrieves the <i>CertRequest</i> contained in this structure.
     */
    public CertRequest getCertReq() {
        return certReq;
    }
    private CertRequest certReq;

    /**
     * Returns <code>true</code> if this <i>CertReqMsg</i> has a
     * <i>regInfo</i> field.
     */
    public boolean hasRegInfo() {
        return regInfo != null;
    }
    /**
     * Returns the <i>regInfo</i> field. Should only be called if the
     * field is present.
     */
    public SEQUENCE getRegInfo() {
        Assert._assert(regInfo != null);
        return regInfo;
    }
    private SEQUENCE regInfo;

    /**
     * Returns <code>true</code> if this <i>CertReqMsg</i> has a
     * <i>pop</i> field.
     */
    public boolean hasPop() {
        return pop != null;
    }
    /**
     * Returns the <i>pop</i> field. Should only be called if the
     * field is present.
     */
    public ProofOfPossession getPop() {
        Assert._assert(pop != null);
        return pop;
    }
    private ProofOfPossession pop = null;

    // no default constructor
    private CertReqMsg() { }

    /**
     * Constructs a <i>CertReqmsg</i> from a <i>CertRequest</i> and, optionally,
     * a <i>pop>/i> and a <i>regInfo</i>.
     * @param pop May be NULL.
     * @param regInfo May be NULL.
     */
    public CertReqMsg( CertRequest certReq, ProofOfPossession pop,
					   SEQUENCE regInfo ) {
        this.certReq = certReq;
		this.pop = pop;
        this.regInfo = regInfo;
    }

    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////
    // Verification
    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////

	public void verify() throws SignatureException,
		InvalidKeyFormatException, NoSuchAlgorithmException,
		org.mozilla.jss.CryptoManager.NotInitializedException,
		TokenException, java.security.InvalidKeyException, IOException{
		ProofOfPossession.Type type = pop.getType();
		if (type == ProofOfPossession.SIGNATURE) {
			POPOSigningKey sigkey = pop.getSignature();
			AlgorithmIdentifier alg = sigkey.getAlgorithmIdentifier();
			BIT_STRING sig_from = sigkey.getSignature();
			ByteArrayOutputStream bo = new ByteArrayOutputStream();
			certReq.encode(bo);
			byte[] toBeVerified = bo.toByteArray();

			PublicKey pubkey = null;
			CertTemplate ct = certReq.getCertTemplate();
			if (ct.hasPublicKey()) {
				SubjectPublicKeyInfo spi = ct.getPublicKey();
				pubkey = (PublicKey) spi.toPublicKey();
			}

			CryptoToken token = CryptoManager.getInstance()
                                .getInternalCryptoToken();
			SignatureAlgorithm sigAlg =
				SignatureAlgorithm.fromOID(alg.getOID());
			Signature sig = token.getSignatureContext(sigAlg);
			sig.initVerify(pubkey);
			sig.update(toBeVerified);
			if( sig.verify(sig_from.getBits()) ) {
				//System.out.println("worked!!");
				return; // success
			} else {
				throw new SignatureException(
					"Signed request information does not "+
					"match signature in POP");
			}

		} else if (type == ProofOfPossession.KEY_ENCIPHERMENT) {
			POPOPrivKey keyEnc = pop.getKeyEncipherment();
			POPOPrivKey.Type ptype = keyEnc.getType();
			if (ptype == POPOPrivKey.THIS_MESSAGE) {
				//BIT_STRING thisMessage = keyEnc.getThisMessage();
				//This should be the same as from the archive control
				//It's verified by DRM.
				
			} else if (ptype == POPOPrivKey.SUBSEQUENT_MESSAGE) {
				new ChallengeResponseException("requested");
			}
		}
	}

    /**
     * Encodes this <i>CertReqMsg</i> to the given OutputStream using
     * DER encoding.
     */
    public void encode(OutputStream ostream) throws IOException {
        //Assert.notYetImplemented("CertReqMsg encoding");
        encode(getTag(),ostream);
    }

    /**
     * Encodes this <i>CertReqMsg</i> to the given OutputStream using
     * DER encoding, with the given implicit tag.
     */
    public void encode(Tag implicit, OutputStream ostream) throws IOException {
        //Assert.notYetImplemented("CertReqMsg encoding");
        SEQUENCE sequence = new SEQUENCE();

        sequence.addElement( certReq );
		if (pop != null)
			sequence.addElement( pop );
		if (regInfo != null)
			sequence.addElement( regInfo );

        sequence.encode(implicit,ostream);
    }

    private static final Template templateInstance = new Template();
    public static Template getTemplate() {
        return templateInstance;
    }

    /**
     * A class for decoding <i>CertReqMsg</i> structures from a BER encoding.
     */
    public static class Template implements ASN1Template {

        public boolean tagMatch(Tag t) {
            return TAG.equals(t);
        }

        /**
         * Decodes a <i>CertReqMsg</i> from the given input stream.
         *
         * @return A new <i>CertReqMsg</i>.  The return value may be cast
         *      to a <code>CertReqMsg</code>.
         * @throws InvalidBERException If the data on the input stream is not
         *      a valid BER encoding of a <i>CertReqMsg</i>.
         */
        public ASN1Value decode(InputStream istream)
            throws IOException, InvalidBERException
        {
            return decode(TAG, istream);
        }

        /**
         * Decodes a <i>CertReqMsg</i> from the given input stream, using
         *      the given implicit tag.
         *
         * @param implicit The implicit tag for this item.  This must be
         *      supplied if the <i>CertReqMsg</i> appears in a context
         *      where it is implicitly tagged.
         * @return A new <i>CertReqMsg</i>.  The return value may be cast
         *      to a <code>CertReqMsg</code>.
         * @throws InvalidBERException If the data on the input stream is not
         *      a valid BER encoding of a <i>CertReqMsg</i>.
         */
        public ASN1Value decode(Tag implicit, InputStream istream)
            throws IOException, InvalidBERException
        {
            SEQUENCE.Template seqt = new SEQUENCE.Template();

            seqt.addElement( new CertRequest.Template() );
			seqt.addOptionalElement( new ProofOfPossession.Template());
            seqt.addOptionalElement(
                    new SEQUENCE.OF_Template( new AVA.Template() ) );

            SEQUENCE seq = (SEQUENCE) seqt.decode(implicit, istream);

            return new CertReqMsg(
                (CertRequest) seq.elementAt(0),
				(ProofOfPossession) seq.elementAt(1),
                (SEQUENCE) seq.elementAt(2)
            );
        }
    }

    public static void main(String args[]) {

      try {
        if( args.length < 1 ) {
            System.err.println("Give an arg");
            System.exit(0);
        }
        FileInputStream fis = new FileInputStream(args[0]);

        SEQUENCE.OF_Template seqt = new SEQUENCE.OF_Template(
                new CertReqMsg.Template() );

        SEQUENCE seq=null;
        byte[] bytes = new byte[ fis.available() ];
        fis.read(bytes);
        for(int i=0; i < 1; i++) {
            seq = (SEQUENCE) seqt.decode(new ByteArrayInputStream(bytes));
        }

        System.out.println("Decoded "+seq.size()+" messages");

        CertReqMsg reqmsg = (CertReqMsg) seq.elementAt(0);

        CertRequest certreq = reqmsg.getCertReq();

        System.out.println("Request ID: "+certreq.getCertReqId() );

        CertTemplate temp = certreq.getCertTemplate();

        if( temp.hasVersion() ) {
            System.out.println("Version: "+temp.getVersion());
        } else {
            System.out.println("No version");
        }
        if( temp.hasSerialNumber() ) {
            System.out.println("Serial Number: "+temp.getSerialNumber());
        } else {
            System.out.println("No serial number");
        }
        if( temp.hasSigningAlg() ) {
            System.out.println("SigningAlg: "+temp.getSigningAlg().getOID());
        } else {
            System.out.println("No signing alg");
        }
        if( temp.hasIssuer() ) {
            System.out.println("Issuer: "+temp.getIssuer().getRFC1485());
        } else {
            System.out.println("No issuer");
        }
        if( temp.hasSubject() ) {
            System.out.println("Subject: "+temp.getSubject().getRFC1485());
        } else {
            System.out.println("No subject: ");
        }
        if( temp.hasPublicKey() ) {
            System.out.println("Public Key: "+
                temp.getPublicKey().getAlgorithmIdentifier().getOID() );
        } else {
            System.out.println("No public key");
        }
        if( temp.hasIssuerUID() ) {
            System.out.println("Issuer UID: "+new BigInteger(temp.getIssuerUID().getBits() ) );
        }  else {
            System.out.println("no issuer uid");
        }
        if( temp.hasSubjectUID() ) {
            System.out.println("Subject UID: "+new BigInteger(temp.getIssuerUID().getBits() ) );
        }  else {
            System.out.println("no subject uid");
        }
        if( temp.hasNotBefore() ) {
            System.out.println("Not Before: "+
                DateFormat.getInstance().format( temp.getNotBefore() ));
        }
        if( temp.hasNotAfter() ) {
            System.out.println("Not After: "+
                DateFormat.getInstance().format( temp.getNotAfter() ));
        }

      } catch( Exception e ) {
        e.printStackTrace();
      }
    }
}
