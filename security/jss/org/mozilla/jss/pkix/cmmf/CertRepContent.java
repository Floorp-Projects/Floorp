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

import org.mozilla.jss.asn1.*;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.FileOutputStream;
import java.io.ByteArrayInputStream;
import java.io.FileInputStream;

/**
 * A CMMF <i>CertRepContent</i>.
 * <pre>
 * CertRepContent ::= SEQUENCE {
 *      caPubs      [1] SEQUENCE SIZE (1..MAX) OF Certificate OPTIONAL,
 *      response    SEQUENCE of CertResponse }
 * </pre>
 * @see org.mozilla.jss.pkix.cmmf.CertResponse
 */
public class CertRepContent implements ASN1Value {

    private byte[][] caPubs; // may be null
    private SEQUENCE response;

    private CertRepContent() { }

    /**
     * Creates a new <code>CertRepContent</code>.
     *
     * @param caPubs An array of DER-encoded X.509 Certificates. It may be
     *      null if the <code>caPubs</code> field is to be omitted.
     * @param response A SEQUENCE of <code>CertResponse</code> objects.
     *      Must not be null.
     */
    public CertRepContent(byte[][] caPubs, SEQUENCE response) {
        this.caPubs = caPubs;
        this.response = response;
    }

    /**
     * Creates a new <code>CertRepContent</code>. The responses can be
     *  added later with <code>addCertResponse</code>.
     *
     * @param caPubs An array of DER-encoded X.509 Certificates, must not
     *      be null and must have at least one element.
     */
    public CertRepContent(byte[][] caPubs) {
        this.caPubs = caPubs;
        response = new SEQUENCE();
    }

    /**
     * Creates a new <code>CertRepContent</code>
     *
     * @param response A SEQUENCE of <code>CertResponse</code> objects.
     *      Must not be null.
     */
    public CertRepContent(SEQUENCE response) {
        this.caPubs = null;
        this.response = response;
    }

    /**
     * Adds another <code>CertResponse</code>.
     */
    public void addCertResponse(CertResponse resp) {
        response.addElement(resp);
    }

    /**
     * Returns the <code>caPubs</code> field, which is an array of
     *   DER-encoded X.509 Certificates. May return <code>null</code> if the
     *      field is not present.
     */
    public byte[][] getCaPubs() {
        return caPubs;
    }

    /**
     * Returns the <code>response</code> field, which is a SEQUENCE
     * of <code>CertResponse</code>
     */
    public SEQUENCE getResponse() {
        return response;
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
        SEQUENCE encoding = new SEQUENCE();

        // create sequence of certificates
        if(caPubs != null) {
            SEQUENCE certs = new SEQUENCE();
            for(int i = 0; i < caPubs.length; i++) {
                certs.addElement( new ANY( SEQUENCE.TAG, caPubs[i] ) );
            }
            encoding.addElement( new Tag(1), certs );
        }

        encoding.addElement( response );

        encoding.encode(implicitTag, ostream);
    }

    public static void main(String argv[]) {

      try {

        if(argv.length != 2) {
            System.out.println("Usage: CertRepContent <certfile> <outputfile>");
            System.out.println("certfile should contain a DER-encoded X.509 "+
                    "certificate");
            System.exit(-1);
        }
        FileInputStream certfile = new FileInputStream(argv[0]);
        FileOutputStream fos = new FileOutputStream(argv[1]);

        byte[][] certs = new byte[2][];
        certs[0] = new byte[ certfile.available() ];
        certfile.read(certs[0]);
        certs[1] = certs[0];

        PKIStatusInfo status = new PKIStatusInfo(PKIStatusInfo.rejection,
                    PKIStatusInfo.badRequest | PKIStatusInfo.badTime );

        status.addFreeText("And your mother dresses you funny");
        status.addFreeText("so there");

        CertifiedKeyPair ckp = new CertifiedKeyPair(
                                new CertOrEncCert( certs[0] ) );
        CertResponse resp = new CertResponse( new INTEGER(54), status, ckp);

        CertRepContent content = new CertRepContent(certs);
        content.addCertResponse(resp);

        content.encode(fos);
        System.out.println("Success!");

      } catch( Exception e ) {
        e.printStackTrace();
      }
    }
}
