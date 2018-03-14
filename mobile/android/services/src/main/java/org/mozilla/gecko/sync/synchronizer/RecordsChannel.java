/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.synchronizer;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import java.util.ArrayList;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.ReflowIsNecessaryException;
import org.mozilla.gecko.sync.SyncException;
import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;
import org.mozilla.gecko.sync.repositories.NoStoreDelegateException;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.DeferredRepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

/**
 * Pulls records from `source`, buffering them and then applying them to `sink` when
 * all records have been pulled.
 * Notifies its delegate of errors and completion.
 *
 * All stores (initiated by a fetch) must have been completed before storeDone
 * is invoked on the sink. This is to avoid the existing stored items being
 * considered as the total set, with onStoreCompleted being called when they're
 * done:
 *
 *   store(A) store(B)
 *   store(C) storeDone()
 *   store(A) finishes. Store job begins.
 *   store(C) finishes. Store job begins.
 *   storeDone() finishes.
 *   Storing of A complete.
 *   Storing of C complete.
 *   We're done! Call onStoreCompleted.
 *   store(B) finishes... uh oh.
 *
 * In other words, storeDone must be gated on the synchronous invocation of every store.
 *
 * Similarly, we require that every store callback have returned before onStoreCompleted is invoked.
 *
 * RecordsChannel exists to enforce this ordering of operations.
 *
 * @author rnewman
 *
 */
public class RecordsChannel implements
  RepositorySessionFetchRecordsDelegate,
  RepositorySessionStoreDelegate {

  private static final String LOG_TAG = "RecordsChannel";
  public RepositorySession source;
  /* package-private */ RepositorySession sink;
  private final RecordsChannelDelegate delegate;

  private volatile ReflowIsNecessaryException reflowException;

  /* package-private */ final AtomicInteger fetchedCount = new AtomicInteger(0);
  final AtomicBoolean fetchFailed = new AtomicBoolean(false);
  private final AtomicBoolean storeFailed = new AtomicBoolean(false);

  private ArrayList<Record> toProcess = new ArrayList<>();

  // Expected value relationships:
  // attempted = accepted + failed
  // reconciled <= accepted <= attempted
  // reconciled = accepted - `new`, where `new` is inferred.
  // TODO these likely don't need to be Atomic, see Bug 1441281.
  final AtomicInteger storeAttemptedCount = new AtomicInteger(0);
  private final AtomicInteger storeAcceptedCount = new AtomicInteger(0);
  private final AtomicInteger storeFailedCount = new AtomicInteger(0);
  private final AtomicInteger storeReconciledCount = new AtomicInteger(0);

  private final StoreBatchTracker storeTracker = new StoreBatchTracker();

  public RecordsChannel(RepositorySession source, RepositorySession sink, RecordsChannelDelegate delegate) {
    this.source = source;
    this.sink = sink;
    this.delegate = delegate;
  }

  protected boolean isReady() {
    return source.isActive() && sink.isActive();
  }

  /**
   * Get the number of records fetched so far.
   *
   * @return number of fetches.
   */
  /* package-private */ int getFetchCount() {
    return fetchedCount.get();
  }

  /**
   * Get the number of fetch failures recorded so far.
   *
   * @return number of fetch failures.
   */
  public boolean didFetchFail() {
    return fetchFailed.get();
  }

  /**
   * Get the number of store attempts (successful or not) so far.
   *
   * @return number of stores attempted.
   */
  public int getStoreAttemptedCount() {
    return storeAttemptedCount.get();
  }

  /* package-private */ int getStoreAcceptedCount() {
    return storeAcceptedCount.get();
  }

  /**
   * Get the number of store failures recorded so far.
   *
   * @return number of store failures.
   */
  public int getStoreFailureCount() {
    return storeFailedCount.get();
  }

  /* package-private */ int getStoreReconciledCount() {
    return storeReconciledCount.get();
  }

  /**
   * Start records flowing through the channel.
   */
  public void flow() {
    if (!isReady()) {
      RepositorySession failed = source;
      if (source.isActive()) {
        failed = sink;
      }
      this.delegate.onFlowBeginFailed(this, new SessionNotBegunException(failed));
      return;
    }

    if (!source.dataAvailable()) {
      Logger.info(LOG_TAG, "No data available: short-circuiting flow from source " + source);
      this.delegate.onFlowCompleted(this);
      return;
    }

    sink.setStoreDelegate(this);
    storeTracker.reset();
    source.fetchModified(this);
  }

  /**
   * Begin both sessions, invoking flow() when done.
   * @throws InvalidSessionTransitionException
   */
  public void beginAndFlow() throws InvalidSessionTransitionException {
    try {
      Logger.trace(LOG_TAG, "Beginning source.");
      source.begin();
      Logger.trace(LOG_TAG, "Beginning sink.");
      sink.begin();
    } catch (SyncException e) {
      delegate.onFlowBeginFailed(this, e);
      return;
    }

    this.flow();
  }

  /* package-local */ ArrayList<StoreBatchTracker.Batch> getStoreBatches() {
    return this.storeTracker.getStoreBatches();
  }

  @Override
  public void onFetchFailed(Exception ex) {
    Logger.warn(LOG_TAG, "onFetchFailed. Calling for immediate stop.", ex);
    if (fetchFailed.getAndSet(true)) {
      return;
    }

    if (ex instanceof ReflowIsNecessaryException) {
      setReflowException((ReflowIsNecessaryException) ex);
    }

    delegate.onFlowFetchFailed(this, ex);

    // We haven't tried storing anything yet, so fine to short-circuit around storeDone.
    delegate.onFlowCompleted(this);
  }

  @Override
  public void onFetchedRecord(Record record) {
    // Don't bother if we've already failed; we'll just ignore these records later on.
    if (fetchFailed.get()) {
      return;
    }
    this.toProcess.add(record);
  }

  @Override
  public void onFetchCompleted() {
    if (fetchFailed.get()) {
      return;
    }

    fetchedCount.set(toProcess.size());

    Logger.info(LOG_TAG, "onFetchCompleted. Fetched " + fetchedCount.get() + " records. Storing...");

    try {
      for (Record record : toProcess) {
        storeAttemptedCount.incrementAndGet();
        storeTracker.onRecordStoreAttempted();
        sink.store(record);
      }
    } catch (NoStoreDelegateException e) {
      // Must not happen, bail out.
      throw new IllegalStateException(e);
    }

    // Allow this buffer to be reclaimed.
    toProcess = null;

    // It's possible that one of the delegate-driven `store` calls above failed.
    // In that case, 'onStoreFailed' would have been already called, and we have nothing left to do.
    if (storeFailed.get()) {
      Logger.info(LOG_TAG, "Store failed while processing records via sink.store(...); bailing out.");
      return;
    }

    // Now we wait for onStoreComplete
    Logger.trace(LOG_TAG, "onFetchCompleted. Calling storeDone.");
    sink.storeDone();
  }

  // Sent for "store" batches.
  @Override
  public void onBatchCommitted() {
    storeTracker.onBatchFinished();
  }

  @Override
  public void onRecordStoreFailed(Exception ex, String recordGuid) {
    Logger.trace(LOG_TAG, "Failed to store record with guid " + recordGuid);
    storeFailedCount.incrementAndGet();
    storeTracker.onRecordStoreFailed();
    delegate.onFlowStoreFailed(this, ex, recordGuid);
  }

  @Override
  public void onRecordStoreSucceeded(int count) {
    storeAcceptedCount.addAndGet(count);
    storeTracker.onRecordStoreSucceeded(count);
  }

  @Override
  public void onRecordStoreReconciled(String guid, String oldGuid, Integer newVersion) {
    storeReconciledCount.incrementAndGet();
  }

  @Override
  public void onStoreCompleted() {
    Logger.info(LOG_TAG, "Performing source cleanup.");
    // Source might have used caches used to facilitate flow of records, so now is a good
    // time to clean up. Particularly pertinent for buffered sources.
    // Rephrasing this in a more concrete way, buffers are cleared only once records have been merged
    // locally and results of the merge have been uploaded to the server successfully.
    this.source.performCleanup();

    if (storeFailed.get()) {
      return;
    }

    Logger.info(LOG_TAG, "onStoreCompleted. Attempted to store " + storeAttemptedCount.get() + " records; Store accepted " + storeAcceptedCount.get() + ", reconciled " + storeReconciledCount.get() + ", failed " + storeFailedCount.get());
    delegate.onFlowCompleted(this);
  }

  @Override
  public void onStoreFailed(Exception ex) {
    if (storeFailed.getAndSet(true)) {
      return;
    }

    Logger.info(LOG_TAG, "onStoreFailed. Calling for immediate stop.", ex);
    if (ex instanceof ReflowIsNecessaryException) {
      setReflowException((ReflowIsNecessaryException) ex);
    }

    storeTracker.onBatchFailed();

    // NB: consumer might or might not be running at this point. There are two cases here:
    // 1) If we're storing records remotely, we might fail due to a 412.
    // -- we might hit 412 at any point, so consumer might be in either state.
    // Action: ignore consumer state, we have nothing else to do other to inform our delegate
    // that we're done with this flow. Based on the reflow exception, it'll determine what to do.

    // 2) If we're storing (merging) records locally, we might fail due to a sync deadline.
    // -- we might hit a deadline only prior to attempting to merge records,
    // -- at which point consumer would have finished already, and storeDone was called.
    // Action: consumer state is known (done), so we can ignore it safely and inform our delegate
    // that we're done.

    delegate.onFlowStoreFailed(this, ex, null);
    delegate.onFlowCompleted(this);
  }

  @Override
  public RepositorySessionStoreDelegate deferredStoreDelegate(final ExecutorService executor) {
    return new DeferredRepositorySessionStoreDelegate(this, executor);
  }

  @Override
  public RepositorySessionFetchRecordsDelegate deferredFetchDelegate(ExecutorService executor) {
    // Lie outright. We know that all of our fetch methods are safe.
    return this;
  }

  @Nullable
  public synchronized ReflowIsNecessaryException getReflowException() {
    return reflowException;
  }

  private synchronized void setReflowException(@NonNull ReflowIsNecessaryException e) {
    // It is a mistake to set reflow exception multiple times.
    if (reflowException != null) {
      throw new IllegalStateException("Reflow exception already set: " + reflowException);
    }
    reflowException = e;
  }
}
