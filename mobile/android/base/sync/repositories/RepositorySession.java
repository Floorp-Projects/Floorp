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

import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionBeginDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFinishDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionGuidsSinceDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.util.Log;

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
  protected SessionStatus status = SessionStatus.UNSTARTED;
  protected Repository repository;

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
  public abstract void fetch(String[] guids, RepositorySessionFetchRecordsDelegate delegate);
  public abstract void fetchAll(RepositorySessionFetchRecordsDelegate delegate);
  public abstract void store(Record record, RepositorySessionStoreDelegate delegate);
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

  private static void error(String msg) {
    System.err.println("ERROR: " + msg);
    Log.e(LOG_TAG, msg);
  }

  /**
   * Synchronously perform the shared work of beginning. Throws on failure.
   * @throws InvalidSessionTransitionException
   *
   */
  protected void sharedBegin() throws InvalidSessionTransitionException {
    if (this.status == SessionStatus.UNSTARTED) {
      this.status = SessionStatus.ACTIVE;
    } else {
      error("Tried to begin() an already active or finished session");
      throw new InvalidSessionTransitionException(null);
    }
  }

  public void begin(RepositorySessionBeginDelegate delegate) {
    try {
      sharedBegin();
      delegate.deferredBeginDelegate().onBeginSucceeded(this);

    } catch (Exception e) {
      delegate.deferredBeginDelegate().onBeginFailed(e);
    }
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
    System.out.println("RepositorySession.getBundle(optional).");
    // Why don't we just persist the old bundle?
    RepositorySessionBundle bundle = (optional == null) ? new RepositorySessionBundle() : optional;
    bundle.put("timestamp", this.lastSyncTimestamp);
    System.out.println("Setting bundle timestamp to " + this.lastSyncTimestamp);
    return bundle;
  }

  /**
   * Just like finish(), but doesn't do any work that should only be performed
   * at the end of a successful sync, and can be called any time.
   *
   * @param delegate
   */
  public void abort(RepositorySessionFinishDelegate delegate) {
    this.status = SessionStatus.DONE;    // TODO: ABORTED?
    delegate.deferredFinishDelegate().onFinishSucceeded(this, this.getBundle(null));
  }

  public void finish(RepositorySessionFinishDelegate delegate) {
    if (this.status == SessionStatus.ACTIVE) {
      this.status = SessionStatus.DONE;
      delegate.deferredFinishDelegate().onFinishSucceeded(this, this.getBundle(null));
    } else {
      Log.e(LOG_TAG, "Tried to finish() an unstarted or already finished session");
      delegate.deferredFinishDelegate().onFinishFailed(new InvalidSessionTransitionException(null));
    }
  }

  public boolean isActive() {
    return status == SessionStatus.ACTIVE;
  }

  public void abort() {
    // TODO: do something here.
    status = SessionStatus.ABORTED;
  }
}
