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

import org.mozilla.jss.*;
import org.mozilla.jss.pkcs11.*;
import org.mozilla.jss.crypto.*;
import java.io.*;
import org.mozilla.jss.util.PasswordCallback;


public class FipsTest {

    public static void main(String args[]) {

      try {

        if( args.length < 2 ) {
            System.out.println("Usage: FipsTest <dbdir> <fipsmode enter: " +
                    "enable OR disable OR chkfips > <password file>");
            return;
        }
        String dbdir = args[0];
        String fipsmode = args[1];

        String password = "";

        if (args.length == 3) {
           password = args[2];
           System.out.println("The password file " +password);
        }

        CryptoManager.InitializationValues vals = new
                CryptoManager.InitializationValues(dbdir);

        System.out.println("output of Initilization values ");
        System.out.println("Manufacturer ID: " + vals.getManufacturerID());
        System.out.println("Library: " + vals.getLibraryDescription());
        System.out.println("Internal Slot: " +
                            vals.getInternalSlotDescription());
        System.out.println("Internal Token: " +
                            vals.getInternalTokenDescription());
        System.out.println("Key Storage Slot: "  +
                            vals.getFIPSKeyStorageSlotDescription());
        System.out.println("Key Storage Token: "  +
                            vals.getInternalKeyStorageTokenDescription());
        System.out.println("FIPS Slot: " +
                            vals.getFIPSSlotDescription());
        System.out.println("FIPS Key Storage: " +
                            vals.getFIPSKeyStorageSlotDescription());


        if (fipsmode.equalsIgnoreCase("enable")) {
            vals.fipsMode = CryptoManager.InitializationValues.FIPSMode.ENABLED;
        } else if (fipsmode.equalsIgnoreCase("disable")){
            vals.fipsMode =
                    CryptoManager.InitializationValues.FIPSMode.DISABLED;
        } else {
            vals.fipsMode =
                    CryptoManager.InitializationValues.FIPSMode.UNCHANGED;
        }

        CryptoManager.initialize(vals);

        CryptoManager cm = CryptoManager.getInstance();

        if (cm.FIPSEnabled() == true ) {
            System.out.println("\n\t\tFIPS enabled\n");
        } else {
            System.out.println("\n\t\tFIPS not enabled\n");
        }


        java.util.Enumeration items;
        items = cm.getModules();
        System.out.println("\nListing of Modules:");
        while(items.hasMoreElements()) {
            System.out.println("\t"+
            ((PK11Module)items.nextElement()).getName() );
        }

        items = cm.getAllTokens();
        System.out.println("\nAll Tokens:");
        while(items.hasMoreElements()) {
            System.out.println("\t"+
            ((CryptoToken)items.nextElement()).getName() );
        }

        items = cm.getExternalTokens();
        System.out.println("\nExternal Tokens:");
        while(items.hasMoreElements()) {
            System.out.println("\t"+
            ((CryptoToken)items.nextElement()).getName() );
        }

        CryptoToken tok;
        String tokenName;

        /* find the Internal Key Storage token */
        if (cm.FIPSEnabled() == true ) {
            tokenName = vals.getFIPSSlotDescription();
        } else {
            tokenName = vals.getInternalKeyStorageTokenDescription();
        }

        /* truncate to 32 bytes and remove trailing white space*/
        tokenName = tokenName.substring(0, 32);
        tokenName = tokenName.trim();
        System.out.println("\nFinding the Internal Key Storage token: "+
                tokenName);
        tok = cm.getTokenByName(tokenName);

        if( ((PK11Token)tok).isInternalKeyStorageToken()
                && tok.equals(cm.getInternalKeyStorageToken()) ) {
            System.out.println("Good, "+tok.getName()+", knows it is " +
                    "the internal Key Storage Token");
        } else {
            System.out.println("ERROR: "+tok.getName()+", doesn't know"+
                " it is the internal key storage token");
        }

        if (!password.equals("")) {
           System.out.println("logging in to the Token: " + tok.getName());
           PasswordCallback cb = new FilePasswordCallback(password);
           tok.login(cb);
           System.out.println("logged in to the Token: " + tok.getName());
        }

        /* find the Internal Crypto token */
        if (cm.FIPSEnabled() == true ) {
            tokenName = vals.getFIPSSlotDescription();
        } else {
            tokenName =  vals.getInternalTokenDescription();
        }

        /* truncate to 32 bytes and remove trailing white space*/
        tokenName = tokenName.substring(0, 32);
        tokenName = tokenName.trim();
        System.out.println("\nFinding the Internal Crypto token: " + tokenName);
        tok = cm.getTokenByName(tokenName);

        if( ((PK11Token)tok).isInternalCryptoToken() &&
                        tok.equals(cm.getInternalCryptoToken() )) {
            System.out.println("Good, "+tok.getName()+
                    ", knows it is the internal Crypto token");
        } else {
            System.out.println("ERROR: "+tok.getName()+
                ", doesn't know that it is the internal Crypto token");
        }

        System.exit(0);

      } catch( Exception e ) {
        e.printStackTrace();
        System.exit(1);
      }
    }
}
