/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import android.content.Context;
import android.os.SystemClock;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.EngineSettings;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.HTTPFailureException;
import org.mozilla.gecko.sync.MetaGlobalException;
import org.mozilla.gecko.sync.NoCollectionKeysSetException;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.ReflowIsNecessaryException;
import org.mozilla.gecko.sync.SynchronizerConfiguration;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.delegates.WipeServerDelegate;
import org.mozilla.gecko.sync.middleware.Crypto5MiddlewareRepository;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.SyncStorageRequest;
import org.mozilla.gecko.sync.net.SyncStorageRequestDelegate;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;
import org.mozilla.gecko.sync.repositories.NonPersistentRepositoryStateProvider;
import org.mozilla.gecko.sync.repositories.RecordFactory;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.RepositorySessionBundle;
import org.mozilla.gecko.sync.repositories.RepositoryStateProvider;
import org.mozilla.gecko.sync.repositories.Server15Repository;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionBeginDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFinishDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;
import org.mozilla.gecko.sync.synchronizer.ServerLocalSynchronizer;
import org.mozilla.gecko.sync.synchronizer.Synchronizer;
import org.mozilla.gecko.sync.synchronizer.SynchronizerDelegate;
import org.mozilla.gecko.sync.synchronizer.SynchronizerSession;

import java.io.IOException;
import java.net.URISyntaxException;
import java.util.Map;
import java.util.concurrent.ExecutorService;

/**
 * Fetch from a server collection into a local repository, encrypting
 * and decrypting along the way.
 *
 * @author rnewman
 *
 */
public abstract class ServerSyncStage extends AbstractSessionManagingSyncStage implements SynchronizerDelegate {

  protected static final String LOG_TAG = "ServerSyncStage";

  protected long stageStartTimestamp = -1;
  protected long stageCompleteTimestamp = -1;

  /**
   * Poor-man's boolean typing.
   * These enums are used to configure {@link org.mozilla.gecko.sync.repositories.ConfigurableServer15Repository}.
   */
  public enum HighWaterMark {
    Enabled,
    Disabled
  }

  public enum MultipleBatches {
    Enabled,
    Disabled
  }

  /**
   * Override these in your subclasses.
   *
   * @return true if this stage should be executed.
   * @throws MetaGlobalException
   */
  protected boolean isEnabled() throws MetaGlobalException {
    EngineSettings engineSettings = null;
    try {
       engineSettings = getEngineSettings();
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Unable to get engine settings for " + this + ": fetching config failed.", e);
      // Fall through; null engineSettings will pass below.
    }

    // We can be disabled by the server's meta/global record, or malformed in the server's meta/global record,
    // or by the user manually in Sync Settings.
    // We catch the subclasses of MetaGlobalException to trigger various resets and wipes in execute().
    boolean enabledInMetaGlobal = session.isEngineRemotelyEnabled(this.getEngineName(), engineSettings);

    // Check for manual changes to engines by the user.
    checkAndUpdateUserSelectedEngines(enabledInMetaGlobal);

    // Check for changes on the server.
    if (!enabledInMetaGlobal) {
      Logger.debug(LOG_TAG, "Stage " + this.getEngineName() + " disabled by server meta/global.");
      return false;
    }

    // We can also be disabled just for this sync.
    boolean enabledThisSync = session.isEngineLocallyEnabled(this.getEngineName()); // For ServerSyncStage, stage name == engine name.
    if (!enabledThisSync) {
      Logger.debug(LOG_TAG, "Stage " + this.getEngineName() + " disabled just for this sync.");
    }
    return enabledThisSync;
  }

  /**
   * Compares meta/global engine state to user selected engines from Sync
   * Settings and throws an exception if they don't match and meta/global needs
   * to be updated.
   *
   * @param enabledInMetaGlobal
   *          boolean of engine sync state in meta/global
   * @throws MetaGlobalException
   *           if engine sync state has been changed in Sync Settings, with new
   *           engine sync state.
   */
  protected void checkAndUpdateUserSelectedEngines(boolean enabledInMetaGlobal) throws MetaGlobalException {
    Map<String, Boolean> selectedEngines = session.config.userSelectedEngines;
    String thisEngine = this.getEngineName();

    if (selectedEngines != null && selectedEngines.containsKey(thisEngine)) {
      boolean enabledInSelection = selectedEngines.get(thisEngine);
      if (enabledInMetaGlobal != enabledInSelection) {
        // Engine enable state has been changed by the user.
        Logger.debug(LOG_TAG, "Engine state has been changed by user. Throwing exception.");
        throw new MetaGlobalException.MetaGlobalEngineStateChangedException(enabledInSelection);
      }
    }
  }

  protected EngineSettings getEngineSettings() throws NonObjectJSONException, IOException {
    Integer version = getStorageVersion();
    if (version == null) {
      Logger.warn(LOG_TAG, "null storage version for " + this + "; using version 0.");
      version = 0;
    }

    SynchronizerConfiguration config = this.getConfig();
    if (config == null) {
      return new EngineSettings(null, version);
    }
    return new EngineSettings(config.syncID, version);
  }

  protected abstract String getCollection();
  protected abstract String getEngineName();
  protected abstract Repository getLocalRepository();
  protected abstract RecordFactory getRecordFactory();

  /**
   * Used to configure a {@link org.mozilla.gecko.sync.repositories.ConfigurableServer15Repository}.
   * Override this if you need a persistent repository state provider.
   *
   * @return Non-persistent state provider.
   */
  protected RepositoryStateProvider getRepositoryStateProvider() {
    return new NonPersistentRepositoryStateProvider();
  }

  /**
   * Used to configure a {@link org.mozilla.gecko.sync.repositories.ConfigurableServer15Repository}.
   * Override this if you want to restrict downloader to just a single batch.
   */
  protected MultipleBatches getAllowedMultipleBatches() {
    return MultipleBatches.Enabled;
  }

  /**
   * Used to configure a {@link org.mozilla.gecko.sync.repositories.ConfigurableServer15Repository}.
   * Override this if you want to allow resuming record downloads from a high-water-mark.
   * Ensure you're using a {@link org.mozilla.gecko.sync.repositories.PersistentRepositoryStateProvider}
   * to persist high-water-mark across syncs.
   */
  protected HighWaterMark getAllowedToUseHighWaterMark() {
    return HighWaterMark.Disabled;
  }

  // Override this in subclasses.
  protected Repository getRemoteRepository() throws URISyntaxException {
    String collection = getCollection();
    return new Server15Repository(collection,
                                  session.getSyncDeadline(),
                                  session.config.storageURL(),
                                  session.getAuthHeaderProvider(),
                                  session.config.infoCollections,
                                  session.config.infoConfiguration,
                                  new NonPersistentRepositoryStateProvider());
  }

  /**
   * Return a Crypto5Middleware-wrapped Server15Repository.
   *
   * @throws NoCollectionKeysSetException
   * @throws URISyntaxException
   */
  protected Repository wrappedServerRepo() throws NoCollectionKeysSetException, URISyntaxException {
    String collection = this.getCollection();
    KeyBundle collectionKey = session.keyBundleForCollection(collection);
    Crypto5MiddlewareRepository cryptoRepo = new Crypto5MiddlewareRepository(getRemoteRepository(), collectionKey);
    cryptoRepo.recordFactory = getRecordFactory();
    return cryptoRepo;
  }

  protected String bundlePrefix() {
    return this.getCollection() + ".";
  }

  protected String statePreferencesPrefix() {
    return this.getCollection() + ".state.";
  }

  protected SynchronizerConfiguration getConfig() throws NonObjectJSONException, IOException {
    return new SynchronizerConfiguration(session.config.getBranch(bundlePrefix()));
  }

  protected void persistConfig(SynchronizerConfiguration synchronizerConfiguration) {
    synchronizerConfiguration.persist(session.config.getBranch(bundlePrefix()));
  }

  public Synchronizer getConfiguredSynchronizer(GlobalSession session) throws NoCollectionKeysSetException, URISyntaxException, NonObjectJSONException, IOException {
    Repository remote = wrappedServerRepo();

    Synchronizer synchronizer = new ServerLocalSynchronizer();
    synchronizer.repositoryA = remote;
    synchronizer.repositoryB = this.getLocalRepository();
    synchronizer.load(getConfig());

    return synchronizer;
  }

  /**
   * Reset timestamps and any repository state.
   */
  @Override
  protected void resetLocal() {
    resetLocalWithSyncID(null);
    if (!getRepositoryStateProvider().resetAndCommit()) {
      // At the very least, we can log this.
      // Failing to reset at this point means that we'll have lingering state for any stages using a
      // persistent provider. In certain cases this might negatively affect first sync of this stage
      // in the future.
      // Our timestamp resetting code in `persistConfig` is affected by the same problem.
      // A way to work around this is to further prefix our persisted SharedPreferences with
      // clientID/syncID, ensuring a very defined scope for any persisted state. See Bug 1332431.
      Logger.warn(LOG_TAG, "Failed to reset repository state");
    }
  }

  /**
   * Reset timestamps and possibly set syncID.
   * @param syncID if non-null, new syncID to persist.
   */
  protected void resetLocalWithSyncID(String syncID) {
    // Clear both timestamps.
    SynchronizerConfiguration config;
    try {
      config = this.getConfig();
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Unable to reset " + this + ": fetching config failed.", e);
      return;
    }

    if (syncID != null) {
      config.syncID = syncID;
      Logger.info(LOG_TAG, "Setting syncID for " + this + " to '" + syncID + "'.");
    }
    config.localBundle.setTimestamp(0L);
    config.remoteBundle.setTimestamp(0L);
    persistConfig(config);
    Logger.info(LOG_TAG, "Reset timestamps for " + this);
  }

  // Not thread-safe. Use with caution.
  private class WipeWaiter {
    public boolean sessionSucceeded = true;
    public boolean wipeSucceeded = true;
    public Exception error;

    public void notify(Exception e, boolean sessionSucceeded) {
      this.sessionSucceeded = sessionSucceeded;
      this.wipeSucceeded = false;
      this.error = e;
      this.notify();
    }
  }

  /**
   * Synchronously wipe this stage by instantiating a local repository session
   * and wiping that.
   * <p>
   * Logs and re-throws an exception on failure.
   */
  @Override
  protected void wipeLocal() throws Exception {
    // Reset, then clear data.
    this.resetLocal();

    final WipeWaiter monitor = new WipeWaiter();
    final Context context = session.getContext();
    final Repository r = this.getLocalRepository();

    final Runnable doWipe = new Runnable() {
      @Override
      public void run() {
        r.createSession(new RepositorySessionCreationDelegate() {

          @Override
          public void onSessionCreated(final RepositorySession session) {
            try {
              session.begin(new RepositorySessionBeginDelegate() {

                @Override
                public void onBeginSucceeded(final RepositorySession session) {
                  session.wipe(new RepositorySessionWipeDelegate() {
                    @Override
                    public void onWipeSucceeded() {
                      try {
                        session.finish(new RepositorySessionFinishDelegate() {

                          @Override
                          public void onFinishSucceeded(RepositorySession session,
                                                        RepositorySessionBundle bundle) {
                            // Hurrah.
                            synchronized (monitor) {
                              monitor.notify();
                            }
                          }

                          @Override
                          public void onFinishFailed(Exception ex) {
                            // Assume that no finish => no wipe.
                            synchronized (monitor) {
                              monitor.notify(ex, true);
                            }
                          }

                          @Override
                          public RepositorySessionFinishDelegate deferredFinishDelegate(ExecutorService executor) {
                            return this;
                          }
                        });
                      } catch (InactiveSessionException e) {
                        // Cannot happen. Call for safety.
                        synchronized (monitor) {
                          monitor.notify(e, true);
                        }
                      }
                    }

                    @Override
                    public void onWipeFailed(Exception ex) {
                      session.abort();
                      synchronized (monitor) {
                        monitor.notify(ex, true);
                      }
                    }

                    @Override
                    public RepositorySessionWipeDelegate deferredWipeDelegate(ExecutorService executor) {
                      return this;
                    }
                  });
                }

                @Override
                public void onBeginFailed(Exception ex) {
                  session.abort();
                  synchronized (monitor) {
                    monitor.notify(ex, true);
                  }
                }

                @Override
                public RepositorySessionBeginDelegate deferredBeginDelegate(ExecutorService executor) {
                  return this;
                }
              });
            } catch (InvalidSessionTransitionException e) {
              session.abort();
              synchronized (monitor) {
                monitor.notify(e, true);
              }
            }
          }

          @Override
          public void onSessionCreateFailed(Exception ex) {
            synchronized (monitor) {
              monitor.notify(ex, false);
            }
          }

          @Override
          public RepositorySessionCreationDelegate deferredCreationDelegate() {
            return this;
          }
        }, context);
      }
    };

    final Thread wiping = new Thread(doWipe);
    synchronized (monitor) {
      wiping.start();
      try {
        monitor.wait();
      } catch (InterruptedException e) {
        Logger.error(LOG_TAG, "Wipe interrupted.");
      }
    }

    if (!monitor.sessionSucceeded) {
      Logger.error(LOG_TAG, "Failed to create session for wipe.");
      throw monitor.error;
    }

    if (!monitor.wipeSucceeded) {
      Logger.error(LOG_TAG, "Failed to wipe session.");
      throw monitor.error;
    }

    Logger.info(LOG_TAG, "Wiping stage complete.");
  }

  /**
   * Asynchronously wipe collection on server.
   */
  protected void wipeServer(final AuthHeaderProvider authHeaderProvider, final WipeServerDelegate wipeDelegate) {
    SyncStorageRequest request;

    try {
      request = new SyncStorageRequest(session.config.collectionURI(getCollection()));
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
        resetLocal();
        wipeDelegate.onWiped(response.normalizedWeaveTimestamp());
      }

      @Override
      public void handleRequestFailure(SyncStorageResponse response) {
        Logger.warn(LOG_TAG, "Got request failure " + response.getStatusCode() + " in wipeServer.");
        // Process HTTP failures here to pick up backoffs, etc.
        session.interpretHTTPFailure(response.httpResponse());
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
        return authHeaderProvider;
      }
    };

    request.delete();
  }

  /**
   * Synchronously wipe the server.
   * <p>
   * Logs and re-throws an exception on failure.
   */
  public void wipeServer(final GlobalSession session) throws Exception {
    this.session = session;

    final WipeWaiter monitor = new WipeWaiter();

    final Runnable doWipe = new Runnable() {
      @Override
      public void run() {
        wipeServer(session.getAuthHeaderProvider(), new WipeServerDelegate() {
          @Override
          public void onWiped(long timestamp) {
            synchronized (monitor) {
              monitor.notify();
            }
          }

          @Override
          public void onWipeFailed(Exception e) {
            synchronized (monitor) {
              monitor.notify(e, false);
            }
          }
        });
      }
    };

    final Thread wiping = new Thread(doWipe);
    synchronized (monitor) {
      wiping.start();
      try {
        monitor.wait();
      } catch (InterruptedException e) {
        Logger.error(LOG_TAG, "Server wipe interrupted.");
      }
    }

    if (!monitor.wipeSucceeded) {
      Logger.error(LOG_TAG, "Failed to wipe server.");
      throw monitor.error;
    }

    Logger.info(LOG_TAG, "Wiping server complete.");
  }

  @Override
  public void execute() throws NoSuchStageException {
    final String name = getEngineName();
    Logger.debug(LOG_TAG, "Starting execute for " + name);

    stageStartTimestamp = SystemClock.elapsedRealtime();

    try {
      if (!this.isEnabled()) {
        Logger.info(LOG_TAG, "Skipping stage " + name + ".");
        session.advance();
        return;
      }
    } catch (MetaGlobalException.MetaGlobalMalformedSyncIDException e) {
      // Bad engine syncID. This should never happen. Wipe the server.
      try {
        session.recordForMetaGlobalUpdate(name, new EngineSettings(Utils.generateGuid(), this.getStorageVersion()));
        Logger.info(LOG_TAG, "Wiping server because malformed engine sync ID was found in meta/global.");
        wipeServer(session);
        Logger.info(LOG_TAG, "Wiped server after malformed engine sync ID found in meta/global.");
      } catch (Exception ex) {
        session.abort(ex, "Failed to wipe server after malformed engine sync ID found in meta/global.");
      }
    } catch (MetaGlobalException.MetaGlobalMalformedVersionException e) {
      // Bad engine version. This should never happen. Wipe the server.
      try {
        session.recordForMetaGlobalUpdate(name, new EngineSettings(Utils.generateGuid(), this.getStorageVersion()));
        Logger.info(LOG_TAG, "Wiping server because malformed engine version was found in meta/global.");
        wipeServer(session);
        Logger.info(LOG_TAG, "Wiped server after malformed engine version found in meta/global.");
      } catch (Exception ex) {
        session.abort(ex, "Failed to wipe server after malformed engine version found in meta/global.");
      }
    } catch (MetaGlobalException.MetaGlobalStaleClientSyncIDException e) {
      // Our syncID is wrong. Reset client and take the server syncID.
      Logger.warn(LOG_TAG, "Remote engine syncID different from local engine syncID:" +
                           " resetting local engine and assuming remote engine syncID.");
      this.resetLocalWithSyncID(e.serverSyncID);
    } catch (MetaGlobalException.MetaGlobalEngineStateChangedException e) {
      boolean isEnabled = e.isEnabled;
      if (!isEnabled) {
        // Engine has been disabled; update meta/global with engine removal for upload.
        session.removeEngineFromMetaGlobal(name);
        session.config.declinedEngineNames.add(name);
      } else {
        session.config.declinedEngineNames.remove(name);
        // Add engine with new syncID to meta/global for upload.
        String newSyncID = Utils.generateGuid();
        session.recordForMetaGlobalUpdate(name, new EngineSettings(newSyncID, this.getStorageVersion()));
        // Update SynchronizerConfiguration w/ new engine syncID.
        this.resetLocalWithSyncID(newSyncID);
      }
      try {
        // Engine sync status has changed. Wipe server.
        Logger.warn(LOG_TAG, "Wiping server because engine sync state changed.");
        wipeServer(session);
        Logger.warn(LOG_TAG, "Wiped server because engine sync state changed.");
      } catch (Exception ex) {
        session.abort(ex, "Failed to wipe server after engine sync state changed");
      }
      if (!isEnabled) {
        Logger.warn(LOG_TAG, "Stage has been disabled. Advancing to next stage.");
        session.advance();
        return;
      }
    } catch (MetaGlobalException e) {
      session.abort(e, "Inappropriate meta/global; refusing to execute " + name + " stage.");
      return;
    }

    Synchronizer synchronizer;
    try {
      synchronizer = this.getConfiguredSynchronizer(session);
    } catch (NoCollectionKeysSetException e) {
      session.abort(e, "No CollectionKeys.");
      return;
    } catch (URISyntaxException e) {
      session.abort(e, "Invalid URI syntax for server repository.");
      return;
    } catch (NonObjectJSONException | IOException e) {
      session.abort(e, "Invalid persisted JSON for config.");
      return;
    }

    Logger.debug(LOG_TAG, "Invoking synchronizer.");
    synchronizer.synchronize(session.getContext(), this);
    Logger.debug(LOG_TAG, "Reached end of execute.");
  }

  /**
   * Express the duration taken by this stage as a String, like "0.56 seconds".
   *
   * @return formatted string.
   */
  protected String getStageDurationString() {
    return Utils.formatDuration(stageStartTimestamp, stageCompleteTimestamp);
  }

  /**
   * We synced this engine!  Persist timestamps and advance the session.
   *
   * @param synchronizer the <code>Synchronizer</code> that succeeded.
   */
  @Override
  public void onSynchronized(Synchronizer synchronizer) {
    stageCompleteTimestamp = SystemClock.elapsedRealtime();
    Logger.debug(LOG_TAG, "onSynchronized.");

    SynchronizerConfiguration newConfig = synchronizer.save();
    if (newConfig != null) {
      persistConfig(newConfig);
    } else {
      Logger.warn(LOG_TAG, "Didn't get configuration from synchronizer after success.");
    }

    final SynchronizerSession synchronizerSession = synchronizer.getSynchronizerSession();
    int inboundCount = synchronizerSession.getInboundCount();
    int outboundCount = synchronizerSession.getOutboundCount();
    Logger.info(LOG_TAG, "Stage " + getEngineName() +
        " received " + inboundCount + " and sent " + outboundCount +
        " records in " + getStageDurationString() + ".");
    Logger.info(LOG_TAG, "Advancing session.");
    session.advance();
  }

  /**
   * We failed to sync this engine! Do not persist timestamps (which means that
   * the next sync will include this sync's data), but do advance the session
   * (if we didn't get a Retry-After header).
   *
   * @param synchronizer the <code>Synchronizer</code> that failed.
   */
  @Override
  public void onSynchronizeFailed(Synchronizer synchronizer,
                                  Exception lastException, String reason) {
    stageCompleteTimestamp = SystemClock.elapsedRealtime();
    Logger.warn(LOG_TAG, "Synchronize failed: " + reason, lastException);

    // This failure could be due to a 503 or a 401 and it could have headers.
    // Interrogate the headers but only abort the global session if Retry-After header is set.
    if (lastException instanceof HTTPFailureException) {
      SyncStorageResponse response = ((HTTPFailureException)lastException).response;
      if (response.retryAfterInSeconds() > 0) {
        session.handleHTTPError(response, reason); // Calls session.abort().
        return;
      } else {
        session.interpretHTTPFailure(response.httpResponse()); // Does not call session.abort().
      }
    }

    // Let global session know that this stage is not complete (due to a 412 or hitting a deadline).
    // This stage will be re-synced once current sync is complete.
    if (lastException instanceof ReflowIsNecessaryException) {
      session.handleIncompleteStage();
    }

    Logger.info(LOG_TAG, "Advancing session even though stage failed (took " + getStageDurationString() +
        "). Timestamps not persisted.");
    session.advance();
  }
}
