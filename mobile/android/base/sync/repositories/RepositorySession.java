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
 * Jason Voll <jvoll@mozilla.com>
 * Richard Newman <rnewman@mozilla.com>
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

package org.mozilla.gecko.sync.repositories;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionBeginDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFinishDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionGuidsSinceDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

/**
 * A RepositorySession is created and used thusly:
 *
 * * Construct, with a reference to its parent Repository, by calling
 *   Repository.createSession().
 * * Populate with saved information by calling unbundle().
 * * Begin a sync by calling begin().
 * * Perform operations such as fetchSince() and store().
 * * Finish by calling finish(), retrieving and storing the current bundle.
 *
 * @author rnewman
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
  public long lastSyncTimestamp;

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

  public void storeDone() {
    // Our default behavior will be to assume that the Runnable is
    // executed as soon as all the stores synchronously finish, so
    // our end timestamp can just be… now.
    storeDone(now());
  }

  public void storeDone(final long end) {
    Logger.debug(LOG_TAG, "Scheduling onStoreCompleted for after storing is done.");
    Runnable command = new Runnable() {
      @Override
      public void run() {
        delegate.onStoreCompleted(end);
      }
    };
    storeWorkQueue.execute(command);
  }

  public abstract void wipe(RepositorySessionWipeDelegate delegate);

  public void unbundle(RepositorySessionBundle bundle) {
    this.lastSyncTimestamp = 0;
    if (bundle == null) {
      return;
    }
    if (bundle.containsKey("timestamp")) {
      try {
        this.lastSyncTimestamp = bundle.getLong("timestamp");
      } catch (Exception e) {
        // Defaults to 0 above.
      }
    }
  }

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

  public void begin(RepositorySessionBeginDelegate delegate) throws InvalidSessionTransitionException {
    sharedBegin();
    delegate.deferredBeginDelegate(delegateQueue).onBeginSucceeded(this);
  }

  protected RepositorySessionBundle getBundle() {
    return this.getBundle(null);
  }

  /**
   * Override this in your subclasses to return values to save between sessions.
   * Note that RepositorySession automatically bumps the timestamp to the time
   * the last sync began. If unbundled but not begun, this will be the same as the
   * value in the input bundle.
   *
   * The Synchronizer most likely wants to bump the bundle timestamp to be a value
   * return from a fetch call.
   *
   * @param optional
   * @return
   */
  protected RepositorySessionBundle getBundle(RepositorySessionBundle optional) {
    Logger.debug(LOG_TAG, "RepositorySession.getBundle(optional).");
    // Why don't we just persist the old bundle?
    RepositorySessionBundle bundle = (optional == null) ? new RepositorySessionBundle() : optional;
    bundle.put("timestamp", this.lastSyncTimestamp);
    Logger.debug(LOG_TAG, "Setting bundle timestamp to " + this.lastSyncTimestamp);
    return bundle;
  }

  /**
   * Just like finish(), but doesn't do any work that should only be performed
   * at the end of a successful sync, and can be called any time.
   *
   * @param delegate
   */
  public void abort(RepositorySessionFinishDelegate delegate) {
    this.abort();
    delegate.deferredFinishDelegate(delegateQueue).onFinishSucceeded(this, this.getBundle(null));
  }

  public void abort() {
    // TODO: do something here.
    this.setStatus(SessionStatus.ABORTED);
    try {
      storeWorkQueue.shutdown();
    } catch (Exception e) {
      Logger.error(LOG_TAG, "Caught exception shutting down store work queue.", e);
    }
    try {
      delegateQueue.shutdown();
    } catch (Exception e) {
      Logger.error(LOG_TAG, "Caught exception shutting down delegate queue.", e);
    }
  }

  public void finish(final RepositorySessionFinishDelegate delegate) throws InactiveSessionException {
    try {
      this.transitionFrom(SessionStatus.ACTIVE, SessionStatus.DONE);
      delegate.deferredFinishDelegate(delegateQueue).onFinishSucceeded(this, this.getBundle(null));
    } catch (InvalidSessionTransitionException e) {
      Logger.error(LOG_TAG, "Tried to finish() an unstarted or already finished session");
      InactiveSessionException ex = new InactiveSessionException(null);
      ex.initCause(e);
      throw ex;
    }

    Logger.info(LOG_TAG, "Shutting down work queues.");
    storeWorkQueue.shutdown();
    delegateQueue.shutdown();
  }

  /**
   * Run the provided command if we're active and our delegate queue
   * is not shut down.
   *
   * @param command
   * @throws InactiveSessionException
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
      trackRecord(out);
    }
    return out;
  }

  /**
   * Depending on the RepositorySession implementation, track
   * that a record — most likely a brand-new record that has been
   * applied unmodified — should be tracked so as to not be uploaded
   * redundantly.
   *
   * The default implementation does nothing.
   *
   * @param record
   */
  protected synchronized void trackRecord(Record record) {
  }

  protected synchronized void untrackRecord(Record record) {
  }

  // Ah, Java. You wretched creature.
  public Iterator<String> getTrackedRecordIDs() {
    return new ArrayList<String>().iterator();
  }
}
