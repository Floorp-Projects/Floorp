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
import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import org.mozilla.jss.util.Assert;

public class Extension implements ASN1Value {
    public static final Tag TAG = SEQUENCE.TAG;
    public Tag getTag() {
        return TAG;
    }

    private OBJECT_IDENTIFIER extnId;
    /**
     * Returns the extension identifier.
     */
    public OBJECT_IDENTIFIER getExtnId() {
        return extnId;
    }

    private boolean critical;
    public boolean getCritical() {
        return critical;
    }

    private OCTET_STRING extnValue;
    public OCTET_STRING getExtnValue() {
        return extnValue;
    }

    private Extension() { }

    public Extension( OBJECT_IDENTIFIER extnId, boolean critical,
        OCTET_STRING extnValue )
    {
        this.extnId = extnId;
        this.critical = critical;
        this.extnValue = extnValue;
    }

    public void encode(OutputStream ostream) throws IOException {
        encode(TAG, ostream);
    }

    public void encode(Tag implicit, OutputStream ostream) throws IOException {
        SEQUENCE seq = new SEQUENCE();

        seq.addElement( extnId );
        if( critical == true ) {
            // false is default, so we only code true
            seq.addElement( new BOOLEAN(true) );
        }
        seq.addElement( extnValue );

        seq.encode(implicit, ostream);
    }

    private static final Template templateInstance = new Template();
    public static Template getTemplate() {
        return templateInstance;
    }

    public static class Template implements ASN1Template {

        private SEQUENCE.Template seqt;

        public Template() {
            seqt = new SEQUENCE.Template();
            seqt.addElement( OBJECT_IDENTIFIER.getTemplate() );
            seqt.addElement( BOOLEAN.getTemplate(), new BOOLEAN(false) );
            seqt.addElement( OCTET_STRING.getTemplate() );
        }

        public boolean tagMatch(Tag t) {
            return TAG.equals(t);
        }

        public ASN1Value decode(InputStream istream)
            throws IOException, InvalidBERException
        {
            return decode(TAG, istream);
        }

        public ASN1Value decode(Tag implicit, InputStream istream)
            throws IOException, InvalidBERException
        {
            SEQUENCE seq = (SEQUENCE) seqt.decode(implicit, istream);

            return new Extension(
                (OBJECT_IDENTIFIER) seq.elementAt(0),
                ((BOOLEAN) seq.elementAt(1)).toBoolean(),
                (OCTET_STRING) seq.elementAt(2)
            );
        }
    }
}
