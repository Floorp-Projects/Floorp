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

/**
 * CRMF <i>PKIPublicationInfo</i>:
 * <pre>
 * PKIPublicationInfo ::= SEQUENCE {
 *      action          INTEGER {
 *          dontPublish     (0),
 *          pleasePublish   (1) },
 *      pubInfos SEQUENCE SIZE (1..MAX) OF SinglePubInfo OPTIONAL }
 *
 * SinglePubInfo ::= SEQUENCE {
 *      pubMethod       INTEGER {
 *          dontCare    (0),
 *          x500        (1),
 *          web         (2),
 *          ldap        (3) },
 *      pubLocation     GeneralName OPTIONAL }
 * </pre>
 */
public class PKIPublicationInfo implements ASN1Value {

    /**
     * A PKIPublicationInfo action.
     */
    public static final int DONT_PUBLISH = 0;
    /**
     * A PKIPublicationInfo action.
     */
    public static final int PLEASE_PUBLISH = 1;

    /**
     * A SinglePubInfo publication method.
     */
    public static final int DONT_CARE = 0;
    /**
     * A SinglePubInfo publication method.
     */
    public static final int X500 = 1;
    /**
     * A SinglePubInfo publication method.
     */
    public static final int WEB = 2;
    /**
     * A SinglePubInfo publication method.
     */
    public static final int LDAP = 3;

    ///////////////////////////////////////////////////////////////////////
    // members and member access
    ///////////////////////////////////////////////////////////////////////

    private int action;
    private SEQUENCE pubInfos; // may be null

    /**
     * Returns the action field.
     */
    public int getAction() {
        return action;
    }

    /**
     * Returns the number of SinglePubInfos.  May be zero.
     */
    public int numPubInfos() {
        if( pubInfos == null ) {
            return 0;
        } else {
            return pubInfos.size();
        }
    }

    /**
     * Returns the pubMethod in the SinglePubInfo at the given index.
     * Should return DONT_CARE, X500, WEB, or LDAP.
     */
    public int getPubMethod(int index) {
        return ((INTEGER)((SEQUENCE)pubInfos.elementAt(index)).
                        elementAt(0)).intValue();
    }

    /**
     * Returns the pubLocation in the SinglePubInfo at the given index.
     * May return null, since pubLocation is an optional field.
     */
    public ANY getPubLocation(int index) {
        return (ANY) ((SEQUENCE)pubInfos.elementAt(index)).elementAt(1);
    }

    ///////////////////////////////////////////////////////////////////////
    // constructors
    ///////////////////////////////////////////////////////////////////////

    private PKIPublicationInfo() { }

    /**
     * Creates a new PKIPublicationInfo.
     * @param action DONT_PUBLISH or PLEASE_PUBLISH.
     * @param pubInfos A SEQUENCE of SinglePubInfo, may be null.
     */
    public PKIPublicationInfo(int action, SEQUENCE pubInfos) {
        this.action = action;
        this.pubInfos = pubInfos;
    }

    ///////////////////////////////////////////////////////////////////////
    // decoding/encoding
    ///////////////////////////////////////////////////////////////////////

    private static final Tag TAG = SEQUENCE.TAG;

    public Tag getTag() {
        return TAG;
    }

    public void encode(OutputStream ostream) throws IOException {
        encode(TAG, ostream);
    }

    public void encode(Tag implicitTag, OutputStream ostream)
            throws IOException {
        SEQUENCE seq = new SEQUENCE();

        seq.addElement( new INTEGER(action) );
        seq.addElement( pubInfos );

        seq.encode(implicitTag, ostream);
    }

    private static final Template templateInstance = new Template();

    public static Template getTemplate() {
        return templateInstance;
    }

    /**
     * A Template for decoding a PKIPublicationInfo.
     */
    public static class Template implements ASN1Template {

        private SEQUENCE.Template seqt;

        public Template() {
            seqt = new SEQUENCE.Template();
            seqt.addElement( INTEGER.getTemplate() );

            SEQUENCE.Template pubInfot = new SEQUENCE.Template();
            pubInfot.addElement(INTEGER.getTemplate());
            pubInfot.addOptionalElement(ANY.getTemplate());

            seqt.addOptionalElement( new SEQUENCE.OF_Template(pubInfot) );
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
            SEQUENCE seq = (SEQUENCE) seqt.decode(implicitTag, istream);

            return new PKIPublicationInfo(
                            ((INTEGER)seq.elementAt(0)).intValue(),
                            (SEQUENCE) seq.elementAt(1) );
        }
    }
}
