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

/* usage:
   java TestCertImport <dbdir> <importfile> [<nickname>] 

   - importfile is the base-64 encoded certificate
   - nickname is an optional to refer to the certificate.
     [if not specified, one is created]
 */

import org.mozilla.jss.*;

import java.io.FileInputStream;
import org.mozilla.jss.util.ConsolePasswordCallback;
import org.mozilla.jss.crypto.*;
import org.mozilla.jss.util.Assert;

public class TestCertImport {

    public static void main(String args[]) {
        String password = null;
        if(args.length == 3) {
            password = args[2];
        } else if(args.length != 2) {
            System.out.println("Usage: java TestCertImport"
                +" <dbdir> <importfile> [<nickname>]");
            return;
        }

      try {
        CryptoManager.InitializationValues vals = 
            new CryptoManager.InitializationValues( args[0]+"/secmodule.db",
                                  args[0]+"/key3.db",
                                  args[0]+"/cert7.db");
        CryptoManager.initialize(vals);
        CryptoManager manager = CryptoManager.getInstance();

        FileInputStream fin = new FileInputStream( args[1] );
        byte[] pkg = new byte[fin.available()];

        fin.read(pkg);

        X509Certificate cert = manager.importCertPackage(pkg, password);

        if(cert instanceof TokenCertificate) {
            System.out.println("Imported user cert!");
        } else {
            Assert.assert( cert instanceof InternalCertificate );
            System.out.println("Imported CA cert chain. Setting trust bits.");
            ((InternalCertificate)cert).setSSLTrust(
                InternalCertificate.VALID_CA |
                InternalCertificate.TRUSTED_CA);
        }

      } catch (Exception e) {
            e.printStackTrace();
      }
    }
}
