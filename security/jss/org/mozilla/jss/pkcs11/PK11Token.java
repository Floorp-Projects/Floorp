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

package org.mozilla.jss.pkcs11;

import java.util.*;
import org.mozilla.jss.util.*;
import org.mozilla.jss.crypto.*;
import java.security.NoSuchAlgorithmException;
import java.security.InvalidKeyException;
import java.security.InvalidParameterException;

/**
 * A PKCS #11 token.  Currently, these can only be obtained from the
 * CryptoManager class.
 *
 * @author nicolson
 * @version $Revision: 1.9 $ $Date: 2006/02/14 23:52:54 $ 
 * @see org.mozilla.jss.CryptoManager
 */
public final class PK11Token implements CryptoToken {

    ////////////////////////////////////////////////////
    //  exceptions
    ////////////////////////////////////////////////////
    /**
     * Thrown if the operation requires that the token be logged in, and it
     * isn't.
     */
    static public class NotInitializedException
		extends IncorrectPasswordException
	{
        public NotInitializedException() {}
        public NotInitializedException(String mesg) {super(mesg);}
    }

    ////////////////////////////////////////////////////
    //  public routines
    ////////////////////////////////////////////////////
	public org.mozilla.jss.crypto.Signature
	getSignatureContext(SignatureAlgorithm algorithm) 
		throws NoSuchAlgorithmException, TokenException
	{
        Assert._assert(algorithm!=null);
		return Tunnel.constructSignature( algorithm,
                    new PK11Signature(this, algorithm) );
	}

	public JSSMessageDigest
	getDigestContext(DigestAlgorithm algorithm)
		throws NoSuchAlgorithmException,
                java.security.DigestException
	{
        if( ! doesAlgorithm(algorithm) ) {
            throw new NoSuchAlgorithmException();
        }

        return new PK11MessageDigest(this, algorithm);
	}

	public Cipher
	getCipherContext(EncryptionAlgorithm algorithm)
		throws NoSuchAlgorithmException, TokenException
	{
        if( ! doesAlgorithm(algorithm) ) {
            throw new NoSuchAlgorithmException(
                algorithm+" is not supported by this token");
        }
        return new PK11Cipher(this, algorithm);
	}

    public KeyGenerator
    getKeyGenerator(KeyGenAlgorithm algorithm)
        throws NoSuchAlgorithmException, TokenException
    {
        if( ! doesAlgorithm(algorithm) ) {
            throw new NoSuchAlgorithmException(
                algorithm+" is not supported by this token");
        }
        return new PK11KeyGenerator(this, algorithm);
    }

    /**
     * Allows a SymmetricKey to be cloned on a different token.
     *
     * @exception SymmetricKey.NotExtractableException If the key material
     *      cannot be extracted from the current token.
     * @exception InvalidKeyException If the owning token cannot process
     *      the key to be cloned.
     */
    public SymmetricKey cloneKey(SymmetricKey key)
        throws SymmetricKey.NotExtractableException,
            InvalidKeyException, TokenException
    {
        return PK11KeyGenerator.clone(key, this);
    }

    public KeyWrapper
    getKeyWrapper(KeyWrapAlgorithm algorithm)
        throws NoSuchAlgorithmException, TokenException
    {
        if( ! doesAlgorithm(algorithm) ) {
            throw new NoSuchAlgorithmException(
                algorithm+" is not supported by this token");
        }
        return new PK11KeyWrapper(this, algorithm);
    }

	public java.security.SecureRandom
	getRandomGenerator()
		throws NotImplementedException, TokenException
	{
		throw new NotImplementedException();
	}

	public org.mozilla.jss.crypto.KeyPairGenerator
	getKeyPairGenerator(KeyPairAlgorithm algorithm)
		throws NoSuchAlgorithmException, TokenException
	{
        Assert._assert(algorithm!=null);
        return new KeyPairGenerator(algorithm,
                                    new PK11KeyPairGenerator(this, algorithm));
	}

    public native boolean isLoggedIn() throws TokenException;

    /**
     * Log into the token. If you are already logged in, this method has
     * no effect, even if the PIN is wrong.
     *
     * @param callback A callback to use to obtain the password, or a 
     *      Password object.
     * @exception NotInitializedException The token has not yet been
     *  initialized.
     * @exception IncorrectPasswordException The specified password
     *      was incorrect.
     */
    public void login(PasswordCallback callback)
        throws NotInitializedException, IncorrectPasswordException,
			TokenException
	{
        if(callback == null) {
            callback = new NullPasswordCallback();
        }
        nativeLogin(callback);
	}

	protected native void nativeLogin(PasswordCallback callback)
        throws NotInitializedException,	IncorrectPasswordException,
        TokenException;

    /**
     * @return true if the token is writable, false if it is read-only.
     *  Writable tokens can have their keys generated on the internal token
     *  and then moved out.
     */
    public native boolean isWritable();

    /**
     * Determines if the given token is present on the system.
     * This would return false, for example, for a smart card reader
     * that didn't have a card inserted.
     */
    public native boolean isPresent();

    /**
     * Log out of the token. 
     *
     * @exception TokenException If you are already logged in, or an
     *  unspecified error occurs.
     */
    public native void logout() throws TokenException;

    public native int getLoginMode() throws TokenException;

    public native void setLoginMode(int mode) throws TokenException;

    public native int getLoginTimeoutMinutes() throws TokenException;

    public native void setLoginTimeoutMinutes(int timeoutMinutes)
            throws TokenException;

    /**
     * Determines whether this is a removable token. For example, a smart card
     * is removable, while the Netscape internal module and a hardware
     * accelerator card are not removable.
     * @return true if the token is removable, false otherwise.
     */
    //public native boolean isRemovable();

    /**
     * Initialize PIN.  This sets the user's new PIN, using the current
     * security officer PIN for authentication.
     *
     * @param ssopw The security officer's current password.
     * @param userpw The user's new password.
     * @exception IncorrectPinException If the security officer PIN is
     *  incorrect.
     * @exception TokenException If the PIN was already initialized,
     *  or there was an unspecified error in the token.
     */
    public void initPassword(PasswordCallback ssopwcb,
		PasswordCallback userpwcb)
        throws IncorrectPasswordException, AlreadyInitializedException,
		TokenException
	{
		byte[] ssopwArray = null;
		byte[] userpwArray = null;
        Password ssopw=null;
        Password userpw=null;
		PasswordCallbackInfo pwcb = makePWCBInfo();

        if(ssopwcb==null) {
            ssopwcb = new NullPasswordCallback();
        }
        if(userpwcb==null) {
            userpwcb = new NullPasswordCallback();
        }

		try {

			// Make sure the password hasn't already been set, doing special
			// checks for the internal module
			if(!PWInitable()) {
				throw new AlreadyInitializedException();
			}

			// Verify the SSO Password, except on internal module
            if( isInternalKeyStorageToken() ) {
                ssopwArray = new byte[] {0};
            } else {
			    ssopw = ssopwcb.getPasswordFirstAttempt(pwcb);
			    ssopwArray = Tunnel.getPasswordByteCopy(ssopw);
			    while( ! SSOPasswordIsCorrect(ssopwArray) ) {
				    Password.wipeBytes(ssopwArray);
                    ssopw.clear();
				    ssopw = ssopwcb.getPasswordAgain(pwcb);
				    ssopwArray = Tunnel.getPasswordByteCopy(ssopw);
			    }
            }

			// Now change the PIN
			userpw = userpwcb.getPasswordFirstAttempt(pwcb);
			userpwArray = Tunnel.getPasswordByteCopy(userpw);
			initPassword(ssopwArray, userpwArray);
			
		} catch (PasswordCallback.GiveUpException e) {
			throw new IncorrectPasswordException(e.toString());
		} finally {
			// zero-out the arrays
			if(ssopwArray != null) {
				Password.wipeBytes(ssopwArray);
			}
            if(ssopw != null) {
                ssopw.clear();
            }
			if(userpwArray != null) {
				Password.wipeBytes(userpwArray);
			}
            if(userpw != null) {
                userpw.clear();
            }
		}
	}

	/**
	 * Make sure the PIN can be initialized.  This is mainly to check the
	 * internal module.
	 */
	protected native boolean PWInitable() throws TokenException;

	protected native boolean SSOPasswordIsCorrect(byte[] ssopw)
		throws TokenException, AlreadyInitializedException;

	protected native void initPassword(byte[] ssopw, byte[] userpw)
		throws IncorrectPasswordException, AlreadyInitializedException,
		TokenException;

	/**
	 * Determine whether the token has been initialized yet.
	 */
	public native boolean
	passwordIsInitialized() throws TokenException;

    /**
     * Change password.  This changes the user's PIN after it has already
     * been initialized.
     *
     * @param oldPIN The user's old PIN.
     * @param newPIN The new PIN.
     * @exception IncorrectPasswordException If the old PIN is incorrect.
     * @exception TokenException If some other error occurs on the token.
     *
     */
    public void changePassword(PasswordCallback oldPINcb,
			PasswordCallback newPINcb)
        throws IncorrectPasswordException, TokenException
	{
		byte[] oldPW = null;
		byte[] newPW = null;
        Password oldPIN=null;
        Password newPIN=null;
		PasswordCallbackInfo pwcb = makePWCBInfo();

        if(oldPINcb==null) {
            oldPINcb = new NullPasswordCallback();
        }
        if(newPINcb==null) {
            newPINcb = new NullPasswordCallback();
        }

		try {

			// Verify the old password
			oldPIN = oldPINcb.getPasswordFirstAttempt(pwcb);
			oldPW = Tunnel.getPasswordByteCopy(oldPIN);
			if( ! userPasswordIsCorrect(oldPW) ) {
				do {
					Password.wipeBytes(oldPW);
                    oldPIN.clear();
					oldPIN = oldPINcb.getPasswordAgain(pwcb);
					oldPW = Tunnel.getPasswordByteCopy(oldPIN);
				} while( ! userPasswordIsCorrect(oldPW) );
			}

			// Now change the PIN
			newPIN = newPINcb.getPasswordFirstAttempt(pwcb);
			newPW = Tunnel.getPasswordByteCopy(newPIN);
			changePassword(oldPW, newPW);
			
		} catch (PasswordCallback.GiveUpException e) {
			throw new IncorrectPasswordException(e.toString());
		} finally {
			if(oldPW != null) {
				Password.wipeBytes(oldPW);
			}
            if(oldPIN != null) {
                oldPIN.clear();
            }
			if(newPW != null) {
				Password.wipeBytes(newPW);
			}
            if(newPIN != null) {
                newPIN.clear();
            }
		}
	}

	protected PasswordCallbackInfo makePWCBInfo() {
		return new TokenCallbackInfo(getName());
	}

	/**
	 * Check the given password, return true if it's right, false if it's
	 * wrong.
	 */
	protected native boolean userPasswordIsCorrect(byte[] pw)
		throws TokenException;

	/**
	 * Change the password on the token from the old one to the new one.
	 */
    protected native void changePassword(byte[] oldPIN, byte[] newPIN)
        throws IncorrectPasswordException, TokenException;

    public native String getName();

	public java.security.Provider
	getProvider() {
		Assert.notYetImplemented("Providers not implemented by PK11Token yet");
		return null;
	}

	public CryptoStore
	getCryptoStore() {
		return cryptoStore;
	}

	/**
	 * Determines whether this token is capable of performing the given
	 * PKCS #11 mechanism.
	 */
/*
	public boolean doesMechanism(Mechanism mech) {
        return doesMechanismNative(mech.getValue());
    }
*/

    /**
     * Deep-comparison operator.
     * 
     * @return true if these tokens point to the same underlying native token.
     *  false otherwise, or if <code>compare</code> is null.
     */
    public boolean equals(Object obj) {
        if(obj==null) {
            return false;
        } else {
            if( ! (obj instanceof PK11Token) ) {
                return false;
            }
            return tokenProxy.equals(((PK11Token)obj).tokenProxy);
        }
    }

    //protected native boolean doesMechanismNative(int mech);

	/**
	 * Determines whether this token is capable of performing the given
	 * algorithm.
	 */
    public native boolean doesAlgorithm(Algorithm alg);

	/**
	 * Generates a PKCS#10 certificate request including Begin/End brackets
	 * @param subject subject dn of the certificate
	 * @param keysize size of the key
	 * @param keyType "rsa" or "dsa"
     * @param P The DSA prime parameter
     * @param Q The DSA sub-prime parameter
     * @param G The DSA base parameter
	 * @return String that represents a PKCS#10 b64 encoded blob with
	 * begin/end brackets
	 */
	public String generateCertRequest(String subject, int keysize,
											 String keyType,
											 byte[] P, byte[] Q,
											 byte[] G)
		throws TokenException, InvalidParameterException, PQGParamGenException
	{

			if (keyType.equalsIgnoreCase("dsa")) {
				if ((P == null) && (Q == null) && (G == null)) {
					PQGParams pqg;
					try {
						 pqg = PQGParams.generate(keysize);
					} catch (PQGParamGenException e) {
						throw e;
					}
					byte[] p = PQGParams.BigIntegerToUnsignedByteArray(pqg.getP());
					byte[] q = PQGParams.BigIntegerToUnsignedByteArray(pqg.getQ());
					byte[] g = PQGParams.BigIntegerToUnsignedByteArray(pqg.getG());
					P = p;
					Q = q;
					G = g;
					String pk10String;
					try {
						pk10String = 
							generatePK10(subject, keysize, keyType, p,
									 q, g);
					} catch (TokenException e) {
						throw e;
					} catch (InvalidParameterException e) {
						throw e;
					}
					
					return ("-----BEGIN NEW CERTIFICATE REQUEST-----\n"+
							pk10String +
							"\n-----END NEW CERTIFICATE REQUEST-----");
				} else if ((P == null) || (Q == null) || (G == null)) {
					throw new InvalidParameterException("need all P, Q, and G");
				}

			}
			String pk10String;
			try {
				pk10String = 
					generatePK10(subject, keysize, keyType, P,
								 Q, G);
			} catch (TokenException e) {
				throw e;
			} catch (InvalidParameterException e) {
				throw e;
			}

			return ("-----BEGIN NEW CERTIFICATE REQUEST-----\n"+
					pk10String +
					"\n-----END NEW CERTIFICATE REQUEST-----");
	}

	protected native String generatePK10(String subject, int keysize,
											 String keyType,
											 byte[] P, byte[] Q,
											 byte[] G)
		throws TokenException, InvalidParameterException;

    ////////////////////////////////////////////////////
    // construction and finalization
    ////////////////////////////////////////////////////

    /*
     * Default constructor should never be called.
     */
    protected PK11Token() {
        Assert._assert(false);
    }

    /**
     * Creates a new PK11Token.  Should only be called from PK11Token's
     * native code. 
     * @param pointer A byte array containing a pointer to a PKCS #11 slot.
     */
    protected PK11Token(byte[] pointer, boolean internal, boolean keyStorage) {
        Assert._assert(pointer!=null);
        tokenProxy = new TokenProxy(pointer);
        mIsInternalCryptoToken = internal;
        mIsInternalKeyStorageToken = keyStorage;
        cryptoStore = new PK11Store(tokenProxy);
    }

/*
	protected PK11Token(TokenProxy proxy) {
        Assert._assert(proxy!=null);
		this.tokenProxy = proxy;
	}
*/

	public TokenProxy getProxy() {
		return tokenProxy;
	}


    /**
     * @return true if this is the internal token used for bulk crypto.
     */
    public boolean isInternalCryptoToken() {
        return mIsInternalCryptoToken;
    }
    protected boolean mIsInternalCryptoToken;

    /**
     * @return true if this is the internal key storage token.
     */
    public boolean isInternalKeyStorageToken() {
        return mIsInternalKeyStorageToken;
    }
    protected boolean mIsInternalKeyStorageToken;

    //////////////////////////////////////////////////
    // Private Data
    //////////////////////////////////////////////////
    protected TokenProxy tokenProxy;

    protected PK11Store cryptoStore;
}

/**
 * This class just hardwires the type to be TOKEN so we don't have to mess
 * with Java constants in native code.
 */
class TokenCallbackInfo extends PasswordCallbackInfo {
	public TokenCallbackInfo(String name) {
		super(name, TOKEN);
	}
}
