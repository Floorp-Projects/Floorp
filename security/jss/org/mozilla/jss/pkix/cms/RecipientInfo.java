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
package org.mozilla.jss.pkix.cms;

import org.mozilla.jss.pkix.primitive.*;

import java.io.*;
import org.mozilla.jss.asn1.*;
import java.util.Vector;
import org.mozilla.jss.util.Assert;
import java.math.BigInteger;
import java.io.ByteArrayInputStream;

public class RecipientInfo implements ASN1Value {

    public static final Tag TAG = SEQUENCE.TAG;
    public Tag getTag() {
        return TAG;
    }

    private INTEGER               version;
    private IssuerAndSerialNumber issuerAndSerialNumber;
    private AlgorithmIdentifier   keyEncryptionAlgorithmID;
    private OCTET_STRING          encryptedKey;
    
    private SEQUENCE sequence = new SEQUENCE();

    public INTEGER getVersion() {
        return version;
    }
    public IssuerAndSerialNumber getissuerAndSerialNumber() {
        return issuerAndSerialNumber;
    }
    public AlgorithmIdentifier getKeyEncryptionAlgorithmID() {
        return keyEncryptionAlgorithmID;
    }
    public OCTET_STRING getEncryptedKey() {
        return encryptedKey;
    }


    private static final Template templateInstance = new Template();
    
    public static Template getTemplate() {
	return templateInstance;
    }


    private RecipientInfo() {
        }

    /**
     * Create a RecipientInfo ASN1 object.
     */

    public RecipientInfo(  INTEGER version,
			   IssuerAndSerialNumber issuerAndSerialNumber,
			   AlgorithmIdentifier keyEncryptionAlgorithmID,
			   OCTET_STRING encryptedKey) {

	Assert._assert(issuerAndSerialNumber != null);
	Assert._assert(keyEncryptionAlgorithmID != null);
	Assert._assert(encryptedKey != null);


        this.version = version;
        this.issuerAndSerialNumber = issuerAndSerialNumber;
        this.keyEncryptionAlgorithmID = keyEncryptionAlgorithmID;
        this.encryptedKey = encryptedKey;


        sequence.addElement(version);
        sequence.addElement(issuerAndSerialNumber);
        sequence.addElement(keyEncryptionAlgorithmID);
        sequence.addElement(encryptedKey);
        
    }

    public void encode(OutputStream ostream) throws IOException {
        encode(getTag(),ostream);
    }

    public void encode(Tag tag, OutputStream ostream) throws IOException {
        sequence.encode(tag,ostream);
    }


    /**
     * A template file for decoding a RecipientInfo blob
     *
     */

    public static class Template implements ASN1Template {
        public Tag getTag() {
            return RecipientInfo.TAG;
        }

        public boolean tagMatch(Tag tag) {
            return (tag.equals(RecipientInfo.TAG));
        }

        public ASN1Value decode(InputStream istream)
            throws IOException, InvalidBERException
            {
                return decode(getTag(),istream);
            }

        public ASN1Value decode(Tag implicitTag, InputStream istream)
            throws IOException, InvalidBERException
            {
                SEQUENCE.Template seqt = new SEQUENCE.Template();
                seqt.addElement(new INTEGER.Template());
                seqt.addElement(new IssuerAndSerialNumber.Template());
                seqt.addElement(new AlgorithmIdentifier.Template());
                seqt.addElement(new OCTET_STRING.Template());

                SEQUENCE seq = (SEQUENCE) seqt.decode(implicitTag,istream);
                Assert._assert(seq.size() ==4);

                return new RecipientInfo(
                    (INTEGER)               seq.elementAt(0),
                    (IssuerAndSerialNumber) seq.elementAt(1),
                    (AlgorithmIdentifier)   seq.elementAt(2),
                    (OCTET_STRING)          seq.elementAt(3)
                  
                    );
            }
    } // end of template

}
