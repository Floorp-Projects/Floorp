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

import org.mozilla.jss.crypto.*;
import org.mozilla.jss.util.*;
import java.security.spec.AlgorithmParameterSpec;
import java.security.*;
import java.security.SecureRandom;
import java.io.ByteArrayOutputStream;
import org.mozilla.jss.crypto.PrivateKey;

final class PK11Signature extends org.mozilla.jss.crypto.SignatureSpi {

	public PK11Signature(PK11Token token, SignatureAlgorithm algorithm)
		throws NoSuchAlgorithmException, TokenException
	{
        Assert._assert(token!=null && algorithm!=null);

        // Make sure this token supports this algorithm.  It's OK if
        // it only supports the signing part; the hashing can be done
        // on the internal module.
		if( ! token.doesAlgorithm(algorithm)  &&
            ! token.doesAlgorithm(algorithm.getSigningAlg()) )
        {
			throw new NoSuchAlgorithmException();
		}

		this.tokenProxy = token.getProxy();
		this.token = token;
		this.algorithm = algorithm;
        if( algorithm.getRawAlg() == algorithm ) {
            raw = true;
            rawInput = new ByteArrayOutputStream();
        }
		this.state = UNINITIALIZED;
	}

	public void engineInitSign(org.mozilla.jss.crypto.PrivateKey privateKey)
		throws InvalidKeyException, TokenException
	{
        PK11PrivKey privKey;

        Assert._assert(privateKey!=null);

        //
        // Scrutinize the key. Make sure it:
        //  -is a PKCS #11 key
        //  -lives on this token
        //  -is the right type for the algorithm
        //
		if( privateKey == null ) {
			throw new InvalidKeyException("private key is null");
		}
		if( ! (privateKey instanceof PK11PrivKey) ) {
			throw new InvalidKeyException("privateKey is not a PKCS #11 "+
				"private key");
		}

        privKey = (PK11PrivKey) privateKey;

        try {
    		privKey.verifyKeyIsOnToken(token);
        } catch(NoSuchItemOnTokenException e) {
            throw new InvalidKeyException(e.toString());
        }

        try {
            if( KeyType.getKeyTypeFromAlgorithm(algorithm)
                             != privKey.getKeyType())
            {
                throw new InvalidKeyException(
                    "Key type is inconsistent with algorithm");
            }
        } catch( NoSuchAlgorithmException e ) {
            Assert.notReached("unknown algorithm: "+algorithm);
            throw new InvalidKeyException();
        }

        // Finally, the key is OK
		key = privKey;

        // Now initialize the signature context
        if( ! raw ) {
            sigContext = null;
            initSigContext();
        }

        // Don't set state until we know everything worked
		state = SIGN;
	}

    /*************************************************************
    ** This is just here for JCA compliance, we don't take randoms this way.
    */
	public void
    engineInitSign(org.mozilla.jss.crypto.PrivateKey privateKey,
                    SecureRandom random)
		throws InvalidKeyException, TokenException
	{
		Assert.notReached("This function is not supported");

		engineInitSign(privateKey);
	}

    /*************************************************************
    ** Creates a signing context, initializes it,
    ** and sets the sigContext field.
    */
    protected native void initSigContext()
        throws TokenException;


	public void engineInitVerify(PublicKey publicKey)
		throws InvalidKeyException, TokenException
	{
		PK11PubKey pubKey;

        Assert._assert(publicKey!=null);

        //
        // Scrutinize the key. Make sure it:
        //  -is a PKCS #11 key
        //  -lives on this token
        //  -is the right type for the algorithm
        //
		if( ! (publicKey instanceof PK11PubKey) ) {
			throw new InvalidKeyException("publicKey is not a PKCS #11 "+
				"public key");
		}
		pubKey = (PK11PubKey) publicKey;

		//try {
		//	pubKey.verifyKeyIsOnToken(token);
		//} catch( NoSuchItemOnTokenException e) {
		//	throw new InvalidKeyException(e.toString());
		//}

        try {
            if( KeyType.getKeyTypeFromAlgorithm(algorithm)
                             != pubKey.getKeyType())
            {
                throw new InvalidKeyException(
                    "Key type is inconsistent with algorithm");
            }
        } catch( NoSuchAlgorithmException e ) {
            Assert.notReached("unknown algorithm: "+algorithm);
            throw new InvalidKeyException();
        }

		key = pubKey;
			
        if( ! raw ) {
            sigContext = null;
            initVfyContext();
        }

        // Don't set state until we know everything worked.
		state = VERIFY;
	}

    protected native void initVfyContext() throws TokenException;

	public void engineUpdate(byte b)
        throws SignatureException, TokenException
    {
        engineUpdate(new byte[] {b}, 0, 1);
    }

    public void engineUpdate(byte[] b, int off, int len)
        throws SignatureException, TokenException
    {
        Assert._assert(b != null);
        if( (state==SIGN || state==VERIFY) ) {
            if(!raw && sigContext==null) {
                Assert.notReached("signature has no context");
                throw new SignatureException("Signature has no context");
            } else if( raw && rawInput==null) {
                Assert.notReached("raw signature has no input stream");
                throw new SignatureException("raw signature has no input "+
                    "stream");
            }
        } else {
            Assert._assert(state == UNINITIALIZED);
            throw new SignatureException("Signature is not initialized");
        }
        Assert._assert(token!=null);
        Assert._assert(tokenProxy!=null);
        Assert._assert(algorithm!=null);
        Assert._assert(key!=null);

        if( raw ) {
            rawInput.write(b, off, len);
        } else {
            engineUpdateNative( b, off, len);
        }
    }

    protected native void engineUpdateNative(byte[] b, int off, int len)
        throws TokenException;
    

    public byte[] engineSign()
        throws SignatureException, TokenException
    {
        if(state != SIGN) {
            throw new SignatureException("Signature is not initialized");
        }
        if(!raw && sigContext==null) {
            throw new SignatureException("Signature has no context");
        } else if(raw && rawInput==null) {
            throw new SignatureException("Signature has no input");
        }
        Assert._assert(token!=null);
        Assert._assert(tokenProxy!=null);
        Assert._assert(algorithm!=null);
        Assert._assert(key!=null);

        byte[] result;
        if( raw ) {
            result = engineRawSignNative(token, (PK11PrivKey)key,
                rawInput.toByteArray());
            rawInput.reset();
        } else {
            result = engineSignNative();
        }
		state = UNINITIALIZED;
		sigContext = null;

		return result;
    }

    public int engineSign(byte[] outbuf, int offset, int len)
        throws SignatureException, TokenException
    {
        Assert._assert(outbuf!=null);
        byte[] sig;
        if( raw ) {
            sig = engineRawSignNative(token, (PK11PrivKey)key,
                rawInput.toByteArray());
            rawInput.reset();
        } else {
		    sig = engineSign();
        }
		if(	(outbuf==null) || (outbuf.length <= offset) ||
			(len < sig.length) || (offset+len > outbuf.length))
		{
			throw new SignatureException(
					"outbuf is not sufficient to hold signature");
		}
		System.arraycopy( (Object)sig, 0, (Object)outbuf, offset, sig.length);
		return sig.length;
    }

    /**
     * Performs raw signing of the given hash with the given private key.
     */
    private static native byte[] engineRawSignNative(PK11Token token,
        PrivateKey key, byte[] hash)
        throws SignatureException, TokenException;

    private native byte[] engineSignNative()
        throws SignatureException, TokenException;

    public boolean engineVerify(byte[] sigBytes)
        throws SignatureException, TokenException
    {
        Assert._assert(sigBytes!=null);
		if(state != VERIFY) {
			throw new SignatureException(
						"Signature is not initialized properly");
		}
		if(!raw && sigContext==null) {
			Assert.notReached("Signature has no context");
			throw new SignatureException("Signature has no context");
		}
        if(raw && rawInput==null) {
            Assert.notReached("Signature has no input");
            throw new SignatureException("Signature has no input");
        }
		Assert._assert(token!=null);
		Assert._assert(tokenProxy!=null);
		Assert._assert(algorithm!=null);
		Assert._assert(key!=null);

		if(sigBytes==null) {
			return false;
		}

        boolean result;
        if( raw ) {
            result = engineRawVerifyNative(token, (PK11PubKey)key,
                rawInput.toByteArray(), sigBytes);
            rawInput.reset();
        } else {
            result = engineVerifyNative(sigBytes);
        }
		state = UNINITIALIZED;
		sigContext = null;

		return result;
    }

    /**
     * Performs raw verification of the signature of a hash using the
     * given public key, on the given token.
     */
    protected static native boolean engineRawVerifyNative(PK11Token token,
        PublicKey key, byte[] hash, byte[] signature)
        throws SignatureException, TokenException;

	native protected boolean engineVerifyNative(byte[] sigBytes)
		throws SignatureException, TokenException;

    public void engineSetParameter(AlgorithmParameterSpec params)
        throws InvalidAlgorithmParameterException, TokenException
    {
        Assert.notYetImplemented("PK11Signature.engineSetParameter");
    }

	protected PK11Token token;
	protected TokenProxy tokenProxy;
	protected Algorithm algorithm;
	protected PK11Key key;
	protected int state;
    protected SigContextProxy sigContext;
    protected boolean raw=false; // raw signing only, no hashing
    protected ByteArrayOutputStream rawInput;

	// states
	static public final int UNINITIALIZED = 0;
	static public final int SIGN = 1;
	static public final int VERIFY = 2;
}

class SigContextProxy extends NativeProxy {
    public SigContextProxy(byte[] pointer) {
        super(pointer);
    }
    protected native void releaseNativeResources();
    protected void finalize() throws Throwable {
        Debug.trace(Debug.OBNOXIOUS, "Finalizing a SigContextProxy");
        super.finalize();
    }
}
