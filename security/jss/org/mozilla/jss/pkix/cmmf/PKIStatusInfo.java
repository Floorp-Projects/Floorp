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

package org.mozilla.jss.pkix.cmmf;

import org.mozilla.jss.util.Assert;
import org.mozilla.jss.asn1.*;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.BitSet;

public class PKIStatusInfo implements ASN1Value {

    private INTEGER status;
    private SEQUENCE statusString;
    private int failInfo; // bitwise AND
    private boolean hasFailInfo;


    // PKIStatus constants
    public static final int granted = 0;
    public static final int grantedWithMods = 1;
    public static final int rejection = 2;
    public static final int waiting = 3;
    public static final int revocationWarning = 4;
    public static final int revocationNotification = 5;
    public static final int keyUpdateWarning = 6;

    // PKIFailureInfo constants
    // The bit string encoded in four bytes, big-endian, bit 0 is MSB.
    public static final int badAlg = 0x80000000;
    public static final int badMessageCheck = 0x40000000;
    public static final int badRequest = 0x20000000;
    public static final int badTime = 0x10000000;
    public static final int badCertId = 0x08000000;
    public static final int badDataFormat = 0x04000000;
    public static final int wrongAuthority = 0x02000000;
    public static final int incorrectData = 0x01000000;
    public static final int missingTimeStamp = 0x00800000;

    private PKIStatusInfo() { }

    /**
     * @param status A PKIStatus constant.
     * @param failInfo The bitwise AND of the PKIFailureInfo constants.
     */
    public PKIStatusInfo(int status, int failInfo) {
        this.status = new INTEGER(status);
        statusString = new SEQUENCE();
        this.failInfo = failInfo;
        hasFailInfo = true;
    }

    /**
     * Create a PKIStatusInfo with no failure info.
     * @param status A PKIStatus constant.
     * @param failInfo The bitwise AND of the PKIFailureInfo constants.
     */
    public PKIStatusInfo(int status) {
        this.status = new INTEGER(status);
        statusString = new SEQUENCE();
        hasFailInfo = false;
    }

    /**
     * Sets the <code>statusString</code> field. May be null, since this
     *  field is optional.
     */
    public void setStatusString(SEQUENCE statusString) {
        this.statusString = statusString;
    }

    /**
     * Adds a string to the statusString SEQUENCE.
     */
    public void addFreeText(String s) {
      try {
        statusString.addElement( new UTF8String(s) );
      } catch( java.io.CharConversionException e ) {
        Assert.notReached("Error encoding to UTF8");
      }
    }

    /**
     * Adds a UTF8String to the statusString SEQUENCE.
     */
    public void addFreeText(UTF8String s) {
        statusString.addElement( s );
    }

    public static final Tag TAG = SEQUENCE.TAG;
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

        seq.addElement(status);
        if( statusString.size() > 0 ) {
            seq.addElement( statusString );
        }

        if(hasFailInfo) {
            // convert failInfo to BIT_STRING
            byte[] bytes = new byte[2];
            bytes[0] = (byte) ((failInfo & 0xff000000) >>> 24);
            bytes[1] = (byte) ((failInfo & 0x00ff0000) >>> 16);
            int padCount = 7; // 7 unused bits
            BIT_STRING bs = new BIT_STRING(bytes, padCount);
            bs.setRemoveTrailingZeroes(true);
            seq.addElement( bs );
        }

        seq.encode(implicitTag, ostream);
    }

    private static final Template templateInstance = new Template();
    public static Template getTemplate() {
        return templateInstance;
    }


    public static class Template implements ASN1Template {

        private SEQUENCE.Template seqt;

        public Template() {
            seqt = new SEQUENCE.Template();
            seqt.addElement( INTEGER.getTemplate() );
            seqt.addElement(new SEQUENCE.OF_Template(UTF8String.getTemplate()));
            seqt.addElement( BIT_STRING.getTemplate() );
        }

        public boolean tagMatch(Tag tag) {
            return TAG.equals(tag);
        }

        public ASN1Value decode(InputStream istream)
                throws InvalidBERException, IOException {
            return decode(TAG, istream);
        }

        public ASN1Value decode(Tag implicitTag, InputStream istream)
                throws InvalidBERException, IOException {

            PKIStatusInfo psi;
            SEQUENCE seq = (SEQUENCE) seqt.decode(implicitTag, istream);

            BIT_STRING failInfo = (BIT_STRING) seq.elementAt(2);

            if( failInfo == null ) {
                psi = new PKIStatusInfo(((INTEGER)seq.elementAt(0)).intValue());
            } else {
                BitSet bs = failInfo.toBitSet();
                int failinfo = 0;
                for(int i = 0, bit = 0x80000000; bit > 0; i++, bit >>>= 1 ) {
                    if( bs.get(i) ) {
                        failinfo |= bit;
                    }
                }
                psi = new PKIStatusInfo(((INTEGER)seq.elementAt(0)).intValue(),
                                        failinfo);
            }

            psi.setStatusString( (SEQUENCE) seq.elementAt(1) );

            return psi;
        }
    }
}
