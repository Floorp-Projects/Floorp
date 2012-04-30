/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import java.util.ArrayList;
import java.util.List;

import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserContract.DeletedColumns;
import org.mozilla.gecko.db.BrowserContract.DeletedPasswords;
import org.mozilla.gecko.db.BrowserContract.Passwords;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.NoStoreDelegateException;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.RecordFilter;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.StoreTrackingRepositorySession;
import org.mozilla.gecko.sync.repositories.android.RepoUtils.QueryHelper;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFinishDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionGuidsSinceDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;
import org.mozilla.gecko.sync.repositories.domain.PasswordRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.content.ContentProviderClient;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.RemoteException;

public class PasswordsRepositorySession extends
    StoreTrackingRepositorySession {

  public static class PasswordsRepository extends Repository {
    @Override
    public void createSession(RepositorySessionCreationDelegate delegate,
        Context context) {
      PasswordsRepositorySession session = new PasswordsRepositorySession(PasswordsRepository.this, context);
      final RepositorySessionCreationDelegate deferredCreationDelegate = delegate.deferredCreationDelegate();
      deferredCreationDelegate.onSessionCreated(session);
    }
  }

  private static final String LOG_TAG = "PasswordsRepoSession";
  private static String COLLECTION = "passwords";

  private RepoUtils.QueryHelper passwordsHelper;
  private RepoUtils.QueryHelper deletedPasswordsHelper;
  private ContentProviderClient passwordsProvider;

  private final Context context;

  public PasswordsRepositorySession(Repository repository, Context context) {
    super(repository);
    this.context = context;
    this.passwordsHelper        = new QueryHelper(context, BrowserContractHelpers.PASSWORDS_CONTENT_URI, LOG_TAG);
    this.deletedPasswordsHelper = new QueryHelper(context, BrowserContractHelpers.DELETED_PASSWORDS_CONTENT_URI, LOG_TAG);
    this.passwordsProvider      = context.getContentResolver().acquireContentProviderClient(BrowserContract.PASSWORDS_AUTHORITY_URI);
  }

  private static final String[] GUID_COLS = new String[] { Passwords.GUID };
  private static final String[] DELETED_GUID_COLS = new String[] { DeletedColumns.GUID };

  private static final String WHERE_GUID_IS = Passwords.GUID + " = ?";
  private static final String WHERE_DELETED_GUID_IS = DeletedPasswords.GUID + " = ?";

  @Override
  public void guidsSince(final long timestamp, final RepositorySessionGuidsSinceDelegate delegate) {
    final Runnable guidsSinceRunnable = new Runnable() {
      @Override
      public void run() {

        if (!isActive()) {
          delegate.onGuidsSinceFailed(new InactiveSessionException(null));
          return;
        }

        // Checks succeeded, now get GUIDs.
        final List<String> guids = new ArrayList<String>();
        try {
          Logger.debug(LOG_TAG, "Fetching guidsSince from data table.");
          final Cursor data = passwordsHelper.safeQuery(passwordsProvider, ".getGUIDsSince", GUID_COLS, dateModifiedWhere(timestamp), null, null);
          try {
            if (data.moveToFirst()) {
              while (!data.isAfterLast()) {
                guids.add(RepoUtils.getStringFromCursor(data, Passwords.GUID));
                data.moveToNext();
              }
            }
          } finally {
            data.close();
          }

          // Fetch guids from deleted table.
          Logger.debug(LOG_TAG, "Fetching guidsSince from deleted table.");
          final Cursor deleted = deletedPasswordsHelper.safeQuery(passwordsProvider, ".getGUIDsSince", DELETED_GUID_COLS, dateModifiedWhereDeleted(timestamp), null, null);
          try {
            if (deleted.moveToFirst()) {
              while (!deleted.isAfterLast()) {
                guids.add(RepoUtils.getStringFromCursor(deleted, DeletedColumns.GUID));
                deleted.moveToNext();
              }
            }
          } finally {
            deleted.close();
          }
        } catch (Exception e) {
          Logger.error(LOG_TAG, "Exception in fetch.");
          delegate.onGuidsSinceFailed(e);
          return;
        }
        String[] guidStrings = new String[guids.size()];
        delegate.onGuidsSinceSucceeded(guids.toArray(guidStrings));
      }
    };

    delegateQueue.execute(guidsSinceRunnable);
  }

  @Override
  public void fetchSince(final long timestamp, final RepositorySessionFetchRecordsDelegate delegate) {
    final RecordFilter filter = this.storeTracker.getFilter();
    final Runnable fetchSinceRunnable = new Runnable() {
      @Override
      public void run() {
        if (!isActive()) {
          delegate.onFetchFailed(new InactiveSessionException(null), null);
          return;
        }

        final long end = now();
        try {
          // Fetch from data table.
          Cursor data = passwordsHelper.safeQuery(passwordsProvider, ".fetchSince",
                                                  getAllColumns(),
                                                  dateModifiedWhere(timestamp),
                                                  null, null);
          if (!fetchAndCloseCursorDeleted(data, false, filter, delegate)) {
            return;
          }

          // Fetch from deleted table.
          Cursor deleted = deletedPasswordsHelper.safeQuery(passwordsProvider, ".fetchSince",
                                                            getAllDeletedColumns(),
                                                            dateModifiedWhereDeleted(timestamp),
                                                            null, null);
          if (!fetchAndCloseCursorDeleted(deleted, true, filter, delegate)) {
            return;
          }

          // Success!
          try {
            delegate.onFetchCompleted(end);
          } catch (Exception e) {
            Logger.error(LOG_TAG, "Delegate fetch completed callback failed.", e);
            // Don't call failure callback.
            return;
          }
        } catch (Exception e) {
          Logger.error(LOG_TAG, "Exception in fetch.");
          delegate.onFetchFailed(e, null);
        }
      }
    };

    delegateQueue.execute(fetchSinceRunnable);
  }

  @Override
  public void fetch(final String[] guids, final RepositorySessionFetchRecordsDelegate delegate) {
    if (guids == null || guids.length < 1) {
      Logger.error(LOG_TAG, "No guids to be fetched.");
      final long end = now();
      delegateQueue.execute(new Runnable() {
        @Override
        public void run() {
          delegate.onFetchCompleted(end);
        }
      });
      return;
    }

    // Checks succeeded, now fetch.
    final RecordFilter filter = this.storeTracker.getFilter();
    final Runnable fetchRunnable = new Runnable() {
      @Override
      public void run() {
        if (!isActive()) {
          delegate.onFetchFailed(new InactiveSessionException(null), null);
          return;
        }

        final long end = now();
        final String where = RepoUtils.computeSQLInClause(guids.length, "guid");
        Logger.trace(LOG_TAG, "Fetch guids where: " + where);

        try {
          // Fetch records from data table.
          Cursor data = passwordsHelper.safeQuery(passwordsProvider, ".fetch",
                                                  getAllColumns(),
                                                  where, guids, null);
          if (!fetchAndCloseCursorDeleted(data, false, filter, delegate)) {
            return;
          }

          // Fetch records from deleted table.
          Cursor deleted = deletedPasswordsHelper.safeQuery(passwordsProvider, ".fetch",
                                                            getAllDeletedColumns(),
                                                            where, guids, null);
          if (!fetchAndCloseCursorDeleted(deleted, true, filter, delegate)) {
            return;
          }

          delegate.onFetchCompleted(end);

        } catch (Exception e) {
          Logger.error(LOG_TAG, "Exception in fetch.");
          delegate.onFetchFailed(e, null);
        }
      }
    };

    delegateQueue.execute(fetchRunnable);
  }

  @Override
  public void fetchAll(RepositorySessionFetchRecordsDelegate delegate) {
    fetchSince(0, delegate);
  }

  @Override
  public void store(final Record record) throws NoStoreDelegateException {
    if (delegate == null) {
      Logger.error(LOG_TAG, "No store delegate.");
      throw new NoStoreDelegateException();
    }
    if (record == null) {
      Logger.error(LOG_TAG, "Record sent to store was null.");
      throw new IllegalArgumentException("Null record passed to PasswordsRepositorySession.store().");
    }
    if (!(record instanceof PasswordRecord)) {
      Logger.error(LOG_TAG, "Can't store anything but a PasswordRecord.");
      throw new IllegalArgumentException("Non-PasswordRecord passed to PasswordsRepositorySession.store().");
    }

    final PasswordRecord remoteRecord = (PasswordRecord) record;

    final Runnable storeRunnable = new Runnable() {
      @Override
      public void run() {
        if (!isActive()) {
          Logger.warn(LOG_TAG, "RepositorySession is inactive. Store failing.");
          delegate.onRecordStoreFailed(new InactiveSessionException(null));
          return;
        }

        final String guid = remoteRecord.guid;
        if (guid == null) {
          delegate.onRecordStoreFailed(new RuntimeException("Can't store record with null GUID."));
          return;
        }

        PasswordRecord existingRecord;
        try {
          existingRecord = retrieveByGUID(guid);
        } catch (NullCursorException e) {
          // Indicates a serious problem.
          delegate.onRecordStoreFailed(e);
          return;
        } catch (RemoteException e) {
          delegate.onRecordStoreFailed(e);
          return;
        }

        long lastLocalRetrieval  = 0;      // lastSyncTimestamp?
        long lastRemoteRetrieval = 0;      // TODO: adjust for clock skew.
        boolean remotelyModified = remoteRecord.lastModified > lastRemoteRetrieval;

        // Check deleted state first.
        if (remoteRecord.deleted) {
          if (existingRecord == null) {
            // Do nothing, record does not exist anyways.
            Logger.info(LOG_TAG, "Incoming record " + remoteRecord.guid + " is deleted, and no local version.");
            return;
          }

          if (existingRecord.deleted) {
            // Record is already tracked as deleted. Delete from local.
            storeRecordDeletion(existingRecord); // different from ABRepoSess.
            Logger.info(LOG_TAG, "Incoming record " + remoteRecord.guid + " and local are both deleted.");
            return;
          }

          // Which one wins?
          if (!remotelyModified) {
            trace("Ignoring deleted record from the past.");
            return;
          }

          boolean locallyModified = existingRecord.lastModified > lastLocalRetrieval;
          if (!locallyModified) {
            trace("Remote modified, local not. Deleting.");
            storeRecordDeletion(remoteRecord);
            return;
          }

          trace("Both local and remote records have been modified.");
          if (remoteRecord.lastModified > existingRecord.lastModified) {
            trace("Remote is newer, and deleted. Deleting local.");
            storeRecordDeletion(remoteRecord);
            return;
          }

          trace("Remote is older, local is not deleted. Ignoring.");
          if (!locallyModified) {
            Logger.warn(LOG_TAG, "Inconsistency: old remote record is deleted, but local record not modified!");
            // Ensure that this is tracked for upload.
          }
          return;
        }
        // End deletion logic.

        // Now we're processing a non-deleted incoming record.
        if (existingRecord == null) {
          trace("Looking up match for record " + remoteRecord.guid);
          existingRecord = findExistingRecord(remoteRecord);
        }

        if (existingRecord == null) {
          // The record is new.
          trace("No match. Inserting.");
          Logger.debug(LOG_TAG, "Didn't find matching record. Inserting.");
          Record inserted = null;
          try {
            inserted = insert(remoteRecord);
          } catch (RemoteException e) {
            Logger.debug(LOG_TAG, "Record insert caused a RemoteException.");
            delegate.onRecordStoreFailed(e);
            return;
          }
          trackRecord(inserted);
          delegate.onRecordStoreSucceeded(inserted);
          return;
        }

        // We found a local dupe.
        trace("Incoming record " + remoteRecord.guid + " dupes to local record " + existingRecord.guid);
        Logger.debug(LOG_TAG, "remote " + remoteRecord + " dupes to " + existingRecord);
        Record toStore = reconcileRecords(remoteRecord, existingRecord, lastRemoteRetrieval, lastLocalRetrieval);

        if (toStore == null) {
          Logger.debug(LOG_TAG, "Reconciling returned null. Not inserting a record.");
          return;
        }

        // TODO: pass in timestamps?
        Logger.debug(LOG_TAG, "Replacing " + existingRecord.guid + " with record " + toStore.guid);
        Logger.debug(LOG_TAG, "existing: " + existingRecord);
        Logger.debug(LOG_TAG, "toStore: " + toStore);
        Record replaced = null;
        try {
          replaced = replace(existingRecord, toStore);
        } catch (RemoteException e) {
          Logger.debug(LOG_TAG, "Record replace caused a RemoteException.");
          delegate.onRecordStoreFailed(e);
          return;
        }

        // Note that we don't track records here; deciding that is the job
        // of reconcileRecords.
        Logger.debug(LOG_TAG, "Calling delegate callback with guid " + replaced.guid +
                              "(" + replaced.androidID + ")");
        delegate.onRecordStoreSucceeded(replaced);
        return;
      }
    };
    storeWorkQueue.execute(storeRunnable);
  }

  @Override
  public void wipe(final RepositorySessionWipeDelegate delegate) {
    Logger.info(LOG_TAG, "Wiping " + BrowserContractHelpers.PASSWORDS_CONTENT_URI + ", " + BrowserContractHelpers.DELETED_PASSWORDS_CONTENT_URI);

    Runnable wipeRunnable = new Runnable() {
      @Override
      public void run() {
        if (!isActive()) {
          delegate.onWipeFailed(new InactiveSessionException(null));
          return;
        }

        // Wipe both data and deleted.
        try {
          context.getContentResolver().delete(BrowserContractHelpers.PASSWORDS_CONTENT_URI, null, null);
          context.getContentResolver().delete(BrowserContractHelpers.DELETED_PASSWORDS_CONTENT_URI, null, null);
        } catch (Exception e) {
          delegate.onWipeFailed(e);
          return;
        }
        delegate.onWipeSucceeded();
      }
    };
    storeWorkQueue.execute(wipeRunnable);
  }

  @Override
  public void abort() {
    passwordsProvider.release();
    super.abort();
  }

  @Override
  public void finish(final RepositorySessionFinishDelegate delegate) throws InactiveSessionException {
    passwordsProvider.release();
    super.finish(delegate);
  }

  public void deleteGUID(String guid) throws RemoteException {
    final String[] args = new String[] { guid };

    int deleted = passwordsProvider.delete(BrowserContractHelpers.PASSWORDS_CONTENT_URI, WHERE_GUID_IS, args) +
                  passwordsProvider.delete(BrowserContractHelpers.DELETED_PASSWORDS_CONTENT_URI, WHERE_DELETED_GUID_IS, args);
    if (deleted == 1) {
      return;
    }
    Logger.warn(LOG_TAG, "Unexpectedly deleted " + deleted + " rows for guid " + guid);
  }

  /**
   * Insert record and return the record with its updated androidId set.
   * @param record
   * @return
   * @throws RemoteException
   */
  public PasswordRecord insert(PasswordRecord record) throws RemoteException {
    record.timePasswordChanged = now();
    // TODO: are these necessary for Fennec autocomplete?
    // record.timesUsed = 1;
    // record.timeLastUsed = now();
    ContentValues cv = getContentValues(record);
    Uri insertedUri = passwordsProvider.insert(BrowserContractHelpers.PASSWORDS_CONTENT_URI, cv);
    record.androidID = RepoUtils.getAndroidIdFromUri(insertedUri);
    return record;
  }

  public Record replace(Record origRecord, Record newRecord) throws RemoteException {
    PasswordRecord newPasswordRecord = (PasswordRecord) newRecord;
    PasswordRecord origPasswordRecord = (PasswordRecord) origRecord;
    propagateTimes(newPasswordRecord, origPasswordRecord);
    ContentValues cv = getContentValues(newPasswordRecord);

    final String[] args = new String[] { origRecord.guid };

    int updated = context.getContentResolver().update(BrowserContractHelpers.PASSWORDS_CONTENT_URI, cv, WHERE_GUID_IS, args);
    if (updated != 1) {
      Logger.warn(LOG_TAG, "Unexpectedly updated " + updated + " rows for guid " + origPasswordRecord.guid);
    }
    return newRecord;
  }

  // When replacing a record, propagate the times.
  private static void propagateTimes(PasswordRecord toRecord, PasswordRecord fromRecord) {
    toRecord.timePasswordChanged = now();
    toRecord.timeCreated  = fromRecord.timeCreated;
    toRecord.timeLastUsed = fromRecord.timeLastUsed;
    toRecord.timesUsed    = fromRecord.timesUsed;
  }

  private static String[] getAllColumns() {
    return BrowserContractHelpers.PasswordColumns;
  }

  private static String[] getAllDeletedColumns() {
    return BrowserContractHelpers.DeletedColumns;
  }

  /**
   * Constructs the DB query string for entry age for deleted records.
   *
   * @param timestamp
   * @return String DB query string for dates to fetch.
   */
  private static String dateModifiedWhereDeleted(long timestamp) {
    return DeletedColumns.TIME_DELETED + " >= " + Long.toString(timestamp);
  }

  /**
   * Constructs the DB query string for entry age for (undeleted) records.
   *
   * @param timestamp
   * @return String DB query string for dates to fetch.
   */
  private static String dateModifiedWhere(long timestamp) {
    return Passwords.TIME_PASSWORD_CHANGED + " >= " + Long.toString(timestamp);
  }


  /**
   * Fetch from the cursor with the given parameters, invoking
   * delegate callbacks and closing the cursor.
   * Returns true on success, false if failure was signaled.
   *
   * @param cursor
            fetch* cursor.
   * @param deleted
   *        true if using deleted table, false when using data table.
   * @param delegate
   *        FetchRecordsDelegate to process records.
   */
  private static boolean fetchAndCloseCursorDeleted(final Cursor cursor,
                                                    final boolean deleted,
                                                    final RecordFilter filter,
                                                    final RepositorySessionFetchRecordsDelegate delegate) {
    if (cursor == null) {
      return true;
    }

    try {
      while (cursor.moveToNext()) {
        Record r = deleted ? deletedPasswordRecordFromCursor(cursor) : passwordRecordFromCursor(cursor);
        if (r != null) {
          if (filter == null || !filter.excludeRecord(r)) {
            Logger.debug(LOG_TAG, "Fetched record " + r);
            delegate.onFetchedRecord(r);
          } else {
            Logger.debug(LOG_TAG, "Skipping filtered record " + r.guid);
          }
        }
      }
    } catch (Exception e) {
      Logger.error(LOG_TAG, "Exception in fetch.");
      delegate.onFetchFailed(e, null);
      return false;
    } finally {
      cursor.close();
    }

    return true;
  }

  private PasswordRecord retrieveByGUID(String guid) throws NullCursorException, RemoteException {
    final String[] guidArg = new String[] { guid };

    // Check data table.
    final Cursor data = passwordsHelper.safeQuery(passwordsProvider, ".store", BrowserContractHelpers.PasswordColumns, WHERE_GUID_IS, guidArg, null);
    try {
      if (data.moveToFirst()) {
        return passwordRecordFromCursor(data);
      }
    } finally {
      data.close();
    }

    // Check deleted table.
    final Cursor deleted = deletedPasswordsHelper.safeQuery(passwordsProvider, ".retrieveByGuid", BrowserContractHelpers.DeletedColumns, WHERE_DELETED_GUID_IS, guidArg, null);
    try {
      if (deleted.moveToFirst()) {
        return deletedPasswordRecordFromCursor(deleted);
      }
    } finally {
      deleted.close();
    }

    return null;
  }

  private static final String WHERE_RECORD_DATA =
    Passwords.HOSTNAME        + " = ? AND " +
    Passwords.HTTP_REALM      + " = ? AND " +
    Passwords.FORM_SUBMIT_URL + " = ? AND " +
    Passwords.USERNAME_FIELD  + " = ? AND " +
    Passwords.PASSWORD_FIELD  + " = ?";

  private PasswordRecord findExistingRecord(PasswordRecord record) {
    PasswordRecord foundRecord = null;
    Cursor cursor = null;
    // Only check the data table.
    // We can't encrypt username directly for query, so run a more general query and then filter.
    final String[] whereArgs = new String[] {
      record.hostname,
      record.httpRealm,
      record.formSubmitURL,
      record.usernameField,
      record.passwordField
    };

    try {
      cursor = passwordsHelper.safeQuery(passwordsProvider, ".findRecord", getAllColumns(), WHERE_RECORD_DATA, whereArgs, null);
      while (cursor.moveToNext()) {
        foundRecord = passwordRecordFromCursor(cursor);

        // We don't directly query for username because the
        // username/password values are encrypted in the db.
        // We don't have the keys for encrypting our query,
        // so we run a more general query and then filter
        // the returned records for a matching username.
        Logger.trace(LOG_TAG, "Checking incoming [" + record.encryptedUsername + "] to [" + foundRecord.encryptedUsername + "]");
        if (record.encryptedUsername.equals(foundRecord.encryptedUsername)) {
          Logger.trace(LOG_TAG, "Found matching record: " + foundRecord);
          return foundRecord;
        }
      }
    } catch (RemoteException e) {
      Logger.error(LOG_TAG, "Remote exception in findExistingRecord.");
      delegate.onRecordStoreFailed(e);
    } catch (NullCursorException e) {
      Logger.error(LOG_TAG, "Null cursor in findExistingRecord.");
      delegate.onRecordStoreFailed(e);
    } finally {
      if (cursor != null) {
        cursor.close();
      }
    }
    Logger.debug(LOG_TAG, "No matching records, returning null.");
    return null;
  }

  private void storeRecordDeletion(Record record) {
    try {
      deleteGUID(record.guid);
    } catch (RemoteException e) {
      Logger.error(LOG_TAG, "RemoteException in password delete.");
      delegate.onRecordStoreFailed(e);
      return;
    }
    delegate.onRecordStoreSucceeded(record);
  }

  /**
   * Make a PasswordRecord from a Cursor.
   * @param cur
   *        Cursor from query.
   * @param deleted
   *        true if creating a deleted Record, false if otherwise.
   * @return
   *        PasswordRecord populated from Cursor.
   */
  private static PasswordRecord passwordRecordFromCursor(Cursor cur) {
    if (cur.isAfterLast()) {
      return null;
    }
    String guid = RepoUtils.getStringFromCursor(cur, BrowserContract.Passwords.GUID);
    long lastModified = RepoUtils.getLongFromCursor(cur, BrowserContract.Passwords.TIME_PASSWORD_CHANGED);

    PasswordRecord rec = new PasswordRecord(guid, COLLECTION, lastModified, false);
    rec.id = RepoUtils.getStringFromCursor(cur, BrowserContract.Passwords.ID);
    rec.hostname = RepoUtils.getStringFromCursor(cur, BrowserContract.Passwords.HOSTNAME);
    rec.httpRealm = RepoUtils.getStringFromCursor(cur, BrowserContract.Passwords.HTTP_REALM);
    rec.formSubmitURL = RepoUtils.getStringFromCursor(cur, BrowserContract.Passwords.FORM_SUBMIT_URL);
    rec.usernameField = RepoUtils.getStringFromCursor(cur, BrowserContract.Passwords.USERNAME_FIELD);
    rec.passwordField = RepoUtils.getStringFromCursor(cur, BrowserContract.Passwords.PASSWORD_FIELD);
    rec.encType = RepoUtils.getStringFromCursor(cur, BrowserContract.Passwords.ENC_TYPE);

    // TODO decryption of username/password here (Bug 711636)
    rec.encryptedUsername = RepoUtils.getStringFromCursor(cur, BrowserContract.Passwords.ENCRYPTED_USERNAME);
    rec.encryptedPassword = RepoUtils.getStringFromCursor(cur, BrowserContract.Passwords.ENCRYPTED_PASSWORD);

    rec.timeCreated = RepoUtils.getLongFromCursor(cur, BrowserContract.Passwords.TIME_CREATED);
    rec.timeLastUsed = RepoUtils.getLongFromCursor(cur, BrowserContract.Passwords.TIME_LAST_USED);
    rec.timePasswordChanged = RepoUtils.getLongFromCursor(cur, BrowserContract.Passwords.TIME_PASSWORD_CHANGED);
    rec.timesUsed = RepoUtils.getLongFromCursor(cur, BrowserContract.Passwords.TIMES_USED);
    return rec;
  }

  private static PasswordRecord deletedPasswordRecordFromCursor(Cursor cur) {
    if (cur.isAfterLast()) {
      return null;
    }
    String guid = RepoUtils.getStringFromCursor(cur, DeletedColumns.GUID);
    long lastModified = RepoUtils.getLongFromCursor(cur, DeletedColumns.TIME_DELETED);
    PasswordRecord rec = new PasswordRecord(guid, COLLECTION, lastModified, true);
    rec.androidID = RepoUtils.getLongFromCursor(cur, DeletedColumns.ID);
    return rec;
  }

  private static ContentValues getContentValues(Record record) {
    PasswordRecord rec = (PasswordRecord) record;

    ContentValues cv = new ContentValues();
    cv.put(BrowserContract.Passwords.GUID,            rec.guid);
    cv.put(BrowserContract.Passwords.HOSTNAME,        rec.hostname);
    cv.put(BrowserContract.Passwords.HTTP_REALM,      rec.httpRealm);
    cv.put(BrowserContract.Passwords.FORM_SUBMIT_URL, rec.formSubmitURL);
    cv.put(BrowserContract.Passwords.USERNAME_FIELD,  rec.usernameField);
    cv.put(BrowserContract.Passwords.PASSWORD_FIELD,  rec.passwordField);

    // TODO Do encryption of username/password here. Bug 711636
    cv.put(BrowserContract.Passwords.ENC_TYPE,           rec.encType);
    cv.put(BrowserContract.Passwords.ENCRYPTED_USERNAME, rec.encryptedUsername);
    cv.put(BrowserContract.Passwords.ENCRYPTED_PASSWORD, rec.encryptedPassword);

    cv.put(BrowserContract.Passwords.TIME_CREATED,          rec.timeCreated);
    cv.put(BrowserContract.Passwords.TIME_LAST_USED,        rec.timeLastUsed);
    cv.put(BrowserContract.Passwords.TIME_PASSWORD_CHANGED, rec.timePasswordChanged);
    cv.put(BrowserContract.Passwords.TIMES_USED,            rec.timesUsed);
    return cv;
  }
}
