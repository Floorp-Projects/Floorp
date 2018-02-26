/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.synchronizer;


import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.ReflowIsNecessaryException;
import org.mozilla.gecko.sync.SyncException;
import org.mozilla.gecko.sync.synchronizer.StoreBatchTracker.Batch;
import org.mozilla.gecko.sync.repositories.FetchFailedException;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.RepositorySessionBundle;
import org.mozilla.gecko.sync.repositories.StoreFailedException;
import org.mozilla.gecko.sync.repositories.delegates.DeferredRepositorySessionFinishDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFinishDelegate;

import android.content.Context;

/**
 * I coordinate the moving parts of a sync started by
 * {@link Synchronizer#synchronize}.
 *
 * I flow records twice: first from A to B, and then from B to A. I provide
 * fine-grained feedback by calling my delegate's callback methods.
 *
 * I handle failure cases during flows as follows (in the order they will occur during a sync):
 * <ul>
 * <li>Remote fetch failures abort.</li>
 * <li>Local store failures are ignored.</li>
 * <li>Local fetch failures abort.</li>
 * <li>Remote store failures abort.</li>
 * </ul>
 *
 * Initialize me by creating me with a Synchronizer and a
 * SynchronizerSessionDelegate. Kick things off by calling `init` with two
 * RepositorySessionBundles, and then call `synchronize` in your `onInitialized`
 * callback.
 *
 * I always call exactly one of my delegate's `onInitialized` or
 * `onSessionError` callback methods from `init`.
 *
 * I call my delegate's `onSynchronizeSkipped` callback method if there is no
 * data to be synchronized in `synchronize`.
 *
 * In addition, I call `onFetchError`, `onStoreError`, and `onSessionError` when
 * I encounter a fetch, store, or session error while synchronizing.
 *
 * Typically my delegate will call `abort` in its error callbacks, which will
 * call my delegate's `onSynchronizeAborted` method and halt the sync.
 *
 * I always call exactly one of my delegate's `onSynchronized` or
 * `onSynchronizeFailed` callback methods if I have not seen an error.
 */
public class SynchronizerSession implements RecordsChannelDelegate, RepositorySessionFinishDelegate {

  protected static final String LOG_TAG = "SynchronizerSession";
  protected Synchronizer synchronizer;
  protected SynchronizerSessionDelegate delegate;
  protected Context context;

  /*
   * Computed during init.
   */
  private RepositorySession sessionA;
  private RepositorySession sessionB;
  private RepositorySessionBundle bundleA;
  private RepositorySessionBundle bundleB;

  // Bug 1392505: for each "side" of the channel, we keep track of lastFetch and lastStore timestamps.
  // For local repositories these timestamps represent our last interactions with local data.
  // For the remote repository these timestamps represent server collection's last-modified
  // timestamp after a corresponding operation (GET or POST) finished. We obtain these from server's
  // response headers.
  // It's important that we never compare timestamps which originated from different clocks.
  private long pendingATimestamp = -1;
  private long pendingBTimestamp = -1;
  private long storeEndATimestamp = -1;
  private long storeEndBTimestamp = -1;
  private boolean flowAToBCompleted = false;
  private boolean flowBToACompleted = false;

  private final AtomicInteger numInboundRecords = new AtomicInteger(-1);
  private final AtomicInteger numInboundRecordsStored = new AtomicInteger(-1);
  private final AtomicInteger numInboundRecordsFailed = new AtomicInteger(-1);
  private final AtomicInteger numInboundRecordsReconciled = new AtomicInteger(-1);
  private final AtomicInteger numOutboundRecords = new AtomicInteger(-1);
  private final AtomicInteger numOutboundRecordsStored = new AtomicInteger(-1);
  private final AtomicInteger numOutboundRecordsFailed = new AtomicInteger(-1);
  // Doesn't need to be ConcurrentLinkedQueue or anything like that since we don't do partial
  // changes to it.
  private final AtomicReference<List<Batch>> outboundBatches = new AtomicReference<>();

  private Exception fetchFailedCauseException;
  private Exception storeFailedCauseException;

  /*
   * Public API: constructor, init, synchronize.
   */
  public SynchronizerSession(Synchronizer synchronizer, SynchronizerSessionDelegate delegate) {
    this.setSynchronizer(synchronizer);
    this.delegate = delegate;
  }

  public Synchronizer getSynchronizer() {
    return synchronizer;
  }

  public void setSynchronizer(Synchronizer synchronizer) {
    this.synchronizer = synchronizer;
  }

  public void initAndSynchronize(Context context, RepositorySessionBundle bundleA, RepositorySessionBundle bundleB) {
    this.context = context;
    this.bundleA = bundleA;
    this.bundleB = bundleB;

    try {
      this.sessionA = this.getSynchronizer().repositoryA.createSession(context);
    } catch (SyncException e) {
      // We no longer need a reference to our context.
      this.context = null;
      this.delegate.onSynchronizeFailed(this, e, "Failed to create session");
      return;
    }

    try {
      this.sessionA.unbundle(bundleA);
    } catch (Exception e) {
      this.delegate.onSynchronizeFailed(this, new UnbundleError(e, sessionA), "Failed to unbundle first session.");
      return;
    }

    try {
      this.sessionB = this.getSynchronizer().repositoryB.createSession(context);
    } catch (final SyncException createException) {
      // We no longer need a reference to our context.
      this.context = null;
      // Finish already created sessionA.
      try {
        this.sessionA.finish(new RepositorySessionFinishDelegate() {
          @Override
          public void onFinishFailed(Exception ex) {
            SynchronizerSession.this.delegate.onSynchronizeFailed(SynchronizerSession.this, createException, "Failed to create second session.");
          }

          @Override
          public void onFinishSucceeded(RepositorySession session, RepositorySessionBundle bundle) {
            SynchronizerSession.this.delegate.onSynchronizeFailed(SynchronizerSession.this, createException, "Failed to create second session.");
          }

          @Override
          public RepositorySessionFinishDelegate deferredFinishDelegate(ExecutorService executor) {
            return new DeferredRepositorySessionFinishDelegate(this, executor);
          }
        });
      } catch (InactiveSessionException finishException) {
        SynchronizerSession.this.delegate.onSynchronizeFailed(SynchronizerSession.this, createException, "Failed to create second session.");
      }
      return;
    }

    try {
      this.sessionB.unbundle(bundleB);
    } catch (Exception e) {
      this.delegate.onSynchronizeFailed(this, new UnbundleError(e, sessionA), "Failed to unbundle second session.");
    }

    synchronize();
  }

  /**
   * Get the number of records fetched from the first repository (usually the
   * server, hence inbound).
   * <p>
   * Valid only after first flow has completed.
   *
   * @return number of records, or -1 if not valid.
   */
  public int getInboundCount() {
    return numInboundRecords.get();
  }

  public int getInboundCountStored() {
    return numInboundRecordsStored.get();
  }

  public int getInboundCountFailed() {
    return numInboundRecordsFailed.get();
  }

  public int getInboundCountReconciled() {
    return numInboundRecordsReconciled.get();
  }

  // Returns outboundBatches.
  public List<Batch> getOutboundBatches() {
    return outboundBatches.get();
  }

  /**
   * Get the number of records fetched from the second repository (usually the
   * local store, hence outbound).
   * <p>
   * Valid only after second flow has completed.
   *
   * @return number of records, or -1 if not valid.
   */
  public int getOutboundCount() {
    return numOutboundRecords.get();
  }

  public int getOutboundCountStored() {
    return numOutboundRecordsStored.get();
  }

  public int getOutboundCountFailed() {
    return numOutboundRecordsFailed.get();
  }

  public Exception getFetchFailedCauseException() {
    return fetchFailedCauseException;
  }

  public Exception getStoreFailedCauseException() {
    return storeFailedCauseException;
  }

  // These are accessed by `abort` and `synchronize`, both of which are synchronized.
  // Guarded by `this`.
  protected RecordsChannel channelAToB;
  protected RecordsChannel channelBToA;

  /**
   * Please don't call this until you've been notified with onInitialized.
   */
  private synchronized void synchronize() {
    numInboundRecords.set(-1);
    numInboundRecordsStored.set(-1);
    numInboundRecordsFailed.set(-1);
    numInboundRecordsReconciled.set(-1);
    numOutboundRecords.set(-1);
    numOutboundRecordsStored.set(-1);
    numOutboundRecordsFailed.set(-1);
    outboundBatches.set(null);

    // First thing: decide whether we should.
    if (sessionA.shouldSkip() ||
        sessionB.shouldSkip()) {
      Logger.info(LOG_TAG, "Session requested skip. Short-circuiting sync.");
      sessionA.abort();
      sessionB.abort();
      this.delegate.onSynchronizeSkipped(this);
      return;
    }

    final SynchronizerSession session = this;

    // TODO: failed record handling.

    // This is the *second* record channel to flow.
    // I, SynchronizerSession, am the delegate for the *second* flow.
    channelBToA = getRecordsChannel(this.sessionB, this.sessionA, this);

    // This is the delegate for the *first* flow.
    RecordsChannelDelegate channelAToBDelegate = new RecordsChannelDelegate() {
      @Override
      public void onFlowCompleted(RecordsChannel recordsChannel) {
        session.onFirstFlowCompleted(recordsChannel);
      }

      @Override
      public void onFlowBeginFailed(RecordsChannel recordsChannel, Exception ex) {
        Logger.warn(LOG_TAG, "First RecordsChannel onFlowBeginFailed. Logging session error.", ex);
        session.delegate.onSynchronizeFailed(session, ex, "Failed to begin first flow.");
      }

      @Override
      public void onFlowFetchFailed(RecordsChannel recordsChannel, Exception ex) {
        Logger.warn(LOG_TAG, "First RecordsChannel onFlowFetchFailed. Logging remote fetch error.", ex);
        fetchFailedCauseException = ex;
      }

      @Override
      public void onFlowStoreFailed(RecordsChannel recordsChannel, Exception ex, String recordGuid) {
        Logger.warn(LOG_TAG, "First RecordsChannel onFlowStoreFailed. Logging local store error.", ex);
        // Currently we're just recording the very last exception which occurred. This is a reasonable
        // approach, but ideally we'd want to categorize the exceptions and count them for the purposes
        // of better telemetry. See Bug 1362208.
        storeFailedCauseException = ex;
      }
    };

    // This is the *first* channel to flow.
    channelAToB = getRecordsChannel(this.sessionA, this.sessionB, channelAToBDelegate);

    Logger.trace(LOG_TAG, "Starting A to B flow. Channel is " + channelAToB);
    try {
      channelAToB.beginAndFlow();
    } catch (InvalidSessionTransitionException e) {
      onFlowBeginFailed(channelAToB, e);
    }
  }

  protected RecordsChannel getRecordsChannel(RepositorySession sink, RepositorySession source, RecordsChannelDelegate delegate) {
    return new RecordsChannel(sink, source, delegate);
  }

  /**
   * Called after the first flow completes.
   * <p>
   * By default, any fetch and store failures are ignored.
   * @param recordsChannel the <code>RecordsChannel</code> (for error testing).
   */
  public void onFirstFlowCompleted(RecordsChannel recordsChannel) {
    // If a "reflow exception" was thrown, consider this synchronization failed.
    final ReflowIsNecessaryException reflowException = recordsChannel.getReflowException();
    if (reflowException != null) {
      final String message = "Reflow is necessary: " + reflowException;
      Logger.warn(LOG_TAG, message + " Aborting session.");
      delegate.onSynchronizeFailed(this, reflowException, message);
      return;
    }

    // Fetch failures always abort.
    if (recordsChannel.didFetchFail()) {
      final String message = "Saw failures fetching remote records!";
      Logger.warn(LOG_TAG, message + " Aborting session.");
      delegate.onSynchronizeFailed(this, new FetchFailedException(), message);
      return;
    }
    Logger.trace(LOG_TAG, "No failures fetching remote records.");

    // Local store failures are ignored.
    int numLocalStoreFailed = recordsChannel.getStoreFailureCount();
    if (numLocalStoreFailed > 0) {
      final String message = "Got " + numLocalStoreFailed + " failures storing local records!";
      Logger.warn(LOG_TAG, message + " Ignoring local store failures and continuing synchronizer session.");
    } else {
      Logger.trace(LOG_TAG, "No failures storing local records.");
    }

    Logger.trace(LOG_TAG, "First RecordsChannel onFlowCompleted.");
    pendingATimestamp = sessionA.getLastFetchTimestamp();
    storeEndBTimestamp = sessionB.getLastStoreTimestamp();
    Logger.debug(LOG_TAG, "Fetch end is " + pendingATimestamp + ". Store end is " + storeEndBTimestamp + ". Starting next.");
    numInboundRecords.set(recordsChannel.getFetchCount());
    numInboundRecordsStored.set(recordsChannel.getStoreAcceptedCount());
    numInboundRecordsFailed.set(recordsChannel.getStoreFailureCount());
    numInboundRecordsReconciled.set(recordsChannel.getStoreReconciledCount());
    flowAToBCompleted = true;
    channelBToA.flow();
  }

  /**
   * Called after the second flow completes.
   * <p>
   * By default, any fetch and store failures are ignored.
   * @param recordsChannel the <code>RecordsChannel</code> (for error testing).
   */
  public void onSecondFlowCompleted(RecordsChannel recordsChannel) {
    // If a "reflow exception" was thrown, consider this synchronization failed.
    final ReflowIsNecessaryException reflowException = recordsChannel.getReflowException();
    if (reflowException != null) {
      final String message = "Reflow is necessary: " + reflowException;
      Logger.warn(LOG_TAG, message + " Aborting session.");
      delegate.onSynchronizeFailed(this, reflowException, message);
      return;
    }

    // Fetch failures always abort.
    if (recordsChannel.didFetchFail()) {
      final String message = "Saw failures fetching local records!";
      Logger.warn(LOG_TAG, message + " Aborting session.");
      delegate.onSynchronizeFailed(this, new FetchFailedException(), message);
      return;
    }
    Logger.trace(LOG_TAG, "No failures fetching local records.");

    // Remote store failures abort!
    int numRemoteStoreFailed = recordsChannel.getStoreFailureCount();
    if (numRemoteStoreFailed > 0) {
      final String message = "Got " + numRemoteStoreFailed + " failures storing remote records!";
      Logger.warn(LOG_TAG, message + " Aborting session.");
      delegate.onSynchronizeFailed(this, new StoreFailedException(), message);
      return;
    }
    Logger.trace(LOG_TAG, "No failures storing remote records.");

    Logger.trace(LOG_TAG, "Second RecordsChannel onFlowCompleted.");
    pendingBTimestamp = sessionB.getLastFetchTimestamp();
    storeEndATimestamp = sessionA.getLastStoreTimestamp();
    Logger.debug(LOG_TAG, "Fetch end is " + pendingBTimestamp + ". Store end is " + storeEndATimestamp + ". Finishing.");
    numOutboundRecords.set(recordsChannel.getFetchCount());
    numOutboundRecordsStored.set(recordsChannel.getStoreAcceptedCount());
    numOutboundRecordsFailed.set(recordsChannel.getStoreFailureCount());
    outboundBatches.set(recordsChannel.getStoreBatches());
    flowBToACompleted = true;

    // Finish the two sessions.
    try {
      this.sessionA.finish(this);
    } catch (InactiveSessionException e) {
      this.onFinishFailed(e);
      return;
    }
  }

  @Override
  public void onFlowCompleted(RecordsChannel recordsChannel) {
    onSecondFlowCompleted(recordsChannel);
  }

  @Override
  public void onFlowBeginFailed(RecordsChannel recordsChannel, Exception ex) {
    Logger.warn(LOG_TAG, "Second RecordsChannel onFlowBeginFailed. Logging session error.", ex);
    this.delegate.onSynchronizeFailed(this, ex, "Failed to begin second flow.");
  }

  @Override
  public void onFlowFetchFailed(RecordsChannel recordsChannel, Exception ex) {
    Logger.warn(LOG_TAG, "Second RecordsChannel onFlowFetchFailed. Logging local fetch error.", ex);
  }

  @Override
  public void onFlowStoreFailed(RecordsChannel recordsChannel, Exception ex, String recordGuid) {
    Logger.warn(LOG_TAG, "Second RecordsChannel onFlowStoreFailed. Logging remote store error.", ex);
  }

  /*
   * RepositorySessionFinishDelegate methods.
   */

  /**
   * I could be called twice: once for sessionA and once for sessionB.
   *
   * If sessionB couldn't be created, I don't fail again.
   */
  @Override
  public void onFinishFailed(Exception ex) {
    if (this.sessionB == null) {
      // Ah, it was a problem cleaning up. Never mind.
      Logger.warn(LOG_TAG, "Got exception cleaning up first after second session creation failed.", ex);
      return;
    }
    String session = (this.sessionA == null) ? "B" : "A";
    this.delegate.onSynchronizeFailed(this, ex, "Finish of session " + session + " failed.");
  }

  /**
   * I should be called twice: first for sessionA and second for sessionB.
   *
   * If I am called for sessionA, I try to finish sessionB.
   *
   * If I am called for sessionB, I call my delegate's `onSynchronized` callback
   * method because my flows should have completed.
   */
  @Override
  public void onFinishSucceeded(RepositorySession session,
                                RepositorySessionBundle bundle) {
    Logger.debug(LOG_TAG, "onFinishSucceeded. Flows? " + flowAToBCompleted + ", " + flowBToACompleted);

    if (session == sessionA) {
      if (flowAToBCompleted) {
        Logger.debug(LOG_TAG, "onFinishSucceeded: bumping session A's timestamp to " + pendingATimestamp + " or " + storeEndATimestamp);
        bundle.bumpTimestamp(Math.max(pendingATimestamp, storeEndATimestamp));
        this.synchronizer.bundleA = bundle;
      } else {
        // Should not happen!
        this.delegate.onSynchronizeFailed(this, new UnexpectedSessionException(sessionA), "Failed to finish first session.");
        return;
      }
      if (this.sessionB != null) {
        Logger.trace(LOG_TAG, "Finishing session B.");
        // On to the next.
        try {
          this.sessionB.finish(this);
        } catch (InactiveSessionException e) {
          this.onFinishFailed(e);
          return;
        }
      }
    } else if (session == sessionB) {
      if (flowBToACompleted) {
        Logger.debug(LOG_TAG, "onFinishSucceeded: bumping session B's timestamp to " + pendingBTimestamp + " or " + storeEndBTimestamp);
        bundle.bumpTimestamp(Math.max(pendingBTimestamp, storeEndBTimestamp));
        this.synchronizer.bundleB = bundle;
        Logger.trace(LOG_TAG, "Notifying delegate.onSynchronized.");
        this.delegate.onSynchronized(this);
      } else {
        // Should not happen!
        this.delegate.onSynchronizeFailed(this, new UnexpectedSessionException(sessionB), "Failed to finish second session.");
        return;
      }
    } else {
      // TODO: hurrrrrr...
    }

    if (this.sessionB == null) {
      this.sessionA = null; // We're done.
    }
  }

  @Override
  public RepositorySessionFinishDelegate deferredFinishDelegate(final ExecutorService executor) {
    return new DeferredRepositorySessionFinishDelegate(this, executor);
  }
}
