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
 * The Original Code is Netscape Security Services for Java.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
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

import org.mozilla.jss.ssl.*;
import org.mozilla.jss.CryptoManager;
import org.mozilla.jss.crypto.*;
import org.mozilla.jss.asn1.*;
import org.mozilla.jss.pkix.primitive.*;
import org.mozilla.jss.pkix.cert.*;
import org.mozilla.jss.pkix.cert.Certificate;
import org.mozilla.jss.util.PasswordCallback;

public class JSSPackageTest {

    private static CryptoManager    cm          = null;

    public static void main(String[] args) {
        String certDbPath = ".";
        try {
            try {
                certDbPath = (String)args[0];
            } catch (Exception e) { 
                System.out.println("Exception caught : " + e.getMessage());
                e.printStackTrace();
                System.exit(1);
            }
            CryptoManager.initialize(certDbPath);

            Package pkg = Package.getPackage("org.mozilla.jss");

            System.out.println("\n---------------------------------------------------------");
            System.out.println("Checking jss jar and library version");
            System.out.println("---------------------------------------------------------");
            System.out.println("              Introspecting jss jar file");
            System.out.println("Package name:\t" + pkg.getName());

            System.out.println("Spec title  :\t" + pkg.getSpecificationTitle());
            System.out.println("Spec vendor :\t" + pkg.getSpecificationVendor());
            System.out.println("Spec version:\t" + pkg.getSpecificationVersion());

            System.out.println("Impl title  :\t" + pkg.getImplementationTitle());
            System.out.println("Impl vendor :\t" + pkg.getImplementationVendor());
            System.out.println("Impl version:\t" + pkg.getImplementationVersion());

            System.out.println("\n  Fetching version information from CryptoManager");
            System.out.println(CryptoManager.JAR_JSS_VERSION);
            System.out.println(CryptoManager.JAR_NSS_VERSION);
            System.out.println(CryptoManager.JAR_NSPR_VERSION);

            System.out.println("\n  Checking for header information in jss library");
            System.exit(0);

        } catch (Exception e) {
            System.out.println("Exception caught : " + e.getMessage());
            e.printStackTrace();
            System.exit(1);
        }
    }
}
