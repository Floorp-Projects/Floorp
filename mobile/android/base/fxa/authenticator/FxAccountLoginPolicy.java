/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.authenticator;

import java.security.GeneralSecurityException;
import java.util.LinkedList;
import java.util.concurrent.Executor;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountClient10;
import org.mozilla.gecko.background.fxa.FxAccountClient10.RequestDelegate;
import org.mozilla.gecko.background.fxa.FxAccountClient10.StatusResponse;
import org.mozilla.gecko.background.fxa.FxAccountClient10.TwoKeys;
import org.mozilla.gecko.background.fxa.FxAccountClient20;
import org.mozilla.gecko.browserid.BrowserIDKeyPair;
import org.mozilla.gecko.browserid.JSONWebTokenUtils;
import org.mozilla.gecko.browserid.SigningPrivateKey;
import org.mozilla.gecko.browserid.VerifyingPublicKey;
import org.mozilla.gecko.fxa.authenticator.FxAccountLoginException.FxAccountLoginAccountNotVerifiedException;
import org.mozilla.gecko.fxa.authenticator.FxAccountLoginException.FxAccountLoginBadPasswordException;
import org.mozilla.gecko.sync.HTTPFailureException;
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

  protected void invokeHandleHardFailure(final FxAccountLoginDelegate delegate, final FxAccountLoginException e) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        delegate.handleError(e);
      }
    });
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
    final LinkedList<LoginStage> stages = new LinkedList<LoginStage>();
    stages.add(new CheckPreconditionsLoginStage());
    stages.add(new CheckVerifiedLoginStage());
    stages.add(new EnsureDerivedKeysLoginStage());
    stages.add(new FetchCertificateLoginStage());

    advance(audience, stages, delegate);
  }

  protected interface LoginStageDelegate {
    public String getAssertionAudience();
    public void handleError(FxAccountLoginException e);
    public void handleStageSuccess();
    public void handleLoginSuccess(String assertion);
  }

  protected interface LoginStage {
    public void execute(LoginStageDelegate delegate);
  }

  /**
   * Pop the next stage off <code>stages</code> and execute it.
   * <p>
   * This trades stack efficiency for implementation simplicity.
   *
   * @param delegate
   * @param stages
   */
  protected void advance(final String audience, final LinkedList<LoginStage> stages, final FxAccountLoginDelegate delegate) {
    LoginStage stage = stages.poll();
    if (stage == null) {
      // No more stages.  But we haven't seen an assertion. Failure!
      Logger.info(LOG_TAG, "No more stages: login failed?");
      invokeHandleHardFailure(delegate, new FxAccountLoginException("No more stages, but no assertion: login failed?"));
      return;
    }

    stage.execute(new LoginStageDelegate() {
      @Override
      public void handleStageSuccess() {
        Logger.info(LOG_TAG, "Stage succeeded.");
        advance(audience, stages, delegate);
      }

      @Override
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

      @Override
      public void handleError(FxAccountLoginException e) {
        invokeHandleHardFailure(delegate, e);
      }

      @Override
      public String getAssertionAudience() {
        return audience;
      }
    });
  }

  /**
   * Verify we have a valid server URI, session token, etc. If not, we have to
   * prompt for credentials.
   */
  public class CheckPreconditionsLoginStage implements LoginStage {
    @Override
    public void execute(final LoginStageDelegate delegate) {
      final String audience = delegate.getAssertionAudience();
      if (audience == null) {
        delegate.handleError(new FxAccountLoginException("Account has no audience."));
        return;
      }

      String serverURI = fxAccount.getServerURI();
      if (serverURI == null) {
        delegate.handleError(new FxAccountLoginException("Account has no server URI."));
        return;
      }

      byte[] sessionToken = fxAccount.getSessionToken();
      if (sessionToken == null) {
        delegate.handleError(new FxAccountLoginBadPasswordException("Account has no session token."));
        return;
      }

      delegate.handleStageSuccess();
    }
  }

  /**
   * Now that we have a server to talk to and a session token, we can use them
   * to check that the account is verified.
   */
  public class CheckVerifiedLoginStage implements LoginStage {
    @Override
    public void execute(final LoginStageDelegate delegate) {
      if (fxAccount.isVerified()) {
        Logger.info(LOG_TAG, "Account is already marked verified. Skipping remote status check.");
        delegate.handleStageSuccess();
        return;
      }

      String serverURI = fxAccount.getServerURI();
      byte[] sessionToken = fxAccount.getSessionToken();
      final FxAccountClient20 client = new FxAccountClient20(serverURI, executor);

      client.status(sessionToken, new RequestDelegate<StatusResponse>() {
        @Override
        public void handleError(Exception e) {
          delegate.handleError(new FxAccountLoginException(e));
        }

        @Override
        public void handleFailure(int status, HttpResponse response) {
          if (status != 401) {
            delegate.handleError(new FxAccountLoginException(new HTTPFailureException(new SyncStorageResponse(response))));
            return;
          }
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

  /**
   * Now we have a verified account, we can make sure that our local keys are
   * consistent with the account's keys.
   */
  public class EnsureDerivedKeysLoginStage implements LoginStage {
    @Override
    public void execute(final LoginStageDelegate delegate) {
      byte[] kA = fxAccount.getKa();
      byte[] kB = fxAccount.getKb();
      if (kA != null && kB != null) {
        Logger.info(LOG_TAG, "Account already has kA and kB. Skipping key derivation stage.");
        delegate.handleStageSuccess();
        return;
      }

      byte[] keyFetchToken = fxAccount.getKeyFetchToken();
      if (keyFetchToken == null) {
        // XXX this might mean something else?
        delegate.handleError(new FxAccountLoginBadPasswordException("Account has no key fetch token."));
        return;
      }

      String serverURI = fxAccount.getServerURI();
      final FxAccountClient20 client = new FxAccountClient20(serverURI, executor);
      client.keys(keyFetchToken, new RequestDelegate<FxAccountClient10.TwoKeys>() {
        @Override
        public void handleError(Exception e) {
          delegate.handleError(new FxAccountLoginException(e));
        }

        @Override
        public void handleFailure(int status, HttpResponse response) {
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
          delegate.handleStageSuccess();
        }
      });
    }
  }

  public class FetchCertificateLoginStage implements LoginStage {
    @Override
    public void execute(final LoginStageDelegate delegate) {
      BrowserIDKeyPair keyPair;
      try {
        keyPair = fxAccount.getAssertionKeyPair();
        if (keyPair == null) {
          Logger.info(LOG_TAG, "Account has no key pair.");
          delegate.handleError(new FxAccountLoginException("Account has no key pair."));
          return;
        }
      } catch (GeneralSecurityException e) {
        delegate.handleError(new FxAccountLoginException(e));
        return;
      }

      final SigningPrivateKey privateKey = keyPair.getPrivate();
      final VerifyingPublicKey publicKey = keyPair.getPublic();

      byte[] sessionToken = fxAccount.getSessionToken();
      String serverURI = fxAccount.getServerURI();
      final FxAccountClient20 client = new FxAccountClient20(serverURI, executor);

      // TODO Make this duration configurable (that is, part of the policy).
      long certificateDurationInMilliseconds = JSONWebTokenUtils.DEFAULT_CERTIFICATE_DURATION_IN_MILLISECONDS;

      client.sign(sessionToken, publicKey.toJSONObject(), certificateDurationInMilliseconds, new RequestDelegate<String>() {
        @Override
        public void handleError(Exception e) {
          delegate.handleError(new FxAccountLoginException(e));
        }

        @Override
        public void handleFailure(int status, HttpResponse response) {
          if (status != 401) {
            delegate.handleError(new FxAccountLoginException(new HTTPFailureException(new SyncStorageResponse(response))));
            return;
          }
          delegate.handleError(new FxAccountLoginBadPasswordException("Auth server rejected session token while fetching status."));
        }

        @Override
        public void handleSuccess(String certificate) {
          try {
            String assertion = JSONWebTokenUtils.createAssertion(privateKey, certificate, delegate.getAssertionAudience());
            if (Logger.LOG_PERSONAL_INFORMATION) {
              Logger.pii(LOG_TAG, "Generated assertion " + assertion);
              JSONWebTokenUtils.dumpAssertion(assertion);
            }
            delegate.handleLoginSuccess(assertion);
          } catch (Exception e) {
            delegate.handleError(new FxAccountLoginException(e));
            return;
          }
        }
      });
    }
  }
}
