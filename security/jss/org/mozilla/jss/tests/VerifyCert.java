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

package org.mozilla.jss.tests;

import org.mozilla.jss.CryptoManager;
import org.mozilla.jss.crypto.*;
import org.mozilla.jss.util.*;
import org.mozilla.jss.pkix.cert.*;
import java.util.Vector;
import java.util.Enumeration;
import java.io.InputStream;
import java.io.BufferedInputStream;
import java.io.FileInputStream;
import java.util.ArrayList;
import java.util.Iterator;


public class VerifyCert {

    public void showCert( String certFile) {
        //Read the cert
        try {

            BufferedInputStream bis = new BufferedInputStream(
                                new FileInputStream(certFile) );

            Certificate cert = (Certificate)
                 Certificate.getTemplate().decode(bis);

            //output the cert
            CertificateInfo info = cert.getInfo();
            info.print(System.out);

//verify the signature of the cert only
//        cert.verify();
        } catch (Exception ex) {
            ex.printStackTrace();
        }
    }

    private void usage() {

        System.out.println("Usage: java org.mozilla.jss.tests.VerifyCert");
        System.out.println("\noptions:\n\n<dbdir> <passwd> " +
                           "<nicknameOfCertinDB> <OCSPResponderURL> " +
                           "<OCSPCertNickname>\n");
        System.out.println("<dbdir> <passwd> " +
                           "<DerEncodeCertFile> <OCSPResponderURL> " +
                           "<OCSPCertNickname>\n");
        System.out.println("Note: <OCSPResponderURL> and " +
                           "<OCSPCertNickname> are optional.\n      But if used, " +
                           "both Url/nickname must be specified.");
    }

    public static void main(String args[]) {

        try {
            VerifyCert vc = new VerifyCert();
            if ( args.length < 3 ) {
                vc.usage();
                return;
            }
            String dbdir = args[0];
            String password = args[1];
            String name = args[2];
            String ResponderURL = null;
            String ResponderNickname = null;
            //if OCSPResponderURL than must have OCSPCertificateNickname
            if (args.length == 4 || args.length > 5)   vc.usage();
            else if (args.length == 5) {
                ResponderURL= args[3];
                ResponderNickname = args[4];
            }

            //initialize JSS
            CryptoManager.InitializationValues vals = new
                                CryptoManager.InitializationValues(dbdir);
            //      configure OCSP
            vals.ocspCheckingEnabled = true;
            if (ResponderURL != null && ResponderNickname != null) {
                vals.ocspResponderCertNickname = ResponderNickname;
                vals.ocspResponderURL = ResponderURL;
            }
            CryptoManager.initialize(vals);
            CryptoManager cm = CryptoManager.getInstance();
            PasswordCallback pwd = new Password(password.toCharArray());
            cm.setPasswordCallback(pwd);

            try {
                FileInputStream fin = new FileInputStream(name);
                byte[] pkg = new byte[fin.available()];
                fin.read(pkg);
                //display the cert
                vc.showCert(name);
                //validate the cert
                vc.validateDerCert(pkg, cm);
            } catch (java.io.FileNotFoundException e) {
                //assume name is a nickname of cert in the db
                vc.validateCertInDB(name, cm);
            }

        } catch ( Exception e ) {
            e.printStackTrace();
        }
    }


    public void validateDerCert(byte[] pkg, CryptoManager cm){
        ArrayList usageList = new ArrayList();
        try {

            Iterator list = CryptoManager.CertUsage.getCertUsages();
            CryptoManager.CertUsage certUsage;
            while(list.hasNext()) {
                certUsage = (CryptoManager.CertUsage) list.next();
                if (
       !certUsage.equals(CryptoManager.CertUsage.UserCertImport) &&
       !certUsage.equals(CryptoManager.CertUsage.ProtectedObjectSigner) &&
       !certUsage.equals(CryptoManager.CertUsage.AnyCA) )
                    {
                        if (cm.isCertValid(pkg, true,
                            certUsage) == true) {
                            usageList.add(certUsage.toString());
                        }
                    }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }

        if (usageList.isEmpty()) {
            System.out.println("The certificate is not valid.");
        } else {
        System.out.println("The certificate is valid for the following usages:\n");
            Iterator iterateUsage = usageList.iterator();
            while (iterateUsage.hasNext()) {
                System.out.println("                       " 
                + iterateUsage.next());
            }
        }
    }

    public void validateCertInDB(String nickname, CryptoManager cm){
        ArrayList usageList = new ArrayList();

        try {

            Iterator list = CryptoManager.CertUsage.getCertUsages();
            CryptoManager.CertUsage certUsage;
            while(list.hasNext()) {
                certUsage = (CryptoManager.CertUsage) list.next();
                if (
       !certUsage.equals(CryptoManager.CertUsage.UserCertImport) &&
       !certUsage.equals(CryptoManager.CertUsage.ProtectedObjectSigner) &&
       !certUsage.equals(CryptoManager.CertUsage.AnyCA) )
                    {
                        if (cm.isCertValid(nickname, true,
                            certUsage) == true) {
                            usageList.add(certUsage.toString());
                        }
                    }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }


        if (usageList.isEmpty()) {
            System.out.println("The certificate is not valid.");
        } else {
            System.out.println("The certificate is valid for the following usages:\n");
            Iterator iterateUsage = usageList.iterator();
            while (iterateUsage.hasNext()) {
                System.out.println("                       " +
                                          iterateUsage.next());
            }
        }
    }

}
