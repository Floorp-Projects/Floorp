/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.synchronizer;

import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.atomic.AtomicInteger;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.ThreadPool;
import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;
import org.mozilla.gecko.sync.repositories.NoStoreDelegateException;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.DeferredRepositorySessionBeginDelegate;
import org.mozilla.gecko.sync.repositories.delegates.DeferredRepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionBeginDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

/**
 * Pulls records from `source`, applying them to `sink`.
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
 * This whole set of guarantees should be achievable thusly:
 *
 * * The fetch process must run in a single thread, and invoke store()
 *   synchronously. After processing every incoming record, storeDone is called,
 *   setting a flag.
 *   If the fetch cannot be implicitly queued, it must be explicitly queued.
 *   In this implementation, we assume that fetch callbacks are strictly ordered in this way.
 *
 * * The store process must be (implicitly or explicitly) queued. When the
 *   queue empties, the consumer checks the storeDone flag. If it's set, and the
 *   queue is exhausted, invoke onStoreCompleted.
 *
 * RecordsChannel exists to enforce this ordering of operations.
 *
 * @author rnewman
 *
 */
public class RecordsChannel implements
  RepositorySessionFetchRecordsDelegate,
  RepositorySessionStoreDelegate,
  RecordsConsumerDelegate,
  RepositorySessionBeginDelegate {

  private static final String LOG_TAG = "RecordsChannel";
  public RepositorySession source;
  public RepositorySession sink;
  private final RecordsChannelDelegate delegate;
  private long fetchEnd = -1;

  protected final AtomicInteger numFetched = new AtomicInteger();
  protected final AtomicInteger numFetchFailed = new AtomicInteger();
  protected final AtomicInteger numStored = new AtomicInteger();
  protected final AtomicInteger numStoreFailed = new AtomicInteger();

  public RecordsChannel(RepositorySession source, RepositorySession sink, RecordsChannelDelegate delegate) {
    this.source    = source;
    this.sink      = sink;
    this.delegate  = delegate;
  }

  /*
   * We push fetched records into a queue.
   * A separate thread is waiting for us to notify it of work to do.
   * When we tell it to stop, it'll stop. We do that when the fetch
   * is completed.
   * When it stops, we tell the sink that there are no more records,
   * and wait for the sink to tell us that storing is done.
   * Then we notify our delegate of completion.
   */
  private RecordConsumer consumer;
  private boolean waitingForQueueDone = false;
  private final ConcurrentLinkedQueue<Record> toProcess = new ConcurrentLinkedQueue<Record>();

  @Override
  public ConcurrentLinkedQueue<Record> getQueue() {
    return toProcess;
  }

  protected boolean isReady() {
    return source.isActive() && sink.isActive();
  }

  /**
   * Get the number of records fetched so far.
   *
   * @return number of fetches.
   */
  public int getFetchCount() {
    return numFetched.get();
  }

  /**
   * Get the number of fetch failures recorded so far.
   *
   * @return number of fetch failures.
   */
  public int getFetchFailureCount() {
    return numFetchFailed.get();
  }

  /**
   * Get the number of store attempts (successful or not) so far.
   *
   * @return number of stores attempted.
   */
  public int getStoreCount() {
    return numStored.get();
  }

  /**
   * Get the number of store failures recorded so far.
   *
   * @return number of store failures.
   */
  public int getStoreFailureCount() {
    return numStoreFailed.get();
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
      long now = System.currentTimeMillis();
      this.delegate.onFlowCompleted(this, now, now);
      return;
    }

    sink.setStoreDelegate(this);
    numFetched.set(0);
    numFetchFailed.set(0);
    numStored.set(0);
    numStoreFailed.set(0);
    // Start a consumer thread.
    this.consumer = new ConcurrentRecordConsumer(this);
    ThreadPool.run(this.consumer);
    waitingForQueueDone = true;

    // Fetch all records that were modified since our previous flow. If our previous flow succeeded,
    // we will use source's last-sync timestamp. If our previous flow didn't complete, resume it,
    // starting from sink's high water mark timestamp.
    // If there was no previous flow (first sync, or data was cleared...), fetch everything.
    // Resuming a flow is supported for buffered RepositorySessions. We degrade gracefully otherwise.
    final long highWaterMark = sink.getHighWaterMarkTimestamp();
    final long lastSync = source.getLastSyncTimestamp();
    final long sinceTimestamp = Math.max(highWaterMark, lastSync);
    source.fetchSince(sinceTimestamp, this);
  }

  /**
   * Begin both sessions, invoking flow() when done.
   * @throws InvalidSessionTransitionException
   */
  public void beginAndFlow() throws InvalidSessionTransitionException {
    Logger.trace(LOG_TAG, "Beginning source.");
    source.begin(this);
  }

  @Override
  public void store(Record record) {
    numStored.incrementAndGet();
    try {
      sink.store(record);
    } catch (NoStoreDelegateException e) {
      Logger.error(LOG_TAG, "Got NoStoreDelegateException in RecordsChannel.store(). This should not occur. Aborting.", e);
      delegate.onFlowStoreFailed(this, e, record.guid);
    }
  }

  @Override
  public void onFetchFailed(Exception ex) {
    Logger.warn(LOG_TAG, "onFetchFailed. Informing sink, calling for immediate stop.", ex);
    numFetchFailed.incrementAndGet();
    this.consumer.halt();
    delegate.onFlowFetchFailed(this, ex);
  }

  @Override
  public void onFetchedRecord(Record record) {
    numFetched.incrementAndGet();
    this.toProcess.add(record);
    this.consumer.doNotify();
  }

  @Override
  public void onFetchCompleted(final long fetchEnd) {
    Logger.trace(LOG_TAG, "onFetchCompleted. Stopping consumer once stores are done.");
    Logger.trace(LOG_TAG, "Fetch timestamp is " + fetchEnd);
    this.fetchEnd = fetchEnd;
    this.consumer.queueFilled();
  }

  @Override
  public void onBatchCompleted() {
  }

  @Override
  public void onRecordStoreFailed(Exception ex, String recordGuid) {
    Logger.trace(LOG_TAG, "Failed to store record with guid " + recordGuid);
    numStoreFailed.incrementAndGet();
    this.consumer.stored();
    delegate.onFlowStoreFailed(this, ex, recordGuid);
    // TODO: abort?
  }

  @Override
  public void onRecordStoreSucceeded(String guid) {
    Logger.trace(LOG_TAG, "Stored record with guid " + guid);
    this.consumer.stored();
  }


  @Override
  public void consumerIsDone(boolean allRecordsQueued) {
    Logger.trace(LOG_TAG, "Consumer is done. Are we waiting for it? " + waitingForQueueDone);
    if (waitingForQueueDone) {
      waitingForQueueDone = false;
      if (!allRecordsQueued) {
        this.sink.storeIncomplete();
      }
      this.sink.storeDone(); // Now we'll be waiting for onStoreCompleted.
    }
  }

  @Override
  public void onStoreCompleted(long storeEnd) {
    Logger.trace(LOG_TAG, "onStoreCompleted. Notifying delegate of onFlowCompleted. " +
                          "Fetch end is " + fetchEnd + ", store end is " + storeEnd);
    // Source might have used caches used to facilitate flow of records, so now is a good
    // time to clean up. Particularly pertinent for buffered sources.
    this.source.performCleanup();
    // TODO: synchronize on consumer callback?
    delegate.onFlowCompleted(this, fetchEnd, storeEnd);
  }

  @Override
  public void onBeginFailed(Exception ex) {
    delegate.onFlowBeginFailed(this, ex);
  }

  @Override
  public void onBeginSucceeded(RepositorySession session) {
    if (session == source) {
      Logger.trace(LOG_TAG, "Source session began. Beginning sink session.");
      try {
        sink.begin(this);
      } catch (InvalidSessionTransitionException e) {
        onBeginFailed(e);
        return;
      }
    }
    if (session == sink) {
      Logger.trace(LOG_TAG, "Sink session began. Beginning flow.");
      this.flow();
      return;
    }

    // TODO: error!
  }

  @Override
  public RepositorySessionStoreDelegate deferredStoreDelegate(final ExecutorService executor) {
    return new DeferredRepositorySessionStoreDelegate(this, executor);
  }

  @Override
  public RepositorySessionBeginDelegate deferredBeginDelegate(final ExecutorService executor) {
    return new DeferredRepositorySessionBeginDelegate(this, executor);
  }

  @Override
  public RepositorySessionFetchRecordsDelegate deferredFetchDelegate(ExecutorService executor) {
    // Lie outright. We know that all of our fetch methods are safe.
    return this;
  }
}
