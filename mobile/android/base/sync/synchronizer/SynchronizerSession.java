/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.synchronizer;


import java.util.concurrent.ExecutorService;
import java.util.concurrent.atomic.AtomicInteger;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.RepositorySessionBundle;
import org.mozilla.gecko.sync.repositories.delegates.DeferrableRepositorySessionCreationDelegate;
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
public class SynchronizerSession
extends DeferrableRepositorySessionCreationDelegate
implements RecordsChannelDelegate,
           RepositorySessionFinishDelegate {

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

  // Bug 726054: just like desktop, we track our last interaction with the server,
  // not the last record timestamp that we fetched. This ensures that we don't re-
  // download the records we just uploaded, at the cost of skipping any records
  // that a concurrently syncing client has uploaded.
  private long pendingATimestamp = -1;
  private long pendingBTimestamp = -1;
  private long storeEndATimestamp = -1;
  private long storeEndBTimestamp = -1;
  private boolean flowAToBCompleted = false;
  private boolean flowBToACompleted = false;

  protected final AtomicInteger numInboundRecords = new AtomicInteger(-1);
  protected final AtomicInteger numOutboundRecords = new AtomicInteger(-1);

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

  public void init(Context context, RepositorySessionBundle bundleA, RepositorySessionBundle bundleB) {
    this.context = context;
    this.bundleA = bundleA;
    this.bundleB = bundleB;
    // Begin sessionA and sessionB, call onInitialized in callbacks.
    this.getSynchronizer().repositoryA.createSession(this, context);
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

  // These are accessed by `abort` and `synchronize`, both of which are synchronized.
  // Guarded by `this`.
  protected RecordsChannel channelAToB;
  protected RecordsChannel channelBToA;

  /**
   * Please don't call this until you've been notified with onInitialized.
   */
  public synchronized void synchronize() {
    numInboundRecords.set(-1);
    numOutboundRecords.set(-1);

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
    channelBToA = new RecordsChannel(this.sessionB, this.sessionA, this);

    // This is the delegate for the *first* flow.
    RecordsChannelDelegate channelAToBDelegate = new RecordsChannelDelegate() {
      public void onFlowCompleted(RecordsChannel recordsChannel, long fetchEnd, long storeEnd) {
        session.onFirstFlowCompleted(recordsChannel, fetchEnd, storeEnd);
      }

      @Override
      public void onFlowBeginFailed(RecordsChannel recordsChannel, Exception ex) {
        Logger.warn(LOG_TAG, "First RecordsChannel onFlowBeginFailed. Logging session error.", ex);
        session.delegate.onSynchronizeFailed(session, ex, "Failed to begin first flow.");
      }

      @Override
      public void onFlowFetchFailed(RecordsChannel recordsChannel, Exception ex) {
        Logger.warn(LOG_TAG, "First RecordsChannel onFlowFetchFailed. Logging remote fetch error.", ex);
      }

      @Override
      public void onFlowStoreFailed(RecordsChannel recordsChannel, Exception ex, String recordGuid) {
        Logger.warn(LOG_TAG, "First RecordsChannel onFlowStoreFailed. Logging local store error.", ex);
      }

      @Override
      public void onFlowFinishFailed(RecordsChannel recordsChannel, Exception ex) {
        Logger.warn(LOG_TAG, "First RecordsChannel onFlowFinishedFailed. Logging session error.", ex);
        session.delegate.onSynchronizeFailed(session, ex, "Failed to finish first flow.");
      }
    };

    // This is the *first* channel to flow.
    channelAToB = new RecordsChannel(this.sessionA, this.sessionB, channelAToBDelegate);

    Logger.trace(LOG_TAG, "Starting A to B flow. Channel is " + channelAToB);
    try {
      channelAToB.beginAndFlow();
    } catch (InvalidSessionTransitionException e) {
      onFlowBeginFailed(channelAToB, e);
    }
  }

  /**
   * Called after the first flow completes.
   * <p>
   * By default, any fetch and store failures are ignored.
   * @param recordsChannel the <code>RecordsChannel</code> (for error testing).
   * @param fetchEnd timestamp when fetches completed.
   * @param storeEnd timestamp when stores completed.
   */
  public void onFirstFlowCompleted(RecordsChannel recordsChannel, long fetchEnd, long storeEnd) {
    Logger.trace(LOG_TAG, "First RecordsChannel onFlowCompleted.");
    Logger.debug(LOG_TAG, "Fetch end is " + fetchEnd + ". Store end is " + storeEnd + ". Starting next.");
    pendingATimestamp = fetchEnd;
    storeEndBTimestamp = storeEnd;
    numInboundRecords.set(recordsChannel.getFetchCount());
    flowAToBCompleted = true;
    channelBToA.flow();
  }

  /**
   * Called after the second flow completes.
   * <p>
   * By default, any fetch and store failures are ignored.
   * @param recordsChannel the <code>RecordsChannel</code> (for error testing).
   * @param fetchEnd timestamp when fetches completed.
   * @param storeEnd timestamp when stores completed.
   */
  public void onSecondFlowCompleted(RecordsChannel recordsChannel, long fetchEnd, long storeEnd) {
    Logger.trace(LOG_TAG, "Second RecordsChannel onFlowCompleted.");
    Logger.debug(LOG_TAG, "Fetch end is " + fetchEnd + ". Store end is " + storeEnd + ". Finishing.");

    pendingBTimestamp = fetchEnd;
    storeEndATimestamp = storeEnd;
    numOutboundRecords.set(recordsChannel.getFetchCount());
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
  public void onFlowCompleted(RecordsChannel recordsChannel, long fetchEnd, long storeEnd) {
    onSecondFlowCompleted(recordsChannel, fetchEnd, storeEnd);
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

  @Override
  public void onFlowFinishFailed(RecordsChannel recordsChannel, Exception ex) {
    Logger.warn(LOG_TAG, "Second RecordsChannel onFlowFinishedFailed. Logging session error.", ex);
    this.delegate.onSynchronizeFailed(this, ex, "Failed to finish second flow.");
  }

  /*
   * RepositorySessionCreationDelegate methods.
   */

  /**
   * I could be called twice: once for sessionA and once for sessionB.
   *
   * I try to clean up sessionA if it is not null, since the creation of
   * sessionB must have failed.
   */
  @Override
  public void onSessionCreateFailed(Exception ex) {
    // Attempt to finish the first session, if the second is the one that failed.
    if (this.sessionA != null) {
      try {
        // We no longer need a reference to our context.
        this.context = null;
        this.sessionA.finish(this);
      } catch (Exception e) {
        // Never mind; best-effort finish.
      }
    }
    // We no longer need a reference to our context.
    this.context = null;
    this.delegate.onSynchronizeFailed(this, ex, "Failed to create session");
  }

  /**
   * I should be called twice: first for sessionA and second for sessionB.
   *
   * If I am called for sessionB, I call my delegate's `onInitialized` callback
   * method because my repository sessions are correctly initialized.
   */
  // TODO: some of this "finish and clean up" code can be refactored out.
  @Override
  public void onSessionCreated(RepositorySession session) {
    if (session == null ||
        this.sessionA == session) {
      // TODO: clean up sessionA.
      this.delegate.onSynchronizeFailed(this, new UnexpectedSessionException(session), "Failed to create session.");
      return;
    }
    if (this.sessionA == null) {
      this.sessionA = session;

      // Unbundle.
      try {
        this.sessionA.unbundle(this.bundleA);
      } catch (Exception e) {
        this.delegate.onSynchronizeFailed(this, new UnbundleError(e, sessionA), "Failed to unbundle first session.");
        // TODO: abort
        return;
      }
      this.getSynchronizer().repositoryB.createSession(this, this.context);
      return;
    }
    if (this.sessionB == null) {
      this.sessionB = session;
      // We no longer need a reference to our context.
      this.context = null;

      // Unbundle. We unbundled sessionA when that session was created.
      try {
        this.sessionB.unbundle(this.bundleB);
      } catch (Exception e) {
        this.delegate.onSynchronizeFailed(this, new UnbundleError(e, sessionA), "Failed to unbundle second session.");
        return;
      }

      this.delegate.onInitialized(this);
      return;
    }
    // TODO: need a way to make sure we don't call any more delegate methods.
    this.delegate.onSynchronizeFailed(this, new UnexpectedSessionException(session), "Failed to create session.");
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
