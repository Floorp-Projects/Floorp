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

package org.mozilla.jss.pkix.crmf;

import org.mozilla.jss.asn1.*;
import java.io.*;
import org.mozilla.jss.pkix.primitive.AVA;
import org.mozilla.jss.util.Assert;

/**
 * A CRMF <code>Control</code>.
 */
public class Control extends AVA implements ASN1Value {

    // general CRMF OIDs
        public static final OBJECT_IDENTIFIER
    id_pkix = new OBJECT_IDENTIFIER( new long[] { 1, 3, 6, 1, 5, 5, 7 } );
        public static final OBJECT_IDENTIFIER
    id_pkip = id_pkix.subBranch( 5 );
        public static final OBJECT_IDENTIFIER
    id_regCtrl = id_pkip.subBranch( 1 );
        

    // Control OIDs
        public static final OBJECT_IDENTIFIER
    id_regCtrl_regToken = id_regCtrl.subBranch(1);
        public static final OBJECT_IDENTIFIER
    id_regCtrl_authenticator = id_regCtrl.subBranch(2);
        public static final OBJECT_IDENTIFIER
    id_regCtrl_pkiPublicationInfo = id_regCtrl.subBranch(3);
        public static final OBJECT_IDENTIFIER
    id_regCtrl_pkiArchiveOptions = id_regCtrl.subBranch(4);
        public static final OBJECT_IDENTIFIER
    id_regCtrl_oldCertID = id_regCtrl.subBranch(5);
        public static final OBJECT_IDENTIFIER
    id_regCtrl_protocolEncrKey = id_regCtrl.subBranch(6);

    public Control(OBJECT_IDENTIFIER oid, ASN1Value value) {
        super(oid, value);
    }

    /**
     * Returns the value of this control as a UTF8String, if it actually
     *  is a UTF8String.
     */
    public UTF8String getUTF8String() throws InvalidBERException {
        return (UTF8String) getValue().decodeWith(UTF8String.getTemplate());
    }

    /**
     * Returns the value of this control as a PKIArchiveOptions, if it
     *  actually is a PKIArchiveOptions.
     */
    public PKIArchiveOptions getPKIArchiveOptions() throws InvalidBERException {
        return (PKIArchiveOptions) getValue().decodeWith(
                    PKIArchiveOptions.getTemplate() );
    }

    /**
     * Returns the value of this control as a PKIPublicationInfo, if it
     *  actually is a PKIPublicationInfo.
     */
    public PKIPublicationInfo getPKIPublicationInfo()
            throws InvalidBERException {
        return (PKIPublicationInfo) getValue().decodeWith(
                    PKIPublicationInfo.getTemplate() );
    }

    /**
     * A template class for decoding a Control from a BER stream.
     */
    public static class Template extends AVA.Template implements ASN1Template {
        private SEQUENCE.Template seqTemplate;

        public Template() {
            seqTemplate = new SEQUENCE.Template();
            seqTemplate.addElement( new OBJECT_IDENTIFIER.Template() );
            seqTemplate.addElement( new ANY.Template()               );
        }

        public ASN1Value decode(InputStream istream)
            throws IOException, InvalidBERException
        {
            return decode(TAG, istream);
        }

        public ASN1Value decode(Tag implicit, InputStream istream)
            throws IOException, InvalidBERException
        {
            SEQUENCE seq = (SEQUENCE) seqTemplate.decode(implicit, istream);
            OBJECT_IDENTIFIER oid = (OBJECT_IDENTIFIER) seq.elementAt(0);
            ANY any = (ANY) seq.elementAt(1);

            return new Control( oid, any );
        }
    }
}
