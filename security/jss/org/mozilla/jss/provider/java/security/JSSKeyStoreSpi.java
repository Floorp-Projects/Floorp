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

package org.mozilla.jss.provider.java.security;

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.security.Key;
import java.security.KeyStoreException;
import java.security.cert.Certificate;
import java.security.cert.CertificateFactory;
import java.security.cert.CertificateException;
import java.security.cert.CertificateEncodingException;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.Iterator;
import org.mozilla.jss.pkcs11.PK11Token;
import org.mozilla.jss.pkcs11.TokenProxy;
import org.mozilla.jss.crypto.CryptoToken;
import org.mozilla.jss.crypto.TokenSupplierManager;
import org.mozilla.jss.crypto.SecretKeyFacade;
import org.mozilla.jss.crypto.X509Certificate;
import org.mozilla.jss.crypto.TokenException;
import org.mozilla.jss.crypto.TokenRuntimeException;
import org.mozilla.jss.CryptoManager;

/**
 * The JSS implementation of the JCA KeyStore SPI.
 * 
 * <p>Implementation notes
 * <ol>
 * <li>deleteEntry will delete all entries with that label. If the entry is a
 * cert with a matching private key, it will also delete the private key.
 * 
 * <li>getCertificate returns first cert with matching nickname. Converts it
 * into a java.security.cert.X509Certificate (not a JSS cert).
 * 
 * <li>getCertificateChain only returns a single certificate. That's because
 * we don't have a way to build a chain from a specific slot--only from
 * the set of all slots. 
 * 
 * <li>getCreationDate is unsupported because NSS doesn't store that
 * information.
 * 
 * <li>getKey first looks for a private/symmetric key with the given label.
 * It returns the first one it finds. If it doesn't find one, it looks for a
 * cert with the given nickname. If it finds one, it returns the private key
 * for that cert.
 * 
 * <li>isCertificateEntry returns true if there is a cert with this nickname,
 * but it doesn't have a private key. isKeyEntry returns true if there is a key
 * with this nickname, or if there is a cert with this nickname and the cert
 * has an associated private key.
 * 
 * <li>load and store are no-ops.
 * 
 * <li>setCertificateEntry doesn't work.NSS doesn't have a way of storing a
 * certificate on a specific token unless it has an associated private key.
 * That rules out trusted certificate entries.
 * 
 * <li>setKeyEntry not supported yet. Need to convert a temporary key
 * into a permanent key.
 * </ol>
 */
public class JSSKeyStoreSpi extends java.security.KeyStoreSpi {

    protected TokenProxy proxy;

    public JSSKeyStoreSpi() {
        CryptoToken token =
            TokenSupplierManager.getTokenSupplier().getThreadToken();
        PK11Token pk11tok = (PK11Token)token;
        proxy = pk11tok.getProxy();
    }

    /**
     * Converts an Iterator into an Enumeration.
     */
    private static class IteratorEnumeration implements Enumeration {
        private Iterator iter;

        public IteratorEnumeration(Iterator iter) {
            this.iter = iter;
        }

        public boolean hasMoreElements() {
            return iter.hasNext();
        }

        public Object nextElement() {
            return iter.next();
        }
    }

    private native HashSet getRawAliases();

    /**
     * Returns a list of unique aliases.
     */
    public Enumeration engineAliases() {
        return new IteratorEnumeration( getRawAliases().iterator() );
    }

    public boolean engineContainsAlias(String alias) {
        return getRawAliases().contains(alias);
    }

    public native void engineDeleteEntry(String alias);

    /*
     * XXX-!!! Is shared cert factory thread safe?
     */
    private CertificateFactory certFactory=null;
    {
      try {
        certFactory = CertificateFactory.getInstance("X.509");
      } catch(CertificateException e) {
        e.printStackTrace();
        throw new RuntimeException(e.getMessage());
      }
    }

    public Certificate engineGetCertificate(String alias) {
        byte[] derCert = getDERCert(alias);
        if( derCert == null ) {
            return null;
        } else {
            try {
                return
                    certFactory.generateCertificate(
                        new ByteArrayInputStream(derCert)
                    );
            } catch( CertificateException e ) {
                e.printStackTrace();
                return null;
            }
        }
    }

    private native byte[] getDERCert(String alias);
    private native X509Certificate getCertObject(String alias);

    public String engineGetCertificateAlias(Certificate cert) {
      try {
        return getCertNickname( cert.getEncoded() );
      } catch(CertificateEncodingException e) {
        return null;
      }
    }

    private native String getCertNickname(byte[] derCert);
        
    public Certificate[] engineGetCertificateChain(String alias) {
      try {
        X509Certificate leaf = getCertObject(alias);
        if( leaf == null ) {
            return null;
        }
        CryptoManager cm = CryptoManager.getInstance();
        X509Certificate[] jssChain = cm.buildCertificateChain(leaf);

        Certificate[] chain = new Certificate[jssChain.length];
        for( int i=0; i < chain.length; ++i) {
            chain[i] = certFactory.generateCertificate(
                        new ByteArrayInputStream(jssChain[i].getEncoded()) );
        }
        return chain;
      } catch(TokenException te ) {
            throw new TokenRuntimeException(te.toString());
      } catch(CryptoManager.NotInitializedException e) {
            throw new RuntimeException("CryptoManager not initialized");
      } catch(CertificateException ce) {
            ce.printStackTrace();
            return null;
      }
    }

    /*
     * Not supported.
     */
    public java.util.Date engineGetCreationDate(String alias) {
        return null;
    }

    public native Key engineGetKey(String alias, char[] password);

    /**
     * Returns true if there is a cert with this nickname but there is no
     * key associated with the cert.
     */
    public native boolean engineIsCertificateEntry(String alias);

    /**
     * Returns true if there is a key with this alias, or if
     * there is a cert with this alias that has an associated key.
     */
    public boolean engineIsKeyEntry(String alias) {
        /* this is somewhat wasteful but we can speed it up later */
        return ( engineGetKey(alias, null) != null );
    }

    public void engineLoad(InputStream stream, char[] password)
        throws IOException
    {
    }

    /** 
     * NSS doesn't have a way of storing a certificate on a specific token
     * unless it has an associated private key.  That rules out
     * trusted certificate entries, so we can't supply this method currently.
     */
    public void engineSetCertificateEntry(String alias, Certificate cert)
            throws KeyStoreException
    {
        throw new KeyStoreException(
            "Storing trusted certificate entries to a JSS KeyStore is not" +
            " supported.");
    }


    public void engineSetKeyEntry(String alias, byte[] key, Certificate[] chain)
        throws KeyStoreException
    {
        throw new KeyStoreException("Storing plaintext keys is not supported."+
            "Store the key as a handle instead.");
    }

    public void engineSetKeyEntry(String alias, Key key, char[] password,
        Certificate[] chain) throws KeyStoreException
    {
        if( key instanceof SecretKeyFacade ) {
            SecretKeyFacade skf = (SecretKeyFacade)key;
            engineSetKeyEntryNative(alias, skf.key, password, chain);
        } else {
            engineSetKeyEntryNative(alias, key, password, chain);
        }
    }

    private native void engineSetKeyEntryNative(String alias, Object key,
        char[] password, Certificate[] chain) throws KeyStoreException;

    public int engineSize() {
        return getRawAliases().size();
    }

    public void engineStore(OutputStream stream, char[] password)
            throws IOException
    {
    }
}
