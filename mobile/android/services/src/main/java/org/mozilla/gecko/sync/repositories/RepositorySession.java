/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionBeginDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFinishDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionGuidsSinceDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

/**
 * A <code>RepositorySession</code> is created and used thusly:
 *
 *<ul>
 * <li>Construct, with a reference to its parent {@link Repository}, by calling
 *   {@link Repository#createSession(RepositorySessionCreationDelegate, android.content.Context)}.</li>
 * <li>Populate with saved information by calling {@link #unbundle(RepositorySessionBundle)}.</li>
 * <li>Begin a sync by calling {@link #begin(RepositorySessionBeginDelegate)}. <code>begin()</code>
 *   is an appropriate place to initialize expensive resources.</li>
 * <li>Perform operations such as {@link #fetchSince(long, RepositorySessionFetchRecordsDelegate)} and
 *   {@link #store(Record)}.</li>
 * <li>Finish by calling {@link #finish(RepositorySessionFinishDelegate)}, retrieving and storing
 *   the current bundle.</li>
 *</ul>
 *
 * If <code>finish()</code> is not called, {@link #abort()} must be called. These calls must
 * <em>always</em> be paired with <code>begin()</code>.
 *
 */
public abstract class RepositorySession {

  public enum SessionStatus {
    UNSTARTED,
    ACTIVE,
    ABORTED,
    DONE
  }

  private static final String LOG_TAG = "RepositorySession";

  protected static void trace(String message) {
    Logger.trace(LOG_TAG, message);
  }

  private SessionStatus status = SessionStatus.UNSTARTED;
  protected Repository repository;
  protected RepositorySessionStoreDelegate delegate;

  /**
   * A queue of Runnables which call out into delegates.
   */
  protected ExecutorService delegateQueue  = Executors.newSingleThreadExecutor();

  /**
   * A queue of Runnables which effect storing.
   * This includes actual store work, and also the consequences of storeDone.
   * This provides strict ordering.
   */
  protected ExecutorService storeWorkQueue = Executors.newSingleThreadExecutor();

  // The time that the last sync on this collection completed, in milliseconds since epoch.
  private long lastSyncTimestamp = 0;

  public long getLastSyncTimestamp() {
    return lastSyncTimestamp;
  }

  public static long now() {
    return System.currentTimeMillis();
  }

  public RepositorySession(Repository repository) {
    this.repository = repository;
  }

  public abstract void guidsSince(long timestamp, RepositorySessionGuidsSinceDelegate delegate);
  public abstract void fetchSince(long timestamp, RepositorySessionFetchRecordsDelegate delegate);
  public abstract void fetch(String[] guids, RepositorySessionFetchRecordsDelegate delegate) throws InactiveSessionException;
  public abstract void fetchAll(RepositorySessionFetchRecordsDelegate delegate);

  /**
   * Override this if you wish to short-circuit a sync when you know --
   * e.g., by inspecting the database or info/collections -- that no new
   * data are available.
   *
   * @return true if a sync should proceed.
   */
  public boolean dataAvailable() {
    return true;
  }

  /**
   * @return true if we cannot safely sync from this <code>RepositorySession</code>.
   */
  public boolean shouldSkip() {
    return false;
  }

  /*
   * Store operations proceed thusly:
   *
   * * Set a delegate
   * * Store an arbitrary number of records. At any time the delegate can be
   *   notified of an error.
   * * Call storeDone to notify the session that no more items are forthcoming.
   * * The store delegate will be notified of error or completion.
   *
   * This arrangement of calls allows for batching at the session level.
   *
   * Store success calls are not guaranteed.
   */
  public void setStoreDelegate(RepositorySessionStoreDelegate delegate) {
    Logger.debug(LOG_TAG, "Setting store delegate to " + delegate);
    this.delegate = delegate;
  }
  public abstract void store(Record record) throws NoStoreDelegateException;

  public void storeIncomplete() {}

  public void storeDone() {
    // Our default behavior will be to assume that the Runnable is
    // executed as soon as all the stores synchronously finish, so
    // our end timestamp can just be… now.
    storeDone(now());
  }

  public void storeDone(final long end) {
    Logger.debug(LOG_TAG, "Scheduling onStoreCompleted for after storing is done: " + end);
    Runnable command = new Runnable() {
      @Override
      public void run() {
        delegate.onStoreCompleted(end);
      }
    };
    storeWorkQueue.execute(command);
  }

  public abstract void wipe(RepositorySessionWipeDelegate delegate);

  /**
   * Synchronously perform the shared work of beginning. Throws on failure.
   * @throws InvalidSessionTransitionException
   *
   */
  protected void sharedBegin() throws InvalidSessionTransitionException {
    Logger.debug(LOG_TAG, "Shared begin.");
    if (delegateQueue.isShutdown()) {
      throw new InvalidSessionTransitionException(null);
    }
    if (storeWorkQueue.isShutdown()) {
      throw new InvalidSessionTransitionException(null);
    }
    this.transitionFrom(SessionStatus.UNSTARTED, SessionStatus.ACTIVE);
  }

  /**
   * Start the session. This is an appropriate place to initialize
   * data access components such as database handles.
   *
   * @param delegate
   * @throws InvalidSessionTransitionException
   */
  public void begin(RepositorySessionBeginDelegate delegate) throws InvalidSessionTransitionException {
    sharedBegin();
    delegate.deferredBeginDelegate(delegateQueue).onBeginSucceeded(this);
  }

  public void unbundle(RepositorySessionBundle bundle) {
    this.lastSyncTimestamp = bundle == null ? 0 : bundle.getTimestamp();
  }

  /**
   * Override this in your subclasses to return values to save between sessions.
   * Note that RepositorySession automatically bumps the timestamp to the time
   * the last sync began. If unbundled but not begun, this will be the same as the
   * value in the input bundle.
   *
   * The Synchronizer most likely wants to bump the bundle timestamp to be a value
   * return from a fetch call.
   */
  protected RepositorySessionBundle getBundle() {
    // Why don't we just persist the old bundle?
    long timestamp = getLastSyncTimestamp();
    RepositorySessionBundle bundle = new RepositorySessionBundle(timestamp);
    Logger.debug(LOG_TAG, "Setting bundle timestamp to " + timestamp + ".");

    return bundle;
  }

  /**
   * Just like finish(), but doesn't do any work that should only be performed
   * at the end of a successful sync, and can be called any time.
   */
  public void abort(RepositorySessionFinishDelegate delegate) {
    this.abort();
    delegate.deferredFinishDelegate(delegateQueue).onFinishSucceeded(this, this.getBundle());
  }

  /**
   * Abnormally terminate the repository session, freeing or closing
   * any resources that were opened during the lifetime of the session.
   */
  public void abort() {
    // TODO: do something here.
    this.setStatus(SessionStatus.ABORTED);
    try {
      storeWorkQueue.shutdownNow();
    } catch (Exception e) {
      Logger.error(LOG_TAG, "Caught exception shutting down store work queue.", e);
    }
    try {
      delegateQueue.shutdown();
    } catch (Exception e) {
      Logger.error(LOG_TAG, "Caught exception shutting down delegate queue.", e);
    }
  }

  /**
   * End the repository session, freeing or closing any resources
   * that were opened during the lifetime of the session.
   *
   * @param delegate notified of success or failure.
   * @throws InactiveSessionException
   */
  public void finish(final RepositorySessionFinishDelegate delegate) throws InactiveSessionException {
    try {
      this.transitionFrom(SessionStatus.ACTIVE, SessionStatus.DONE);
      delegate.deferredFinishDelegate(delegateQueue).onFinishSucceeded(this, this.getBundle());
    } catch (InvalidSessionTransitionException e) {
      Logger.error(LOG_TAG, "Tried to finish() an unstarted or already finished session");
      throw new InactiveSessionException(e);
    }

    Logger.trace(LOG_TAG, "Shutting down work queues.");
    storeWorkQueue.shutdown();
    delegateQueue.shutdown();
  }

  /**
   * Run the provided command if we're active and our delegate queue
   * is not shut down.
   */
  protected synchronized void executeDelegateCommand(Runnable command)
      throws InactiveSessionException {
    if (!isActive() || delegateQueue.isShutdown()) {
      throw new InactiveSessionException(null);
    }
    delegateQueue.execute(command);
  }

  public synchronized void ensureActive() throws InactiveSessionException {
    if (!isActive()) {
      throw new InactiveSessionException(null);
    }
  }

  public synchronized boolean isActive() {
    return status == SessionStatus.ACTIVE;
  }

  public synchronized SessionStatus getStatus() {
    return status;
  }

  public synchronized void setStatus(SessionStatus status) {
    this.status = status;
  }

  public synchronized void transitionFrom(SessionStatus from, SessionStatus to) throws InvalidSessionTransitionException {
    if (from == null || this.status == from) {
      Logger.trace(LOG_TAG, "Successfully transitioning from " + this.status + " to " + to);

      this.status = to;
      return;
    }
    Logger.warn(LOG_TAG, "Wanted to transition from " + from + " but in state " + this.status);
    throw new InvalidSessionTransitionException(null);
  }

  /**
   * Produce a record that is some combination of the remote and local records
   * provided.
   *
   * The returned record must be produced without mutating either remoteRecord
   * or localRecord. It is acceptable to return either remoteRecord or localRecord
   * if no modifications are to be propagated.
   *
   * The returned record *should* have the local androidID and the remote GUID,
   * and some optional merge of data from the two records.
   *
   * This method can be called with records that are identical, or differ in
   * any regard.
   *
   * This method will not be called if:
   *
   * * either record is marked as deleted, or
   * * there is no local mapping for a new remote record.
   *
   * Otherwise, it will be called precisely once.
   *
   * Side-effects (e.g., for transactional storage) can be hooked in here.
   *
   * @param remoteRecord
   *        The record retrieved from upstream, already adjusted for clock skew.
   * @param localRecord
   *        The record retrieved from local storage.
   * @param lastRemoteRetrieval
   *        The timestamp of the last retrieved set of remote records, adjusted for
   *        clock skew.
   * @param lastLocalRetrieval
   *        The timestamp of the last retrieved set of local records.
   * @return
   *        A Record instance to apply, or null to apply nothing.
   */
  protected Record reconcileRecords(final Record remoteRecord,
                                    final Record localRecord,
                                    final long lastRemoteRetrieval,
                                    final long lastLocalRetrieval) {
    Logger.debug(LOG_TAG, "Reconciling remote " + remoteRecord.guid + " against local " + localRecord.guid);

    if (localRecord.equalPayloads(remoteRecord)) {
      if (remoteRecord.lastModified > localRecord.lastModified) {
        Logger.debug(LOG_TAG, "Records are equal. No record application needed.");
        return null;
      }

      // Local wins.
      return null;
    }

    // TODO: Decide what to do based on:
    // * Which of the two records is modified;
    // * Whether they are equal or congruent;
    // * The modified times of each record (interpreted through the lens of clock skew);
    // * ...
    boolean localIsMoreRecent = localRecord.lastModified > remoteRecord.lastModified;
    Logger.debug(LOG_TAG, "Local record is more recent? " + localIsMoreRecent);
    Record donor = localIsMoreRecent ? localRecord : remoteRecord;

    // Modify the local record to match the remote record's GUID and values.
    // Preserve the local Android ID, and merge data where possible.
    // It sure would be nice if copyWithIDs didn't give a shit about androidID, mm?
    Record out = donor.copyWithIDs(remoteRecord.guid, localRecord.androidID);

    // We don't want to upload the record if the remote record was
    // applied without changes.
    // This logic will become more complicated as reconciling becomes smarter.
    if (!localIsMoreRecent) {
      trackGUID(out.guid);
    }
    return out;
  }

  /**
   * Depending on the RepositorySession implementation, track
   * that a record — most likely a brand-new record that has been
   * applied unmodified — should be tracked so as to not be uploaded
   * redundantly.
   *
   * The default implementations do nothing.
   */
  protected void trackGUID(String guid) {
  }

  protected synchronized void untrackGUIDs(Collection<String> guids) {
  }

  protected void untrackGUID(String guid) {
  }

  // Ah, Java. You wretched creature.
  public Iterator<String> getTrackedRecordIDs() {
    return new ArrayList<String>().iterator();
  }
}
