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

import org.mozilla.jss.CryptoManager;
import org.mozilla.jss.util.ConsolePasswordCallback;
import org.mozilla.jss.util.Debug;
import org.mozilla.jss.crypto.*;
import java.math.BigInteger;

public class TestCryptoStore {

    public static void main(String args[]) {
        int i;
        if(args.length!=2) {
            System.out.println("Usage: jre TestCryptoStore <dbdir> <token>");
            return;
        }

        try {

        CryptoManager.InitializationValues vals = new
            CryptoManager.InitializationValues( args[0]+"/secmodule.db",
                                  args[0]+"/key3.db",
                                  args[0]+"/cert7.db" );
        try {
            vals.setInternalTokenDescription
                ("TestCryptoStore Internal Token    "); // too long
            System.out.println("ERROR: accepted invalid token description");
            return;
        } catch(CryptoManager.InvalidLengthException e) {
        }
        vals.setInternalKeyStorageTokenDescription
            ("TestCryptoStore Key Token        ");
        vals.setInternalSlotDescription
("TestCryptoStore Internal Slot Description                        ");
        CryptoManager.initialize(vals);
        CryptoManager manager = CryptoManager.getInstance();
        CryptoStore store;

		System.out.println("Opening token : "+args[1]);
        CryptoToken token = manager.getTokenByName(args[1]);

        store = token.getCryptoStore();
        PrivateKey[] keys = store.getPrivateKeys();
        System.out.println("Keys:");
        for(i=0; i < keys.length; i++) {
			PrivateKey key = (PrivateKey)keys[i];
            byte[] keyID = key.getUniqueID();
            if(keyID.length!=20) {
                System.out.println("ERROR: keyID length != 20");
            }
            BigInteger bigint = new BigInteger(keyID);
            System.out.println("UniqueID: "+ bigint);
        }

        X509Certificate[] certs = store.getCertificates();
        System.out.println("\n====================\nCerts:");
        for(i=0; i < certs.length; i++) {
            X509Certificate cert = (X509Certificate)certs[i];
            System.out.println("Name: "+cert.getNickname());
            System.out.println("Subject: "+cert.getSubjectDN());
            System.out.println("Issuer: "+cert.getIssuerDN());
            System.out.println("Serial Number: "+cert.getSerialNumber());
            System.out.println("Version: "+cert.getVersion());

            if(cert instanceof TokenCertificate) {
                TokenCertificate tcert = (TokenCertificate)cert;
                byte[] id = tcert.getUniqueID();
                if(id.length != 20) {
                    System.out.println("ERROR: cert ID length != 20");
                }
                BigInteger bigint = new BigInteger(id);
                System.out.println("UniqueID: "+bigint);
            }

            if(cert instanceof InternalCertificate) {
                InternalCertificate icert = (InternalCertificate)cert;
                System.out.println("SSL     Trust bitmask: "+icert.getSSLTrust());
                System.out.println("Email   Trust bitmask: "+icert.getEmailTrust());
                System.out.println("ObjSign Trust bitmask: "+icert.getObjectSigningTrust());
            }
            System.out.println();
        }

        certs = manager.getCACerts();
        System.out.println("\n====================\nCA Certs:");
        for(i=0; i < certs.length; i++) {
            X509Certificate cert = (X509Certificate)certs[i];
            System.out.println("Name: "+cert.getNickname());
            System.out.println("Subject: "+cert.getSubjectDN());
            System.out.println("Issuer: "+cert.getIssuerDN());
            System.out.println("Serial Number: "+cert.getSerialNumber());
            System.out.println("Version: "+cert.getVersion());

            if(cert instanceof TokenCertificate) {
                TokenCertificate tcert = (TokenCertificate)cert;
                byte[] id = tcert.getUniqueID();
                if(id.length != 20) {
                    System.out.println("ERROR: cert ID length != 20");
                }
                BigInteger bigint = new BigInteger(id);
                System.out.println("UniqueID: "+bigint);
            }


            if(cert instanceof InternalCertificate) {
                InternalCertificate icert = (InternalCertificate)cert;
                System.out.println("SSL Trust: "+icert.getSSLTrust());
                System.out.println("Email Trust: "+icert.getEmailTrust());
                System.out.println("OS Trust: "+icert.getObjectSigningTrust());
            }
            System.out.println();
        }

        System.out.println("done.");

        } catch( Exception e) {
            e.printStackTrace();
        }
    }

}
