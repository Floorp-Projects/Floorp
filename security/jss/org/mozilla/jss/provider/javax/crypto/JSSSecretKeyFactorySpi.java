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

import javax.crypto.*;
import javax.crypto.spec.*;
import java.io.CharConversionException;
import java.security.*;
import java.security.spec.*;
import org.mozilla.jss.crypto.*;
import org.mozilla.jss.CryptoManager;
import org.mozilla.jss.util.Assert;
import org.mozilla.jss.util.Password;
import org.mozilla.jss.crypto.PBEKeyGenParams;
import java.lang.reflect.*;

class JSSSecretKeyFactorySpi extends SecretKeyFactorySpi {

    private KeyGenAlgorithm alg = null;
    private CryptoToken token = null;

    private JSSSecretKeyFactorySpi() { }

    protected JSSSecretKeyFactorySpi(KeyGenAlgorithm alg) {
        this.alg = alg;
        token = TokenSupplierManager.getTokenSupplier().getThreadToken();
    }

    private SecretKey
    generateKeyFromBits(byte[] bits, SymmetricKey.Type keyType)
        throws NoSuchAlgorithmException, TokenException,
        InvalidKeySpecException, InvalidAlgorithmParameterException
    {
      try {
        KeyWrapper wrapper = token.getKeyWrapper(KeyWrapAlgorithm.PLAINTEXT);

        wrapper.initUnwrap();

        SymmetricKey symk = wrapper.unwrapSymmetric(bits, keyType, 0);

        return new SecretKeyFacade(symk);
      } catch(InvalidKeyException e) {
            throw new InvalidKeySpecException(e.getMessage());
      }
    }

    private static PBEKeyGenParams
    makePBEKeyGenParams(PBEKeySpec spec)
        throws InvalidKeySpecException
    {
        // The PBEKeySpec in JCE 1.2.1 does not contain salt or iteration.
        // The PBEKeySpec in JDK 1.4 does contain salt and iteration.
        // If this is a JCE 1.2.1 PBEKeySpec, we don't have enough
        // information and we have to throw an exception. If it's a JDK 1.4
        // PBEKeySpec, we can get the information. The only way I know of
        // to find this out at runtime and be compatible with both
        // versions is to use the reflection API.
        Class specClass = spec.getClass();
        try {
            Method getSaltMethod = specClass.getMethod("getSalt", null);
            Method getIterationMethod =
                specClass.getMethod("getIterationCount", null);

            byte[] salt = (byte[]) getSaltMethod.invoke(spec, null);
            
            Integer itCountObj =
                (Integer) getIterationMethod.invoke(spec,null);
            int iterationCount = itCountObj.intValue();

            Password pass = new Password(spec.getPassword());
            
            PBEKeyGenParams params =
                new PBEKeyGenParams(pass, salt, iterationCount);
            pass.clear();
            return params;
        } catch(NoSuchMethodException nsme) {
            // fall through
        } catch(SecurityException se) {
            throw new InvalidKeySpecException(
                "SecurityException calling getMethod() on the key " +
                "spec's class: " + se.getMessage());
        } catch(IllegalAccessException iae) {
            throw new InvalidKeySpecException(
                "IllegalAccessException invoking method on PBEKeySpec: " +
                iae.getMessage());
        } catch(InvocationTargetException ite) {
            String message="";
            Throwable t = ite.getTargetException();
            if( t != null ) {
                message = t.getMessage();
            }
            throw new InvalidKeySpecException(
                "InvocationTargetException invoking method on PBEKeySpec: "+
                message);
        }

        throw new InvalidKeySpecException(
            "This version of PBEKeySpec is unsupported. It must " +
            "implement getSalt() and getIterationCount(). The PBEKeySpec in " +
            "JDK 1.4 works, as does org.mozilla.jss.crypto.PBEKeyGenParams. " +
            "The PBEKeySpec in JCE 1.2.1 and earlier does NOT work.");
    }

    public SecretKey
    engineGenerateSecret(KeySpec spec) throws InvalidKeySpecException
    {
      try {
        if( spec instanceof PBEKeySpec ||
            spec instanceof PBEKeyGenParams) {
            
            PBEKeyGenParams params;
            if( spec instanceof PBEKeySpec ) {
                params = makePBEKeyGenParams((PBEKeySpec)spec);
            } else {
                params = (org.mozilla.jss.crypto.PBEKeyGenParams) spec;
            }
            org.mozilla.jss.crypto.KeyGenerator gen =token.getKeyGenerator(alg);
            gen.initialize(params);
            SymmetricKey symk = gen.generate();
            params.clear();
            return new SecretKeyFacade(symk);
        } else if (spec instanceof DESedeKeySpec) {
            if( alg != KeyGenAlgorithm.DES3 ) {
                throw new InvalidKeySpecException(
                    "Incorrect KeySpec type (" + spec.getClass().getName() +
                    ") for algorithm (" + alg.toString() + ")");
            }
            return generateKeyFromBits(
                ((DESedeKeySpec)spec).getKey(), SymmetricKey.Type.DES3 );
        } else if (spec instanceof DESKeySpec) {
            if( alg != KeyGenAlgorithm.DES ) {
                throw new InvalidKeySpecException(
                    "Incorrect KeySpec type (" + spec.getClass().getName() +
                    ") for algorithm (" + alg.toString() + ")");
            }
            return generateKeyFromBits(
                ((DESKeySpec)spec).getKey(), SymmetricKey.Type.DES );
        } else if( spec instanceof SecretKeySpec ) {
            SecretKeySpec kspec = (SecretKeySpec) spec;
            SymmetricKey.Type type =
                SymmetricKey.Type.fromName( kspec.getAlgorithm());   
            return generateKeyFromBits( kspec.getEncoded(), type);
        } else {
            throw new InvalidKeySpecException(
                "Unsupported KeySpec: " + spec.getClass().getName());
        }
      } catch(TokenException te) {
            throw new TokenRuntimeException(te.getMessage());
      } catch(InvalidAlgorithmParameterException iape) {
            throw new InvalidKeySpecException(
                "InvalidAlgorithmParameterException: " + iape.getMessage());
      } catch(IllegalStateException ise) {
            Assert.notReached("IllegalStateException");
            throw new TokenRuntimeException("IllegalStateException: " +
                ise.getMessage());
      } catch(CharConversionException cce) {
            throw new InvalidKeySpecException("CharConversionException: " +
                cce.getMessage());
      } catch(NoSuchAlgorithmException nsae) {
            throw new InvalidKeySpecException("NoSuchAlgorithmException: " +
                nsae.getMessage());
      }
    }

    public KeySpec engineGetKeySpec(SecretKey key, Class keySpec)
            throws InvalidKeySpecException
    {
      try {
        if( ! (key instanceof SecretKeyFacade) ) {
            throw new InvalidKeySpecException("key is not a JSS key");
        }
        SymmetricKey symkey = ((SecretKeyFacade)key).key;
        byte[] keyBits = symkey.getKeyData();
        SymmetricKey.Type keyType = symkey.getType();
        if( keySpec.equals(DESedeKeySpec.class) ) {
            if( keyType != SymmetricKey.Type.DES3 ) {
                throw new InvalidKeySpecException(
                    "key/spec mismatch: " + keyType + " key, DESede spec");
            }
            return new DESedeKeySpec(keyBits);
        } else if( keySpec.equals(DESKeySpec.class) ) {
            if( keyType != SymmetricKey.Type.DES ) {
                throw new InvalidKeySpecException(
                    "key/spec mismatch: " + keyType + " key, DES spec");
            }
            return new DESKeySpec(keyBits);
        } else if( keySpec.equals(SecretKeySpec.class) ) {
            return new SecretKeySpec(keyBits, keyType.toString());
        } else {
            throw new InvalidKeySpecException(
                "Unsupported key spec: " + keySpec.getName());
        }
      } catch(SymmetricKey.NotExtractableException nee) {
          throw new InvalidKeySpecException("key is not extractable");
      } catch(InvalidKeyException ike) {
          // This gets thrown by the key spec constructor if there's something
          // wrong with the key bits. But since those key bits came from
          // a real key, this should never happen.
          Assert.notReached("Invalid key: " + ike.getMessage());
          throw new InvalidKeySpecException("Invalid key: " + ike.getMessage());
      }
    }

    public SecretKey engineTranslateKey(SecretKey key)
        throws InvalidKeyException
    {
        if( key instanceof SecretKeyFacade ) {
            // try cloning the key
            try {
                SymmetricKey oldkey = ((SecretKeyFacade)key).key;
                CryptoToken owningToken = oldkey.getOwningToken();
                org.mozilla.jss.crypto.KeyGenerator keygen =
                    token.getKeyGenerator(oldkey.getType().getKeyGenAlg());

                SymmetricKey newkey = keygen.clone(oldkey);

                return new SecretKeyFacade(newkey);
            } catch(SymmetricKey.NotExtractableException nee) {
                // no way around this, we fail
                throw new InvalidKeyException("key is not extractable");
            } catch(TokenException te) {
                // fall through and try doing it the long way
            } catch(NoSuchAlgorithmException nsae) {
                throw new InvalidKeyException("Unsupported algorithm: " +
                    nsae.getMessage());
            }
        }

        // try extracting the key value and then creating a new key
        try {
            byte[] keyBits = key.getEncoded();
            if( keyBits == null ) {
                throw new InvalidKeyException("Key is not extractable");
            }
            SymmetricKey.Type keyType =
                SymmetricKey.Type.fromName( key.getAlgorithm() );
            return generateKeyFromBits( keyBits, keyType);
        } catch( NoSuchAlgorithmException nsae ) {
            throw new InvalidKeyException("Unsupported algorithm: "
                + key.getAlgorithm());
        } catch(TokenException te) {
            throw new InvalidKeyException("Token failed to process key: " +
                te.getMessage());
        } catch(InvalidKeySpecException ikse) {
            throw new InvalidKeyException("Invalid key spec: "
                + ikse.getMessage());
        } catch(InvalidAlgorithmParameterException iape) {
            throw new InvalidKeyException("Invalid algorithm parameters: " +
                iape.getMessage());
        }
    }

    public static void main(String args[]) {

      try {
        CryptoManager.initialize(".");

        CryptoManager cm = CryptoManager.getInstance();
        CryptoToken tok = cm.getInternalCryptoToken();
        cm.setThreadToken(tok);

        org.mozilla.jss.crypto.KeyGenerator keygen =
            tok.getKeyGenerator(KeyGenAlgorithm.DES3);

        SymmetricKey symk = keygen.generate();
        SecretKeyFacade origKey = new SecretKeyFacade(symk);

        JSSSecretKeyFactorySpi fact =
            new JSSSecretKeyFactorySpi(KeyGenAlgorithm.DES3);

        DESedeKeySpec kspec = (DESedeKeySpec)
                fact.engineGetKeySpec(origKey, DESedeKeySpec.class);

        SecretKeyFacade newKey = (SecretKeyFacade)
                fact.engineGenerateSecret(kspec);

        org.mozilla.jss.crypto.Cipher cipher =
            tok.getCipherContext(EncryptionAlgorithm.DES3_ECB);
        cipher.initEncrypt(origKey.key);
        String original = "Hello, World!!!!";
        byte[] cipherText = cipher.doFinal( original.getBytes("UTF-8") );
        System.out.println("ciphertext is " + cipherText.length + " bytes");

        cipher.initDecrypt(newKey.key);
        byte[] plainText = cipher.doFinal(cipherText);
        System.out.println("recovered plaintext is " + plainText.length +
            " bytes");

        String recovered = new String(plainText, "UTF-8");
        System.out.println("Recovered '" + recovered + "'");
        if( ! recovered.equals(original) ) {
            throw new Exception("recovered string is different from original");
        }

        char[] pw = "foobarpw".toCharArray();
        byte[] salt = new byte[] { 0, 1, 2, 3, 4, 5, 6, 7 };
        int iterationCount = 2;

        // generate a PBE key the old-fashioned way
        keygen = tok.getKeyGenerator(PBEAlgorithm.PBE_SHA1_DES3_CBC);
        PBEKeyGenParams jssKeySpec = 
            new PBEKeyGenParams(pw, salt, iterationCount);
        keygen.initialize(jssKeySpec);
        symk = keygen.generate();
        byte[] keydata = symk.getKeyData();

        // generate a PBE key with the JCE
        SecretKeyFactory keyFact = SecretKeyFactory.getInstance("PBEWithSHA1AndDESede", "Mozilla-JSS");
        newKey = (SecretKeyFacade) keyFact.generateSecret(jssKeySpec);
        byte[] newkeydata = newKey.key.getKeyData();
        if( ! java.util.Arrays.equals(keydata, newkeydata) ) {
            throw new Exception("generated PBE keys are different");
        }
        System.out.println("generated PBE keys are the same");

/* XXX JDK 1.4 ONLY 
        // now try with a JDK 1.4 PBEKeySpec
        PBEKeySpec keySpec = new PBEKeySpec(pw, salt, iterationCount);
        newKey = (SecretKeyFacade) keyFact.generateSecret(keySpec);
        if( ! java.util.Arrays.equals(keydata, newKey.key.getKeyData()) ) {
            throw new Exception("generated PBE keys are different");
        }
        System.out.println("generated PBE keys are the same");
*/
        
        System.exit(0);
      } catch(Throwable t) {
            t.printStackTrace();
            System.exit(-1);
      }
    }

    public static class DES extends JSSSecretKeyFactorySpi {
        public DES() {
            super(KeyGenAlgorithm.DES);
        }
    }
    public static class DESede extends JSSSecretKeyFactorySpi {
        public DESede() {
            super(KeyGenAlgorithm.DESede);
        }
    }
    public static class AES extends JSSSecretKeyFactorySpi {
        public AES() {
            super(KeyGenAlgorithm.AES);
        }
    }
    public static class RC4 extends JSSSecretKeyFactorySpi {
        public RC4() {
            super(KeyGenAlgorithm.RC4);
        }
    }
    public static class PBE_MD5_DES_CBC extends JSSSecretKeyFactorySpi {
        public PBE_MD5_DES_CBC() {
            super(PBEAlgorithm.PBE_MD5_DES_CBC);
        }
    }
    public static class PBE_SHA1_DES_CBC extends JSSSecretKeyFactorySpi {
        public PBE_SHA1_DES_CBC() {
            super(PBEAlgorithm.PBE_SHA1_DES_CBC);
        }
    }
    public static class PBE_SHA1_DES3_CBC extends JSSSecretKeyFactorySpi {
        public PBE_SHA1_DES3_CBC() {
            super(PBEAlgorithm.PBE_SHA1_DES3_CBC);
        }
    }
    public static class PBE_SHA1_RC4_128 extends JSSSecretKeyFactorySpi {
        public PBE_SHA1_RC4_128() {
            super(PBEAlgorithm.PBE_SHA1_RC4_128);
        }
    }

    /**
     * @deprecated This class name is misleading. This algorithm
     * is used for generating Password-Based Authentication keys
     * for use with HmacSHA1. Use PBAHmacSHA1 instead.
     */
    public static class HmacSHA1 extends JSSSecretKeyFactorySpi {
        public HmacSHA1() {
            super(KeyGenAlgorithm.PBA_SHA1_HMAC);
        }
    }

    public static class PBAHmacSHA1 extends JSSSecretKeyFactorySpi {
        public PBAHmacSHA1() {
            super(KeyGenAlgorithm.PBA_SHA1_HMAC);
        }
    }

}
