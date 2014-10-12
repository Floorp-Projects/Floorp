/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.EnumMap;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;
import java.util.concurrent.atomic.AtomicLong;

import org.json.simple.JSONArray;
import org.json.simple.parser.ParseException;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.crypto.CryptoException;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.delegates.BaseGlobalSessionCallback;
import org.mozilla.gecko.sync.delegates.ClientsDataDelegate;
import org.mozilla.gecko.sync.delegates.FreshStartDelegate;
import org.mozilla.gecko.sync.delegates.JSONRecordFetchDelegate;
import org.mozilla.gecko.sync.delegates.KeyUploadDelegate;
import org.mozilla.gecko.sync.delegates.MetaGlobalDelegate;
import org.mozilla.gecko.sync.delegates.NodeAssignmentCallback;
import org.mozilla.gecko.sync.delegates.WipeServerDelegate;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.HttpResponseObserver;
import org.mozilla.gecko.sync.net.SyncResponse;
import org.mozilla.gecko.sync.net.SyncStorageRecordRequest;
import org.mozilla.gecko.sync.net.SyncStorageRequest;
import org.mozilla.gecko.sync.net.SyncStorageRequestDelegate;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.mozilla.gecko.sync.stage.AndroidBrowserBookmarksServerSyncStage;
import org.mozilla.gecko.sync.stage.AndroidBrowserHistoryServerSyncStage;
import org.mozilla.gecko.sync.stage.CheckPreconditionsStage;
import org.mozilla.gecko.sync.stage.CompletedStage;
import org.mozilla.gecko.sync.stage.EnsureClusterURLStage;
import org.mozilla.gecko.sync.stage.EnsureCrypto5KeysStage;
import org.mozilla.gecko.sync.stage.FennecTabsServerSyncStage;
import org.mozilla.gecko.sync.stage.FetchInfoCollectionsStage;
import org.mozilla.gecko.sync.stage.FetchMetaGlobalStage;
import org.mozilla.gecko.sync.stage.FormHistoryServerSyncStage;
import org.mozilla.gecko.sync.stage.GlobalSyncStage;
import org.mozilla.gecko.sync.stage.GlobalSyncStage.Stage;
import org.mozilla.gecko.sync.stage.NoSuchStageException;
import org.mozilla.gecko.sync.stage.PasswordsServerSyncStage;
import org.mozilla.gecko.sync.stage.SyncClientsEngineStage;
import org.mozilla.gecko.sync.stage.UploadMetaGlobalStage;

import android.content.Context;
import ch.boye.httpclientandroidlib.HttpResponse;

public class GlobalSession implements HttpResponseObserver {
  private static final String LOG_TAG = "GlobalSession";

  public static final long STORAGE_VERSION = 5;

  public SyncConfiguration config = null;

  protected Map<Stage, GlobalSyncStage> stages;
  public Stage currentState = Stage.idle;

  public final BaseGlobalSessionCallback callback;
  protected final Context context;
  protected final ClientsDataDelegate clientsDelegate;
  protected final NodeAssignmentCallback nodeAssignmentCallback;

  /**
   * Map from engine name to new settings for an updated meta/global record.
   * Engines to remove will have <code>null</code> EngineSettings.
   */
  public final Map<String, EngineSettings> enginesToUpdate = new HashMap<String, EngineSettings>();

   /*
   * Key accessors.
   */
  public KeyBundle keyBundleForCollection(String collection) throws NoCollectionKeysSetException {
    return config.getCollectionKeys().keyBundleForCollection(collection);
  }

  /*
   * Config passthrough for convenience.
   */
  public AuthHeaderProvider getAuthHeaderProvider() {
    return config.getAuthHeaderProvider();
  }

  public URI wboURI(String collection, String id) throws URISyntaxException {
    return config.wboURI(collection, id);
  }

  public GlobalSession(SyncConfiguration config,
                       BaseGlobalSessionCallback callback,
                       Context context,
                       ClientsDataDelegate clientsDelegate, NodeAssignmentCallback nodeAssignmentCallback)
    throws SyncConfigurationException, IllegalArgumentException, IOException, ParseException, NonObjectJSONException {

    if (callback == null) {
      throw new IllegalArgumentException("Must provide a callback to GlobalSession constructor.");
    }

    this.callback        = callback;
    this.context         = context;
    this.clientsDelegate = clientsDelegate;
    this.nodeAssignmentCallback = nodeAssignmentCallback;

    this.config = config;
    registerCommands();
    prepareStages();

    if (config.stagesToSync == null) {
      Logger.info(LOG_TAG, "No stages to sync specified; defaulting to all valid engine names.");
      config.stagesToSync = Collections.unmodifiableCollection(SyncConfiguration.validEngineNames());
    }

    // TODO: data-driven plan for the sync, referring to prepareStages.
  }

  /**
   * Register commands this global session knows how to process.
   * <p>
   * Re-registering a command overwrites any existing registration.
   */
  protected static void registerCommands() {
    final CommandProcessor processor = CommandProcessor.getProcessor();

    processor.registerCommand("resetEngine", new CommandRunner(1) {
      @Override
      public void executeCommand(final GlobalSession session, List<String> args) {
        HashSet<String> names = new HashSet<String>();
        names.add(args.get(0));
        session.resetStagesByName(names);
      }
    });

    processor.registerCommand("resetAll", new CommandRunner(0) {
      @Override
      public void executeCommand(final GlobalSession session, List<String> args) {
        session.resetAllStages();
      }
    });

    processor.registerCommand("wipeEngine", new CommandRunner(1) {
      @Override
      public void executeCommand(final GlobalSession session, List<String> args) {
        HashSet<String> names = new HashSet<String>();
        names.add(args.get(0));
        session.wipeStagesByName(names);
      }
    });

    processor.registerCommand("wipeAll", new CommandRunner(0) {
      @Override
      public void executeCommand(final GlobalSession session, List<String> args) {
        session.wipeAllStages();
      }
    });

    processor.registerCommand("displayURI", new CommandRunner(3) {
      @Override
      public void executeCommand(final GlobalSession session, List<String> args) {
        CommandProcessor.displayURI(args, session.getContext());
      }
    });
  }

  protected void prepareStages() {
    Map<Stage, GlobalSyncStage> stages = new EnumMap<Stage, GlobalSyncStage>(Stage.class);

    stages.put(Stage.checkPreconditions,      new CheckPreconditionsStage());
    stages.put(Stage.ensureClusterURL,        new EnsureClusterURLStage(nodeAssignmentCallback));
    stages.put(Stage.fetchInfoCollections,    new FetchInfoCollectionsStage());
    stages.put(Stage.fetchMetaGlobal,         new FetchMetaGlobalStage());
    stages.put(Stage.ensureKeysStage,         new EnsureCrypto5KeysStage());
    stages.put(Stage.syncClientsEngine,       new SyncClientsEngineStage());

    stages.put(Stage.syncTabs,                new FennecTabsServerSyncStage());
    stages.put(Stage.syncPasswords,           new PasswordsServerSyncStage());
    stages.put(Stage.syncBookmarks,           new AndroidBrowserBookmarksServerSyncStage());
    stages.put(Stage.syncHistory,             new AndroidBrowserHistoryServerSyncStage());
    stages.put(Stage.syncFormHistory,         new FormHistoryServerSyncStage());

    stages.put(Stage.uploadMetaGlobal,        new UploadMetaGlobalStage());
    stages.put(Stage.completed,               new CompletedStage());

    this.stages = Collections.unmodifiableMap(stages);
  }

  public GlobalSyncStage getSyncStageByName(String name) throws NoSuchStageException {
    return getSyncStageByName(Stage.byName(name));
  }

  public GlobalSyncStage getSyncStageByName(Stage next) throws NoSuchStageException {
    GlobalSyncStage stage = stages.get(next);
    if (stage == null) {
      throw new NoSuchStageException(next);
    }
    return stage;
  }

  public Collection<GlobalSyncStage> getSyncStagesByEnum(Collection<Stage> enums) {
    ArrayList<GlobalSyncStage> out = new ArrayList<GlobalSyncStage>();
    for (Stage name : enums) {
      try {
        GlobalSyncStage stage = this.getSyncStageByName(name);
        out.add(stage);
      } catch (NoSuchStageException e) {
        Logger.warn(LOG_TAG, "Unable to find stage with name " + name);
      }
    }
    return out;
  }

  public Collection<GlobalSyncStage> getSyncStagesByName(Collection<String> names) {
    ArrayList<GlobalSyncStage> out = new ArrayList<GlobalSyncStage>();
    for (String name : names) {
      try {
        GlobalSyncStage stage = this.getSyncStageByName(name);
        out.add(stage);
      } catch (NoSuchStageException e) {
        Logger.warn(LOG_TAG, "Unable to find stage with name " + name);
      }
    }
    return out;
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
    // If we have a backoff, request a backoff and don't advance to next stage.
    long existingBackoff = largestBackoffObserved.get();
    if (existingBackoff > 0) {
      this.abort(null, "Aborting sync because of backoff of " + existingBackoff + " milliseconds.");
      return;
    }

    this.callback.handleStageCompleted(this.currentState, this);
    Stage next = nextStage(this.currentState);
    GlobalSyncStage nextStage;
    try {
      nextStage = this.getSyncStageByName(next);
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
      return;
    }
  }

  public Context getContext() {
    return this.context;
  }

  /**
   * Begin a sync.
   * <p>
   * The caller is responsible for:
   * <ul>
   * <li>Verifying that any backoffs/minimum next sync requests are respected.</li>
   * <li>Ensuring that the device is online.</li>
   * <li>Ensuring that dependencies are ready.</li>
   * </ul>
   *
   * @throws AlreadySyncingException
   */
  public void start() throws AlreadySyncingException {
    if (this.currentState != GlobalSyncStage.Stage.idle) {
      throw new AlreadySyncingException(this.currentState);
    }
    installAsHttpResponseObserver(); // Uninstalled by completeSync or abort.
    this.advance();
  }

  /**
   * Stop this sync and start again.
   * @throws AlreadySyncingException
   */
  protected void restart() throws AlreadySyncingException {
    this.currentState = GlobalSyncStage.Stage.idle;
    if (callback.shouldBackOffStorage()) {
      this.callback.handleAborted(this, "Told to back off.");
      return;
    }
    this.start();
  }

  /**
   * We're finished (aborted or succeeded): release resources.
   */
  protected void cleanUp() {
    uninstallAsHttpResponseObserver();
    this.stages = null;
  }

  public void completeSync() {
    cleanUp();
    this.currentState = GlobalSyncStage.Stage.idle;
    this.callback.handleSuccess(this);
  }

  /**
   * Record that an updated meta/global record should be uploaded with the given
   * settings for the given engine.
   *
   * @param engineName engine to update.
   * @param engineSettings new syncID and version.
   */
  public void recordForMetaGlobalUpdate(String engineName, EngineSettings engineSettings) {
    enginesToUpdate.put(engineName, engineSettings);
  }

  /**
   * Record that an updated meta/global record should be uploaded without the
   * given engine name.
   *
   * @param engineName
   *          engine to remove.
   */
  public void removeEngineFromMetaGlobal(String engineName) {
    enginesToUpdate.put(engineName, null);
  }

  public boolean hasUpdatedMetaGlobal() {
    if (enginesToUpdate.isEmpty()) {
      Logger.info(LOG_TAG, "Not uploading updated meta/global record since there are no engines requesting upload.");
      return false;
    }

    if (Logger.shouldLogVerbose(LOG_TAG)) {
      Logger.trace(LOG_TAG, "Uploading updated meta/global record since there are engine changes to meta/global.");
      Logger.trace(LOG_TAG, "Engines requesting update [" + Utils.toCommaSeparatedString(enginesToUpdate.keySet()) + "]");
    }

    return true;
  }

  public void updateMetaGlobalInPlace() {
    config.metaGlobal.declined = this.declinedEngineNames();
    ExtendedJSONObject engines = config.metaGlobal.getEngines();
    for (Entry<String, EngineSettings> pair : enginesToUpdate.entrySet()) {
      if (pair.getValue() == null) {
        engines.remove(pair.getKey());
      } else {
        engines.put(pair.getKey(), pair.getValue().toJSONObject());
      }
    }

    enginesToUpdate.clear();
  }

  /**
   * Synchronously upload an updated meta/global.
   * <p>
   * All problems are logged and ignored.
   */
  public void uploadUpdatedMetaGlobal() {
    updateMetaGlobalInPlace();

    Logger.debug(LOG_TAG, "Uploading updated meta/global record.");
    final Object monitor = new Object();

    Runnable doUpload = new Runnable() {
      @Override
      public void run() {
        config.metaGlobal.upload(new MetaGlobalDelegate() {
          @Override
          public void handleSuccess(MetaGlobal global, SyncStorageResponse response) {
            Logger.info(LOG_TAG, "Successfully uploaded updated meta/global record.");
            // Engine changes are stored as diffs, so update enabled engines in config to match uploaded meta/global.
            config.enabledEngineNames = config.metaGlobal.getEnabledEngineNames();
            // Clear userSelectedEngines because they are updated in config and meta/global.
            config.userSelectedEngines = null;

            synchronized (monitor) {
              monitor.notify();
            }
          }

          @Override
          public void handleMissing(MetaGlobal global, SyncStorageResponse response) {
            Logger.warn(LOG_TAG, "Got 404 missing uploading updated meta/global record; shouldn't happen.  Ignoring.");
            synchronized (monitor) {
              monitor.notify();
            }
          }

          @Override
          public void handleFailure(SyncStorageResponse response) {
            Logger.warn(LOG_TAG, "Failed to upload updated meta/global record; ignoring.");
            synchronized (monitor) {
              monitor.notify();
            }
          }

          @Override
          public void handleError(Exception e) {
            Logger.warn(LOG_TAG, "Got exception trying to upload updated meta/global record; ignoring.", e);
            synchronized (monitor) {
              monitor.notify();
            }
          }
        });
      }
    };

    final Thread upload = new Thread(doUpload);
    synchronized (monitor) {
      try {
        upload.start();
        monitor.wait();
        Logger.debug(LOG_TAG, "Uploaded updated meta/global record.");
      } catch (InterruptedException e) {
        Logger.error(LOG_TAG, "Uploading updated meta/global interrupted; continuing.");
      }
    }
  }


  public void abort(Exception e, String reason) {
    Logger.warn(LOG_TAG, "Aborting sync: " + reason, e);
    cleanUp();
    long existingBackoff = largestBackoffObserved.get();
    if (existingBackoff > 0) {
      callback.requestBackoff(existingBackoff);
    }
    if (!(e instanceof HTTPFailureException)) {
      //  e is null, or we aborted for a non-HTTP reason; okay to upload new meta/global record.
      if (this.hasUpdatedMetaGlobal()) {
        this.uploadUpdatedMetaGlobal(); // Only logs errors; does not call abort.
      }
    }
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
    long responseBackoff = (new SyncResponse(response)).totalBackoffInMilliseconds();
    if (responseBackoff > 0) {
      callback.requestBackoff(responseBackoff);
    }

    if (response.getStatusLine() != null) {
      final int statusCode = response.getStatusLine().getStatusCode();
      switch(statusCode) {

      case 400:
        SyncStorageResponse storageResponse = new SyncStorageResponse(response);
        this.interpretHTTPBadRequestBody(storageResponse);
        break;

      case 401:
        /*
         * Alert our callback we have a 401 on a cluster URL. This GlobalSession
         * will fail, but the next one will fetch a new cluster URL and will
         * distinguish between "node reassignment" and "user password changed".
         */
        callback.informUnauthorizedResponse(this, config.getClusterURL());
        break;
      }
    }
  }

  protected void interpretHTTPBadRequestBody(final SyncStorageResponse storageResponse) {
    try {
      final String body = storageResponse.body();
      if (body == null) {
        return;
      }
      if (SyncStorageResponse.RESPONSE_CLIENT_UPGRADE_REQUIRED.equals(body)) {
        callback.informUpgradeRequiredResponse(this);
        return;
      }
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Exception parsing HTTP 400 body.", e);
    }
  }

  public void fetchInfoCollections(JSONRecordFetchDelegate callback) throws URISyntaxException {
    final JSONRecordFetcher fetcher = new JSONRecordFetcher(config.infoCollectionsURL(), getAuthHeaderProvider());
    fetcher.fetch(callback);
  }

  /**
   * Upload new crypto/keys.
   *
   * @param keys
   *          new keys.
   * @param keyUploadDelegate
   *          a delegate.
   */
  public void uploadKeys(final CollectionKeys keys,
                         final KeyUploadDelegate keyUploadDelegate) {
    SyncStorageRecordRequest request;
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
        Logger.debug(LOG_TAG, "Keys uploaded.");
        BaseResource.consumeEntity(response); // We don't need the response at all.
        keyUploadDelegate.onKeysUploaded();
      }

      @Override
      public void handleRequestFailure(SyncStorageResponse response) {
        Logger.debug(LOG_TAG, "Failed to upload keys.");
        GlobalSession.this.interpretHTTPFailure(response.httpResponse());
        BaseResource.consumeEntity(response); // The exception thrown should not need the body of the response.
        keyUploadDelegate.onKeyUploadFailed(new HTTPFailureException(response));
      }

      @Override
      public void handleRequestError(Exception ex) {
        Logger.warn(LOG_TAG, "Got exception trying to upload keys", ex);
        keyUploadDelegate.onKeyUploadFailed(ex);
      }

      @Override
      public AuthHeaderProvider getAuthHeaderProvider() {
        return GlobalSession.this.getAuthHeaderProvider();
      }
    };

    // Convert keys to an encrypted crypto record.
    CryptoRecord keysRecord;
    try {
      keysRecord = keys.asCryptoRecord();
      keysRecord.setKeyBundle(config.syncKeyBundle);
      keysRecord.encrypt();
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Got exception trying creating crypto record from keys", e);
      keyUploadDelegate.onKeyUploadFailed(e);
      return;
    }

    request.put(keysRecord);
  }

  /*
   * meta/global callbacks.
   */
  public void processMetaGlobal(MetaGlobal global) {
    config.metaGlobal = global;

    Long storageVersion = global.getStorageVersion();
    if (storageVersion == null) {
      Logger.warn(LOG_TAG, "Malformed remote meta/global: could not retrieve remote storage version.");
      freshStart();
      return;
    }
    if (storageVersion < STORAGE_VERSION) {
      Logger.warn(LOG_TAG, "Outdated server: reported " +
          "remote storage version " + storageVersion + " < " +
          "local storage version " + STORAGE_VERSION);
      freshStart();
      return;
    }
    if (storageVersion > STORAGE_VERSION) {
      Logger.warn(LOG_TAG, "Outdated client: reported " +
          "remote storage version " + storageVersion + " > " +
          "local storage version " + STORAGE_VERSION);
      requiresUpgrade();
      return;
    }
    String remoteSyncID = global.getSyncID();
    if (remoteSyncID == null) {
      Logger.warn(LOG_TAG, "Malformed remote meta/global: could not retrieve remote syncID.");
      freshStart();
      return;
    }
    String localSyncID = config.syncID;
    if (!remoteSyncID.equals(localSyncID)) {
      Logger.warn(LOG_TAG, "Remote syncID different from local syncID: resetting client and assuming remote syncID.");
      resetAllStages();
      config.purgeCryptoKeys();
      config.syncID = remoteSyncID;
    }
    // Compare lastModified timestamps for remote/local engine selection times.
    Logger.debug(LOG_TAG, "Comparing local engine selection timestamp [" + config.userSelectedEnginesTimestamp + "] to server meta/global timestamp [" + config.persistedMetaGlobal().lastModified() + "].");
    if (config.userSelectedEnginesTimestamp < config.persistedMetaGlobal().lastModified()) {
      // Remote has later meta/global timestamp. Don't upload engine changes.
      config.userSelectedEngines = null;
    }
    // Persist enabled engine names.
    config.enabledEngineNames = global.getEnabledEngineNames();
    if (config.enabledEngineNames == null) {
      Logger.warn(LOG_TAG, "meta/global reported no enabled engine names!");
    } else {
      if (Logger.shouldLogVerbose(LOG_TAG)) {
        Logger.trace(LOG_TAG, "Persisting enabled engine names '" +
            Utils.toCommaSeparatedString(config.enabledEngineNames) + "' from meta/global.");
      }
    }

    // Persist declined.
    // Our declined engines at any point are:
    // Whatever they were remotely, plus whatever they were locally, less any
    // engines that were just enabled locally or remotely.
    // If remote just 'won', our recently enabled list just got cleared.
    final HashSet<String> allDeclined = new HashSet<String>();

    final Set<String> newRemoteDeclined = global.getDeclinedEngineNames();
    final Set<String> oldLocalDeclined = config.declinedEngineNames;

    allDeclined.addAll(newRemoteDeclined);
    allDeclined.addAll(oldLocalDeclined);

    if (config.userSelectedEngines != null) {
      for (Entry<String, Boolean> selection : config.userSelectedEngines.entrySet()) {
        if (selection.getValue()) {
          allDeclined.remove(selection.getKey());
        }
      }
    }

    config.declinedEngineNames = allDeclined;
    if (config.declinedEngineNames.isEmpty()) {
      Logger.debug(LOG_TAG, "meta/global reported no declined engine names, and we have none declined locally.");
    } else {
      if (Logger.shouldLogVerbose(LOG_TAG)) {
        Logger.trace(LOG_TAG, "Persisting declined engine names '" +
            Utils.toCommaSeparatedString(config.declinedEngineNames) + "' from meta/global.");
      }
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
  public void freshStart() {
    final GlobalSession globalSession = this;
    freshStart(this, new FreshStartDelegate() {

      @Override
      public void onFreshStartFailed(Exception e) {
        globalSession.abort(e, "Fresh start failed.");
      }

      @Override
      public void onFreshStart() {
        try {
          Logger.warn(LOG_TAG, "Fresh start succeeded; restarting global session.");
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
   * <p>
   * <ol>
   * <li>Wipe the server storage.</li>
   * <li>Reset all stages and purge cached state: (meta/global and crypto/keys records).</li>
   * <li>Upload fresh meta/global record.</li>
   * <li>Upload fresh crypto/keys record.</li>
   * <li>Restart the sync entirely in order to re-download meta/global and crypto/keys record.</li>
   * </ol>
   * @param session the current session.
   * @param freshStartDelegate delegate to notify on fresh start or failure.
   */
  protected static void freshStart(final GlobalSession session, final FreshStartDelegate freshStartDelegate) {
    Logger.debug(LOG_TAG, "Fresh starting.");

    final MetaGlobal mg = session.generateNewMetaGlobal();

    session.wipeServer(session.getAuthHeaderProvider(), new WipeServerDelegate() {

      @Override
      public void onWiped(long timestamp) {
        Logger.debug(LOG_TAG, "Successfully wiped server.  Resetting all stages and purging cached meta/global and crypto/keys records.");

        session.resetAllStages();
        session.config.purgeMetaGlobal();
        session.config.purgeCryptoKeys();
        session.config.persistToPrefs();

        Logger.info(LOG_TAG, "Uploading new meta/global with sync ID " + mg.syncID + ".");

        // It would be good to set the X-If-Unmodified-Since header to `timestamp`
        // for this PUT to ensure at least some level of transactionality.
        // Unfortunately, the servers don't support it after a wipe right now
        // (bug 693893), so we're going to defer this until bug 692700.
        mg.upload(new MetaGlobalDelegate() {
          @Override
          public void handleSuccess(MetaGlobal uploadedGlobal, SyncStorageResponse uploadResponse) {
            Logger.info(LOG_TAG, "Uploaded new meta/global with sync ID " + uploadedGlobal.syncID + ".");

            // Generate new keys.
            CollectionKeys keys = null;
            try {
              keys = session.generateNewCryptoKeys();
            } catch (CryptoException e) {
              Logger.warn(LOG_TAG, "Got exception generating new keys; failing fresh start.", e);
              freshStartDelegate.onFreshStartFailed(e);
            }
            if (keys == null) {
              Logger.warn(LOG_TAG, "Got null keys from generateNewKeys; failing fresh start.");
              freshStartDelegate.onFreshStartFailed(null);
            }

            // Upload new keys.
            Logger.info(LOG_TAG, "Uploading new crypto/keys.");
            session.uploadKeys(keys, new KeyUploadDelegate() {
              @Override
              public void onKeysUploaded() {
                Logger.info(LOG_TAG, "Uploaded new crypto/keys.");
                freshStartDelegate.onFreshStart();
              }

              @Override
              public void onKeyUploadFailed(Exception e) {
                Logger.warn(LOG_TAG, "Got exception uploading new keys.", e);
                freshStartDelegate.onFreshStartFailed(e);
              }
            });
          }

          @Override
          public void handleMissing(MetaGlobal global, SyncStorageResponse response) {
            // Shouldn't happen on upload.
            Logger.warn(LOG_TAG, "Got 'missing' response uploading new meta/global.");
            freshStartDelegate.onFreshStartFailed(new Exception("meta/global missing while uploading."));
          }

          @Override
          public void handleFailure(SyncStorageResponse response) {
            Logger.warn(LOG_TAG, "Got failure " + response.getStatusCode() + " uploading new meta/global.");
            session.interpretHTTPFailure(response.httpResponse());
            freshStartDelegate.onFreshStartFailed(new HTTPFailureException(response));
          }

          @Override
          public void handleError(Exception e) {
            Logger.warn(LOG_TAG, "Got error uploading new meta/global.", e);
            freshStartDelegate.onFreshStartFailed(e);
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

  // Note that we do not yet implement wipeRemote: it's only necessary for
  // first sync options.
  // -- reset local stages, wipe server for each stage *except* clients
  //    (stages only, not whole server!), send wipeEngine commands to each client.
  //
  // Similarly for startOver (because we don't receive that notification).
  // -- remove client data from server, reset local stages, clear keys, reset
  //    backoff, clear all prefs, discard credentials.
  //
  // Change passphrase: wipe entire server, reset client to force upload, sync.
  //
  // When an engine is disabled: wipe its collections on the server, reupload
  // meta/global.
  //
  // On syncing each stage: if server has engine version 0 or old, wipe server,
  // reset client to prompt reupload.
  // If sync ID mismatch: take that syncID and reset client.

  protected void wipeServer(final AuthHeaderProvider authHeaderProvider, final WipeServerDelegate wipeDelegate) {
    SyncStorageRequest request;
    final GlobalSession self = this;

    try {
      request = new SyncStorageRequest(config.storageURL());
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
      public AuthHeaderProvider getAuthHeaderProvider() {
        return GlobalSession.this.getAuthHeaderProvider();
      }
    };
    request.delete();
  }

  public void wipeAllStages() {
    Logger.info(LOG_TAG, "Wiping all stages.");
    // Includes "clients".
    this.wipeStagesByEnum(Stage.getNamedStages());
  }

  public void wipeStages(Collection<GlobalSyncStage> stages) {
    for (GlobalSyncStage stage : stages) {
      try {
        Logger.info(LOG_TAG, "Wiping " + stage);
        stage.wipeLocal(this);
      } catch (Exception e) {
        Logger.error(LOG_TAG, "Ignoring wipe failure for stage " + stage, e);
      }
    }
  }

  public void wipeStagesByEnum(Collection<Stage> stages) {
    wipeStages(this.getSyncStagesByEnum(stages));
  }

  public void wipeStagesByName(Collection<String> names) {
    wipeStages(this.getSyncStagesByName(names));
  }

  public void resetAllStages() {
    Logger.info(LOG_TAG, "Resetting all stages.");
    // Includes "clients".
    this.resetStagesByEnum(Stage.getNamedStages());
  }

  public void resetStages(Collection<GlobalSyncStage> stages) {
    for (GlobalSyncStage stage : stages) {
      try {
        Logger.info(LOG_TAG, "Resetting " + stage);
        stage.resetLocal(this);
      } catch (Exception e) {
        Logger.error(LOG_TAG, "Ignoring reset failure for stage " + stage, e);
      }
    }
  }

  public void resetStagesByEnum(Collection<Stage> stages) {
    resetStages(this.getSyncStagesByEnum(stages));
  }

  public void resetStagesByName(Collection<String> names) {
    resetStages(this.getSyncStagesByName(names));
  }

  /**
   * Engines to explicitly mark as declined in a fresh meta/global record.
   * <p>
   * Returns an empty array if the user hasn't elected to customize data types,
   * or an array of engines that the user un-checked during customization.
   * <p>
   * Engines that Android Sync doesn't recognize are <b>not</b> included in
   * the returned array.
   *
   * @return a new JSONArray of engine names.
   */
  @SuppressWarnings("unchecked")
  protected JSONArray declinedEngineNames() {
    final JSONArray declined = new JSONArray();
    for (String engine : config.declinedEngineNames) {
      declined.add(engine);
    };

    return declined;
  }

  /**
   * Engines to include in a fresh meta/global record.
   * <p>
   * Returns either the persisted engine names (perhaps we have been node
   * re-assigned and are initializing a clean server: we want to upload the
   * persisted engine names so that we don't accidentally disable engines that
   * Android Sync doesn't recognize), or the set of engines names that Android
   * Sync implements.
   *
   * @return set of engine names.
   */
  protected Set<String> enabledEngineNames() {
    if (config.enabledEngineNames != null) {
      return config.enabledEngineNames;
    }

    // These are the default set of engine names.
    Set<String> validEngineNames = SyncConfiguration.validEngineNames();

    // If the user hasn't set any selected engines, that's okay -- default to
    // everything.
    if (config.userSelectedEngines == null) {
      return validEngineNames;
    }

    // userSelectedEngines has keys that are engine names, and boolean values
    // corresponding to whether the user asked for the engine to sync or not. If
    // an engine is not present, that means the user didn't change its sync
    // setting. Since we default to everything on, that means the user didn't
    // turn it off; therefore, it's included in the set of engines to sync.
    Set<String> validAndSelectedEngineNames = new HashSet<String>();
    for (String engineName : validEngineNames) {
      if (config.userSelectedEngines.containsKey(engineName) &&
          !config.userSelectedEngines.get(engineName)) {
        continue;
      }
      validAndSelectedEngineNames.add(engineName);
    }
    return validAndSelectedEngineNames;
  }

  /**
   * Generate fresh crypto/keys collection.
   * @return crypto/keys collection.
   * @throws CryptoException
   */
  @SuppressWarnings("static-method")
  public CollectionKeys generateNewCryptoKeys() throws CryptoException {
    return CollectionKeys.generateCollectionKeys();
  }

  /**
   * Generate a fresh meta/global record.
   * @return meta/global record.
   */
  public MetaGlobal generateNewMetaGlobal() {
    final String newSyncID   = Utils.generateGuid();
    final String metaURL     = this.config.metaURL();

    ExtendedJSONObject engines = new ExtendedJSONObject();
    for (String engineName : enabledEngineNames()) {
      EngineSettings engineSettings = null;
      try {
        GlobalSyncStage globalStage = this.getSyncStageByName(engineName);
        Integer version = globalStage.getStorageVersion();
        if (version == null) {
          continue; // Don't want this stage to be included in meta/global.
        }
        engineSettings = new EngineSettings(Utils.generateGuid(), version);
      } catch (NoSuchStageException e) {
        // No trouble; Android Sync might not recognize this engine yet.
        // By default, version 0.  Other clients will see the 0 version and reset/wipe accordingly.
        engineSettings = new EngineSettings(Utils.generateGuid(), 0);
      }
      engines.put(engineName, engineSettings.toJSONObject());
    }

    MetaGlobal metaGlobal = new MetaGlobal(metaURL, this.getAuthHeaderProvider());
    metaGlobal.setSyncID(newSyncID);
    metaGlobal.setStorageVersion(STORAGE_VERSION);
    metaGlobal.setEngines(engines);

    // We assume that the config's declined engines have been updated
    // according to the user's selections.
    metaGlobal.setDeclinedEngineNames(this.declinedEngineNames());

    return metaGlobal;
  }

  /**
   * Suggest that your Sync client needs to be upgraded to work
   * with this server.
   */
  public void requiresUpgrade() {
    Logger.info(LOG_TAG, "Client outdated storage version; requires update.");
    // TODO: notify UI.
    this.abort(null, "Requires upgrade");
  }

  /**
   * If meta/global is missing or malformed, throws a MetaGlobalException.
   * Otherwise, returns true if there is an entry for this engine in the
   * meta/global "engines" object.
   * <p>
   * This is a global/permanent setting, not a local/temporary setting. For the
   * latter, see {@link GlobalSession#isEngineLocallyEnabled(String)}.
   *
   * @param engineName the name to check (e.g., "bookmarks").
   * @param engineSettings
   *        if non-null, verify that the server engine settings are congruent
   *        with this, throwing the appropriate MetaGlobalException if not.
   * @return
   *        true if the engine with the provided name is present in the
   *        meta/global "engines" object, and verification passed.
   *
   * @throws MetaGlobalException
   */
  public boolean isEngineRemotelyEnabled(String engineName, EngineSettings engineSettings) throws MetaGlobalException {
    if (this.config.metaGlobal == null) {
      throw new MetaGlobalNotSetException();
    }

    // This should not occur.
    if (this.config.enabledEngineNames == null) {
      Logger.error(LOG_TAG, "No enabled engines in config. Giving up.");
      throw new MetaGlobalMissingEnginesException();
    }

    if (!(this.config.enabledEngineNames.contains(engineName))) {
      Logger.debug(LOG_TAG, "Engine " + engineName + " not enabled: no meta/global entry.");
      return false;
    }

    // If we have a meta/global, check that it's safe for us to sync.
    // (If we don't, we'll create one later, which is why we return `true` above.)
    if (engineSettings != null) {
      // Throws if there's a problem.
      this.config.metaGlobal.verifyEngineSettings(engineName, engineSettings);
    }

    return true;
  }


  /**
   * Return true if the named stage should be synced this session.
   * <p>
   * This is a local/temporary setting, in contrast to the meta/global record,
   * which is a global/permanent setting. For the latter, see
   * {@link GlobalSession#isEngineRemotelyEnabled(String, EngineSettings)}.
   *
   * @param stageName
   *          to query.
   * @return true if named stage is enabled for this sync.
   */
  public boolean isEngineLocallyEnabled(String stageName) {
    if (config.stagesToSync == null) {
      return true;
    }
    return config.stagesToSync.contains(stageName);
  }

  public ClientsDataDelegate getClientsDelegate() {
    return this.clientsDelegate;
  }

  /**
   * The longest backoff observed to date; -1 means no backoff observed.
   */
  protected final AtomicLong largestBackoffObserved = new AtomicLong(-1);

  /**
   * Reset any observed backoff and start observing HTTP responses for backoff
   * requests.
   */
  protected void installAsHttpResponseObserver() {
    Logger.debug(LOG_TAG, "Installing " + this + " as BaseResource HttpResponseObserver.");
    BaseResource.setHttpResponseObserver(this);
    largestBackoffObserved.set(-1);
  }

  /**
   * Stop observing HttpResponses for backoff requests.
   */
  protected void uninstallAsHttpResponseObserver() {
    Logger.debug(LOG_TAG, "Uninstalling " + this + " as BaseResource HttpResponseObserver.");
    BaseResource.setHttpResponseObserver(null);
  }

  /**
   * Observe all HTTP response for backoff requests on all status codes, not just errors.
   */
  @Override
  public void observeHttpResponse(HttpResponse response) {
    long responseBackoff = (new SyncResponse(response)).totalBackoffInMilliseconds(); // TODO: don't allocate object?
    if (responseBackoff <= 0) {
      return;
    }

    Logger.debug(LOG_TAG, "Observed " + responseBackoff + " millisecond backoff request.");
    while (true) {
      long existingBackoff = largestBackoffObserved.get();
      if (existingBackoff >= responseBackoff) {
        return;
      }
      if (largestBackoffObserved.compareAndSet(existingBackoff, responseBackoff)) {
        return;
      }
    }
  }
}
