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
import org.mozilla.jss.asn1.*;
import java.util.Vector;
import org.mozilla.jss.util.Assert;
import java.math.BigInteger;
import java.io.ByteArrayInputStream;

public class EnvelopedData implements ASN1Value {
    public static final Tag TAG = SEQUENCE.TAG;
    public Tag getTag() {
        return TAG;
    }

    private INTEGER              version;
    private SET                  recipientInfos;
    private EncryptedContentInfo         encryptedContentInfo;

    private SEQUENCE sequence = new SEQUENCE();

    public INTEGER getVersion() {
        return version;
    }
    public SET getRecipientInfos() {
        return recipientInfos;
    }
    public EncryptedContentInfo getEncryptedContentInfo() {
        return encryptedContentInfo;
    }


     
    private EnvelopedData() {
        }

    /**
     * Create a EnvelopedData ASN1 object. 
     */

    public EnvelopedData(  INTEGER version, SET recipientInfos,
                        EncryptedContentInfo encryptedContentInfo) {

        this.version = version;
        this.recipientInfos = recipientInfos;
        this.encryptedContentInfo = encryptedContentInfo;
  
        sequence.addElement(version);
        sequence.addElement(recipientInfos);
        sequence.addElement(encryptedContentInfo);
    }

    public void encode(OutputStream ostream) throws IOException {
        encode(getTag(),ostream);
    }

    public void encode(Tag tag, OutputStream ostream) throws IOException {
        sequence.encode(tag,ostream);
    }


    /**
     * A template file for decoding a EnvelopedData blob
     *
     */

    public static class Template implements ASN1Template {
        public Tag getTag() {
            return EnvelopedData.TAG;
        }

        public boolean tagMatch(Tag tag) {
            return (tag.equals(EnvelopedData.TAG));
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
                seqt.addElement(new SET.OF_Template(new RecipientInfo.Template()));
                seqt.addElement(new EncryptedContentInfo.Template());

                SEQUENCE seq = (SEQUENCE) seqt.decode(implicitTag,istream);
                Assert._assert(seq.size() ==3);

                return new EnvelopedData(
                    (INTEGER)               seq.elementAt(0),
                    (SET)                   seq.elementAt(1),
                    (EncryptedContentInfo)  seq.elementAt(2)
                    );
            }
    } // end of template

}
