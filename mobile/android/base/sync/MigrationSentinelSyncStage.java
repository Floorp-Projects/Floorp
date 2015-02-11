/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import java.net.URISyntaxException;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.common.telemetry.TelemetryWrapper;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.MigratedFromSync11;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.SyncStorageRecordRequest;
import org.mozilla.gecko.sync.net.SyncStorageRequestDelegate;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.mozilla.gecko.sync.stage.AbstractNonRepositorySyncStage;
import org.mozilla.gecko.sync.stage.NoSuchStageException;
import org.mozilla.gecko.sync.telemetry.TelemetryContract;

/**
 * The purpose of this class is to talk to a Sync 1.1 server and check
 * for a Firefox Accounts migration sentinel.
 *
 * If one is found, a Firefox Account is created, and the existing
 * Firefox Sync account disabled (or deleted).
 */
public class MigrationSentinelSyncStage extends AbstractNonRepositorySyncStage {
  private static final String LOG_TAG = MigrationSentinelSyncStage.class.getSimpleName();

  private static final String META_COLLECTION = "meta";

  public static class MigrationChecker {
    private static final String META_FXA_CREDENTIALS = "/meta/fxa_credentials";

    private static final String CREDENTIALS_KEY_ID = "id";
    private static final String CREDENTIALS_KEY_SENTINEL = "sentinel";

    private static final String RECORD_ID = "fxa_credentials";

    private static final String SENTINEL_KEY_EMAIL = "email";
    private static final String SENTINEL_KEY_UID = "uid";
    private static final String SENTINEL_KEY_VERIFIED = "verified";
    private static final String SENTINEL_KEY_PASSWORD = "password";
    private static final String SENTINEL_KEY_PREFS = "prefs";

    private static final String PREFS_KEY_AUTH_SERVER_ENDPOINT = "identity.fxaccounts.auth.uri";
    private static final String PREFS_KEY_TOKEN_SERVER_ENDPOINT = "services.sync.tokenServerURI";

    private final GlobalSession session;
    private long fetchTimestamp = -1L;

    MigrationChecker(GlobalSession session) {
      this.session = session;
    }

    private void setTimestamp(long timestamp) {
      if (timestamp == -1L) {
        this.fetchTimestamp = System.currentTimeMillis();
      } else {
        this.fetchTimestamp = timestamp;
      }
    }

    private boolean migrate(String email,
                            String uid,
                            boolean verified,
                            String password,
                            String authServerURI,
                            String tokenServerURI) throws Exception {
      final String profile = "default";

      final State state = new MigratedFromSync11(email, uid, verified, password);

      final AndroidFxAccount fxAccount = AndroidFxAccount.addAndroidAccount(session.context,
          email,
          profile,
          authServerURI,
          tokenServerURI,
          state,
          AndroidFxAccount.DEFAULT_AUTHORITIES_TO_SYNC_AUTOMATICALLY_MAP);

      if (fxAccount == null) {
        Logger.warn(LOG_TAG, "Could not add Android account named like: " + Utils.obfuscateEmail(email));
        return false;
      } else {
        return true;
      }
    }

    private void migrate(CryptoRecord record) throws Exception {
      // If something goes wrong, we don't want to try this again.
      session.config.persistLastMigrationSentinelCheckTimestamp(fetchTimestamp);

      record.keyBundle = session.config.syncKeyBundle;
      record.decrypt();

      Logger.info(LOG_TAG, "Migration sentinel could be decrypted; extracting credentials.");

      final ExtendedJSONObject payload = record.payload;
      if (!RECORD_ID.equals(payload.getString(CREDENTIALS_KEY_ID))) {
        onError(null, "Record payload 'id' was not '" + RECORD_ID + "'; ignoring.");
        return;
      }

      final ExtendedJSONObject sentinel = payload.getObject(CREDENTIALS_KEY_SENTINEL);
      if (sentinel == null) {
        onError(null, "Record payload did not contain object with key '" + CREDENTIALS_KEY_SENTINEL + "'; ignoring.");
        return;
      }

      sentinel.throwIfFieldsMissingOrMisTyped(new String[] { SENTINEL_KEY_EMAIL, SENTINEL_KEY_UID } , String.class);
      sentinel.throwIfFieldsMissingOrMisTyped(new String[] { SENTINEL_KEY_VERIFIED } , Boolean.class);
      final String email = sentinel.getString(SENTINEL_KEY_EMAIL);
      final String uid = sentinel.getString(SENTINEL_KEY_UID);
      final boolean verified = sentinel.getBoolean(SENTINEL_KEY_VERIFIED);
      // We never expect this to be set, but we're being forward thinking here.
      final String password = sentinel.getString(SENTINEL_KEY_PASSWORD);

      final ExtendedJSONObject prefs = payload.getObject(SENTINEL_KEY_PREFS);
      String authServerURI = null;
      String tokenServerURI = null;
      if (prefs != null) {
        authServerURI = prefs.getString(PREFS_KEY_AUTH_SERVER_ENDPOINT);
        tokenServerURI = prefs.getString(PREFS_KEY_TOKEN_SERVER_ENDPOINT);
      }
      if (authServerURI == null) {
        authServerURI = FxAccountConstants.DEFAULT_AUTH_SERVER_ENDPOINT;
      }
      if (tokenServerURI == null) {
        tokenServerURI = FxAccountConstants.DEFAULT_TOKEN_SERVER_ENDPOINT;
      }

      Logger.info(LOG_TAG, "Migration sentinel contained valid credentials.");
      if (FxAccountUtils.LOG_PERSONAL_INFORMATION) {
        FxAccountUtils.pii(LOG_TAG, "payload: " + payload.toJSONString());
        FxAccountUtils.pii(LOG_TAG, "email: " + email);
        FxAccountUtils.pii(LOG_TAG, "uid: " + uid);
        FxAccountUtils.pii(LOG_TAG, "verified: " + verified);
        FxAccountUtils.pii(LOG_TAG, "password: " + password);
        FxAccountUtils.pii(LOG_TAG, "authServer: " + authServerURI);
        FxAccountUtils.pii(LOG_TAG, "tokenServerURI: " + tokenServerURI);
      }

      if (migrate(email, uid, verified, password, authServerURI, tokenServerURI)) {
        session.callback.informMigrated(session);
        onMigrated();
      } else {
        onError(null, "Could not add Android account.");
      }
    }

    private void onMigrated() {
      Logger.info(LOG_TAG, "Account migrated!");
      TelemetryWrapper.addToHistogram(TelemetryContract.SYNC11_MIGRATIONS_SUCCEEDED, 1);
      session.config.persistLastMigrationSentinelCheckTimestamp(fetchTimestamp);
      session.abort(null, "Account migrated.");
    }

    private void onCompletedUneventfully() {
      session.config.persistLastMigrationSentinelCheckTimestamp(fetchTimestamp);
      session.advance();
    }

    private void onError(Exception ex, String reason) {
      Logger.info(LOG_TAG, "Could not migrate: " + reason, ex);
      TelemetryWrapper.addToHistogram(TelemetryContract.SYNC11_MIGRATIONS_FAILED, 1);
      session.abort(ex, reason);
    }

    public void check() {
      final String url = session.config.storageURL() + META_FXA_CREDENTIALS;
      try {
        final SyncStorageRecordRequest request = new SyncStorageRecordRequest(url);
        request.delegate = new SyncStorageRequestDelegate() {

          @Override
          public String ifUnmodifiedSince() {
            return null;
          }

          @Override
          public void handleRequestSuccess(SyncStorageResponse response) {
            Logger.info(LOG_TAG, "Found " + META_FXA_CREDENTIALS + " record; attempting migration.");
            setTimestamp(response.normalizedWeaveTimestamp());

            TelemetryWrapper.addToHistogram(TelemetryContract.SYNC11_MIGRATION_SENTINELS_SEEN, 1);

            try {
              final ExtendedJSONObject body = response.jsonObjectBody();
              final CryptoRecord cryptoRecord = CryptoRecord.fromJSONRecord(body);
              migrate(cryptoRecord);
            } catch (Exception e) {
              onError(e, "Unable to parse credential response.");
            }
          }

          @Override
          public void handleRequestFailure(SyncStorageResponse response) {
            setTimestamp(response.normalizedWeaveTimestamp());
            if (response.getStatusCode() == 404) {
              // Great!
              onCompletedUneventfully();
              return;
            }
            onError(null, "Failed to fetch.");
          }

          @Override
          public void handleRequestError(Exception ex) {
            onError(ex, "Failed to fetch.");
          }

          @Override
          public AuthHeaderProvider getAuthHeaderProvider() {
            return session.getAuthHeaderProvider();
          }
        };

        request.get();
      } catch (URISyntaxException e) {
        onError(e, "Malformed credentials URI.");
      }
    }
  }

  public MigrationSentinelSyncStage() {
  }

  @Override
  protected void execute() throws NoSuchStageException {
    final InfoCollections infoCollections = session.config.infoCollections;
    if (infoCollections == null) {
      session.abort(null, "No info/collections set in MigrationSentinelSyncStage.");
      return;
    }

    final long lastModified = session.config.getLastMigrationSentinelCheckTimestamp();
    if (!infoCollections.updateNeeded(META_COLLECTION, lastModified)) {
      Logger.info(LOG_TAG, "No need to check fresh meta/fxa_credentials.");
      session.advance();
      return;
    }

    // Let's try a fetch.
    Logger.info(LOG_TAG, "Fetching meta/fxa_credentials to check for migration sentinel.");
    new MigrationChecker(session).check();
  }
}
