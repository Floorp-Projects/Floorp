/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.HashMap;
import java.util.Map;

import org.json.simple.parser.ParseException;
import org.mozilla.gecko.sync.crypto.CryptoException;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.delegates.ClientsDataDelegate;
import org.mozilla.gecko.sync.delegates.FreshStartDelegate;
import org.mozilla.gecko.sync.delegates.GlobalSessionCallback;
import org.mozilla.gecko.sync.delegates.InfoCollectionsDelegate;
import org.mozilla.gecko.sync.delegates.KeyUploadDelegate;
import org.mozilla.gecko.sync.delegates.MetaGlobalDelegate;
import org.mozilla.gecko.sync.delegates.WipeServerDelegate;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.SyncStorageRecordRequest;
import org.mozilla.gecko.sync.net.SyncStorageRequest;
import org.mozilla.gecko.sync.net.SyncStorageRequestDelegate;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.mozilla.gecko.sync.stage.AndroidBrowserBookmarksServerSyncStage;
import org.mozilla.gecko.sync.stage.AndroidBrowserHistoryServerSyncStage;
import org.mozilla.gecko.sync.stage.CheckPreconditionsStage;
import org.mozilla.gecko.sync.stage.CompletedStage;
import org.mozilla.gecko.sync.stage.EnsureClusterURLStage;
import org.mozilla.gecko.sync.stage.EnsureKeysStage;
import org.mozilla.gecko.sync.stage.FennecTabsServerSyncStage;
import org.mozilla.gecko.sync.stage.FetchInfoCollectionsStage;
import org.mozilla.gecko.sync.stage.FetchMetaGlobalStage;
import org.mozilla.gecko.sync.stage.GlobalSyncStage;
import org.mozilla.gecko.sync.stage.GlobalSyncStage.Stage;
import org.mozilla.gecko.sync.stage.NoSuchStageException;
import org.mozilla.gecko.sync.stage.SyncClientsEngineStage;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.util.Log;
import ch.boye.httpclientandroidlib.HttpResponse;

public class GlobalSession implements CredentialsSource, PrefsSource {
  private static final String LOG_TAG = "GlobalSession";

  public static final String API_VERSION   = "1.1";
  public static final long STORAGE_VERSION = 5;

  private static final String HEADER_RETRY_AFTER     = "retry-after";
  private static final String HEADER_X_WEAVE_BACKOFF = "x-weave-backoff";

  public SyncConfiguration config = null;

  protected Map<Stage, GlobalSyncStage> stages;
  public Stage currentState = Stage.idle;

  public final GlobalSessionCallback callback;
  private Context context;
  private ClientsDataDelegate clientsDelegate;

  /*
   * Key accessors.
   */
  public void setCollectionKeys(CollectionKeys k) {
    config.setCollectionKeys(k);
  }
  @Override
  public CollectionKeys getCollectionKeys() {
    return config.collectionKeys;
  }
  @Override
  public KeyBundle keyForCollection(String collection) throws NoCollectionKeysSetException {
    return config.keyForCollection(collection);
  }

  /*
   * Config passthrough for convenience.
   */
  @Override
  public String credentials() {
    return config.credentials();
  }

  public URI wboURI(String collection, String id) throws URISyntaxException {
    return config.wboURI(collection, id);
  }

  /*
   * Validators.
   */
  private static boolean isInvalidString(String s) {
    return s == null ||
           s.trim().length() == 0;
  }

  private static boolean anyInvalidStrings(String s, String...strings) {
    if (isInvalidString(s)) {
      return true;
    }
    for (String str : strings) {
      if (isInvalidString(str)) {
        return true;
      }
    }
    return false;
  }

  public GlobalSession(String userAPI,
                       String serverURL,
                       String username,
                       String password,
                       String prefsPath,
                       KeyBundle syncKeyBundle,
                       GlobalSessionCallback callback,
                       Context context,
                       Bundle extras,
                       ClientsDataDelegate clientsDelegate)
                           throws SyncConfigurationException, IllegalArgumentException, IOException, ParseException, NonObjectJSONException {
    if (callback == null) {
      throw new IllegalArgumentException("Must provide a callback to GlobalSession constructor.");
    }

    if (anyInvalidStrings(username, password)) {
      throw new SyncConfigurationException();
    }

    Logger.info(LOG_TAG, "GlobalSession initialized with bundle " + extras);
    URI serverURI;
    try {
      serverURI = (serverURL == null) ? null : new URI(serverURL);
    } catch (URISyntaxException e) {
      throw new SyncConfigurationException();
    }

    if (syncKeyBundle == null ||
        syncKeyBundle.getEncryptionKey() == null ||
        syncKeyBundle.getHMACKey() == null) {
      throw new SyncConfigurationException();
    }

    this.callback        = callback;
    this.context         = context;
    this.clientsDelegate = clientsDelegate;

    config = new SyncConfiguration(prefsPath, this);
    config.userAPI       = userAPI;
    config.serverURL     = serverURI;
    config.username      = username;
    config.password      = password;
    config.syncKeyBundle = syncKeyBundle;

    prepareStages();
  }

  protected void prepareStages() {
    stages = new HashMap<Stage, GlobalSyncStage>();
    stages.put(Stage.checkPreconditions,      new CheckPreconditionsStage());
    stages.put(Stage.ensureClusterURL,        new EnsureClusterURLStage());
    stages.put(Stage.fetchInfoCollections,    new FetchInfoCollectionsStage());
    stages.put(Stage.fetchMetaGlobal,         new FetchMetaGlobalStage());
    stages.put(Stage.ensureKeysStage,         new EnsureKeysStage());
    stages.put(Stage.syncClientsEngine,       new SyncClientsEngineStage());

    // TODO: more stages.
    stages.put(Stage.syncTabs,                new FennecTabsServerSyncStage());
    stages.put(Stage.syncBookmarks,           new AndroidBrowserBookmarksServerSyncStage());
    stages.put(Stage.syncHistory,             new AndroidBrowserHistoryServerSyncStage());
    stages.put(Stage.completed,               new CompletedStage());
  }

  protected GlobalSyncStage getStageByName(Stage next) throws NoSuchStageException {
    GlobalSyncStage stage = stages.get(next);
    if (stage == null) {
      throw new NoSuchStageException(next);
    }
    return stage;
  }

  /**
   * Advance and loop around the stages of a sync.
   * @param current
   * @return
   *        The next stage to execute.
   */
  public static Stage nextStage(Stage current) {
    int index = current.ordinal() + 1;
    int max   = Stage.completed.ordinal() + 1;
    return Stage.values()[index % max];
  }

  /**
   * Move to the next stage in the syncing process.
   */
  public void advance() {
    this.callback.handleStageCompleted(this.currentState, this);
    Stage next = nextStage(this.currentState);
    GlobalSyncStage nextStage;
    try {
      nextStage = this.getStageByName(next);
    } catch (NoSuchStageException e) {
      this.abort(e, "No such stage " + next);
      return;
    }
    this.currentState = next;
    Logger.info(LOG_TAG, "Running next stage " + next + " (" + nextStage + ")...");
    try {
      nextStage.execute(this);
    } catch (Exception ex) {
      Logger.warn(LOG_TAG, "Caught exception " + ex + " running stage " + next);
      this.abort(ex, "Uncaught exception in stage.");
    }
  }

  private String getSyncID() {
    return config.syncID;
  }

  private String generateSyncID() {
    config.syncID = Utils.generateGuid();
    return config.syncID;
  }

  /*
   * PrefsSource methods.
   */
  @Override
  public SharedPreferences getPrefs(String name, int mode) {
    return this.getContext().getSharedPreferences(name, mode);
  }

  @Override
  public Context getContext() {
    return this.context;
  }

  /**
   * Begin a sync.
   *
   * The caller is responsible for:
   *
   * * Verifying that any backoffs/minimum next sync are respected
   * * Ensuring that the device is online
   * * Ensuring that dependencies are ready
   *
   * @throws AlreadySyncingException
   *
   */
  public void start() throws AlreadySyncingException {
    if (this.currentState != GlobalSyncStage.Stage.idle) {
      throw new AlreadySyncingException(this.currentState);
    }
    this.advance();
  }

  /**
   * Stop this sync and start again.
   * @throws AlreadySyncingException
   */
  protected void restart() throws AlreadySyncingException {
    this.currentState = GlobalSyncStage.Stage.idle;
    if (callback.shouldBackOff()) {
      this.callback.handleAborted(this, "Told to back off.");
      return;
    }
    this.start();
  }

  public void completeSync() {
    this.currentState = GlobalSyncStage.Stage.idle;
    this.callback.handleSuccess(this);
  }

  public void abort(Exception e, String reason) {
    Logger.warn(LOG_TAG, "Aborting sync: " + reason, e);
    this.callback.handleError(this, e);
  }

  public void handleHTTPError(SyncStorageResponse response, String reason) {
    // TODO: handling of 50x (backoff), 401 (node reassignment or auth error).
    // Fall back to aborting.
    Logger.warn(LOG_TAG, "Aborting sync due to HTTP " + response.getStatusCode());
    this.interpretHTTPFailure(response.httpResponse());
    this.abort(new HTTPFailureException(response), reason);
  }

  /**
   * Perform appropriate backoff etc. extraction.
   */
  public void interpretHTTPFailure(HttpResponse response) {
    // TODO: handle permanent rejection.
    long retryAfter = 0;
    long weaveBackoff = 0;
    if (response.containsHeader(HEADER_RETRY_AFTER)) {
      // Handles non-decimals just fine.
      String headerValue = response.getFirstHeader(HEADER_RETRY_AFTER).getValue();
      retryAfter = Utils.decimalSecondsToMilliseconds(headerValue);
    }
    if (response.containsHeader(HEADER_X_WEAVE_BACKOFF)) {
      // Handles non-decimals just fine.
      String headerValue = response.getFirstHeader(HEADER_X_WEAVE_BACKOFF).getValue();
      weaveBackoff = Utils.decimalSecondsToMilliseconds(headerValue);
    }
    long backoff = Math.max(retryAfter, weaveBackoff);
    if (backoff > 0) {
      callback.requestBackoff(backoff);
    }

    if (response.getStatusLine() != null && response.getStatusLine().getStatusCode() == 401) {
      /*
       * Alert our callback we have a 401 on a cluster URL. This GlobalSession
       * will fail, but the next one will fetch a new cluster URL and will
       * distinguish between "node reassignment" and "user password changed".
       */
      callback.informUnauthorizedResponse(this, config.getClusterURL());
    }
  }

  public void fetchMetaGlobal(MetaGlobalDelegate callback) throws URISyntaxException {
    if (this.config.metaGlobal == null) {
      this.config.metaGlobal = new MetaGlobal(config.metaURL(), credentials());
    }
    this.config.metaGlobal.fetch(callback);
  }

  public void fetchInfoCollections(InfoCollectionsDelegate callback) throws URISyntaxException {
    if (this.config.infoCollections == null) {
      this.config.infoCollections = new InfoCollections(config.infoURL(), credentials());
    }
    this.config.infoCollections.fetch(callback);
  }

  public void uploadKeys(CryptoRecord keysRecord,
                         final KeyUploadDelegate keyUploadDelegate) {
    SyncStorageRecordRequest request;
    final GlobalSession self = this;
    try {
      request = new SyncStorageRecordRequest(this.config.keysURI());
    } catch (URISyntaxException e) {
      keyUploadDelegate.onKeyUploadFailed(e);
      return;
    }

    request.delegate = new SyncStorageRequestDelegate() {

      @Override
      public String ifUnmodifiedSince() {
        return null;
      }

      @Override
      public void handleRequestSuccess(SyncStorageResponse response) {
        BaseResource.consumeEntity(response); // We don't need the response at all.
        keyUploadDelegate.onKeysUploaded();
      }

      @Override
      public void handleRequestFailure(SyncStorageResponse response) {
        self.interpretHTTPFailure(response.httpResponse());
        BaseResource.consumeEntity(response); // The exception thrown should not need the body of the response.
        keyUploadDelegate.onKeyUploadFailed(new HTTPFailureException(response));
      }

      @Override
      public void handleRequestError(Exception ex) {
        keyUploadDelegate.onKeyUploadFailed(ex);
      }

      @Override
      public String credentials() {
        return self.credentials();
      }
    };

    keysRecord.setKeyBundle(config.syncKeyBundle);
    try {
      keysRecord.encrypt();
    } catch (UnsupportedEncodingException e) {
      keyUploadDelegate.onKeyUploadFailed(e);
      return;
    } catch (CryptoException e) {
      keyUploadDelegate.onKeyUploadFailed(e);
      return;
    }
    request.put(keysRecord);
  }


  /*
   * meta/global callbacks.
   */
  public void processMetaGlobal(MetaGlobal global) {
    Long storageVersion = global.getStorageVersion();
    if (storageVersion < STORAGE_VERSION) {
      // Outdated server.
      freshStart();
      return;
    }
    if (storageVersion > STORAGE_VERSION) {
      // Outdated client!
      requiresUpgrade();
      return;
    }
    String remoteSyncID = global.getSyncID();
    if (remoteSyncID == null) {
      // Corrupt meta/global.
      freshStart();
      return;
    }
    String localSyncID = this.getSyncID();
    if (!remoteSyncID.equals(localSyncID)) {
      // Sync ID has changed. Reset timestamps and fetch new keys.
      resetClient();
      if (config.collectionKeys != null) {
        config.collectionKeys.clear();
      }
      config.syncID = remoteSyncID;
      // TODO TODO TODO
    }
    config.persistToPrefs();
    advance();
  }

  public void processMissingMetaGlobal(MetaGlobal global) {
    freshStart();
  }

  /**
   * Do a fresh start then quietly finish the sync, starting another.
   */
  protected void freshStart() {
    final GlobalSession globalSession = this;
    freshStart(this, new FreshStartDelegate() {

      @Override
      public void onFreshStartFailed(Exception e) {
        globalSession.abort(e, "Fresh start failed.");
      }

      @Override
      public void onFreshStart() {
        try {
          globalSession.config.persistToPrefs();
          globalSession.restart();
        } catch (Exception e) {
          Logger.warn(LOG_TAG, "Got exception when restarting sync after freshStart.", e);
          globalSession.abort(e, "Got exception after freshStart.");
        }
      }
    });
  }

  /**
   * Clean the server, aborting the current sync.
   */
  protected void freshStart(final GlobalSession session, final FreshStartDelegate freshStartDelegate) {

    final String newSyncID   = session.generateSyncID();
    final String metaURL     = session.config.metaURL();
    final String credentials = session.credentials();

    wipeServer(session, new WipeServerDelegate() {

      @Override
      public void onWiped(long timestamp) {
        session.resetClient();
        session.config.collectionKeys.clear();      // TODO: make sure we clear our keys timestamp.
        session.config.persistToPrefs();

        MetaGlobal mg = new MetaGlobal(metaURL, credentials);
        mg.setSyncID(newSyncID);
        mg.setStorageVersion(STORAGE_VERSION);

        // It would be good to set the X-If-Unmodified-Since header to `timestamp`
        // for this PUT to ensure at least some level of transactionality.
        // Unfortunately, the servers don't support it after a wipe right now
        // (bug 693893), so we're going to defer this until bug 692700.
        mg.upload(new MetaGlobalDelegate() {

          @Override
          public void handleSuccess(MetaGlobal global, SyncStorageResponse response) {
            session.config.metaGlobal = global;
            Logger.info(LOG_TAG, "New meta/global uploaded with sync ID " + newSyncID);

            // Generate and upload new keys.
            try {
              session.uploadKeys(CollectionKeys.generateCollectionKeys().asCryptoRecord(), new KeyUploadDelegate() {
                @Override
                public void onKeysUploaded() {
                  // Now we can download them.
                  freshStartDelegate.onFreshStart();
                }

                @Override
                public void onKeyUploadFailed(Exception e) {
                  Log.e(LOG_TAG, "Got exception uploading new keys.", e);
                  freshStartDelegate.onFreshStartFailed(e);
                }
              });
            } catch (NoCollectionKeysSetException e) {
              Log.e(LOG_TAG, "Got exception generating new keys.", e);
              freshStartDelegate.onFreshStartFailed(e);
            } catch (CryptoException e) {
              Log.e(LOG_TAG, "Got exception generating new keys.", e);
              freshStartDelegate.onFreshStartFailed(e);
            }
          }

          @Override
          public void handleMissing(MetaGlobal global, SyncStorageResponse response) {
            // Shouldn't happen.
            Logger.warn(LOG_TAG, "Got 'missing' response uploading new meta/global.");
            freshStartDelegate.onFreshStartFailed(new Exception("meta/global missing"));
          }

          @Override
          public void handleFailure(SyncStorageResponse response) {
            // TODO: respect backoffs etc.
            Logger.warn(LOG_TAG, "Got failure " + response.getStatusCode() + " uploading new meta/global.");
            session.interpretHTTPFailure(response.httpResponse());
            freshStartDelegate.onFreshStartFailed(new HTTPFailureException(response));
          }

          @Override
          public void handleError(Exception e) {
            Logger.warn(LOG_TAG, "Got error uploading new meta/global.", e);
            freshStartDelegate.onFreshStartFailed(e);
          }

          @Override
          public MetaGlobalDelegate deferred() {
            final MetaGlobalDelegate self = this;
            return new MetaGlobalDelegate() {

              @Override
              public void handleSuccess(final MetaGlobal global, final SyncStorageResponse response) {
                ThreadPool.run(new Runnable() {
                  @Override
                  public void run() {
                    self.handleSuccess(global, response);
                  }});
              }

              @Override
              public void handleMissing(final MetaGlobal global, final SyncStorageResponse response) {
                ThreadPool.run(new Runnable() {
                  @Override
                  public void run() {
                    self.handleMissing(global, response);
                  }});
              }

              @Override
              public void handleFailure(final SyncStorageResponse response) {
                ThreadPool.run(new Runnable() {
                  @Override
                  public void run() {
                    self.handleFailure(response);
                  }});
              }

              @Override
              public void handleError(final Exception e) {
                ThreadPool.run(new Runnable() {
                  @Override
                  public void run() {
                    self.handleError(e);
                  }});
              }

              @Override
              public MetaGlobalDelegate deferred() {
                return this;
              }
            };
          }
        });
      }

      @Override
      public void onWipeFailed(Exception e) {
        Logger.warn(LOG_TAG, "Wipe failed.");
        freshStartDelegate.onFreshStartFailed(e);
      }
    });

  }

  private void wipeServer(final CredentialsSource credentials, final WipeServerDelegate wipeDelegate) {
    SyncStorageRequest request;
    final GlobalSession self = this;

    try {
      request = new SyncStorageRequest(config.storageURL(false));
    } catch (URISyntaxException ex) {
      Logger.warn(LOG_TAG, "Invalid URI in wipeServer.");
      wipeDelegate.onWipeFailed(ex);
      return;
    }

    request.delegate = new SyncStorageRequestDelegate() {

      @Override
      public String ifUnmodifiedSince() {
        return null;
      }

      @Override
      public void handleRequestSuccess(SyncStorageResponse response) {
        BaseResource.consumeEntity(response);
        wipeDelegate.onWiped(response.normalizedWeaveTimestamp());
      }

      @Override
      public void handleRequestFailure(SyncStorageResponse response) {
        Logger.warn(LOG_TAG, "Got request failure " + response.getStatusCode() + " in wipeServer.");
        // Process HTTP failures here to pick up backoffs, etc.
        self.interpretHTTPFailure(response.httpResponse());
        BaseResource.consumeEntity(response); // The exception thrown should not need the body of the response.
        wipeDelegate.onWipeFailed(new HTTPFailureException(response));
      }

      @Override
      public void handleRequestError(Exception ex) {
        Logger.warn(LOG_TAG, "Got exception in wipeServer.", ex);
        wipeDelegate.onWipeFailed(ex);
      }

      @Override
      public String credentials() {
        return credentials.credentials();
      }
    };
    request.delete();
  }

  /**
   * Reset our state. Clear our sync ID, reset each engine, drop any
   * cached records.
   */
  private void resetClient() {
    // TODO: futz with config?!
    // TODO: engines?!

  }

  /**
   * Suggest that your Sync client needs to be upgraded to work
   * with this server.
   */
  public void requiresUpgrade() {
    Logger.info(LOG_TAG, "Client outdated storage version; requires update.");
    // TODO: notify UI.
  }

  /**
   * If meta/global is missing or malformed, throws a MetaGlobalException.
   * Otherwise, returns true if there is an entry for this engine in the
   * meta/global "engines" object.
   *
   * @param engineName
   * @return
   *        true if the engine with the provided name is present in the
   *        meta/global "engines" object.
   *
   * @throws MetaGlobalException
   */
  public boolean engineIsEnabled(String engineName) throws MetaGlobalException {
    if (this.config.metaGlobal == null) {
      throw new MetaGlobalNotSetException();
    }
    if (this.config.metaGlobal.engines == null) {
      throw new MetaGlobalMissingEnginesException();
    }
    return this.config.metaGlobal.engines.get(engineName) != null;
  }

  public ClientsDataDelegate getClientsDelegate() {
    return this.clientsDelegate;
  }
}
