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

package org.mozilla.jss.pkix.cms;

import org.mozilla.jss.asn1.*;
import java.io.*;

/**
 * The PKCS #7 structure <i>EncryptedData</i>.
 */
public class EncryptedData implements ASN1Value {

    ///////////////////////////////////////////////////////////////////////
    // Members
    ///////////////////////////////////////////////////////////////////////
    private INTEGER version;
    private EncryptedContentInfo encryptedContentInfo;
    private SEQUENCE sequence;

    /**
     * The default version number.  This should always be used unless
     * you really know what you are doing.
     */
    public static final INTEGER DEFAULT_VERSION = new INTEGER(0);

    ///////////////////////////////////////////////////////////////////////
    // Constructors
    ///////////////////////////////////////////////////////////////////////
    private EncryptedData() { }

    /**
     * Creates a new EncryptedData.
     * 
     * @param version Should usually be DEFAULT_VERSION unless you are being
     *      very clever.
     */
    public EncryptedData(   INTEGER version,
                            EncryptedContentInfo encryptedContentInfo )
    {
        if( version == null || encryptedContentInfo == null ) {
            throw new IllegalArgumentException("null parameter");
        }

        sequence = new SEQUENCE();

        this.version = version;
        sequence.addElement(version);
        this.encryptedContentInfo = encryptedContentInfo;
        sequence.addElement(encryptedContentInfo);
    }

    /**
     * Creates an EncryptedData with the default version.
     */
    public EncryptedData( EncryptedContentInfo encryptedContentInfo ) {
        this( DEFAULT_VERSION, encryptedContentInfo );
    }

    ///////////////////////////////////////////////////////////////////////
    // Accessor Methods
    ///////////////////////////////////////////////////////////////////////
    public INTEGER getVersion() {
        return version;
    }

    public EncryptedContentInfo getEncryptedContentInfo() {
        return encryptedContentInfo;
    }

    
    ///////////////////////////////////////////////////////////////////////
    //  DER encoding
    ///////////////////////////////////////////////////////////////////////

    private static final Tag TAG = SEQUENCE.TAG;
    public Tag getTag() {
        return TAG;
    }

    public void encode(OutputStream ostream) throws IOException {
        sequence.encode(ostream);
    }

    public void encode(Tag implicitTag, OutputStream ostream)
        throws IOException
    {
        sequence.encode(implicitTag, ostream);
    }

    public static Template getTemplate() {
        return templateInstance;
    }
    private static final Template templateInstance = new Template();

    /**
     * A Template for decoding EncryptedData items.
     */
    public static class Template implements ASN1Template {

        private SEQUENCE.Template seqt;

        public Template() {
            seqt = new SEQUENCE.Template();
            seqt.addElement( INTEGER.getTemplate() );
            seqt.addElement( EncryptedContentInfo.getTemplate() );
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

            SEQUENCE seq = (SEQUENCE) seqt.decode(implicitTag, istream);

            return new EncryptedData(
                        (INTEGER)               seq.elementAt(0),
                        (EncryptedContentInfo)  seq.elementAt(1) );
        }
    }
}
