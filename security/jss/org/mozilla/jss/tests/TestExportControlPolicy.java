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

package com.netscape.jss;

import com.netscape.jss.*;
import com.netscape.jss.crypto.*;

/**
 * A command-line utility for testing Export Control Policies in "jssjava".
 */
public class TestExportControlPolicy {

    public static void main(String args[]) {
        CryptoManager cm;
		int policy;

		try {
			CryptoManager.InitializationValues vals = new
			CryptoManager.InitializationValues( "./secmodule.db",
												"./key3.db",
												"./cert7.db");
			CryptoManager.initialize( vals );

			cm = CryptoManager.getInstance();


			// (1) Test getExportControlPolicyType() method.
			//
			//     NOTE:
			//            domestic: "the DOMESTIC"
			//            export:   "the EXPORT"
			//            france:   "the FRANCE"
			//
			System.out.print( "\ngetExportControlPolicyType() utilizes " );

			policy = cm.getExportControlPolicyType();

			if( policy == CryptoManager.NULL_POLICY ) {
				System.out.print( "the NULL " );
			} else if( policy == CryptoManager.DOMESTIC_POLICY ) {
				System.out.print( "the DOMESTIC " );
			} else if( policy == CryptoManager.EXPORT_POLICY ) {
				System.out.print( "the EXPORT " );
			} else if( policy == CryptoManager.FRANCE_POLICY ) {
				System.out.print( "the FRANCE " );
			} else {
				System.out.print( "NO " );
			}

			System.out.println( "export control policy." );


			// (2) Test isDomestic() method.
			//
			//     NOTE:
			//            domestic: "is"
			//            export:   "is NOT"
			//            france:   "is NOT"
			//
			System.out.print( "isDomestic() says that jssjava " );

			if( cm.isDomestic() == true ) {
				System.out.print( "is " );
			} else {
				System.out.print( "is NOT " );
			}

			System.out.println( "domestic." );


			///////////////////////////////
			//                           //
			// NOTE:  For the remainder  //
			// of this test, please use  //
			// the following algorithms: //
			//                           //
			//    EncryptionAlgorithm    //
			//    KeyGenAlgorithm        //
			//    KeyPairAlgorithm       //
			//    KeyWrapAlgorithm       //
			//    PBEAlgorithm           //
			//    SignatureAlgorithm     //
			//                           //
			///////////////////////////////


			// (3) Test the isAllowed() method using DES_CBC for
			//     CRS BULK ENCRYPTION
			//
			//     NOTE:
			//            domestic:  "allowed"
			//            export:    "allowed"
			//            france:    "NOT allowed"
			//
			if( ! KeyWrapAlgorithm.DES_CBC.isAllowed(
					Algorithm.Usage.JAVA_CRS_BULK_ENCRYPTION ) ) {
				System.out.println( "DES_CBC is NOT allowed for " +
									"CRS BULK ENCRYPTION!" );
			} else {
				System.out.println( "DES_CBC is allowed for " +
									"CRS BULK ENCRYPTION!" );
			}


			// (4) Test the isAllowed() method using RC4 for KRA STORAGE
			//
			//     NOTE:
			//            domestic:  "NOT allowed"
			//            export:    "NOT allowed"
			//            france:    "NOT allowed"
			//
			if( ! KeyGenAlgorithm.RC4.isAllowed(
					Algorithm.Usage.JAVA_KRA_STORAGE ) ) {
				System.out.println( "RC4 is NOT allowed for KRA STORAGE!" );
			} else {
				System.out.println( "RC4 is allowed for KRA STORAGE!" );
			}

			// (5) Test the isAllowed() method using PBE_SHA1_DES3_CBC for
			//     KRA PKCS 12
			//
			//     NOTE:
			//            domestic:  "allowed"
			//            export:    "allowed"
			//            france:    "allowed"
			//
			if( ! PBEAlgorithm.PBE_SHA1_DES3_CBC.isAllowed(
					Algorithm.Usage.JAVA_KRA_PKCS_12 ) ) {
				System.out.println( "PBE_SHA1_DES3_CBC is NOT allowed for " +
									"KRA PKCS 12!" );
			} else {
				System.out.println( "PBE_SHA1_DES3_CBC is allowed for " +
									"KRA PKCS 12!" );
			}

			// (6) Test the getStrongestKeySize() method
			//     SEC_OID_PKCS1_RSA_ENCRYPTION for SSL KEY EXCHANGE
			//
			//     NOTE:
			//            domestic:   512
			//            export:     512
			//            france:    4096
			//
			System.out.println( "Strongest key size of " +
								"SEC_OID_PKCS1_RSA_ENCRYPTION is " +
								KeyWrapAlgorithm.RSA.getStrongestKeySize(
									Algorithm.Usage.JAVA_SSL_KEY_EXCHANGE ) +
								"-bits for KeyWrapping." );

			// (7) Test the getStrongestKeySize() method
			//     for DES_CBC for CRS BULK ENCRYPTION 
			//
			//     NOTE:
			//            domestic:  56
			//            export:    56
			//            france:     0
			//
			System.out.println( "Strongest key size of " +
								"DES_CBC is " +
								KeyWrapAlgorithm.DES_CBC.getStrongestKeySize(
									Algorithm.Usage.JAVA_CRS_BULK_ENCRYPTION ) +
								"-bits for KeyWrapping." );


			// (8) Test getAllAlgorithms() method
			//     (currently unused; ergo untested)

			System.out.println( "\n" );
		} catch(NumberFormatException e) {
			System.err.println("Invalid key size: "+e);
		} catch(java.security.InvalidParameterException e) {
			System.err.println("Invalid key size: "+e);
		} catch(Exception e) {
			System.err.println(e);
		}
	}
}

