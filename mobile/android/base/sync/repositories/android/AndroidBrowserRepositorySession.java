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
import org.mozilla.gecko.sync.repositories.InvalidBookmarkTypeException;
import org.mozilla.gecko.sync.repositories.InvalidRequestException;
import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;
import org.mozilla.gecko.sync.repositories.MultipleRecordsForGuidException;
import org.mozilla.gecko.sync.repositories.NoGuidForIdException;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.ParentNotFoundException;
import org.mozilla.gecko.sync.repositories.ProfileDatabaseException;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionBeginDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionGuidsSinceDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.database.Cursor;
import android.util.Log;

public abstract class AndroidBrowserRepositorySession extends RepositorySession {

  protected AndroidBrowserRepositoryDataAccessor dbHelper;
  protected static final String LOG_TAG = "AndroidBrowserRepositorySession";
  private HashMap<String, String> recordToGuid;

  public AndroidBrowserRepositorySession(Repository repository) {
    super(repository);
  }

  @Override
  public void begin(RepositorySessionBeginDelegate delegate) {
    try {
      super.sharedBegin();
    } catch (InvalidSessionTransitionException e) {
      delegate.onBeginFailed(e);
      return;
    }

    try {
      // We do this check here even though it results in one extra call to the DB
      // because if we didn't, we have to do a check on every other call since there
      // is no way of knowing which call would be hit first.
      checkDatabase();
    } catch (ProfileDatabaseException e) {
      Log.e(LOG_TAG, "ProfileDatabaseException from begin. Fennec must be launched once until this error is fixed");
      delegate.onBeginFailed(e);
      return;
    } catch (NullCursorException e) {
      delegate.onBeginFailed(e);
      return;
    } catch (Exception e) {
      delegate.onBeginFailed(e);
      return;
    }
    delegate.onBeginSucceeded(this);
  }

  protected abstract String buildRecordString(Record record);

  protected void checkDatabase() throws ProfileDatabaseException, NullCursorException {
    try {
      dbHelper.fetch(new String[] { "none" });
    } catch (NullPointerException e) {
      throw new ProfileDatabaseException(e);
    }
  }

  // guids since method and thread
  @Override
  public void guidsSince(long timestamp, RepositorySessionGuidsSinceDelegate delegate) {
    GuidsSinceThread thread = new GuidsSinceThread(timestamp, delegate);
    thread.start();
  }

  class GuidsSinceThread extends Thread {

    private long                                   timestamp;
    private RepositorySessionGuidsSinceDelegate    delegate;

    public GuidsSinceThread(long timestamp,
        RepositorySessionGuidsSinceDelegate delegate) {
      this.timestamp = timestamp;
      this.delegate = delegate;
    }

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

      ArrayList<String> guids = new ArrayList<String>();
      cur.moveToFirst();
      while (!cur.isAfterLast()) {
        guids.add(RepoUtils.getStringFromCursor(cur, "guid"));
        cur.moveToNext();
      }
      cur.close();

      String guidsArray[] = new String[guids.size()];
      guids.toArray(guidsArray);
      delegate.onGuidsSinceSucceeded(guidsArray);

    }
  }

  protected Record[] compileIntoRecordsArray(Cursor cur) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
    ArrayList<Record> records = new ArrayList<Record>();
    cur.moveToFirst();
    while (!cur.isAfterLast()) {
      records.add(recordFromMirrorCursor(cur));
      cur.moveToNext();
    }
    cur.close();

    Record[] recordArray = new Record[records.size()];
    records.toArray(recordArray);
    return recordArray;
  }

  protected abstract Record recordFromMirrorCursor(Cursor cur) throws NoGuidForIdException, NullCursorException, ParentNotFoundException;
  
  

  // Fetch since method and thread
  @Override
  public void fetchSince(long timestamp,
                         RepositorySessionFetchRecordsDelegate delegate) {
    FetchSinceThread thread = new FetchSinceThread(timestamp, now(), delegate);
    thread.start();
  }

  class FetchSinceThread extends Thread {

    private long since;
    private long end;
    private RepositorySessionFetchRecordsDelegate delegate;

    public FetchSinceThread(long since,
                            long end,
                            RepositorySessionFetchRecordsDelegate delegate) {
      this.since    = since;
      this.end      = end;
      this.delegate = delegate;
    }

    public void run() {
      if (!isActive()) {
        delegate.onFetchFailed(new InactiveSessionException(null), null);
        return;
      }

      try {
        delegate.onFetchSucceeded(doFetchSince(since), end);
      } catch (NoGuidForIdException e) {
        delegate.onFetchFailed(e, null);
        return;
      } catch (NullCursorException e) {
        delegate.onFetchFailed(e, null);
        return;
      } catch (Exception e) {
        delegate.onFetchFailed(e, null);
        return;
      }
    }
  }
  
  protected Record[] doFetchSince(long since) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
    return compileIntoRecordsArray(dbHelper.fetchSince(since));
  }

  // Fetch method and thread
  @Override
  public void fetch(String[] guids,
                    RepositorySessionFetchRecordsDelegate delegate) {
    FetchThread thread = new FetchThread(guids, now(), delegate);
    thread.start();
  }

  class FetchThread extends Thread {
    private String[] guids;
    private long     end;
    private RepositorySessionFetchRecordsDelegate delegate;

    public FetchThread(String[] guids,
                       long end,
                       RepositorySessionFetchRecordsDelegate delegate) {
      this.guids    = guids;
      this.end      = end;
      this.delegate = delegate;
    }

    public void run() {
      if (!isActive()) {
        delegate.onFetchFailed(new InactiveSessionException(null), null);
        return;
      }

      if (guids == null || guids.length < 1) {
        Log.e(LOG_TAG, "No guids sent to fetch");
        delegate.onFetchFailed(new InvalidRequestException(null), null);
      } else {
        try {
          delegate.onFetchSucceeded(doFetch(guids), end);
        } catch (NoGuidForIdException e) {
          delegate.onFetchFailed(e, null);
        } catch (NullCursorException e) {
          delegate.onFetchFailed(e, null);
        } catch (Exception e) {
        delegate.onFetchFailed(e, null);
        return;
        }
      }
    }
  }

  protected Record[] doFetch(String[] guids) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
    Cursor cur = dbHelper.fetch(guids);
    return compileIntoRecordsArray(cur);
  }

  // Fetch all method and thread
  @Override
  public void fetchAll(RepositorySessionFetchRecordsDelegate delegate) {
    FetchAllThread thread = new FetchAllThread(now(), delegate);
    thread.start();
  }

  class FetchAllThread extends Thread {
    private long end;
    private RepositorySessionFetchRecordsDelegate delegate;

    public FetchAllThread(long end, RepositorySessionFetchRecordsDelegate delegate) {
      this.end      = end;
      this.delegate = delegate;
    }

    public void run() {
      if (!isActive()) {
        delegate.onFetchFailed(new InactiveSessionException(null), null);
        return;
      }

      try {
        delegate.onFetchSucceeded(doFetchAll(), end);
      } catch (NoGuidForIdException e) {
        delegate.onFetchFailed(e, null);
        return;
      } catch (NullCursorException e) {
        delegate.onFetchFailed(e, null);
        return;
      } catch (Exception e) {
        delegate.onFetchFailed(e, null);
        return;
      }
    }
  }
  
  protected Record[] doFetchAll() throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
    return compileIntoRecordsArray(dbHelper.fetchAll());
  }

  // Store method and thread
  @Override
  public void store(Record record, RepositorySessionStoreDelegate delegate) {
    StoreThread thread = new StoreThread(record, delegate);
    thread.start();
  }

  class StoreThread extends Thread {
    private Record                         record;
    private RepositorySessionStoreDelegate delegate;

    public StoreThread(Record record, RepositorySessionStoreDelegate delegate) {
      if (record == null) {
        Log.e(LOG_TAG, "Record sent to store was null");
        throw new IllegalArgumentException("record is null.");
      }
      this.record = record;
      this.delegate = delegate;
    }

    public void run() {
      if (!isActive()) {
        delegate.onStoreFailed(new InactiveSessionException(null));
        return;
      }

      // Check that the record is a valid type
      // TODO Currently for bookmarks we only take care of folders
      // and bookmarks, all other types are ignored and thrown away
      if (!checkRecordType(record)) {
        delegate.onStoreFailed(new InvalidBookmarkTypeException(null));
        return;
      }

      Record existingRecord;
      try {
        existingRecord = findExistingRecord(this.record);

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
        delegate.onStoreFailed(e);
        return;
      } catch (NoGuidForIdException e) {
        delegate.onStoreFailed(e);
        return;
      } catch (NullCursorException e) {
        delegate.onStoreFailed(e);
        return;
      } catch (Exception e) {
        delegate.onStoreFailed(e);
        return;
      }

      // Invoke callback with result.
      delegate.onStoreSucceeded(record);
    }

  }
  
  protected long insert(Record record) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
    putRecordToGuidMap(buildRecordString(record), record.guid);
    return RepoUtils.getAndroidIdFromUri(dbHelper.insert(record));
  }

  // Check if record already exists locally
  protected Record findExistingRecord(Record record) throws MultipleRecordsForGuidException,
    NoGuidForIdException, NullCursorException, ParentNotFoundException {
    Record[] records = doFetch(new String[] { record.guid });
    if (records.length == 1) {
      return records[0];
    } else if (records.length > 1) {
      throw (new MultipleRecordsForGuidException(null));
    } else {
      // Check to see if record exists but with a different guid
      String recordString = buildRecordString(record);
      String guid = getRecordToGuidMap().get(recordString);
      if (guid != null) {
        return doFetch(new String[] { guid })[0];
      }
    }
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
    cur.moveToFirst();
    while (!cur.isAfterLast()) {
      Record record = recordFromMirrorCursor(cur);
      recordToGuid.put(buildRecordString(record), record.guid);
      cur.moveToNext();
    }
    cur.close();
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

  // Must be overrriden by AndroidBookmarkRepositorySession
  protected boolean checkRecordType(Record record) {
    return true;
  }

  // Wipe method and thread.
  @Override
  public void wipe(RepositorySessionWipeDelegate delegate) {
    WipeThread thread = new WipeThread(delegate);
    thread.start();
  }

  class WipeThread extends Thread {
    private RepositorySessionWipeDelegate delegate;

    public WipeThread(RepositorySessionWipeDelegate delegate) {
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
