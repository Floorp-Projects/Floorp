/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reading;

import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.SYNC_CHANGE_FAVORITE_CHANGED;
import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.SYNC_CHANGE_FLAGS;
import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.SYNC_CHANGE_UNREAD_CHANGED;
import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.SYNC_STATUS;
import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.SYNC_STATUS_MODIFIED;
import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.SYNC_STATUS_NEW;

import java.util.ArrayList;
import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserContract.ReadingListItems;
import org.mozilla.gecko.sync.repositories.android.RepoUtils;

import android.content.ContentProviderClient;
import android.content.ContentProviderOperation;
import android.content.ContentProviderResult;
import android.content.ContentValues;
import android.content.OperationApplicationException;
import android.database.Cursor;
import android.net.Uri;
import android.os.RemoteException;

public class LocalReadingListStorage implements ReadingListStorage {

  private static final String WHERE_STATUS_NEW = "(" + SYNC_STATUS + " = " + SYNC_STATUS_NEW + ")";

  final class LocalReadingListChangeAccumulator implements ReadingListChangeAccumulator {
    private static final String LOG_TAG = "RLChanges";

    /**
     * These are changes that result from uploading new or changed records to the server.
     * They always correspond to local records.
     */
    private final Queue<ClientReadingListRecord> changes;

    /**
     * These are deletions that result from uploading new or changed records to the server.
     * They should always correspond to local records.
     * These are not common: they should only occur if a conflict occurs.
     */
    private final Queue<ClientReadingListRecord> deletions;
    private final Queue<String> deletedGUIDs;

    /**
     * These are additions or changes fetched from the server.
     * At the point of collection we don't know if they're records
     * that exist locally.
     *
     * Batching these here, rather than in the client or the synchronizer,
     * puts the storage implementation in control of when batches are flushed,
     * or if batches are used at all.
     */
    private final Queue<ServerReadingListRecord> additionsOrChanges;

    LocalReadingListChangeAccumulator() {
      this.changes = new ConcurrentLinkedQueue<>();
      this.deletions = new ConcurrentLinkedQueue<>();
      this.deletedGUIDs = new ConcurrentLinkedQueue<>();
      this.additionsOrChanges = new ConcurrentLinkedQueue<>();
    }

    public boolean flushDeletions() throws RemoteException {
      if (deletions.isEmpty() && deletedGUIDs.isEmpty()) {
        return true;
      }

      long[] ids = new long[deletions.size()];
      String[] guids = new String[deletions.size() + deletedGUIDs.size()];
      int iID = 0;
      int iGUID = 0;
      for (ClientReadingListRecord record : deletions) {
        if (record.clientMetadata.id > -1L) {
          ids[iID++] = record.clientMetadata.id;
        } else {
          final String guid = record.getGUID();
          if (guid == null) {
            continue;
          }
          guids[iGUID++] = guid;
        }
      }
      for (String guid : deletedGUIDs) {
        guids[iGUID++] = guid;
      }

      if (iID > 0) {
        client.delete(URI_WITH_DELETED, RepoUtils.computeSQLLongInClause(ids, ReadingListItems._ID), null);
      }

      if (iGUID > 0) {
        client.delete(URI_WITH_DELETED, RepoUtils.computeSQLInClause(iGUID, ReadingListItems.GUID), guids);
      }

      deletions.clear();
      deletedGUIDs.clear();
      return true;
    }

    public boolean flushRecordChanges() throws RemoteException {
      if (changes.isEmpty() && additionsOrChanges.isEmpty()) {
        return true;
      }

      // For each returned record, apply it to the local store and clear all sync flags.
      // We can do this because the server always returns the entire record.
      //
      // <https://github.com/mozilla-services/readinglist/issues/138> tracks not doing so
      // for certain patches, which allows us to optimize here.
      ArrayList<ContentProviderOperation> operations = new ArrayList<>(changes.size() + additionsOrChanges.size());
      for (ClientReadingListRecord rec : changes) {
        operations.add(makeUpdateOp(rec));
      }

      for (ServerReadingListRecord rec : additionsOrChanges) {
        // TODO: skip records for which the local copy of the server timestamp
        // matches the timestamp in the incoming record.
        // TODO: we can do this by maintaining a lookup table, rather
        // than hitting the DB. When we do an insert after an upload, say, we
        // can make a note of it so the next download flush doesn't apply it twice.
        operations.add(makeUpdateOrInsertOp(rec));
      }

      // TODO: tell delegate of success or failure.
      try {
        Logger.debug(LOG_TAG, "Applying " + operations.size() + " operations.");
        ContentProviderResult[] results = client.applyBatch(operations);
      } catch (OperationApplicationException e) {
        // Oops.
        Logger.warn(LOG_TAG, "Applying operations failed.", e);
        return false;
      }
      return true;
    }

    private ContentProviderOperation makeUpdateOrInsertOp(ServerReadingListRecord rec) throws RemoteException {
      final ClientReadingListRecord clientRec = new ClientReadingListRecord(rec.serverMetadata, null, rec.fields);

      // TODO: use UPDATE OR INSERT equivalent, rather than querying here.
      if (hasGUID(rec.serverMetadata.guid)) {
        return makeUpdateOp(clientRec);
      }

      final ContentValues values = ReadingListClientContentValuesFactory.fromClientRecord(clientRec);
      return ContentProviderOperation.newInsert(URI_WITHOUT_DELETED)
                                     .withValues(values)
                                     .build();
    }

    private ContentProviderOperation makeUpdateOp(ClientReadingListRecord rec) {
      // We can't use SQLiteQueryBuilder, because it can't do UPDATE,
      // nor can it give us a WHERE clause.
      final StringBuilder selection = new StringBuilder();
      final String[] selectionArgs;

      // We don't apply changes that we've already applied.
      // We know they've already been applied because our local copy of the
      // server's version code/timestamp matches the value in the incoming record.
      long serverLastModified = rec.getServerLastModified();
      if (serverLastModified != -1L) {
        // This should always be the case here.
        selection.append("(" + ReadingListItems.SERVER_LAST_MODIFIED + " IS NOT ");
        selection.append(serverLastModified);
        selection.append(") AND ");
      }

      if (rec.clientMetadata.id > -1L) {
        selection.append("(");
        selection.append(ReadingListItems._ID + " = ");
        selection.append(rec.clientMetadata.id);
        selection.append(")");
        selectionArgs = null;
      } else if (rec.serverMetadata.guid != null) {
        selection.append("(" + ReadingListItems.GUID + " = ?)");
        selectionArgs = new String[] { rec.serverMetadata.guid };
      } else {
        final String url = rec.fields.getString("url");
        final String resolvedURL = rec.fields.getString("resolved_url");

        if (url == null && resolvedURL == null) {
          // We're outta luck.
          return null;
        }

        selection.append("((" + ReadingListItems.URL + " = ?) OR (" + ReadingListItems.RESOLVED_URL + " = ?))");
        if (url != null && resolvedURL != null) {
          selectionArgs = new String[] { url, resolvedURL };
        } else {
          final String arg = url == null ? resolvedURL : url;
          selectionArgs = new String[] { arg, arg };
        }
      }

      final ContentValues values = ReadingListClientContentValuesFactory.fromClientRecord(rec);
      return ContentProviderOperation.newUpdate(URI_WITHOUT_DELETED)
                                     .withSelection(selection.toString(), selectionArgs)
                                     .withValues(values)
                                     .build();
    }

    @Override
    public void finish() throws Exception {
      flushDeletions();
      flushRecordChanges();
    }

    @Override
    public void addDeletion(ClientReadingListRecord record) {
      deletions.add(record);
    }

    @Override
    public void addDeletion(String guid) {
      deletedGUIDs.add(guid);
    }

    @Override
    public void addChangedRecord(ClientReadingListRecord record) {
      changes.add(record);
    }

    @Override
    public void addDownloadedRecord(ServerReadingListRecord down) {
      final Boolean deleted = down.fields.getBoolean("deleted");
      if (deleted != null && deleted.booleanValue()) {
        addDeletion(down.getGUID());
      } else {
        additionsOrChanges.add(down);
      }
    }
  }

  private final ContentProviderClient client;
  private final Uri URI_WITHOUT_DELETED = BrowserContract.READING_LIST_AUTHORITY_URI
      .buildUpon()
      .appendPath("items")
      .appendQueryParameter(BrowserContract.PARAM_IS_SYNC, "1")
      .appendQueryParameter(BrowserContract.PARAM_SHOW_DELETED, "0")
      .build();

  private final Uri URI_WITH_DELETED = BrowserContract.READING_LIST_AUTHORITY_URI
      .buildUpon()
      .appendPath("items")
      .appendQueryParameter(BrowserContract.PARAM_IS_SYNC, "1")
      .appendQueryParameter(BrowserContract.PARAM_SHOW_DELETED, "1")
      .build();

  public LocalReadingListStorage(final ContentProviderClient client) {
    this.client = client;
  }

  public boolean hasGUID(String guid) throws RemoteException {
    final String[] projection = new String[] { ReadingListItems.GUID };
    final String selection = ReadingListItems.GUID + " = ?";
    final String[] selectionArgs = new String[] { guid };
    final Cursor cursor = this.client.query(URI_WITHOUT_DELETED, projection, selection, selectionArgs, null);
    try {
      return cursor.moveToFirst();
    } finally {
      cursor.close();
    }
  }

  public Cursor getModifiedWithSelection(final String selection) {
    final String[] projection = new String[] {
      ReadingListItems.GUID,
      ReadingListItems.IS_FAVORITE,
      ReadingListItems.RESOLVED_TITLE,
      ReadingListItems.RESOLVED_URL,
      ReadingListItems.EXCERPT,
      // TODO: ReadingListItems.IS_ARTICLE,
      // TODO: ReadingListItems.WORD_COUNT,
    };

    try {
      return client.query(URI_WITHOUT_DELETED, projection, selection, null, null);
    } catch (RemoteException e) {
      throw new IllegalStateException(e);
    }
  }

  @Override
  public Cursor getModified() {
    final String selection = ReadingListItems.SYNC_STATUS + " = " + ReadingListItems.SYNC_STATUS_MODIFIED;
    return getModifiedWithSelection(selection);
  }

  // Return changed items that aren't just status changes.
  // This isn't necessary because we insist on processing status changes before modified items.
  // Currently we only need this for tests...
  public Cursor getNonStatusModified() {
    final String selection = ReadingListItems.SYNC_STATUS + " = " + ReadingListItems.SYNC_STATUS_MODIFIED +
                             " AND ((" + ReadingListItems.SYNC_CHANGE_FLAGS + " & " + ReadingListItems.SYNC_CHANGE_RESOLVED + ") > 0)";

    return getModifiedWithSelection(selection);
  }

  // These will never conflict (in the case of unread status changes), or
  // we don't care if they overwrite the server value (in the case of favorite changes).
  // N.B., don't actually send each field if the appropriate change flag isn't set!
  @Override
  public Cursor getStatusChanges() {
    final String[] projection = new String[] {
      ReadingListItems.GUID,
      ReadingListItems.IS_FAVORITE,
      ReadingListItems.IS_UNREAD,
      ReadingListItems.MARKED_READ_BY,
      ReadingListItems.MARKED_READ_ON,
      ReadingListItems.SYNC_CHANGE_FLAGS,
    };

    final String selection =
        SYNC_STATUS + " = " + SYNC_STATUS_MODIFIED + " AND " +
        "((" + SYNC_CHANGE_FLAGS + " & (" + SYNC_CHANGE_UNREAD_CHANGED + " | " + SYNC_CHANGE_FAVORITE_CHANGED + ")) > 0)";

    try {
      return client.query(URI_WITHOUT_DELETED, projection, selection, null, null);
    } catch (RemoteException e) {
      throw new IllegalStateException(e);
    }
  }

  @Override
  public Cursor getDeletedItems() {
    final String[] projection = new String[] {
      ReadingListItems.GUID,
    };

    final String selection = "(" + ReadingListItems.IS_DELETED + " = 1) AND (" + ReadingListItems.GUID + " IS NOT NULL)";
    try {
      return client.query(URI_WITH_DELETED, projection, selection, null, null);
    } catch (RemoteException e) {
      throw new IllegalStateException(e);
    }
  }

  @Override
  public Cursor getNew() {
    // N.B., query for items that have no GUID, regardless of status.
    // They should all be marked as NEW, but belt and braces.
    final String selection = WHERE_STATUS_NEW + " OR (" + ReadingListItems.GUID + " IS NULL)";

    try {
      return client.query(URI_WITHOUT_DELETED, null, selection, null, null);
    } catch (RemoteException e) {
      throw new IllegalStateException(e);
    }
  }

  @Override
  public Cursor getAll() {
    try {
      return client.query(URI_WITHOUT_DELETED, null, null, null, null);
    } catch (RemoteException e) {
      throw new IllegalStateException(e);
    }
  }

  private ContentProviderOperation updateAddedByNames(final String local) {
    String[] selectionArgs = new String[] {"$local"};
    String selection = WHERE_STATUS_NEW + " AND (" + ReadingListItems.ADDED_BY + " = ?)";
    return ContentProviderOperation.newUpdate(URI_WITHOUT_DELETED)
                                   .withValue(ReadingListItems.ADDED_BY, local)
                                   .withSelection(selection, selectionArgs)
                                   .build();
  }

  private ContentProviderOperation updateMarkedReadByNames(final String local) {
    String[] selectionArgs = new String[] {"$local"};
    String selection = ReadingListItems.MARKED_READ_BY + " = ?";
    return ContentProviderOperation.newUpdate(URI_WITHOUT_DELETED)
                                   .withValue(ReadingListItems.MARKED_READ_BY, local)
                                   .withSelection(selection, selectionArgs)
                                   .build();
  }

  /**
   * Consumers of the reading list provider don't know the device name.
   * Rather than smearing that logic into callers, or requiring the database
   * to be able to figure out the name of the device, we have the SyncAdapter
   * do it.
   *
   * After all, the SyncAdapter knows everything -- prefs, channels, profiles,
   * Firefox Account details, etc.
   *
   * To allow this, the CP writes the magic string "$local" wherever a device
   * name is needed. Here in storage, we run a quick UPDATE pass prior to
   * synchronizing, so the device name is 'calcified' at the time of the first
   * sync of that record. The SyncAdapter calls this prior to invoking the
   * synchronizer.
   */
  public void updateLocalNames(final String local) {
    ArrayList<ContentProviderOperation> ops = new ArrayList<ContentProviderOperation>(2);
    ops.add(updateAddedByNames(local));
    ops.add(updateMarkedReadByNames(local));

    try {
      client.applyBatch(ops);
    } catch (RemoteException e) {
      return;
    } catch (OperationApplicationException e) {
      return;
    }
  }

  @Override
  public ReadingListChangeAccumulator getChangeAccumulator() {
    return new LocalReadingListChangeAccumulator();
  }

  /**
   * Unused: we implicitly do this when we apply the server record.
   */
  /*
  public void markStatusChangedItemsAsSynced(Collection<String> uploaded) {
    ContentValues values = new ContentValues();
    values.put(ReadingListItems.SYNC_CHANGE_FLAGS, ReadingListItems.SYNC_CHANGE_NONE);
    values.put(ReadingListItems.SYNC_STATUS, ReadingListItems.SYNC_STATUS_SYNCED);
    final String where = RepoUtils.computeSQLInClause(uploaded.size(), ReadingListItems.GUID);
    final String[] args = uploaded.toArray(new String[uploaded.size()]);
    try {
      client.update(URI_WITHOUT_DELETED, values, where, args);
    } catch (RemoteException e) {
      // Nothing we can do.
    }
  }
  */
}
