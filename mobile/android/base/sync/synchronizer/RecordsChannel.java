/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Android Sync Client.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Richard Newman <rnewman@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko.sync.synchronizer;

import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.ExecutorService;

import org.mozilla.gecko.sync.ThreadPool;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.NoStoreDelegateException;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.DeferredRepositorySessionBeginDelegate;
import org.mozilla.gecko.sync.repositories.delegates.DeferredRepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionBeginDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.util.Log;

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
class RecordsChannel implements
  RepositorySessionFetchRecordsDelegate,
  RepositorySessionStoreDelegate,
  RecordsConsumerDelegate,
  RepositorySessionBeginDelegate {

  private static final String LOG_TAG = "RecordsChannel";
  public RepositorySession source;
  public RepositorySession sink;
  private RecordsChannelDelegate delegate;
  private long timestamp;
  private long end = -1;                     // Oo er, missus.

  public RecordsChannel(RepositorySession source, RepositorySession sink, RecordsChannelDelegate delegate) {
    this.source    = source;
    this.sink      = sink;
    this.delegate  = delegate;
    this.timestamp = source.lastSyncTimestamp;
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
  private ConcurrentLinkedQueue<Record> toProcess = new ConcurrentLinkedQueue<Record>();

  @Override
  public ConcurrentLinkedQueue<Record> getQueue() {
    return toProcess;
  }

  protected boolean isReady() {
    return source.isActive() && sink.isActive();
  }


  private static void info(String message) {
    Utils.logToStdout(LOG_TAG, "::INFO: ", message);
    Log.i(LOG_TAG, message);
  }

  private static void trace(String message) {
    if (!Utils.ENABLE_TRACE_LOGGING) {
      return;
    }
    Utils.logToStdout(LOG_TAG, "::TRACE: ", message);
    Log.d(LOG_TAG, message);
  }

  private static void error(String message, Exception e) {
    Utils.logToStdout(LOG_TAG, "::ERROR: ", message);
    Log.e(LOG_TAG, message, e);
  }

  private static void warn(String message, Exception e) {
    Utils.logToStdout(LOG_TAG, "::WARN: ", message);
    Log.w(LOG_TAG, message, e);
  }

  /**
   * Attempt to abort an outstanding fetch. Finish both sessions.
   */
  public void abort() {
    if (source.isActive()) {
      source.abort();
    }
    if (sink.isActive()) {
      sink.abort();
    }
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
    }
    sink.setStoreDelegate(this);
    // Start a consumer thread.
    this.consumer = new ConcurrentRecordConsumer(this);
    ThreadPool.run(this.consumer);
    waitingForQueueDone = true;
    source.fetchSince(timestamp, this);
  }

  /**
   * Begin both sessions, invoking flow() when done.
   */
  public void beginAndFlow() {
    info("Beginning source.");
    source.begin(this);
  }

  @Override
  public void store(Record record) {
    try {
      sink.store(record);
    } catch (NoStoreDelegateException e) {
      error("Got NoStoreDelegateException in RecordsChannel.store(). This should not occur. Aborting.", e);
      delegate.onFlowStoreFailed(this, e);
      this.abort();
    }
  }

  @Override
  public void onFetchFailed(Exception ex, Record record) {
    warn("onFetchFailed. Calling for immediate stop.", ex);
    this.consumer.halt();
  }

  @Override
  public void onFetchedRecord(Record record) {
    this.toProcess.add(record);
    this.consumer.doNotify();
  }

  @Override
  public void onFetchSucceeded(Record[] records, long end) {
    for (Record record : records) {
      this.toProcess.add(record);
    }
    this.consumer.doNotify();
    this.onFetchCompleted(end);
  }

  @Override
  public void onFetchCompleted(long end) {
    info("onFetchCompleted. Stopping consumer once stores are done.");
    info("Fetch timestamp is " + end);
    this.end = end;
    this.consumer.queueFilled();
  }

  @Override
  public void onRecordStoreFailed(Exception ex) {
    this.consumer.stored();
    delegate.onFlowStoreFailed(this, ex);
    // TODO: abort?
  }

  @Override
  public void onRecordStoreSucceeded(Record record) {
    this.consumer.stored();
  }


  @Override
  public void consumerIsDone(boolean allRecordsQueued) {
    trace("Consumer is done. Are we waiting for it? " + waitingForQueueDone);
    if (waitingForQueueDone) {
      waitingForQueueDone = false;
      this.sink.storeDone();                 // Now we'll be waiting for onStoreCompleted.
    }
  }

  @Override
  public void onStoreCompleted() {
    info("onStoreCompleted. Notifying delegate of onFlowCompleted. End is " + end);
    // TODO: synchronize on consumer callback?
    delegate.onFlowCompleted(this, end);
  }

  @Override
  public void onBeginFailed(Exception ex) {
    delegate.onFlowBeginFailed(this, ex);
  }

  @Override
  public void onBeginSucceeded(RepositorySession session) {
    if (session == source) {
      info("Source session began. Beginning sink session.");
      sink.begin(this);
    }
    if (session == sink) {
      info("Sink session began. Beginning flow.");
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
