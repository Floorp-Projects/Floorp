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

import org.mozilla.jss.util.*;
import org.mozilla.jss.crypto.*;
import org.mozilla.jss.*;
import org.mozilla.jss.pkcs11.*;
import java.io.*;
import java.awt.*;
import java.security.cert.*;

public class SelfTest {

    public static void main(String[] args) throws Throwable {
        CryptoToken tok;
        CryptoToken intTok;
        CryptoManager manager;
        Password pass1=null, pass2=null;
        KeyPairGenerator keyPairGenerator;
        java.security.KeyPair keyPair;
		java.util.Enumeration items;
		char[] passchar1 = {'f', 'o', 'o', 'b', 'a', 'r'};
		char[] passchar2 = {'n', 'e', 't', 's', 'c', 'a', 'p', 'e'};

        if(args.length != 1) {
            System.err.println("Usage: java ...SelfTest <dbdir>");
            return;
        }

        CryptoManager.InitializationValues vals = new
            CryptoManager.InitializationValues( args[0] );
        CryptoManager.initialize(vals);
        try {
            manager = CryptoManager.getInstance();
        } catch( CryptoManager.NotInitializedException e ) {
            System.out.println("CryptoManager not initialized");
            return;
        }
        Debug.setLevel(Debug.OBNOXIOUS);

        try {
            tok = manager.getTokenByName("asdffda");
            System.out.println("ERROR: found a nonexistent token");
        } catch (NoSuchTokenException e) {
            System.out.println("Good, could not find non-existent token");
        }

        try {

			items = manager.getModules();
			System.out.println("Modules:");
			while(items.hasMoreElements()) {
				System.out.println("\t"+
					((PK11Module)items.nextElement()).getName() );
			}

			items = manager.getAllTokens();
			System.out.println("All Tokens:");
			while(items.hasMoreElements()) {
				System.out.println("\t"+
					((CryptoToken)items.nextElement()).getName() );
			}
			
			items = manager.getExternalTokens();
			System.out.println("External Tokens:");
			while(items.hasMoreElements()) {
				System.out.println("\t"+
					((CryptoToken)items.nextElement()).getName() );
			}
			

            tok = manager.getTokenByName("Internal Key Storage Token");
            System.out.println("Good, found internal DB token");

            if( tok.equals(manager.getInternalKeyStorageToken()) ) {
                System.out.println("Good, it really is the key storage token");
            } else {
                System.out.println("ERROR: it's not the same as the key "+
                    "storage token!");
            }
            if( ((PK11Token)tok).isInternalKeyStorageToken() ) {
                System.out.println("Good, "+tok.getName()+" knows "+
                    "what it is");
            } else {
                System.out.println("ERROR: "+tok.getName()+" doesn't know"+
                    " it is key storage token");
            }

            intTok = manager.getInternalCryptoToken();
            if( ((PK11Token)intTok).isInternalCryptoToken() ) {
                System.out.println("Good, "+tok.getName()+
                    " knows it is the internal token");
            } else {
                System.out.println("ERROR: "+tok.getName()+
                    " doesn't know what that it is the internal token");
            }


            if(tok.isLoggedIn() == false) {
                System.out.println("Good, isLoggedIn correctly says we're"+
                    " not logged in");
            } else {
                System.out.println("ERROR: isLoggedIn incorrectly says we're"+
                    " logged in");
            }

            System.out.println("Good, successfully opened token \""+
                tok.getName()+"\"");

			pass1 = new Password( (char[]) passchar1.clone());
			pass2 = new Password( new char[]{0} );
            tok.initPassword(pass2, pass1);
			pass1.clear();
			pass2.clear();
            System.out.println("Good, initialized PIN");
            tok.logout();

            try {
				pass1 = new Password( (char[]) passchar2.clone());
                tok.login(pass1);
                System.out.println("ERROR: Successfully logged in with wrong"+
                    " PIN");
            } catch (IncorrectPasswordException e) {
                System.out.println("Good, unable to login with wrong PIN");
            } finally {
				pass1.clear();
			}

			pass1 = new Password( (char[]) passchar1.clone());
            tok.login(pass1);
			pass1.clear();
            System.out.println("Good, logged in");

            if(tok.isLoggedIn() == true) {
                System.out.println("Good, isLoggedIn correctly says we're"+
                    " logged in");
            } else {
                System.out.println("ERROR: isLoggedIn incorrectly says we're"+
                    " not logged in");
            }

			pass1 = new Password( (char[]) passchar1.clone());
			pass2 = new Password( (char[]) passchar2.clone());
            tok.changePassword(pass1, pass2);
			pass1.clear(); pass2.clear();
            System.out.println("Good, changed PIN");

            try {
				pass1 = new Password( (char[]) passchar1.clone());
                tok.login(pass1);
                // Should still be logged in
                System.out.println("Good, logging in with wrong PIN ok if "+
                    " already logged in");
            } catch (IncorrectPasswordException e) {
                System.out.println("ERROR: logged in second time with wrong"+
                    "PIN, but we should still be logged in");
            } finally {
				pass1.clear();
			}

            try {
                tok.logout();
                System.out.println("Good, logged out successfully.");
            } catch (TokenException e) {
                System.out.println("ERROR: failed to logout from token");
            }

            if(tok.isLoggedIn() == false) {
                System.out.println("Good, isLoggedIn correctly says we're"+
                    " not logged in");
            } else {
                System.out.println("ERROR: isLoggedIn incorrectly says we're"+
                    " logged in");
            }

            try {
                tok.logout();
                System.out.println("ERROR: logged out twice in a row");
            } catch (TokenException e) {
                System.out.println("Good, got an exception when we tried"+
                    " to log out twice in a row");
            }
            try {
				pass1 = new Password( (char[]) passchar1.clone());
                tok.login(pass1);
				pass1.clear();
                System.out.println("ERROR: logged in with wrong pw");
            } catch (IncorrectPasswordException e) {
                System.out.println("Good, logging in with wrong PIN gave err");
            }

            System.out.println("Test completed");

            tok = null;
    
        } catch (IncorrectPasswordException e) {
            System.out.println("Got an incorrect PIN: "+e);
		} catch (AlreadyInitializedException e) {
			System.out.println(
				"ERROR: This test only works with uninitialized databases");
        } catch (TokenException e) {
            System.out.println("Token error: " + e);
        } catch (NoSuchTokenException e) {
            System.out.println("ERROR: could not find internal DB token");
        } finally {
			if(pass1 != null) {
				pass1.clear();
			}
			if(pass2 != null) {
				pass2.clear();
			}
		}

        //System.gc();
        //NativeProxy.assertRegistryEmpty();
        //System.runFinalization();
    }
}
