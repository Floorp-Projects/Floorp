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

import java.io.*;
import java.util.Vector;
import org.mozilla.jss.util.Assert;
import java.math.BigInteger;
import java.io.ByteArrayInputStream;
import org.mozilla.jss.asn1.*;
import org.mozilla.jss.pkix.primitive.*;
import org.mozilla.jss.pkix.cert.Certificate;

/*  CUT THIS???
 * <p>Although all the normal functionality of a <i>SignedData</i> is supported
 *  with high-level methods, there is a low-level API for setting the
 *  individual fields of the structure. Using these calls is an unsupported
 *  way of accomplishing unforeseen tasks. If you have reason to use these
 *  methods, please notify the authors in case it is something that should
 *  be added to the high-level interface.
 *
*/

/**
 * A CMS <i>SignedData</i> structure. 
 * <p>The certificates field should only contain X.509 certificates.
 * PKCS #6 extended certificates will fail to decode properly.
 * @author stevep
 * @author nicolson
 * @author mzhao
 */
public class SignedData implements ASN1Value {

    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////
    // Members
    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////

    private INTEGER     version;
    private SET         digestAlgorithms;
    private EncapsulatedContentInfo contentInfo;
    private SET         certificates;  // [0] optional, may be null
    private SET         crls;          // [1] optional, may be null
    private SET         signerInfos;

    // This class implements version 3 of the spec.
    private static final INTEGER VERSION = new INTEGER(3);

    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////
    // Accessor methods
    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////

    private static void verifyNotNull(Object obj) {
        if( obj == null ) {
            throw new IllegalArgumentException();
        }
    }

    /**
     * Returns the version of this SignedData.  The current version of the
     * spec is version 3.
     */
    public INTEGER getVersion() {
        return version;
    }

    /**
     * Low-level function to set the version.
     * @param version Must not be null.
    public void setVersion(INTEGER version) {
        verifyNotNull(version);
        this.version = version;
    }
     */

    /**
     * Returns the digest algorithms used by the signers to digest the
     * signed content. There may be more than one, if different signers
     * use different digesting algorithms.
     */
    public SET getDigestAlgorithmIdentifiers() {
        return digestAlgorithms;
    }

    /**
     * Low-level function to set the digest algorithm identifiers.
     * @param digestAlgIds Must not be null.
    public void setDigestAlgorithmIdentifiers(SET digestAlgIds) {
        verifyNotNull(digestAlgIds);
        this.digestAlgorithms = digestAlgIds;
    }
     */

    /**
     * Returns the EncapsulatedContentInfo containing the signed content.  The simple
     * case is for the content to be of type <i>data</i>, although any
     * content type can be signed.
     */
    public EncapsulatedContentInfo getContentInfo() {
        return contentInfo;
    }

    /**
     * Low-level function to set the EncapsulatedcontentInfo.
     * @param ci Must not be null.
    public void setContentInfo(EncapsulatedContentInfo ci) {
        verifyNotNull(ci);
        this.contentInfo = ci;
    }
     */

    /**
     * Returns the certificates field, which is a SET of
     * X.509 certificates (org.mozilla.jss.pkix.cert.Certificate).
     * PKCS #6 Extended Certificates are not supported by this implementation.
     * Returns <code>null</code> if this optional field is not present.
     *
     */
    public SET getCertificates() {
        return certificates;
    }

    /**
     * Low-level function to set the certificates.
     * @param certs May be null to signify that the <code>certificates</code>
     *  field is not present.
    public void setCertificates(SET certs) {
        this.certificates = certs;
    }
     */

    /**
     * Returns true if the <code>certificates</code> field is present.
     */
    public boolean hasCertificates() {
        return (certificates!=null);
    }

    /**
     * Returns the crls field, which contains a SET of certificate
     * revocation lists represented by ANYs (org.mozilla.jss.asn1.ANY).
     *
     */
    public SET getCrls() {
        return crls;
    }

    /**
     * Low-level function to set the crls.
     * @param certs May be null to signify that the <code>crls</code>
     *  field is not present.
    public void setCrls(SET crls) {
        this.crls = crls;
    }
     */

    /**
     * Returns true if the <code>crls</code> field is present.
     */
    public boolean hasCrls() {
        return (crls != null);
    }

    /**
     * Returns the signerInfos field, which is a SET of
     *  org.mozilla.jss.pkcs7.SignerInfo.
     */
    public SET getSignerInfos() {
        return signerInfos;
    }

    /**
     * Low-level function to set the SignerInfos.
     * @param signerInfos Must not be null.
    public void setSignerInfos(SET signerInfos) {
        verifyNotNull(sis);
        this.signerInfos = signerInfos;
    }
     */

    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////
    // Constructors
    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////

    /**
     * Low-level constructor that merely initializes all fields to null.
    private SignedData() {}
     */

    /**
     * Create a SignedData ASN1 object. Both certificates and crls
     * are optional. If you pass in a null for either value, that
     * parameter will not get written in the sequence.
     *
     * @param digestAlgorithms A SET of zero or more
     *      algorithm identifiers.  The purpose of this item is to list
     *      the digest algorithms used by the various signers to digest
     *      the signed content. This field will also be updated by
     *      the <code>addSigner</code> method. If all the signers are added
     *      with <code>addSigner</code>, it is not necessary to list
     *      the digest algorithms here.
     *      <p> If <code>null</code> is passed in, the
     *      <code>digestAlgorithms</code> field will be initialized
     *      with an empty <code>SET</code>.
     * @param contentInfo The content that is being signed. This parameter
     *      may not be <code>null</code>.  However, the <code>content</code>
     *      field of the contentInfo may be omitted, in which case the
     *      signatures contained in the <code>SignerInfo</code> structures
     *      are presumed to be on externally-supplied data.
     * @param certificates A SET of org.mozilla.jss.pkix.cert.Certificate,
     *      the certificates
     *      containing the public keys used to sign the content.  It may
     *      also contain elements of the CA chain extending from the leaf
     *      certificates. It is not necessary to include the CA chain, or
     *      indeed to include any certificates, if the certificates are
     *      expected to already be possessed by the recipient.  The recipient
     *      can use the issuer and serial number in the SignerInfo structure
     *      to search for the necessary certificates. If this parameter is
     *      <code>null</code>, the <code>certificates</code> field will be
     *      omitted.
     * @param crls A SET of ASN1Values, which should encode to the ASN1 type
     *      <i>CertificateRevocationList</i>. This implementation does
     *      not interpret crls. If this parameter is <code>null</code>,
     *      the <code>crls</code> field will be omitted.
     * @param signerInfos <i>SignerInfo</i> structures containing signatures
     *      of the content.  Additional signerInfos can be added with
     *      the <code>addSigner</code> method. If this parameter is
     *      <code>null</code>, the field will be initialized with an
     *      empty <code>SET</code>.
     */
    public SignedData(  SET digestAlgorithms,
                        EncapsulatedContentInfo contentInfo, SET certificates,
                        SET crls, SET signerInfos) {

        version = VERSION;

        if(digestAlgorithms == null ) {
            this.digestAlgorithms = new SET();
        } else {
            this.digestAlgorithms = digestAlgorithms;
        }

        verifyNotNull(contentInfo);
        this.contentInfo = contentInfo;

        // certificates may be null
        this.certificates = certificates;

        // crls may be null
        this.crls = crls;

        if(signerInfos == null) {
            this.signerInfos = new SET();
        } else {
            this.signerInfos = signerInfos;
        }
    }

    /**
     * Constructor for creating a SignedData from its encoding.
     */
    SignedData( INTEGER version,
                SET digestAlgorithms,
                EncapsulatedContentInfo contentInfo,
                SET certificates,
                SET crls,
                SET signerInfos )
    {

        verifyNotNull(version);
        this.version = version;

        verifyNotNull(digestAlgorithms);
        this.digestAlgorithms = digestAlgorithms;

        verifyNotNull(contentInfo);
        this.contentInfo = contentInfo;

        // certificates may be null
        this.certificates = certificates;

        // crls may be null
        this.crls = crls;

        verifyNotNull(signerInfos);
        this.signerInfos = signerInfos;
    }

    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////
    // Cryptographic functions
    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////


    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////
    // DER encoding
    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////

    static final Tag TAG = SEQUENCE.TAG;
    public Tag getTag() {
        return TAG;
    }

    public void encode(OutputStream ostream) throws IOException {
        encode(getTag(),ostream);
    }

    public void encode(Tag tag, OutputStream ostream) throws IOException {
        SEQUENCE sequence = new SEQUENCE();

        sequence.addElement(version);
        sequence.addElement(digestAlgorithms);
        sequence.addElement(contentInfo);
        if( certificates != null ) {
            sequence.addElement( new Tag(0), certificates );
        }
        if( crls != null ) {
            sequence.addElement( new Tag(1), crls );
        }
        sequence.addElement(signerInfos);

        sequence.encode(tag,ostream);
    }


    private static final Template templateInstance = new Template();
    public static Template getTemplate() {
        return templateInstance;
    }

    /**
     * A template file for decoding a SignedData blob
     *
     */
    public static class Template implements ASN1Template {
            private SEQUENCE.Template seqt;

        public Template() {
            seqt = new SEQUENCE.Template();

            // version
            seqt.addElement(INTEGER.getTemplate());

            // digestAlgorithms
            seqt.addElement(new SET.OF_Template(
                            AlgorithmIdentifier.getTemplate()));

            // content info
            seqt.addElement(EncapsulatedContentInfo.getTemplate());

            // [0] IMPLICIT certificates OPTIONAL
            seqt.addOptionalElement(
                        new Tag(0),
                        new SET.OF_Template(Certificate.getTemplate()));

            // [1] IMPLICIT CertificateRevocationLists OPTIONAL
            seqt.addOptionalElement(
                        new Tag(1),
                        new SET.OF_Template(ANY.getTemplate()));

            // signerInfos
            seqt.addElement(new SET.OF_Template(SignerInfo.getTemplate()));
        }

        public boolean tagMatch(Tag tag) {
            return TAG.equals(tag);
        }

        public ASN1Value decode(InputStream istream) 
            throws IOException, InvalidBERException
            {
                return decode(TAG, istream);
            }

        public ASN1Value decode(Tag implicitTag, InputStream istream)
            throws IOException, InvalidBERException
            {
                SEQUENCE seq = (SEQUENCE) seqt.decode(implicitTag, istream);
                Assert._assert(seq.size() == 6);

                return new SignedData(
                    (INTEGER)     seq.elementAt(0),
                    (SET)         seq.elementAt(1),
                    (EncapsulatedContentInfo) seq.elementAt(2),
                    (SET)         seq.elementAt(3),
                    (SET)         seq.elementAt(4),
                    (SET)         seq.elementAt(5)
                    );
            }
    } // end of template

}
