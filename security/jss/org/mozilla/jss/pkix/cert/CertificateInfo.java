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

package org.mozilla.jss.pkix.cert;

import org.mozilla.jss.asn1.*;
import org.mozilla.jss.pkix.primitive.*;
import org.mozilla.jss.util.*;
import java.security.cert.CertificateException;
import java.security.PublicKey;
import java.util.Calendar;
import java.util.Date;
import java.util.TimeZone;
import java.io.InputStream;
import java.io.PrintStream;
import java.io.OutputStream;
import java.io.IOException;

/**
 * A TBSCertificate (to-be-signed certificate), the actual information in
 * a certificate apart from the signature.
 */
public class CertificateInfo implements ASN1Value {

    /**
     * An X.509 Certificate version.
     */
    public static class Version {
        private Version() { }
        private Version(int vers, String string) {
            versionNumber = vers;
            this.string = string;
        }

        int versionNumber;
        String string;

        public boolean equals(Object obj) {
            if(obj == null || !(obj instanceof Version)) {
                return false;
            }
            return ((Version)obj).versionNumber == versionNumber;
        }

        public int getNumber() {
            return versionNumber;
        }

        /**
         * Creates a version number from its numeric encoding. 
         */
        public static Version fromInt(int versionNum)
            throws InvalidBERException
        {
            if( versionNum == 0 ) {
                return v1;
            } else if( versionNum == 1) {
                return v2;
            } else if( versionNum == 2) {
                return v3;
            } else {
                throw new InvalidBERException("Unrecognized version number");
            }
        }

        public String toString() {
            return string;
        }

        public static final Version v1 = new Version(0, "v1");
        public static final Version v2 = new Version(1, "v2");
        public static final Version v3 = new Version(2, "v3");
    }
    public static final Version v1 = Version.v1;
    public static final Version v2 = Version.v2;
    public static final Version v3 = Version.v3;

    // the last year for using UTCTime
    static final int UTCTIME_CUTOFF_YEAR = 2049;

    private Version version = v1;
    private INTEGER serialNumber;
    private AlgorithmIdentifier signatureAlgId;
    private Name issuer;
    private Date notBefore;
    private Date notAfter;
    private Name subject;
    private SubjectPublicKeyInfo subjectPublicKeyInfo;
    private BIT_STRING issuerUniqueIdentifier; // may be null
    private BIT_STRING subjectUniqueIdentifier; // may be null
    private SEQUENCE extensions; // if no extensions, size() == 0.  Never null.


    /**
     * Creates a CertificateInfo with the required fields.
     */
    public CertificateInfo(Version version, INTEGER serialNumber,
        AlgorithmIdentifier signatureAlgId, Name issuer, Date notBefore,
        Date notAfter, Name subject, SubjectPublicKeyInfo subjectPublicKeyInfo)
    {
        setVersion(version);
        setSerialNumber(serialNumber);
        setSignatureAlgId(signatureAlgId);
        setIssuer(issuer);
        setNotBefore(notBefore);
        setNotAfter(notAfter);
        setSubject(subject);
        setSubjectPublicKeyInfo(subjectPublicKeyInfo);
        extensions = new SEQUENCE();
    }

    public void setVersion(Version version) {
        verifyNotNull(version);
        this.version = version;
    }
    public Version getVersion() {
        return version;
    }

    public void setSerialNumber(INTEGER serialNumber) {
        verifyNotNull(serialNumber);
        this.serialNumber = serialNumber;
    }
    public INTEGER getSerialNumber() {
        return serialNumber;
    }

    public void setSignatureAlgId(AlgorithmIdentifier signatureAlgId) {
        verifyNotNull(signatureAlgId);
        this.signatureAlgId = signatureAlgId;
    }
    public AlgorithmIdentifier getSignatureAlgId() {
        return signatureAlgId;
    }

    public void setIssuer(Name issuer) {
        verifyNotNull(issuer);
        this.issuer = issuer;
    }
    public Name getIssuer() {
        return issuer;
    }

    public void setNotBefore(Date notBefore) {
        verifyNotNull(notBefore);
        this.notBefore = notBefore;
    }
    public Date getNotBefore() {
        return notBefore;
    }

    public void setNotAfter(Date notAfter) {
        verifyNotNull(notAfter);
        this.notAfter = notAfter;
    }
    public Date getNotAfter() {
        return notAfter;
    }

    public void setSubject(Name subject) {
        verifyNotNull(subject);
        this.subject = subject;
    }
    public Name getSubject() {
        return subject;
    }

    public void setSubjectPublicKeyInfo(
                    SubjectPublicKeyInfo subjectPublicKeyInfo)
    {
        verifyNotNull(subjectPublicKeyInfo);
        this.subjectPublicKeyInfo = subjectPublicKeyInfo;
    }
    /**
     * Extracts the SubjectPublicKeyInfo from the given public key and
     * stores it in the CertificateInfo.
     *
     * @exception InvalidBERException If an error occurs decoding the
     *      the information extracted from the public key.
     */
    public void setSubjectPublicKeyInfo( PublicKey pubk ) 
        throws InvalidBERException, IOException
    {
        verifyNotNull(pubk);
        setSubjectPublicKeyInfo( new SubjectPublicKeyInfo(pubk) );
    }
    public SubjectPublicKeyInfo getSubjectPublicKeyInfo() {
        return subjectPublicKeyInfo;
    }

    /**
     * @exception CertificateException If the certificate is a v1 certificate.
     */
    public void setIssuerUniqueIdentifier(BIT_STRING issuerUniqueIdentifier)
        throws CertificateException
    {
        if(version==v1) {
            throw new CertificateException("issuerUniqueIdentifier cannot"+
                " be specified for v1 certificate");
        }
        verifyNotNull(issuerUniqueIdentifier);
        this.issuerUniqueIdentifier = issuerUniqueIdentifier;
    }
    public boolean hasIssuerUniqueIdentifier() {
        return (issuerUniqueIdentifier!=null);
    }
    /**
     * Should only be called if this field is present.
     */
    public BIT_STRING getIssuerUniqueIdentifier() {
        Assert._assert(issuerUniqueIdentifier != null);
        return issuerUniqueIdentifier;
    }

    /**
     * @exception CertificateException If the certificate is a v1 certificate.
     */
    public void setSubjectUniqueIdentifier(BIT_STRING subjectUniqueIdentifier)
        throws CertificateException
    {
        if(version==v1) {
            throw new CertificateException("subjectUniqueIdentifier cannot"+
                " be specified for v1 certificate");
        }
        verifyNotNull(subjectUniqueIdentifier);
        this.subjectUniqueIdentifier = subjectUniqueIdentifier;
    }
    public boolean hasSubjectUniqueIdentifier() {
        return (subjectUniqueIdentifier!=null);
    }
    public BIT_STRING getSubjectUniqueIdentifier() {
        Assert._assert(subjectUniqueIdentifier != null);
        return subjectUniqueIdentifier;
    }

    public boolean hasExtensions() {
        return extensions.size() > 0;
    }
    /**
     * Returns the extensions of this certificate.  The sequence may be
     * empty, but this method will never return <code>null</code>.
     */
    public SEQUENCE getExtensions() {
        return extensions;
    }

    /**
     * Linearly searches the extension list for an extension with the given
     * object identifier. If it finds one, returns <tt>true</tt>. Otherwise,
     * returns <tt>false</tt>.
     */
    public boolean isExtensionPresent(OBJECT_IDENTIFIER oid) {
        return ( getExtension(oid) != null );
    }

    /**
     * Linearly searches the extension list for an extension with the given
     * object identifier. It returns the first one it finds. If none are found,
     * returns <tt>null</tt>.
     */
    public Extension getExtension(OBJECT_IDENTIFIER oid) {
        int numExtensions = extensions.size();
        for( int i=0; i < numExtensions; ++i) {
            Extension ext = (Extension) extensions.elementAt(i);
            if( oid.equals(ext.getExtnId()) ) {
                return ext;
            }
        }
        return null;
    }

    /**
     * @exception CertificateException If the certificate is not a v3
     *      certificate.
     */
    public void setExtensions(SEQUENCE extensions) throws CertificateException {
        if(version != v3) {
            throw new CertificateException("extensions can only be added to"+
                " v3 certificates");
        }
        if( Debug.DEBUG ) {
            int size = extensions.size();
            for(int i=0; i < size; i++) {
                Assert._assert(extensions.elementAt(i) instanceof Extension);
            }
        }
        verifyNotNull(extensions);
        this.extensions = extensions;
    }

    /**
     * @exception CertificateException If the certificate is not a v3
     *      certificate.
     */
    public void addExtension(Extension extension) throws CertificateException {
        if(version != v3) {
            throw new CertificateException("extensions can only be added to"+
                " v3 certificates");
        }
        extensions.addElement(extension);
    }

    private void verifyNotNull(Object obj) {
        if( obj == null ) {
            throw new NullPointerException();
        }
    }

    static final Tag TAG = SEQUENCE.TAG;
    public Tag getTag() {
        return TAG;
    }

    public void encode(OutputStream ostream) throws IOException {
        encode(TAG, ostream);
    }

    public void encode(Tag implicitTag, OutputStream ostream)
        throws IOException
    {
        SEQUENCE seq = new SEQUENCE();

        if( version != v1 ) {
            // v1 is the default
            seq.addElement(new EXPLICIT( new Tag(0),
                                         new INTEGER(version.getNumber()) ) );
        }
        seq.addElement(serialNumber);
        seq.addElement(signatureAlgId);
        seq.addElement(issuer);

        SEQUENCE validity = new SEQUENCE();
        validity.addElement( encodeValidityDate(notBefore) );
        validity.addElement( encodeValidityDate(notAfter) );
        seq.addElement(validity);

        seq.addElement(subject);
        seq.addElement(subjectPublicKeyInfo);

        if( issuerUniqueIdentifier != null ) {
            seq.addElement(new Tag(1), issuerUniqueIdentifier);
        }

        if( subjectUniqueIdentifier != null ) {
            seq.addElement(new Tag(2), subjectUniqueIdentifier);
        }

        if( extensions.size() > 0 ) {
            seq.addElement(new EXPLICIT(new Tag(3), extensions) );
        }

        seq.encode(implicitTag, ostream);
    }

    /**
     * Returns the correct ASN1Value (UTCTime or GeneralizedTime) to represent
     * the given certificate validity date.
     */
    private static ASN1Value encodeValidityDate(Date d) {
        Calendar cal = Calendar.getInstance( TimeZone.getTimeZone("GMT") );
        cal.setTime(d);
        if( cal.get(Calendar.YEAR) <= UTCTIME_CUTOFF_YEAR) {
            return new UTCTime(d);
        } else {
            return new GeneralizedTime(d);
        }
    }

    private static final Template templateInstance = new Template();
    public static Template getTemplate() {
        return templateInstance;
    }

    public void print(PrintStream ps) throws IOException, InvalidBERException {
        ps.println("CertificateInfo:");
        ps.println("Version: "+version);
        ps.println("Serial Number: "+serialNumber);
        ps.println("Sig OID: "+signatureAlgId.getOID());
        ps.println("Issuer: "+issuer.getRFC1485());
        ps.println("Not Before: "+notBefore);
        ps.println("Not After: "+notAfter);
        ps.println("Subject: "+subject.getRFC1485());
    }

    /**
     * Template class for decoding a CertificateInfo.
     */
    public static class Template implements ASN1Template {

        SEQUENCE.Template seqt;

        public Template() {
            seqt = new SEQUENCE.Template();

            EXPLICIT.Template versionTemp = EXPLICIT.getTemplate( new Tag(0),
                                            INTEGER.getTemplate() );
            EXPLICIT defVersion =
                        new EXPLICIT(new Tag(0), new INTEGER(v1.getNumber()) );
            seqt.addElement( versionTemp, defVersion );
            seqt.addElement(INTEGER.getTemplate()); //serial number
            seqt.addElement(AlgorithmIdentifier.getTemplate()); //signatureAlgId
            seqt.addElement(Name.getTemplate()); // issuer

            // deal with validity
            SEQUENCE.Template validity = new SEQUENCE.Template();
            CHOICE.Template timeChoice = CHOICE.getTemplate();
            timeChoice.addElement( UTCTime.getTemplate() );
            timeChoice.addElement( GeneralizedTime.getTemplate() );
            validity.addElement( timeChoice ); // notBefore
            validity.addElement( timeChoice ); // notAfter

            seqt.addElement(validity);
            seqt.addElement(Name.getTemplate()); //subject
            seqt.addElement(SubjectPublicKeyInfo.getTemplate());
            seqt.addOptionalElement( new Tag(1), BIT_STRING.getTemplate() );
            seqt.addOptionalElement( new Tag(2), BIT_STRING.getTemplate() );

            // deal with extensions
            SEQUENCE.OF_Template extnTemp =
                new SEQUENCE.OF_Template( Extension.getTemplate() );
            seqt.addOptionalElement( new EXPLICIT.Template(
                                        new Tag(3), extnTemp) );
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
          try {
            SEQUENCE seq = (SEQUENCE) seqt.decode(implicitTag, istream);

            EXPLICIT exp = (EXPLICIT) seq.elementAt(0);
            Version version = Version.fromInt(
                                    ((INTEGER)exp.getContent()).intValue() );

            SEQUENCE validity = (SEQUENCE) seq.elementAt(4);
            CHOICE choice = (CHOICE)validity.elementAt(0);
            Date notBefore = ((TimeBase)choice.getValue()).toDate();
            choice = (CHOICE)validity.elementAt(1);
            Date notAfter = ((TimeBase)choice.getValue()).toDate();
            
            CertificateInfo cinfo = new CertificateInfo(
                    version,
                    (INTEGER) seq.elementAt(1),     // serial num
                    (AlgorithmIdentifier) seq.elementAt(2), //sigAlgId
                    (Name) seq.elementAt(3),        // issuer
                    notBefore,
                    notAfter,
                    (Name) seq.elementAt(5),        // subject
                    (SubjectPublicKeyInfo) seq.elementAt(6)
                );

            if( seq.elementAt(7) != null ) {
                cinfo.setIssuerUniqueIdentifier( (BIT_STRING)seq.elementAt(7) );
            }
            if( seq.elementAt(8) != null ) {
                cinfo.setSubjectUniqueIdentifier( (BIT_STRING)seq.elementAt(8));
            }
            if( seq.elementAt(9) != null ) {
                exp = (EXPLICIT)seq.elementAt(9);
                cinfo.setExtensions( (SEQUENCE) exp.getContent() );
            }

            return cinfo;

          } catch( CertificateException e ) {
                throw new InvalidBERException(e.getMessage());
          }
        }
    }
}
