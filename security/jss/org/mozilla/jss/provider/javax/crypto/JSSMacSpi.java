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

import java.security.*;
import java.security.spec.*;
import org.mozilla.jss.crypto.JSSMessageDigest;
import org.mozilla.jss.crypto.CryptoToken;
import org.mozilla.jss.crypto.TokenSupplierManager;
import org.mozilla.jss.crypto.JSSMessageDigest;
import org.mozilla.jss.crypto.SecretKeyFacade;
import org.mozilla.jss.crypto.HMACAlgorithm;
import org.mozilla.jss.crypto.TokenRuntimeException;

class JSSMacSpi extends javax.crypto.MacSpi {

    private JSSMessageDigest digest=null;
    private HMACAlgorithm alg;

    private JSSMacSpi() { }

    protected JSSMacSpi(HMACAlgorithm alg) {
      try {
        this.alg = alg;
        CryptoToken token =
            TokenSupplierManager.getTokenSupplier().getThreadToken();
        digest = token.getDigestContext(alg);
      } catch( DigestException de) {
            throw new TokenRuntimeException(de.getMessage());
      } catch(NoSuchAlgorithmException nsae) {
            throw new TokenRuntimeException(nsae.getMessage());
      }
    }


    public int engineGetMacLength() {
        return alg.getOutputSize();
    }

    public void engineInit(Key key, AlgorithmParameterSpec params)
        throws InvalidKeyException, InvalidAlgorithmParameterException
    {
      try {
        if( ! (key instanceof SecretKeyFacade) ) {
            throw new InvalidKeyException("Must use a JSS key");
        }
        SecretKeyFacade facade = (SecretKeyFacade)key;
        digest.initHMAC(facade.key);
      } catch(DigestException de) {
        throw new InvalidKeyException(
            "DigestException: " + de.getMessage());
      }
    }

    public void engineUpdate(byte input) {
      try {
        digest.update(input);
      } catch(DigestException de) {
        throw new TokenRuntimeException("DigestException: " + de.getMessage());
      }
    }

    public void engineUpdate(byte[] input, int offset, int len) {
      try {
        digest.update(input, offset, len);
      } catch(DigestException de) {
        throw new TokenRuntimeException("DigestException: " + de.getMessage());
      }
    }

    public byte[] engineDoFinal() {
      try {
        return digest.digest();
      } catch(DigestException de) {
        throw new TokenRuntimeException("DigestException: " + de.getMessage());
      }
    }

    public void engineReset() {
      try {
        digest.reset();
      } catch(DigestException de) {
        throw new TokenRuntimeException("DigestException: " + de.getMessage());
      }
    }

    public Object clone() throws CloneNotSupportedException {
        throw new CloneNotSupportedException();
    }

    public static class HmacSHA1 extends JSSMacSpi {
        public HmacSHA1() {
            super(HMACAlgorithm.SHA1);
        }
    }
}
