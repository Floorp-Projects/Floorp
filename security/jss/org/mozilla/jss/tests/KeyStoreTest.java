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
 * The Original Code is Network Security Services for Java.
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

import org.mozilla.jss.CryptoManager;
import org.mozilla.jss.crypto.CryptoToken;
import org.mozilla.jss.crypto.KeyGenerator;
import org.mozilla.jss.crypto.KeyGenAlgorithm;
import org.mozilla.jss.crypto.SecretKeyFacade;
import org.mozilla.jss.pkcs11.PK11Token;
import org.mozilla.jss.util.ConsolePasswordCallback;
import java.security.*;
import java.security.cert.CertificateFactory;
import java.util.Enumeration;
import java.security.cert.Certificate;
import java.io.*;
import javax.crypto.SecretKey;

public class KeyStoreTest {

    public static void printUsage() {
        System.out.println("Usage: KeyStoreTest <dbdir> " +
            "<operation> [<args>...]");
        System.out.println("Operations:\n" +
            "getAliases\n" +
            "deleteEntry <alias> . . .\n" +
            "getCertByName <alias> . . .\n" +
            "getCertByDER <DER cert filename>\n" +
            "getKey <alias>\n" +
            "addKey <alias>\n" +
            "isTrustedCert <alias>\n");
    }

    public static void main(String argv[]) {
      try {

        if( argv.length < 2 ) {
            printUsage();
            System.exit(1);
        }

        String op = argv[1];
        String[] args = new String[ argv.length - 2 ];
        for(int i=2; i < argv.length; ++i) {
            args[i-2] = argv[i];
        }

        CryptoManager.initialize(argv[0]);
        CryptoManager cm = CryptoManager.getInstance();


        // login to the token
        CryptoToken token = cm.getInternalKeyStorageToken();
        //CryptoToken token = cm.getTokenByName("Builtin Object Token");
        try {
            token.login(new ConsolePasswordCallback());
        } catch(PK11Token.NotInitializedException ex) { }
        cm.setThreadToken(token);

        KeyStore ks = KeyStore.getInstance("Mozilla-JSS");
        ks.load(null, null);

        if( op.equalsIgnoreCase("getAliases") ) {
            dumpAliases(ks);
        } else if( op.equalsIgnoreCase("deleteEntry") ) {
            for(int j=0; j < args.length; ++j) {
                ks.deleteEntry(args[j]);
            }
        } else if( op.equalsIgnoreCase("getCertByName") ) {
            for(int j=0; j < args.length; ++j) {
                dumpCert(ks, args[j]);
            }
        } else if( op.equalsIgnoreCase("getCertByDER") ) {
            if( args.length < 1 ) {
                printUsage();
                System.exit(1);
            }   
            getCertByDER(ks, args[0]);
        } else if( op.equalsIgnoreCase("getKey") ) {
            if( args.length != 1 ) {
                printUsage();
                System.exit(1);
            }
            getKey(ks, args[0]);
        } else if( op.equalsIgnoreCase("isTrustedCert") ) {
            if( args.length != 1 ) {
                printUsage();
                System.exit(1);
            }
            isTrustedCert(ks, args[0]);
        } else if( op.equalsIgnoreCase("addKey") ) {
            if( args.length != 1 ) {
                printUsage();
                System.exit(1);
            }
            addKey(ks, args[0]);
        } else {
            printUsage();
            System.exit(1);
        }

      } catch(Throwable t) {
        t.printStackTrace();
        System.exit(1);
      }
    }

    public static void dumpCert(KeyStore ks, String alias)
        throws Throwable
    {
        Certificate cert = ks.getCertificate(alias);
        if( cert == null ) {
            System.out.println("Certificate with alias \"" + alias +
                "\" not found");
        } else {
            System.out.println(cert.toString());
        }
    }

    public static void dumpAliases(KeyStore ks) throws Throwable {
        Enumeration aliases = ks.aliases();

        System.out.println("Aliases:");
        while( aliases.hasMoreElements() ) {
            String alias = (String) aliases.nextElement();
            System.out.println( "\"" + alias + "\"");
        }
        System.out.println();
    }

    public static void getCertByDER(KeyStore ks, String derCertFilename)
            throws Throwable {

        FileInputStream fis = new FileInputStream(derCertFilename);
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        byte[] buf = new byte[1024];
        int numRead;

        while( (numRead = fis.read(buf)) != -1 ) {
            bos.write(buf, 0, numRead);
        }
        ByteArrayInputStream bis = new ByteArrayInputStream(bos.toByteArray());

        CertificateFactory fact = CertificateFactory.getInstance("X.509");
        Certificate cert = fact.generateCertificate( bis );

        String nick = ks.getCertificateAlias(cert);

        if( nick == null ) {
            System.out.println("No matching certificate was found.");
        } else {
            System.out.println("Found matching certificate \"" + nick + "\"");
        }
    }

    public static void getKey(KeyStore ks, String alias)
            throws Throwable {

        Key key = ks.getKey(alias, null);

        if( key == null ) {
            System.out.println("Could not find key for alias \"" +
                alias + "\"");
            System.exit(1);
        } else {
            String clazz = key.getClass().getName();
            System.out.println("Found " + clazz + " for alias \"" +
                alias + "\"");
        }
    }           

    public static void isTrustedCert(KeyStore ks, String alias)
            throws Throwable {

        if( ks.isCertificateEntry(alias) ) {
            System.out.println("\"" + alias + "\" is a trusted certificate" +
                " entry");
        } else {
            System.out.println("\"" + alias + "\" is NOT a trusted certificate"
                + " entry");
        }
    }

    public static void addKey(KeyStore ks, String alias)
            throws Throwable
    {
        KeyPairGenerator kpg = KeyPairGenerator.getInstance("RSA",
            "Mozilla-JSS");

        kpg.initialize(1024);
        KeyPair pair = kpg.genKeyPair();
        Certificate [] certs = new Certificate[1];

        ks.setKeyEntry(alias, pair.getPrivate(), null, certs);

        CryptoManager cm = CryptoManager.getInstance();
        CryptoToken tok = cm.getInternalKeyStorageToken();
        KeyGenerator kg = tok.getKeyGenerator( KeyGenAlgorithm.DES3 );
        SecretKey key = new SecretKeyFacade(kg.generate());

        ks.setKeyEntry(alias+"sym", key, null, null);
    }
}
