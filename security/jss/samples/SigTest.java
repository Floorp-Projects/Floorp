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


/* This program demonstrates how to sign data with keys from JSS
 * The token name can be either the name of a hardware token, or
 * one of the internal tokens:
 *  Internal Crypto Services Token
 *  Internal Key Storage Token    (keys stored in key3.db)
 */


import com.netscape.jss.crypto.*;
import com.netscape.jss.crypto.Signature;
import com.netscape.jss.crypto.KeyPairGenerator;
import java.security.*;
import java.security.cert.X509Certificate;
import java.io.*;
import java.lang.*;
import java.util.*;
import com.netscape.jss.util.*;
import com.netscape.jss.pkcs11.*;
import com.netscape.jss.*;

public class SigTest {

	public static void usage() {
		System.out.println(
			"Usage: java com.netscape.jss.crypto.SigTest <dbdir> <tokenname>");
	}

	public static void main(String args[]) {
		CryptoToken token;
		CryptoManager manager;
		byte[] data = new byte[] {1,2,3,4,5,6,7,8,9};
		byte[] signature;
		Signature signer;
		PublicKey pubk;
		KeyPairGenerator kpgen;
		KeyPair keyPair;

		if(args.length != 2) {
			usage();
			return;
		}
		String dbdir = args[0];
		String tokenname = args[1];

		try {
            CryptoManager.InitializationValues vals = new
                CryptoManager.InitializationValues
			                  ( args[0]+"/secmod.db",
								args[0]+"/key3.db",
								args[0]+"/cert7.db" );
            CryptoManager.initialize(vals);
		    manager = CryptoManager.getInstance();

			/* Print out list of available tokens */
			Enumeration en = manager.getAllTokens();
			System.out.println("Available tokens:");
			while (en.hasMoreElements()) {
				PK11Token p = (PK11Token)en.nextElement();
				System.out.println(" token : "+p.getName());
			}

			token = manager.getTokenByName(tokenname);

			// Generate an RSA keypair
			kpgen = token.getKeyPairGenerator(KeyPairAlgorithm.RSA);
			kpgen.initialize(1024);
			keyPair = kpgen.genKeyPair();

			// RSA MD5
			signer = token.getSignatureContext(
							SignatureAlgorithm.RSASignatureWithMD5Digest);
			System.out.println("Created a signing context");
			signer.initSign(
				(com.netscape.jss.crypto.PrivateKey)keyPair.getPrivate());
			System.out.println("initialized the signing operation");

			signer.update(data);
			System.out.println("updated signature with data");
			signature = signer.sign();
			System.out.println("Successfully signed!");

			signer.initVerify(keyPair.getPublic());
			System.out.println("initialized verification");
			signer.update(data);
			System.out.println("updated verification with data");
			if( signer.verify(signature) ) {
				System.out.println("Signature Verified Successfully!");
			} else {
				System.out.println("ERROR: Signature failed to verify.");
			}

		} catch(Exception e) {
			e.printStackTrace();
        }
	}
}
