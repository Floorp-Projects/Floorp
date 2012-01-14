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
 *   Jason Voll <jvoll@mozilla.com>
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

package org.mozilla.gecko.sync.repositories.android;

import java.util.ArrayList;
import java.util.HashMap;

import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.InvalidRequestException;
import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;
import org.mozilla.gecko.sync.repositories.MultipleRecordsForGuidException;
import org.mozilla.gecko.sync.repositories.NoGuidForIdException;
import org.mozilla.gecko.sync.repositories.NoStoreDelegateException;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.ParentNotFoundException;
import org.mozilla.gecko.sync.repositories.ProfileDatabaseException;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionBeginDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionGuidsSinceDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.database.Cursor;
import android.util.Log;

/**
 * You'll notice that all delegate calls *either*:
 *
 * - request a deferred delegate with the appropriate work queue, then
 *   make the appropriate call, or
 * - create a Runnable which makes the appropriate call, and pushes it
 *   directly into the appropriate work queue.
 *
 * This is to ensure that all delegate callbacks happen off the current
 * thread. This provides lock safety (we don't enter another method that
 * might try to take a lock already taken in our caller), and ensures
 * that operations take place off the main thread.
 *
 * Don't do both -- the two approaches are equivalent -- and certainly
 * don't do neither unless you know what you're doing!
 *
 * Similarly, all store calls go through the appropriate store queue. This
 * ensures that store() and storeDone() consequences occur before-after.
 *
 * @author rnewman
 *
 */
public abstract class AndroidBrowserRepositorySession extends RepositorySession {

  protected AndroidBrowserRepositoryDataAccessor dbHelper;
  protected static final String LOG_TAG = "AndroidBrowserRepositorySession";
  private HashMap<String, String> recordToGuid;

  public AndroidBrowserRepositorySession(Repository repository) {
    super(repository);
  }

  /**
   * Override this.
   * Return null if this record should not be processed.
   *
   * @param cur
   * @return
   * @throws NoGuidForIdException
   * @throws NullCursorException
   * @throws ParentNotFoundException
   */
  protected abstract Record recordFromMirrorCursor(Cursor cur) throws NoGuidForIdException, NullCursorException, ParentNotFoundException;

  // Must be overriden by AndroidBookmarkRepositorySession.
  protected boolean checkRecordType(Record record) {
    return true;
  }

  /**
   * Override in subclass to implement record extension.
   * Return null if this record should not be processed.
   *
   * @param record
   *        The record to transform. Can be null.
   * @return The transformed record. Can be null.
   * @throws NullCursorException
   */
  protected Record transformRecord(Record record) throws NullCursorException {
    return record;
  }

  @Override
  public void begin(RepositorySessionBeginDelegate delegate) {
    RepositorySessionBeginDelegate deferredDelegate = delegate.deferredBeginDelegate(delegateQueue);
    try {
      super.sharedBegin();
    } catch (InvalidSessionTransitionException e) {
      deferredDelegate.onBeginFailed(e);
      return;
    }

    try {
      // We do this check here even though it results in one extra call to the DB
      // because if we didn't, we have to do a check on every other call since there
      // is no way of knowing which call would be hit first.
      checkDatabase();
    } catch (ProfileDatabaseException e) {
      Log.e(LOG_TAG, "ProfileDatabaseException from begin. Fennec must be launched once until this error is fixed");
      deferredDelegate.onBeginFailed(e);
      return;
    } catch (NullCursorException e) {
      deferredDelegate.onBeginFailed(e);
      return;
    } catch (Exception e) {
      deferredDelegate.onBeginFailed(e);
      return;
    }
    deferredDelegate.onBeginSucceeded(this);
  }

  protected abstract String buildRecordString(Record record);

  protected void checkDatabase() throws ProfileDatabaseException, NullCursorException {
    Log.i(LOG_TAG, "Checking database.");
    try {
      dbHelper.fetch(new String[] { "none" }).close();
    } catch (NullPointerException e) {
      throw new ProfileDatabaseException(e);
    }
  }

  @Override
  public void guidsSince(long timestamp, RepositorySessionGuidsSinceDelegate delegate) {
    GuidsSinceRunnable command = new GuidsSinceRunnable(timestamp, delegate);
    delegateQueue.execute(command);
  }

  class GuidsSinceRunnable implements Runnable {

    private RepositorySessionGuidsSinceDelegate delegate;
    private long                                timestamp;

    public GuidsSinceRunnable(long timestamp,
                              RepositorySessionGuidsSinceDelegate delegate) {
      this.timestamp = timestamp;
      this.delegate = delegate;
    }

    @Override
    public void run() {
      if (!isActive()) {
        delegate.onGuidsSinceFailed(new InactiveSessionException(null));
        return;
      }

      Cursor cur;
      try {
        cur = dbHelper.getGUIDsSince(timestamp);
      } catch (NullCursorException e) {
        delegate.onGuidsSinceFailed(e);
        return;
      } catch (Exception e) {
        delegate.onGuidsSinceFailed(e);
        return;
      }

      ArrayList<String> guids;
      try {
        if (!cur.moveToFirst()) {
          delegate.onGuidsSinceSucceeded(new String[] {});
          return;
        }
        guids = new ArrayList<String>();
        while (!cur.isAfterLast()) {
          guids.add(RepoUtils.getStringFromCursor(cur, "guid"));
          cur.moveToNext();
        }
      } finally {
        Log.d(LOG_TAG, "Closing cursor after guidsSince.");
        cur.close();
      }

      String guidsArray[] = new String[guids.size()];
      guids.toArray(guidsArray);
      delegate.onGuidsSinceSucceeded(guidsArray);
    }
  }

  @Override
  public void fetch(String[] guids,
                    RepositorySessionFetchRecordsDelegate delegate) {
    FetchRunnable command = new FetchRunnable(guids, now(), delegate);
    delegateQueue.execute(command);
  }

  abstract class FetchingRunnable implements Runnable {
    protected RepositorySessionFetchRecordsDelegate delegate;

    public FetchingRunnable(RepositorySessionFetchRecordsDelegate delegate) {
      this.delegate = delegate;
    }

    protected void fetchFromCursor(Cursor cursor, long end) {
      Log.d(LOG_TAG, "Fetch from cursor:");
      try {
        try {
          if (!cursor.moveToFirst()) {
            delegate.onFetchCompleted(end);
            return;
          }
          while (!cursor.isAfterLast()) {
            Log.d(LOG_TAG, "... one more record.");
            Record r = transformRecord(recordFromMirrorCursor(cursor));
            if (r != null) {
              delegate.onFetchedRecord(r);
            }
            cursor.moveToNext();
          }
          delegate.onFetchCompleted(end);
        } catch (NoGuidForIdException e) {
          Log.w(LOG_TAG, "No GUID for ID.", e);
          delegate.onFetchFailed(e, null);
        } catch (Exception e) {
          Log.w(LOG_TAG, "Exception in fetchFromCursor.", e);
          delegate.onFetchFailed(e, null);
          return;
        }
      } finally {
        Log.d(LOG_TAG, "Closing cursor after fetch.");
        cursor.close();
      }
    }
  }

  class FetchRunnable extends FetchingRunnable {
    private String[] guids;
    private long     end;

    public FetchRunnable(String[] guids,
                         long end,
                         RepositorySessionFetchRecordsDelegate delegate) {
      super(delegate);
      this.guids = guids;
      this.end   = end;
    }

    @Override
    public void run() {
      if (!isActive()) {
        delegate.onFetchFailed(new InactiveSessionException(null), null);
        return;
      }

      if (guids == null || guids.length < 1) {
        Log.e(LOG_TAG, "No guids sent to fetch");
        delegate.onFetchFailed(new InvalidRequestException(null), null);
        return;
      }

      try {
        Cursor cursor = dbHelper.fetch(guids);
        this.fetchFromCursor(cursor, end);
      } catch (NullCursorException e) {
        delegate.onFetchFailed(e, null);
      }
    }
  }

  @Override
  public void fetchSince(long timestamp,
                         RepositorySessionFetchRecordsDelegate delegate) {
    Log.i(LOG_TAG, "Running fetchSince(" + timestamp + ").");
    FetchSinceRunnable command = new FetchSinceRunnable(timestamp, now(), delegate);
    delegateQueue.execute(command);
  }

  class FetchSinceRunnable extends FetchingRunnable {
    private long since;
    private long end;

    public FetchSinceRunnable(long since,
                              long end,
                              RepositorySessionFetchRecordsDelegate delegate) {
      super(delegate);
      this.since = since;
      this.end   = end;
    }

    @Override
    public void run() {
      if (!isActive()) {
        delegate.onFetchFailed(new InactiveSessionException(null), null);
        return;
      }

      try {
        Cursor cursor = dbHelper.fetchSince(since);
        this.fetchFromCursor(cursor, end);
      } catch (NullCursorException e) {
        delegate.onFetchFailed(e, null);
        return;
      }
    }
  }

  @Override
  public void fetchAll(RepositorySessionFetchRecordsDelegate delegate) {
    this.fetchSince(0, delegate);
  }

  @Override
  public void store(final Record record) throws NoStoreDelegateException {
    if (delegate == null) {
      throw new NoStoreDelegateException();
    }
    if (record == null) {
      Log.e(LOG_TAG, "Record sent to store was null");
      throw new IllegalArgumentException("Null record passed to AndroidBrowserRepositorySession.store().");
    }

    // Store Runnables *must* complete synchronously. It's OK, they
    // run on a background thread.
    Runnable command = new Runnable() {

      @Override
      public void run() {
        if (!isActive()) {
          delegate.onRecordStoreFailed(new InactiveSessionException(null));
          return;
        }

        // Check that the record is a valid type
        // TODO Currently for bookmarks we only take care of folders
        // and bookmarks, all other types are ignored and thrown away
        if (!checkRecordType(record)) {
          Log.d(LOG_TAG, "Ignoring record " + record.guid + " due to unknown record type.");

          // Don't throw: we don't want to abort the entire sync when we get a livemark!
          // delegate.onRecordStoreFailed(new InvalidBookmarkTypeException(null));
          return;
        }

        // TODO:
        // TODO: rnewman 2012-01-13: read and improve this code.
        // TODO:
        Record existingRecord;
        try {
          existingRecord = findExistingRecord(record);

          // If the record is new and not deleted, store it
          if (existingRecord == null && !record.deleted) {
            record.androidID = insert(record);
          } else if (existingRecord != null) {

            dbHelper.delete(existingRecord);
            // Or clause: We won't store a remotely deleted record ever, but if it is marked deleted
            // and our existing record has a newer timestamp, we will restore the existing record
            if (!record.deleted || (record.deleted && existingRecord.lastModified > record.lastModified)) {
              // Record exists already, need to figure out what to store
              Record store = reconcileRecords(existingRecord, record);
              record.androidID = insert(store);
            }
          }
        } catch (MultipleRecordsForGuidException e) {
          Log.e(LOG_TAG, "Multiple records returned for given guid: " + record.guid);
          delegate.onRecordStoreFailed(e);
          return;
        } catch (NoGuidForIdException e) {
          Log.e(LOG_TAG, "Store failed for " + record.guid, e);
          delegate.onRecordStoreFailed(e);
          return;
        } catch (NullCursorException e) {
          Log.e(LOG_TAG, "Store failed for " + record.guid, e);
          delegate.onRecordStoreFailed(e);
          return;
        } catch (Exception e) {
          Log.e(LOG_TAG, "Store failed for " + record.guid, e);
          delegate.onRecordStoreFailed(e);
          return;
        }

        // Invoke callback with result.
        delegate.onRecordStoreSucceeded(record);
      }
    };
    storeWorkQueue.execute(command);
  }
  
  protected long insert(Record record) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
    putRecordToGuidMap(buildRecordString(record), record.guid);
    return RepoUtils.getAndroidIdFromUri(dbHelper.insert(record));
  }

  protected Record recordForGUID(String guid) throws
                                             NoGuidForIdException,
                                             NullCursorException,
                                             ParentNotFoundException,
                                             MultipleRecordsForGuidException {
    Cursor cursor = dbHelper.fetch(new String[] { guid });
    try {
      if (!cursor.moveToFirst()) {
        return null;
      }

      Record r = recordFromMirrorCursor(cursor);

      cursor.moveToNext();
      if (cursor.isAfterLast()) {
        // Got one record!
        return r; // Not transformed.
      }

      // More than one. Oh dear.
      throw (new MultipleRecordsForGuidException(null));
    } finally {
      cursor.close();
    }
  }

  // Check if record already exists locally.
  protected Record findExistingRecord(Record record) throws MultipleRecordsForGuidException,
    NoGuidForIdException, NullCursorException, ParentNotFoundException {

    Log.d(LOG_TAG, "Finding existing record for GUID " + record.guid);
    Record r = recordForGUID(record.guid);

    // One result. (Multiple throws an exception.)
    if (r != null) {
      Log.d(LOG_TAG, "Found one by GUID.");
      return r;
    }

    // Empty result.
    // Check to see if record exists but with a different guid.
    String recordString = buildRecordString(record);
    Log.d(LOG_TAG, "Searching with record string " + recordString);
    String guid = getRecordToGuidMap().get(recordString);
    if (guid != null) {
      Log.d(LOG_TAG, "Found one. Returning computed record.");
      return recordForGUID(guid);
    }
    Log.d(LOG_TAG, "findExistingRecord failed to find one for " + record.guid);
    return null;
  }

  public HashMap<String, String> getRecordToGuidMap() throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
    if (recordToGuid == null) {
      createRecordToGuidMap();
    }
    return recordToGuid;
  }

  private void createRecordToGuidMap() throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
    recordToGuid = new HashMap<String, String>();
    Cursor cur = dbHelper.fetchAll();
    try {
      if (!cur.moveToFirst()) {
        return;
      }
      while (!cur.isAfterLast()) {
        Record record = recordFromMirrorCursor(cur);
        if (record != null) {
          recordToGuid.put(buildRecordString(record), record.guid);
        }
        cur.moveToNext();
      }
    } finally {
      cur.close();
    }
  }

  public void putRecordToGuidMap(String guid, String recordString) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
    if (recordToGuid == null) {
      createRecordToGuidMap();
    }
    recordToGuid.put(guid, recordString);
  }

  protected Record reconcileRecords(Record local, Record remote) {
    Log.i(LOG_TAG, "Reconciling " + local.guid + " against " + remote.guid);

    // Determine which record is newer since this is the one we will take in case of conflict.
    // Yes, clock drift. *sigh*
    Record newer;
    if (local.lastModified > remote.lastModified) {
      newer = local;
    } else {
      newer = remote;
    }

    if (newer.guid != remote.guid) {
      newer.guid = remote.guid;
    }
    newer.androidID = local.androidID;

    return newer;
  }

  // Wipe method and thread.
  @Override
  public void wipe(RepositorySessionWipeDelegate delegate) {
    Runnable command = new WipeRunnable(delegate);
    storeWorkQueue.execute(command);
  }

  class WipeRunnable implements Runnable {
    private RepositorySessionWipeDelegate delegate;

    public WipeRunnable(RepositorySessionWipeDelegate delegate) {
      this.delegate = delegate;
    }

    public void run() {
      if (!isActive()) {
        delegate.onWipeFailed(new InactiveSessionException(null));
        return;
      }
      dbHelper.wipe();
      delegate.onWipeSucceeded();
    }
  }
}
