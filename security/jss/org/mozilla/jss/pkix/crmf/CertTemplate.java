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

import org.mozilla.jss.asn1.*;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintStream;
import java.io.IOException;
import org.mozilla.jss.util.Assert;
import java.util.Date;
import org.mozilla.jss.pkix.primitive.*;
import org.mozilla.jss.pkix.cert.Extension;
import java.util.Calendar;
import java.util.TimeZone;

/**
 * This class models a CRMF <i>CertTemplate</i> structure.
 */
public class CertTemplate implements ASN1Value {

    // All of these may be null
    private INTEGER version;
    private INTEGER serialNumber;
    private AlgorithmIdentifier signingAlg;
    private Name issuer;
    private Date notBefore;
    private Date notAfter;
    private Name subject;
    private SubjectPublicKeyInfo publicKey;
    private BIT_STRING issuerUID;
    private BIT_STRING subjectUID;
    private SEQUENCE extensions;

    /**
     * Creates an empty CertTemplate. Use the accessor methods to fill it
     * up with stuff.
     */
    public CertTemplate() { }

    /**
     * Returns true if the version field is present.
     */
    public boolean hasVersion() {
        return (version!=null);
    }

    /**
     * Returns the <i>version</i> field of this <i>CertTemplate</i>.
     */
    public INTEGER getVersion() {
        return version;
    }

    /**
     * Sets the <i>version</i> field of this <i>CertTemplate</i>.
     */
    public void setVersion(INTEGER version) {
        this.version = version;
    }

    /**
     * Returns true if the serialNumber field is present.
     */
    public boolean hasSerialNumber() {
        return (serialNumber!=null);
    }

    /**
     * Returns the <i>serialNumber</i> field of this <i>CertTemplate</i>.
     */
    public INTEGER getSerialNumber() {
        return serialNumber;
    }

    /**
     * Sets the <i>serialNumber</i> field of this <i>CertTemplate</i>.
     */
    public void setSerialNumber(INTEGER serialNumber) {
        this.serialNumber = serialNumber;
    }

    /**
     * Returns true if the signingAlg field is present.
     */
    public boolean hasSigningAlg() {
        return (signingAlg!=null);
    }

    /**
     * Returns the <i>signingAlg</i> field of this <i>CertTemplate</i>.
     */
    public AlgorithmIdentifier getSigningAlg() {
        return signingAlg;
    }

    /**
     * Sets the <i>signingAlg</i> field of this <i>CertTemplate</i>.
     */
    public void setSigningAlg(AlgorithmIdentifier signingAlg) {
        this.signingAlg = signingAlg;
    }

    /**
     * Returns true if the issuer field is present.
     */
    public boolean hasIssuer() {
        return (issuer!=null);
    }

    /**
     * Returns the <i>issuer</i> field of this <i>CertTemplate</i>.
     */
    public Name getIssuer() {
        return issuer;
    }

    /**
     * Sets the <i>issuer</i> field of this <i>CertTemplate</i>.
     */
    public void setIssuer(Name issuer) {
        this.issuer = issuer;
    }

    /**
     * Returns true if the notBefore field is present.
     */
    public boolean hasNotBefore() {
        return (notBefore!=null);
    }

    /**
     * Returns the <i>notBefore</i> field of this <i>CertTemplate</i>.
     */
    public Date getNotBefore() {
        return notBefore;
    }

    /**
     * Sets the <i>version</i> field of this <i>CertTemplate</i>.
     */
    public void setNotBefore(Date date) {
        this.notBefore = date;
    }

    /**
     * Returns true if the notAfter field is present.
     */
    public boolean hasNotAfter() {
        return (notAfter!=null);
    }

    /**
     * Returns the <i>notAfter</i> field of this <i>CertTemplate</i>.
     */
    public Date getNotAfter() {
        return notAfter;
    }

    /**
     * Sets the <i>notAfter</i> field of this <i>CertTemplate</i>.
     */
    public void setNotAfter(Date date) {
        this.notAfter = date;
    }

    /**
     * Returns true if the subject field is present.
     */
    public boolean hasSubject() {
        return (subject!=null);
    }

    /**
     * Sets the <i>subject</i> field of this <i>CertTemplate</i>.
     */
    public Name getSubject() {
        return subject;
    }

    /**
     * Sets the <i>subject</i> field of this <i>CertTemplate</i>.
     */
    public void setSubject(Name subject) {
        this.subject = subject;
    }

    /**
     * Returns true if the publicKey field is present.
     */
    public boolean hasPublicKey() {
        return (publicKey!=null);
    }

    /**
     * Returns the <i>publicKey</i> field of this <i>CertTemplate</i>.
     */
    public SubjectPublicKeyInfo getPublicKey() {
        return publicKey;
    }

    /**
     * Sets the <i>publicKey</i> field of this <i>CertTemplate</i>.
     */
    public void setPublicKey(SubjectPublicKeyInfo publicKey) {
        this.publicKey = publicKey;
    }

    /**
     * Returns true if the issuerUID field is present.
     */
    public boolean hasIssuerUID() {
        return (issuerUID!=null);
    }

    /**
     * Returns the <i>issuerUID</i> field of this <i>CertTemplate</i>.
     */
    public BIT_STRING getIssuerUID() {
        return issuerUID;
    }

    /**
     * Sets the <i>issuerUID</i> field of this <i>CertTemplate</i>.
     */
    public void setIssuerUID(BIT_STRING issuerUID) {
        this.issuerUID = issuerUID;
    }

    /**
     * Returns true if the subjectUID field is present.
     */
    public boolean hasSubjectUID() {
        return (subjectUID!=null);
    }

    /**
     * Returns the <i>subjectUID</i> field of this <i>CertTemplate</i>.
     */
    public BIT_STRING getSubjectUID() {
        return subjectUID;
    }

    /**
     * Sets the <i>subjectUID</i> field of this <i>CertTemplate</i>.
     */
    public void setSubjectUID(BIT_STRING subjectUID) {
        this.subjectUID = subjectUID;
    }

    /**
     * Returns true if the extensions field is present.
     */
    public boolean hasExtensions() {
        return (extensions!=null);
    }

    /**
     * Sets the <i>extensions</i> field of this <i>CertTemplate</i>.
     */
    public void setExtensions(SEQUENCE extensions) {
        this.extensions = extensions;
    }

    /**
     * Returns the number of extensions present in the template.  May be zero.
     */
    public int numExtensions() {
        if(extensions == null) {
            return 0;
        } else {
            return extensions.size();
        }
    }

    /**
     * Returns the <i>i</i>th extension.
     * @param idx The index of the extension to retrieve.  Must be in the
     *      range [ 0, numExtensions()-1 ].
     */
    public Extension extensionAt(int idx) {
        if(extensions == null) {
            throw new ArrayIndexOutOfBoundsException();
        }
        return (Extension) extensions.elementAt(idx);
    }

    public void print(PrintStream ps, int indentSpaces)
            throws InvalidBERException, IOException {
        StringBuffer indentBuf = new StringBuffer();
        for(int i=0; i < indentSpaces; i++) {
            indentBuf.append(" ");
        }
        String indent = indentBuf.toString();

        if(version!=null) {
            ps.println("Version: "+version.toString());
        }
        if(serialNumber!=null) {
            ps.println("Serial Number: "+serialNumber.toString());
        }
        if(signingAlg!=null) {
            ps.println("Signing Algorithm: "+signingAlg.getOID().toString());
        }
        if(issuer!=null) {
            ps.println("Issuer: "+issuer.getRFC1485());
        }
        if(notBefore!=null) {
            ps.println("Not Before: " + notBefore);
        }
        if(notAfter!=null) {
            ps.println("Not After: " + notAfter);
        }
        if(subject!=null) {
            ps.println("Subject: " + subject.getRFC1485());
        }
        if(publicKey!=null) {
            ps.println("publicKey is present");
        }
        if(issuerUID != null ) {
            ps.println("issuerUID is present");
        }
        if(subjectUID != null ) {
            ps.println("subjectUID is present");
        }
        if(extensions != null ) {
            ps.println("Extensions is present, with "+extensions.size()+
                " elements");
        }
    }

    public static final Tag TAG = SEQUENCE.TAG;
    public Tag getTag() {
        return TAG;
    }

    public void encode(OutputStream ostream) throws IOException {
        encode(TAG, ostream);
    }

    // the last year for using UTCTime
    static final int UTCTIME_CUTOFF_YEAR = 2049;

    /**
     * Converts a Date into a UTCTime or GeneralizedTime, depending on
     * whether it falls before or after the cutoff date.
     */
    private static TimeBase dateToASN1(Date d) {
        if(d==null) {
            return null;
        }
        Calendar cal = Calendar.getInstance( TimeZone.getTimeZone("GMT") );
        cal.setTime(d);
        if( cal.get(Calendar.YEAR) <= UTCTIME_CUTOFF_YEAR) {
            return new UTCTime(d);
        } else {
            return new GeneralizedTime(d);
        }
    }

    public void encode(Tag t, OutputStream ostream) throws IOException {
        SEQUENCE seq = new SEQUENCE();

        seq.addElement(Tag.get(0), version);
        seq.addElement(Tag.get(1), serialNumber);
        seq.addElement(Tag.get(2), signingAlg);
        if( issuer!=null ) {
            // issuer is a CHOICE, so it must be EXPLICITly tagged
            seq.addElement(new EXPLICIT(Tag.get(3), issuer ));
        }
        if( notBefore!=null || notAfter!=null ) {
            SEQUENCE optionalVal = new SEQUENCE();
            // notBefore & notAfter are CHOICES, so must be EXPLICITly tagged
            if( notBefore!=null ) {
                optionalVal.addElement( new EXPLICIT(
                    Tag.get(0), dateToASN1(notBefore) ) );
            }
            if( notAfter!=null ) {
                optionalVal.addElement( new EXPLICIT(
                    Tag.get(1), dateToASN1(notAfter) ) );
            }
            seq.addElement(Tag.get(4), optionalVal);
        }
        if( subject!=null ) {
            // subject is a CHOICE, so it must be EXPLICITly tagged
            seq.addElement(new EXPLICIT(Tag.get(5), subject));
        }
        seq.addElement(Tag.get(6), publicKey);
        seq.addElement(Tag.get(7), issuerUID);
        seq.addElement(Tag.get(8), subjectUID);
        seq.addElement(Tag.get(9), extensions);

        seq.encode(t, ostream);
    }

    private static Template templateInstance = new Template();
    public static Template getTemplate() {
        return templateInstance;
    }

    /**
     * A class for decoding <i>CertTemplate</i>s.
     */
    public static class Template implements ASN1Template {

        public boolean tagMatch(Tag tag) {
            return TAG.equals(tag);
        }

        /**
         * Decodes a <i>CertTemplate</i> from its BER encoding.  The return
         * value of this method
         */
        public ASN1Value decode(InputStream istream)
            throws IOException, InvalidBERException
        {
            return decode(TAG, istream);
        }

        public ASN1Value decode(Tag implicit, InputStream istream)
            throws IOException, InvalidBERException
        {
            CHOICE.Template timeChoice = new CHOICE.Template();
            timeChoice.addElement( new GeneralizedTime.Template() );
            timeChoice.addElement( new UTCTime.Template() );

            // optional validity. The times are CHOICEs, so they are
            // EXPLICITly tagged
            SEQUENCE.Template validity = new SEQUENCE.Template();
            validity.addOptionalElement( new EXPLICIT.Template(
                            Tag.get(0), timeChoice));
            validity.addOptionalElement( new EXPLICIT.Template(
                            Tag.get(1), timeChoice));

            SEQUENCE.Template seqt = new SEQUENCE.Template();

            seqt.addOptionalElement( Tag.get(0), new INTEGER.Template() );
            seqt.addOptionalElement( Tag.get(1), new INTEGER.Template() );
            seqt.addOptionalElement( Tag.get(2),
                                     new AlgorithmIdentifier.Template() );
            seqt.addOptionalElement( new EXPLICIT.Template(Tag.get(3),
                                            new Name.Template() ));
            seqt.addOptionalElement( Tag.get(4), validity );
            seqt.addOptionalElement( new EXPLICIT.Template(Tag.get(5),
                                            new Name.Template() ));
            seqt.addOptionalElement( Tag.get(6),
                                new SubjectPublicKeyInfo.Template() );
            seqt.addOptionalElement( Tag.get(7), new BIT_STRING.Template() );
            seqt.addOptionalElement( Tag.get(8), new BIT_STRING.Template() );
            seqt.addOptionalElement( Tag.get(9),
                    new SEQUENCE.OF_Template( new Extension.Template() ) );

            SEQUENCE seq = (SEQUENCE) seqt.decode(implicit, istream);

            CertTemplate ct = new CertTemplate();

            ct.setVersion( (INTEGER) seq.elementAt(0) );
            ct.setSerialNumber( (INTEGER) seq.elementAt(1) );
            ct.setSigningAlg( (AlgorithmIdentifier)
                                    seq.elementAt(2) );
            if( seq.elementAt(3) != null ) {
                ct.setIssuer((Name)((EXPLICIT)seq.elementAt(3)).getContent() );
            }

            // validity
            EXPLICIT explicit;
            CHOICE choice;
            ASN1Value val;
            if( seq.elementAt(4) != null ) {
                explicit = (EXPLICIT) ((SEQUENCE)seq.elementAt(4)).elementAt(0);
                if( explicit != null ) {
                    choice = (CHOICE) explicit.getContent();
                    val = choice.getValue();
                    if( val instanceof TimeBase ) {
                        ct.setNotBefore( ((TimeBase)val).toDate() );
                    }
                }
                explicit = (EXPLICIT) ((SEQUENCE)seq.elementAt(4)).elementAt(1);
                if( explicit != null ) {
                    choice = (CHOICE) explicit.getContent();
                    val = choice.getValue();
                    if( val instanceof TimeBase ) {
                        ct.setNotAfter( ((TimeBase)val).toDate() );
                    }
                }
            }

            if( seq.elementAt(5) != null ) {
                ct.setSubject((Name)((EXPLICIT)seq.elementAt(5)).getContent() );
            }
            ct.setPublicKey( (SubjectPublicKeyInfo) seq.elementAt(6) );
            ct.setIssuerUID( (BIT_STRING) seq.elementAt(7) );
            ct.setSubjectUID( (BIT_STRING) seq.elementAt(8) );
            ct.setExtensions( (SEQUENCE) seq.elementAt(9) );

            return ct;
        }
    }

    public static void main(String args[]) {

      try {

        CertTemplate ct = new CertTemplate();
        Name name;

        ct.setVersion(new INTEGER(5));
        ct.setSerialNumber(new INTEGER(13112));
        
        name = new Name();
        name.addCommonName("You");
        name.addStateOrProvinceName("California");
        ct.setIssuer(name);
        ct.setNotBefore(new Date());
        name = new Name();
        name.addCommonName("Me");
        name.addCountryName("US");
        ct.setSubject(name);
        ct.setIssuerUID( new BIT_STRING( new byte[] {0x00, 0x01}, 0 ) );

        System.out.println("Constructed CertTemplate:");

        byte[] encoded = ASN1Util.encode(ct);
        java.io.FileOutputStream fos = new java.io.FileOutputStream("certTemplate");
        fos.write(encoded);
        fos.close();

        ct.print(System.out, 0);

        CertTemplate newCt = (CertTemplate) ASN1Util.decode(
                CertTemplate.getTemplate(), encoded );

        System.out.println("\nDecoded CertTemplate:");
        newCt.print(System.out, 0);

      } catch( Exception e ) {
            e.printStackTrace();
      }
    }
}
