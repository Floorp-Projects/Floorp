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

import org.mozilla.jss.asn1.*;
import org.mozilla.jss.pkix.primitive.*;
import java.io.*;

public class SignedAndEnvelopedData implements ASN1Value {

    ///////////////////////////////////////////////////////////////////////
    // members and member access
    ///////////////////////////////////////////////////////////////////////
    private INTEGER version;
    private SET recipientInfos;
    private SET digestAlgorithms;
    private EncryptedContentInfo encryptedContentInfo;
    private SET certificates; // may be null
    private SET crls; // may be null
    private SET signerInfos;
    private SEQUENCE sequence; // for encoding

    /**
     * Returns the version number.  The current version is 1.
     */
    public INTEGER getVersion() {
        return version;
    }

    /**
     * Returns a SET of RecipientInfo.
     */
    public SET getRecipientInfos() {
        return recipientInfos;
    }

    /**
     * Returns a SET of AlgorithmIdentifier.
     */
    public SET getDigestAlgorithms() {
        return digestAlgorithms;
    }

    /**
     * Returns the encrypted content.
     */
    public EncryptedContentInfo getEncryptedContentInfo() {
        return encryptedContentInfo;
    }

    /**
     * Returns a SET of ANYs. May return <code>null</code> if the
     * <i>certificates</i> field is not present.
     */
    public SET getCertificates() {
        return certificates;
    }

    /**
     * Returns a SET of ANYs. May return <code>null</code> if the <i>crls</i>
     * field is not present.
     */
    public SET getCrls() {
        return crls;
    }

    /**
     * Returns a SET of SignerInfo.
     */
    public SET getSignerInfos() {
        return signerInfos;
    }

    ///////////////////////////////////////////////////////////////////////
    // Constructors
    ///////////////////////////////////////////////////////////////////////

    private SignedAndEnvelopedData() { }

    public SignedAndEnvelopedData(
                        INTEGER version,
                        SET recipientInfos,
                        SET digestAlgorithms,
                        EncryptedContentInfo encryptedContentInfo,
                        SET certificates,
                        SET crls,
                        SET signerInfos)
    {
        if( version==null || recipientInfos==null || digestAlgorithms==null
            || encryptedContentInfo==null || signerInfos==null ) {
            throw new IllegalArgumentException(
                "SignedAndEnvelopedData constructor parameter is null");
        }

        this.version = version;
        this.recipientInfos = recipientInfos;
        this.digestAlgorithms = digestAlgorithms;
        this.encryptedContentInfo = encryptedContentInfo;
        this.certificates = certificates;
        this.crls = crls;
        this.signerInfos = signerInfos;

        sequence = new SEQUENCE();
        sequence.addElement(version);
        sequence.addElement(recipientInfos);
        sequence.addElement(digestAlgorithms);
        sequence.addElement(encryptedContentInfo);
        if( certificates!=null ) {
            sequence.addElement(certificates);
        }
        if( crls!=null ) {
            sequence.addElement(crls);
        }
        sequence.addElement( signerInfos );
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


    /**
     * A Template class for decoding BER-encoded SignedAndEnvelopedData items.
     */
    public static class Template implements ASN1Template {

        private SEQUENCE.Template seqt;

        public Template() {
            seqt = new SEQUENCE.Template();

            seqt.addElement(INTEGER.getTemplate());
            seqt.addElement(new SET.OF_Template(RecipientInfo.getTemplate()));
            seqt.addElement(new SET.OF_Template(
                                    AlgorithmIdentifier.getTemplate()) );
            seqt.addElement(EncryptedContentInfo.getTemplate());
            seqt.addOptionalElement(new Tag(0),
                    new SET.OF_Template(ANY.getTemplate()));
            seqt.addOptionalElement(new Tag(1),
                    new SET.OF_Template(ANY.getTemplate()));
            seqt.addElement(new SET.OF_Template(SignerInfo.getTemplate()));
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

            return new SignedAndEnvelopedData(
                        (INTEGER) seq.elementAt(0),
                        (SET) seq.elementAt(1),
                        (SET) seq.elementAt(2),
                        (EncryptedContentInfo) seq.elementAt(3),
                        (SET) seq.elementAt(4),
                        (SET) seq.elementAt(5),
                        (SET) seq.elementAt(6) );
        }
    }
}
