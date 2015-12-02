/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.fxa;

import java.io.UnsupportedEncodingException;
import java.math.BigInteger;
import java.security.NoSuchAlgorithmException;

import org.json.simple.JSONObject;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.net.SRPConstants;

public class FxAccount10AuthDelegate implements FxAccountClient10.AuthDelegate {
  // Fixed by protocol.
  protected final BigInteger N;
  protected final BigInteger g;
  protected final int modNLengthBytes;

  // Configured at construction time.
  protected final String email;
  protected final byte[] stretchedPWBytes;

  // Encapsulate state.
  protected static class AuthState {
    protected String srpToken;
    protected String mainSalt;
    protected String srpSalt;

    protected BigInteger x;
    protected BigInteger A;
    protected byte[] Kbytes;
    protected byte[] Mbytes;
  }

  // State should be written exactly once.
  protected AuthState internalAuthState = null;

  public FxAccount10AuthDelegate(String email, byte[] stretchedPWBytes) {
    this.email = email;
    this.stretchedPWBytes = stretchedPWBytes;
    this.N = SRPConstants._2048.N;
    this.g = SRPConstants._2048.g;
    this.modNLengthBytes = SRPConstants._2048.byteLength;
  }

  protected BigInteger generateSecretValue() {
    return Utils.generateBigIntegerLessThan(N);
  }

  public static class FxAccountClientMalformedAuthException extends FxAccountClientException {
    private static final long serialVersionUID = 3585262174699395505L;

    public FxAccountClientMalformedAuthException(String detailMessage) {
      super(detailMessage);
    }
  }

  @SuppressWarnings("unchecked")
  @Override
  public JSONObject getAuthStartBody() throws FxAccountClientException {
    try {
      final JSONObject body = new JSONObject();
      body.put("email", FxAccountUtils.bytes(email));
      return body;
    } catch (UnsupportedEncodingException e) {
      throw new FxAccountClientException(e);
    }
  }

  @Override
  public void onAuthStartResponse(final ExtendedJSONObject body) throws FxAccountClientException {
    if (this.internalAuthState != null) {
      throw new FxAccountClientException("auth must not be written before calling onAuthStartResponse");
    }

    String srpToken = null;
    String srpSalt = null;
    String srpB = null;
    String mainSalt = null;

    try {
      srpToken = body.getString("srpToken");
      if (srpToken == null) {
        throw new FxAccountClientMalformedAuthException("srpToken must be a non-null object");
      }
      ExtendedJSONObject srp = body.getObject("srp");
      if (srp == null) {
        throw new FxAccountClientMalformedAuthException("srp must be a non-null object");
      }
      srpSalt = srp.getString("salt");
      if (srpSalt == null) {
        throw new FxAccountClientMalformedAuthException("srp.salt must not be null");
      }
      srpB = srp.getString("B");
      if (srpB == null) {
        throw new FxAccountClientMalformedAuthException("srp.B must not be null");
      }
      ExtendedJSONObject passwordStretching = body.getObject("passwordStretching");
      if (passwordStretching == null) {
        throw new FxAccountClientMalformedAuthException("passwordStretching must be a non-null object");
      }
      mainSalt = passwordStretching.getString("salt");
      if (mainSalt == null) {
        throw new FxAccountClientMalformedAuthException("srp.passwordStretching.salt must not be null");
      }
      throwIfParametersAreBad(passwordStretching);

      this.internalAuthState = authStateFromParameters(srpToken, mainSalt, srpSalt, srpB, generateSecretValue());
    } catch (FxAccountClientException e) {
      throw e;
    } catch (Exception e) {
      throw new FxAccountClientException(e);
    }
  }

  /**
   * Expect object like:
   * "passwordStretching": {
   *   "type": "PBKDF2/scrypt/PBKDF2/v1",
   *   "PBKDF2_rounds_1": 20000,
   *   "scrypt_N": 65536,
   *   "scrypt_r": 8,
   *   "scrypt_p": 1,
   *   "PBKDF2_rounds_2": 20000,
   *   "salt": "996bc6b1aa63cd69856a2ec81cbf19d5c8a604713362df9ee15c2bf07128efab"
   * }
   * @param params to verify.
   * @throws FxAccountClientMalformedAuthException
   */
  protected void throwIfParametersAreBad(ExtendedJSONObject params) throws FxAccountClientMalformedAuthException {
    if (params == null ||
        params.size() != 7 ||
        params.getString("salt") == null ||
        !("PBKDF2/scrypt/PBKDF2/v1".equals(params.getString("type"))) ||
        20000 != params.getLong("PBKDF2_rounds_1") ||
        65536 != params.getLong("scrypt_N") ||
        8 != params.getLong("scrypt_r") ||
        1 != params.getLong("scrypt_p") ||
        20000 != params.getLong("PBKDF2_rounds_2")) {
      throw new FxAccountClientMalformedAuthException("malformed passwordStretching parameters: '" + params.toJSONString() + "'.");
    }
  }

  /**
   * All state is written in this method.
   */
  protected AuthState authStateFromParameters(String srpToken, String mainSalt, String srpSalt, String srpB, BigInteger a) throws NoSuchAlgorithmException, UnsupportedEncodingException {
    AuthState authState = new AuthState();
    authState.srpToken = srpToken;
    authState.mainSalt = mainSalt;
    authState.srpSalt = srpSalt;

    authState.x = FxAccountUtils.srpVerifierLowercaseX(email.getBytes("UTF-8"), this.stretchedPWBytes, Utils.hex2Byte(srpSalt, FxAccountUtils.SALT_LENGTH_BYTES));

    authState.A = g.modPow(a, N);
    String srpA = FxAccountUtils.hexModN(authState.A, N);
    BigInteger B = new BigInteger(srpB, 16);

    byte[] srpABytes = Utils.hex2Byte(srpA, modNLengthBytes);
    byte[] srpBBytes = Utils.hex2Byte(srpB, modNLengthBytes);

    // u = H(pad(A) | pad(B))
    byte[] uBytes = Utils.sha256(Utils.concatAll(
        srpABytes,
        srpBBytes));
    BigInteger u = new BigInteger(Utils.byte2Hex(uBytes, FxAccountUtils.HASH_LENGTH_HEX), 16);

    // S = (B - k*g^x)^(a  u*x) % N
    // k = H(pad(N) | pad(g))
    int byteLength = (N.bitLength() + 7) / 8;
    byte[] kBytes = Utils.sha256(Utils.concatAll(
        Utils.hex2Byte(N.toString(16), byteLength),
        Utils.hex2Byte(g.toString(16), byteLength)));
    BigInteger k = new BigInteger(Utils.byte2Hex(kBytes, FxAccountUtils.HASH_LENGTH_HEX), 16);

    BigInteger base = B.subtract(k.multiply(g.modPow(authState.x, N)).mod(N)).mod(N);
    BigInteger pow = a.add(u.multiply(authState.x));
    BigInteger S = base.modPow(pow, N);
    String srpS = FxAccountUtils.hexModN(S, N);

    byte[] sBytes = Utils.hex2Byte(srpS, modNLengthBytes);

    // M = H(pad(A) | pad(B) | pad(S))
    authState.Mbytes = Utils.sha256(Utils.concatAll(
        srpABytes,
        srpBBytes,
        sBytes));

    // K = H(pad(S))
    authState.Kbytes = Utils.sha256(sBytes);

    return authState;
  }

  @SuppressWarnings("unchecked")
  @Override
  public JSONObject getAuthFinishBody() throws FxAccountClientException {
    if (internalAuthState == null) {
      throw new FxAccountClientException("auth must be successfully written before calling getAuthFinishBody.");
    }
    JSONObject body = new JSONObject();
    body.put("srpToken", internalAuthState.srpToken);
    body.put("A", FxAccountUtils.hexModN(internalAuthState.A, N));
    body.put("M", Utils.byte2Hex(internalAuthState.Mbytes, FxAccountUtils.HASH_LENGTH_HEX));
    return body;
  }

  @Override
  public byte[] getSharedBytes() throws FxAccountClientException {
    if (internalAuthState == null) {
      throw new FxAccountClientException("auth must be successfully finished before calling getSharedBytes.");
    }
    return internalAuthState.Kbytes;
  }
}
