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
import org.mozilla.jss.util.Assert;

public class DigestInfo implements ASN1Value {

    private AlgorithmIdentifier digestAlgorithm;
    private OCTET_STRING digest;
    private SEQUENCE sequence;

    private DigestInfo() { }

    public DigestInfo(AlgorithmIdentifier digestAlgorithm, OCTET_STRING digest){
        if( digestAlgorithm==null || digest==null ) {
            throw new IllegalArgumentException();
        }
        sequence = new SEQUENCE();
        this.digestAlgorithm = digestAlgorithm;
        sequence.addElement(digestAlgorithm);
        this.digest = digest;
        sequence.addElement(digest);
    }

    public AlgorithmIdentifier
    getDigestAlgorithm() {
        return digestAlgorithm;
    }

    public OCTET_STRING
    getDigest() {
        return digest;
    }

    private static final Tag TAG = SEQUENCE.TAG;
    public Tag getTag() {
        return TAG;
    }

    public boolean equals(Object obj) {
        if( obj==null || !(obj instanceof DigestInfo)) {
            return false;
        }
        DigestInfo di = (DigestInfo)obj;

        return byteArraysAreSame(di.digest.toByteArray(), digest.toByteArray());
    }

    /**
     * Compares two non-null byte arrays.  Returns true if they are identical,
     * false otherwise.
     */
    private static boolean byteArraysAreSame(byte[] left, byte[] right) {

        Assert._assert(left!=null && right!=null);

        if( left.length != right.length ) {
            return false;
        }

        for(int i = 0 ; i < left.length ; i++ ) {
            if( left[i] != right[i] ) {
                return false;
            }
        }

        return true;
    }

    public void encode(OutputStream ostream) throws IOException {
        sequence.encode(ostream);
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

    /**
     * A class for decoding the BER encoding of a DigestInfo.
     */
    public static class Template implements ASN1Template {

        private SEQUENCE.Template seqt;

        public Template() {
            seqt = new SEQUENCE.Template();
            seqt.addElement( AlgorithmIdentifier.getTemplate());
            seqt.addElement( OCTET_STRING.getTemplate() );
        }

        public boolean tagMatch(Tag tag) {
            return TAG.equals(tag);
        }

        public ASN1Value decode(InputStream ostream)
            throws InvalidBERException, IOException
        {
            return decode(TAG, ostream);
        }

        public ASN1Value decode(Tag implicitTag, InputStream ostream)
            throws InvalidBERException, IOException
        {
            SEQUENCE seq = (SEQUENCE) seqt.decode(implicitTag, ostream);

            return new DigestInfo(
                    (AlgorithmIdentifier)       seq.elementAt(0),
                    (OCTET_STRING)              seq.elementAt(1) );
        }
    }
}
