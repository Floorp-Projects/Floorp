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

package org.mozilla.jss.pkix.cmmf;

import org.mozilla.jss.asn1.*;
import java.io.OutputStream;
import java.io.IOException;
import org.mozilla.jss.util.Assert;
import java.io.ByteArrayOutputStream;

public class CertOrEncCert implements ASN1Value {

    private ANY certificate;
    byte[] encoding;

    /**
     * @exception InvalidBERException If the certificate is not a valid
     *      BER-encoding.
     */
    public CertOrEncCert(byte[] encodedCert) throws IOException,
            InvalidBERException
    {
        certificate = new ANY( new Tag(0), encodedCert );
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        certificate.encodeWithAlternateTag(new Tag(0), bos);
        encoding = bos.toByteArray();
    }

    public static final Tag TAG = new Tag(0);
    public Tag getTag() {
        return TAG;
    }

    public void encode(OutputStream ostream) throws IOException {
        ostream.write(encoding);
    }

    /**
     * @param implicitTag <b>This parameter is ignored</b>, because a CHOICE
     *  cannot have an implicit tag.
     */
    public void encode(Tag implicitTag, OutputStream ostream)
        throws IOException
    {
        Assert._assert( implicitTag.equals(TAG) );
        ostream.write(encoding);
    }
}
