/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.authenticator;

import java.util.LinkedList;
import java.util.concurrent.Executor;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountClient;
import org.mozilla.gecko.background.fxa.FxAccountClient10;
import org.mozilla.gecko.background.fxa.FxAccountClient10.RequestDelegate;
import org.mozilla.gecko.background.fxa.FxAccountClient10.StatusResponse;
import org.mozilla.gecko.background.fxa.FxAccountClient10.TwoKeys;
import org.mozilla.gecko.background.fxa.FxAccountClient20;
import org.mozilla.gecko.background.fxa.FxAccountClient20.LoginResponse;
import org.mozilla.gecko.background.fxa.SkewHandler;
import org.mozilla.gecko.browserid.BrowserIDKeyPair;
import org.mozilla.gecko.browserid.JSONWebTokenUtils;
import org.mozilla.gecko.browserid.VerifyingPublicKey;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.authenticator.FxAccountLoginException.FxAccountLoginAccountNotVerifiedException;
import org.mozilla.gecko.fxa.authenticator.FxAccountLoginException.FxAccountLoginBadPasswordException;
import org.mozilla.gecko.sync.HTTPFailureException;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

import android.content.Context;
import ch.boye.httpclientandroidlib.HttpResponse;

public class FxAccountLoginPolicy {
  public static final String LOG_TAG = FxAccountLoginPolicy.class.getSimpleName();

  public final Context context;
  public final AbstractFxAccount fxAccount;
  public final Executor executor;

  public FxAccountLoginPolicy(Context context, AbstractFxAccount fxAccount, Executor executor) {
    this.context = context;
    this.fxAccount = fxAccount;
    this.executor = executor;
  }

  public long certificateDurationInMilliseconds = JSONWebTokenUtils.DEFAULT_CERTIFICATE_DURATION_IN_MILLISECONDS;
  public long assertionDurationInMilliseconds = JSONWebTokenUtils.DEFAULT_ASSERTION_DURATION_IN_MILLISECONDS;

  public long getCertificateDurationInMilliseconds() {
    return certificateDurationInMilliseconds;
  }

  public long getAssertionDurationInMilliseconds() {
    return assertionDurationInMilliseconds;
  }

  protected FxAccountClient makeFxAccountClient() {
    String serverURI = fxAccount.getAccountServerURI();
    return new FxAccountClient20(serverURI, executor);
  }

  private SkewHandler skewHandler;

  /**
   * Check if this certificate is not worth generating an assertion from: for
   * example, because it is not well-formed, or it is already expired.
   *
   * @param certificate
   *          to check.
   * @return if it is definitely not worth generating an assertion from this
   *         certificate.
   */
  protected boolean isInvalidCertificate(String certificate) {
    return false;
  }

  /**
   * Check if this assertion is not worth presenting to the token server: for
   * example, because it is not well-formed, or it is already expired.
   *
   * @param assertion
   *          to check.
   * @return if assertion is definitely not worth presenting to the token
   *         server.
   */
  protected boolean isInvalidAssertion(String assertion) {
    return false;
  }

  protected long now() {
    return System.currentTimeMillis();
  }

  public enum AccountState {
    Invalid,
    NeedsSessionToken,
    NeedsVerification,
    NeedsKeys,
    NeedsCertificate,
    NeedsAssertion,
    Valid,
  };

  public AccountState getAccountState(AbstractFxAccount fxAccount) {
    String serverURI = fxAccount.getAccountServerURI();
    byte[] emailUTF8 = fxAccount.getEmailUTF8();
    byte[] quickStretchedPW = fxAccount.getQuickStretchedPW();
    if (!fxAccount.isValid() || serverURI == null || emailUTF8 == null || quickStretchedPW == null) {
      return AccountState.Invalid;
    }

    byte[] sessionToken = fxAccount.getSessionToken();
    if (sessionToken == null) {
      return AccountState.NeedsSessionToken;
    }

    if (!fxAccount.isVerified()) {
      return AccountState.NeedsVerification;
    }

    // Verify against server?  Tricky.
    if (fxAccount.getKa() == null || fxAccount.getKb() == null) {
      return AccountState.NeedsKeys;
    }

    String certificate = fxAccount.getCertificate();
    if (certificate == null || isInvalidCertificate(certificate)) {
      return AccountState.NeedsCertificate;
    }

    String assertion = fxAccount.getAssertion();
    if (assertion == null || isInvalidAssertion(assertion)) {
      return AccountState.NeedsAssertion;
    }

    return AccountState.Valid;
  }

  protected interface LoginStage {
    public void execute(LoginStageDelegate delegate) throws Exception;
  }

  protected LinkedList<LoginStage> getStages(AccountState state) {
    final LinkedList<LoginStage> stages = new LinkedList<LoginStage>();
    if (state == AccountState.Invalid) {
      stages.addFirst(new FailStage());
      return stages;
    }

    stages.addFirst(new SuccessStage());
    if (state == AccountState.Valid) {
      return stages;
    }
    stages.addFirst(new EnsureAssertionStage());
    if (state == AccountState.NeedsAssertion) {
      return stages;
    }
    stages.addFirst(new EnsureCertificateStage());
    if (state == AccountState.NeedsCertificate) {
      return stages;
    }
    stages.addFirst(new EnsureKeysStage());
    stages.addFirst(new EnsureKeyFetchTokenStage());
    if (state == AccountState.NeedsKeys) {
      return stages;
    }
    stages.addFirst(new EnsureVerificationStage());
    if (state == AccountState.NeedsVerification) {
      return stages;
    }
    stages.addFirst(new EnsureSessionTokenStage());
    if (state == AccountState.NeedsSessionToken) {
      return stages;
    }
    return stages;
  }

  public void login(final String audience, final FxAccountLoginDelegate delegate, final SkewHandler skewHandler) {
    this.skewHandler = skewHandler;
    this.login(audience, delegate);
  }

  /**
   * Do as much of a Firefox Account login dance as possible.
   * <p>
   * To avoid deeply nested callbacks, we maintain a simple queue of stages to
   * execute in sequence.
   *
   * @param audience to generate assertion for.
   * @param delegate providing callbacks to invoke.
   */
  public void login(final String audience, final FxAccountLoginDelegate delegate) {
    final AccountState initialState = getAccountState(fxAccount);
    Logger.info(LOG_TAG, "Logging in account from initial state " + initialState + ".");

    final LinkedList<LoginStage> stages = getStages(initialState);
    final LinkedList<String> stageNames = new LinkedList<String>();
    for (LoginStage stage : stages) {
      stageNames.add(stage.getClass().getSimpleName());
    }
    Logger.info(LOG_TAG, "Executing stages: [" + Utils.toCommaSeparatedString(stageNames) + "]");

    LoginStageDelegate loginStageDelegate = new LoginStageDelegate(stages, audience, delegate);
    loginStageDelegate.advance();
  }

  protected class LoginStageDelegate {
    public final LinkedList<LoginStage> stages;
    public final String audience;
    public final FxAccountLoginDelegate delegate;
    public final FxAccountClient client;

    protected LoginStage currentStage = null;

    public LoginStageDelegate(LinkedList<LoginStage> stages, String audience, FxAccountLoginDelegate delegate) {
      this.stages = stages;
      this.audience = audience;
      this.delegate = delegate;
      this.client = makeFxAccountClient();
    }

    protected void invokeHandleHardFailure(final FxAccountLoginDelegate delegate, final FxAccountLoginException e) {
      executor.execute(new Runnable() {
        @Override
        public void run() {
          delegate.handleError(e);
        }
      });
    }

    public void advance() {
      currentStage = stages.poll();
      if (currentStage == null) {
        // No more stages.  But we haven't seen an assertion. Failure!
        Logger.info(LOG_TAG, "No more stages: login failed?");
        invokeHandleHardFailure(delegate, new FxAccountLoginException("No more stages, but no assertion: login failed?"));
        return;
      }

      try {
        Logger.info(LOG_TAG, "Executing stage: " + currentStage.getClass().getSimpleName());
        currentStage.execute(this);
      } catch (Exception e) {
        Logger.info(LOG_TAG, "Got exception during stage.", e);
        invokeHandleHardFailure(delegate, new FxAccountLoginException(e));
        return;
      }
    }

    public void handleStageSuccess() {
      Logger.info(LOG_TAG, "Stage succeeded: " + currentStage.getClass().getSimpleName());
      advance();
    }

    public void handleLoginSuccess(final String assertion) {
      Logger.info(LOG_TAG, "Login succeeded.");
      executor.execute(new Runnable() {
        @Override
        public void run() {
          delegate.handleSuccess(assertion);
        }
      });
      return;
    }

    public void handleError(FxAccountLoginException e) {
      invokeHandleHardFailure(delegate, e);
    }
  }

  public class EnsureSessionTokenStage implements LoginStage {
    @Override
    public void execute(final LoginStageDelegate delegate) throws Exception {
      byte[] emailUTF8 = fxAccount.getEmailUTF8();
      if (emailUTF8 == null) {
        throw new IllegalStateException("emailUTF8 must not be null");
      }
      byte[] quickStretchedPW = fxAccount.getQuickStretchedPW();
      if (quickStretchedPW == null) {
        throw new IllegalStateException("quickStretchedPW must not be null");
      }

      delegate.client.loginAndGetKeys(emailUTF8, quickStretchedPW, new RequestDelegate<FxAccountClient20.LoginResponse>() {
        @Override
        public void handleError(Exception e) {
          delegate.handleError(new FxAccountLoginException(e));
        }

        @Override
        public void handleFailure(int status, HttpResponse response) {
          if (skewHandler != null) {
            skewHandler.updateSkew(response, now());
          }

          if (status != 401) {
            delegate.handleError(new FxAccountLoginException(new HTTPFailureException(new SyncStorageResponse(response))));
            return;
          }
          // We just got denied for a sessionToken. That's a problem with
          // our email or password. Only thing to do is mark the account
          // invalid and ask for user intervention.
          fxAccount.setInvalid();
          delegate.handleError(new FxAccountLoginBadPasswordException("Auth server rejected email/password while fetching sessionToken."));
        }

        @Override
        public void handleSuccess(LoginResponse result) {
          fxAccount.setSessionToken(result.sessionToken);
          fxAccount.setKeyFetchToken(result.keyFetchToken);
          if (FxAccountConstants.LOG_PERSONAL_INFORMATION) {
            FxAccountConstants.pii(LOG_TAG, "Fetched sessionToken : " + Utils.byte2Hex(result.sessionToken));
            FxAccountConstants.pii(LOG_TAG, "Fetched keyFetchToken: " + Utils.byte2Hex(result.keyFetchToken));
          }
          delegate.handleStageSuccess();
        }
      });
    }
  }

  /**
   * Now that we have a server to talk to and a session token, we can use them
   * to check that the account is verified.
   */
  public class EnsureVerificationStage implements LoginStage {
    @Override
    public void execute(final LoginStageDelegate delegate) {
      byte[] sessionToken = fxAccount.getSessionToken();
      if (sessionToken == null) {
        throw new IllegalArgumentException("sessionToken must not be null");
      }

      delegate.client.status(sessionToken, new RequestDelegate<StatusResponse>() {
        @Override
        public void handleError(Exception e) {
          delegate.handleError(new FxAccountLoginException(e));
        }

        @Override
        public void handleFailure(int status, HttpResponse response) {
          if (skewHandler != null) {
            skewHandler.updateSkew(response, now());
          }

          if (status != 401) {
            delegate.handleError(new FxAccountLoginException(new HTTPFailureException(new SyncStorageResponse(response))));
            return;
          }
          // We just got denied due to our sessionToken.  Invalidate it.
          fxAccount.setSessionToken(null);
          delegate.handleError(new FxAccountLoginBadPasswordException("Auth server rejected session token while fetching status."));
        }

        @Override
        public void handleSuccess(StatusResponse result) {
          // We're not yet verified.  We can't go forward yet.
          if (!result.verified) {
            delegate.handleError(new FxAccountLoginAccountNotVerifiedException("Account is not yet verified."));
            return;
          }
          // We've transitioned to verified state.  Make a note of it, and continue past go.
          fxAccount.setVerified();
          delegate.handleStageSuccess();
        }
      });
    }
  }

  public static int[] DUMMY = null;

  public class EnsureKeyFetchTokenStage implements LoginStage {
    @Override
    public void execute(final LoginStageDelegate delegate) throws Exception {
      byte[] emailUTF8 = fxAccount.getEmailUTF8();
      if (emailUTF8 == null) {
        throw new IllegalStateException("emailUTF8 must not be null");
      }
      byte[] quickStretchedPW = fxAccount.getQuickStretchedPW();
      if (quickStretchedPW == null) {
        throw new IllegalStateException("quickStretchedPW must not be null");
      }

      boolean verified = fxAccount.isVerified();
      if (!verified) {
        throw new IllegalStateException("must be verified");
      }

      // We might already have a valid keyFetchToken. If so, try it. If it's not
      // valid, we'll invalidate it in EnsureKeysStage.
      if (fxAccount.getKeyFetchToken() != null) {
        Logger.info(LOG_TAG, "Using existing keyFetchToken.");
        delegate.handleStageSuccess();
        return;
      }

      delegate.client.loginAndGetKeys(emailUTF8, quickStretchedPW, new RequestDelegate<FxAccountClient20.LoginResponse>() {
        @Override
        public void handleError(Exception e) {
          delegate.handleError(new FxAccountLoginException(e));
        }

        @Override
        public void handleFailure(int status, HttpResponse response) {
          if (skewHandler != null) {
            skewHandler.updateSkew(response, now());
          }

          if (status != 401) {
            delegate.handleError(new FxAccountLoginException(new HTTPFailureException(new SyncStorageResponse(response))));
            return;
          }
          // We just got denied for a keyFetchToken. That's a problem with
          // our email or password. Only thing to do is mark the account
          // invalid and ask for user intervention.
          fxAccount.setInvalid();
          delegate.handleError(new FxAccountLoginBadPasswordException("Auth server rejected email/password while fetching keyFetchToken."));
        }

        @Override
        public void handleSuccess(LoginResponse result) {
          fxAccount.setKeyFetchToken(result.keyFetchToken);
          if (FxAccountConstants.LOG_PERSONAL_INFORMATION) {
            FxAccountConstants.pii(LOG_TAG, "Fetched keyFetchToken: " + Utils.byte2Hex(result.keyFetchToken));
          }
          delegate.handleStageSuccess();
        }
      });
    }
  }

  /**
   * Now we have a verified account, we can make sure that our local keys are
   * consistent with the account's keys.
   */
  public class EnsureKeysStage implements LoginStage {
    @Override
    public void execute(final LoginStageDelegate delegate) throws Exception {
      byte[] keyFetchToken = fxAccount.getKeyFetchToken();
      if (keyFetchToken == null) {
        throw new IllegalStateException("keyFetchToken must not be null");
      }

      // Make sure we don't use a keyFetchToken twice. This conveniently
      // invalidates any invalid keyFetchToken we might try, too.
      fxAccount.setKeyFetchToken(null);

      delegate.client.keys(keyFetchToken, new RequestDelegate<FxAccountClient10.TwoKeys>() {
        @Override
        public void handleError(Exception e) {
          delegate.handleError(new FxAccountLoginException(e));
        }

        @Override
        public void handleFailure(int status, HttpResponse response) {
          if (skewHandler != null) {
            skewHandler.updateSkew(response, now());
          }

          if (status != 401) {
            delegate.handleError(new FxAccountLoginException(new HTTPFailureException(new SyncStorageResponse(response))));
            return;
          }
          delegate.handleError(new FxAccountLoginBadPasswordException("Auth server rejected key token while fetching keys."));
        }

        @Override
        public void handleSuccess(TwoKeys result) {
          fxAccount.setKa(result.kA);
          fxAccount.setWrappedKb(result.wrapkB);
          if (FxAccountConstants.LOG_PERSONAL_INFORMATION) {
            FxAccountConstants.pii(LOG_TAG, "Fetched kA: " + Utils.byte2Hex(result.kA));
            FxAccountConstants.pii(LOG_TAG, "And wrapkB: " + Utils.byte2Hex(result.wrapkB));
            FxAccountConstants.pii(LOG_TAG, "Giving kB : " + Utils.byte2Hex(fxAccount.getKb()));
          }
          delegate.handleStageSuccess();
        }
      });
    }
  }

  public class EnsureCertificateStage implements LoginStage {
    @Override
    public void execute(final LoginStageDelegate delegate) throws Exception{
      byte[] sessionToken = fxAccount.getSessionToken();
      if (sessionToken == null) {
        throw new IllegalStateException("keyPair must not be null");
      }
      BrowserIDKeyPair keyPair = fxAccount.getAssertionKeyPair();
      if (keyPair == null) {
        // If we can't fetch a keypair, we probably have some crypto
        // configuration error on device, which we are never going to recover
        // from. Mark the account invalid.
        fxAccount.setInvalid();
        throw new IllegalStateException("keyPair must not be null");
      }
      final VerifyingPublicKey publicKey = keyPair.getPublic();

      delegate.client.sign(sessionToken, publicKey.toJSONObject(), getCertificateDurationInMilliseconds(), new RequestDelegate<String>() {
        @Override
        public void handleError(Exception e) {
          delegate.handleError(new FxAccountLoginException(e));
        }

        @Override
        public void handleFailure(int status, HttpResponse response) {
          if (skewHandler != null) {
            skewHandler.updateSkew(response, now());
          }

          if (status != 401) {
            delegate.handleError(new FxAccountLoginException(new HTTPFailureException(new SyncStorageResponse(response))));
            return;
          }
          // Our sessionToken was just rejected; we should get a new
          // sessionToken. TODO: Make sure the exception below is fine
          // enough grained.
          // Since this is the place we'll see the majority of lifecylcle
          // auth problems, we should be much more aggressive bumping the
          // state machine out of this state when we don't get success.
          fxAccount.setSessionToken(null);
          delegate.handleError(new FxAccountLoginBadPasswordException("Auth server rejected session token while fetching status."));
        }

        @Override
        public void handleSuccess(String certificate) {
          fxAccount.setCertificate(certificate);
          if (FxAccountConstants.LOG_PERSONAL_INFORMATION) {
            FxAccountConstants.pii(LOG_TAG, "Fetched certificate: " + certificate);
            JSONWebTokenUtils.dumpCertificate(certificate);
          }
          delegate.handleStageSuccess();
        }
      });
    }
  }

  public class EnsureAssertionStage implements LoginStage {
    @Override
    public void execute(final LoginStageDelegate delegate) throws Exception {
      BrowserIDKeyPair keyPair = fxAccount.getAssertionKeyPair();
      if (keyPair == null) {
        throw new IllegalStateException("keyPair must not be null");
      }
      String certificate = fxAccount.getCertificate();
      if (certificate == null) {
        throw new IllegalStateException("certificate must not be null");
      }
      String assertion;
      try {
        long now = System.currentTimeMillis();
        assertion = JSONWebTokenUtils.createAssertion(keyPair.getPrivate(), certificate, delegate.audience,
            JSONWebTokenUtils.DEFAULT_ASSERTION_ISSUER, now, getAssertionDurationInMilliseconds());
      } catch (Exception e) {
        // If we can't sign an assertion, we probably have some crypto
        // configuration error on device, which we are never going to recover
        // from. Mark the account invalid before raising the exception.
        fxAccount.setInvalid();
        throw e;
      }
      fxAccount.setAssertion(assertion);
      if (FxAccountConstants.LOG_PERSONAL_INFORMATION) {
        FxAccountConstants.pii(LOG_TAG, "Generated assertion: " + assertion);
        JSONWebTokenUtils.dumpAssertion(assertion);
      }
      delegate.handleStageSuccess();
    }
  }

  public class SuccessStage implements LoginStage {
    @Override
    public void execute(final LoginStageDelegate delegate) throws Exception {
      String assertion = fxAccount.getAssertion();
      if (assertion == null) {
        throw new IllegalStateException("assertion must not be null");
      }
      delegate.handleLoginSuccess(assertion);
    }
  }

  public class FailStage implements LoginStage {
    @Override
    public void execute(final LoginStageDelegate delegate) {
      AccountState finalState = getAccountState(fxAccount);
      Logger.info(LOG_TAG, "Failed to login account; final state is " + finalState + ".");
      delegate.handleError(new FxAccountLoginException("Failed to login."));
    }
  }
}
