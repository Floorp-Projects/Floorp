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

package org.mozilla.jss.provider.javax.crypto;

import java.io.CharConversionException;
import java.security.*;
import java.security.spec.*;
import org.mozilla.jss.crypto.CryptoToken;
import org.mozilla.jss.crypto.KeyGenAlgorithm;
import org.mozilla.jss.crypto.TokenException;
import org.mozilla.jss.crypto.TokenRuntimeException;
import org.mozilla.jss.crypto.KeyGenerator;
import org.mozilla.jss.crypto.SymmetricKey;
import org.mozilla.jss.crypto.SecretKeyFacade;
import org.mozilla.jss.crypto.TokenSupplierManager;
import javax.crypto.*;

class JSSKeyGeneratorSpi extends javax.crypto.KeyGeneratorSpi {
    private KeyGenerator keyGenerator= null;

    private JSSKeyGeneratorSpi() {}

    protected JSSKeyGeneratorSpi(KeyGenAlgorithm alg) {
      try {
        CryptoToken token =
            TokenSupplierManager.getTokenSupplier().getThreadToken();
        keyGenerator = token.getKeyGenerator(alg);
      } catch( TokenException te) {
            throw new TokenRuntimeException(te.getMessage());
      } catch(NoSuchAlgorithmException nsae) {
            throw new TokenRuntimeException(nsae.getMessage());
      }
    }

    protected void engineInit(int keysize,  SecureRandom random)
            throws InvalidParameterException
    {
      try {
        keyGenerator.initialize(keysize);
      } catch(InvalidAlgorithmParameterException e) {
            throw new InvalidParameterException(e.getMessage());
      }
    }

    protected void engineInit( SecureRandom random)
            throws InvalidParameterException
    {
        // no-op. KeyGenerator.initialize isn't called if there
        // are no arguments.
    }

    protected void engineInit(AlgorithmParameterSpec params,
                    SecureRandom random)
        throws InvalidAlgorithmParameterException
    {
        keyGenerator.initialize(params);
    }

    protected SecretKey engineGenerateKey() {
      try {
        return new SecretKeyFacade( keyGenerator.generate() );
      } catch(IllegalStateException ise) {
        throw new TokenRuntimeException(
            "IllegalStateException: " + ise.getMessage());
      } catch(TokenException te) {
        throw new TokenRuntimeException( te.getMessage());
      } catch(CharConversionException cce) {
        throw new TokenRuntimeException(
            "CharConversionException: " + cce.getMessage());
      }
    }

    public static class DES extends JSSKeyGeneratorSpi {
        public DES() {
            super(KeyGenAlgorithm.DES);
        }
    }
    public static class DESede extends JSSKeyGeneratorSpi {
        public DESede() {
            super(KeyGenAlgorithm.DESede);
        }
    }
    public static class AES extends JSSKeyGeneratorSpi {
        public AES() {
            super(KeyGenAlgorithm.AES);
        }
    }
    public static class RC4 extends JSSKeyGeneratorSpi {
        public RC4() {
            super(KeyGenAlgorithm.RC4);
        }
    }

    /**
     * @deprecated This class name is misleading. This algorithm
     * is used for generating Password-Based Authentication keys
     * for use with HmacSHA1. Use PBAHmacSHA1 instead.
     */
    public static class HmacSHA1 extends JSSKeyGeneratorSpi {
        public HmacSHA1() {
            super(KeyGenAlgorithm.PBA_SHA1_HMAC);
        }
    }

    public static class PBAHmacSHA1 extends JSSKeyGeneratorSpi {
        public PBAHmacSHA1() {
            super(KeyGenAlgorithm.PBA_SHA1_HMAC);
        }
    }

}
