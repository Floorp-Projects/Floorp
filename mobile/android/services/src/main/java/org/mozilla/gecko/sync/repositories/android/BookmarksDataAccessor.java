/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.Map;

import org.json.simple.JSONArray;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.domain.BookmarkRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;

public class BookmarksDataAccessor extends DataAccessor {

  private static final String LOG_TAG = "BookmarksDataAccessor";

  /*
   * Fragments of SQL to make our lives easier.
   */
  private static final String BOOKMARK_IS_FOLDER = BrowserContract.Bookmarks.TYPE + " = " +
                                                   BrowserContract.Bookmarks.TYPE_FOLDER;

  // SQL fragment to retrieve GUIDs whose ID mappings should be tracked by this session.
  // Exclude folders we don't want to sync.
  private static final String GUID_SHOULD_TRACK = BrowserContract.SyncColumns.GUID + " NOT IN ('" +
                                                  BrowserContract.Bookmarks.TAGS_FOLDER_GUID + "', '" +
                                                  BrowserContract.Bookmarks.PLACES_FOLDER_GUID + "', '" +
                                                  BrowserContract.Bookmarks.PINNED_FOLDER_GUID + "')";

  private static final String EXCLUDE_SPECIAL_GUIDS_WHERE_CLAUSE;
  static {
    if (BookmarksRepositorySession.SPECIAL_GUIDS.length > 0) {
      StringBuilder b = new StringBuilder(BrowserContract.SyncColumns.GUID + " NOT IN (");

      int remaining = BookmarksRepositorySession.SPECIAL_GUIDS.length - 1;
      for (String specialGuid : BookmarksRepositorySession.SPECIAL_GUIDS) {
        b.append('"');
        b.append(specialGuid);
        b.append('"');
        if (remaining-- > 0) {
          b.append(", ");
        }
      }
      b.append(')');
      EXCLUDE_SPECIAL_GUIDS_WHERE_CLAUSE = b.toString();
    } else {
      EXCLUDE_SPECIAL_GUIDS_WHERE_CLAUSE = null;       // null is a valid WHERE clause.
    }
  }

  public static final String TYPE_FOLDER = "folder";
  public static final String TYPE_BOOKMARK = "bookmark";

  private final RepoUtils.QueryHelper queryHelper;

  public BookmarksDataAccessor(Context context) {
    super(context);
    this.queryHelper = new RepoUtils.QueryHelper(context, getUri(), LOG_TAG);
  }

  @Override
  protected Uri getUri() {
    return BrowserContractHelpers.BOOKMARKS_CONTENT_URI;
  }

  /**
   * Order bookmarks by type, ensuring that folders will be processed before other records during
   * an upload. This is in support of payload-size validation. See Bug 1343726.
   */
  public Cursor fetchModified() throws NullCursorException {
    return queryHelper.safeQuery(".fetchModified",
            getAllColumns(),
            BrowserContract.VersionColumns.LOCAL_VERSION + " > " + BrowserContract.VersionColumns.SYNC_VERSION,
            null, BrowserContract.Bookmarks.TYPE + " ASC");
  }

  protected static Uri getPositionsUri() {
    return BrowserContractHelpers.BOOKMARKS_POSITIONS_CONTENT_URI;
  }

  @Override
  public void wipe() {
    Uri uri = getUri();
    Logger.info(LOG_TAG, "wiping (except for special guids): " + uri);
    context.getContentResolver().delete(uri, EXCLUDE_SPECIAL_GUIDS_WHERE_CLAUSE, null);
  }

  private final String[] GUID_AND_ID = new String[] { BrowserContract.Bookmarks.GUID,
                                                BrowserContract.Bookmarks._ID };

  protected Cursor getGuidsIDsForFolders() throws NullCursorException {
    // Exclude items that we don't want to sync (pinned items, reading list, 
    // tags, the places root), in case they've ended up in the DB.
    String where = BOOKMARK_IS_FOLDER + " AND " + GUID_SHOULD_TRACK;
    return queryHelper.safeQuery(".getGuidsIDsForFolders", GUID_AND_ID, where, null, null);
  }

  /**
   * Update a record identified by GUID to new values, but only if assertion passes that localVersion
   * did not change. This is expected to be called during record reconciliation, and so we also request
   * that localVersion is incremented in the process.
   */
  /* package-private */ boolean updateAssertingLocalVersion(String guid, int expectedLocalVersion, boolean shouldIncrementLocalVersion, Record newRecord) {
    final ContentValues cv = getContentValues(newRecord);
    final Bundle data = new Bundle();
    data.putString(BrowserContract.SyncColumns.GUID, guid);
    data.putInt(BrowserContract.VersionColumns.LOCAL_VERSION, expectedLocalVersion);
    data.putParcelable(BrowserContract.METHOD_PARAM_DATA, cv);

    final Uri callUri;
    if (shouldIncrementLocalVersion) {
      callUri = withLocalVersionIncrement(getUri());
    } else {
      callUri = getUri();
    }

    final Bundle result = context.getContentResolver().call(
            callUri,
            BrowserContract.METHOD_UPDATE_BY_GUID_ASSERTING_LOCAL_VERSION,
            callUri.toString(),
            data
    );
    if (result == null) {
      throw new IllegalStateException("Unexpected null result after METHOD_UPDATE_BY_GUID_ASSERTING_LOCAL_VERSION");
    }
    return (boolean) result.getSerializable(BrowserContract.METHOD_RESULT);
  }

  private Uri withLocalVersionIncrement(Uri baseUri) {
    return baseUri.buildUpon().appendQueryParameter(BrowserContractHelpers.PARAM_INCREMENT_LOCAL_VERSION_FROM_SYNC, "true").build();
  }


  /**
   * Issue a request to the Content Provider to update the positions of the
   * records named by the provided GUIDs to the index of their GUID in the
   * provided array.
   *
   * @param childArray
   *        A sequence of GUID strings.
   */
  public int updatePositions(ArrayList<String> childArray) {
    final int size = childArray.size();
    if (size == 0) {
      return 0;
    }

    Logger.debug(LOG_TAG, "Updating positions for " + size + " items.");
    String[] args = childArray.toArray(new String[size]);
    return context.getContentResolver().update(getPositionsUri(), new ContentValues(), null, args);
  }

  public int bumpModifiedByGUID(Collection<String> ids, long modified) {
    final int size = ids.size();
    if (size == 0) {
      return 0;
    }

    Logger.debug(LOG_TAG, "Bumping modified for " + size + " items to " + modified);
    String where = RepoUtils.computeSQLInClause(size, BrowserContract.Bookmarks.GUID);
    String[] selectionArgs = ids.toArray(new String[size]);
    ContentValues values = new ContentValues();
    values.put(BrowserContract.Bookmarks.DATE_MODIFIED, modified);

    return context.getContentResolver().update(
            withLocalVersionIncrement(getUri()),
            values, where, selectionArgs);
  }

  /**
   * Bump the modified time of a record by ID.
   */
  public int bumpModified(long id, long modified) {
    Logger.debug(LOG_TAG, "Bumping modified for " + id + " to " + modified);
    String where = BrowserContract.Bookmarks._ID + " = ?";
    String[] selectionArgs = new String[] { String.valueOf(id) };
    ContentValues values = new ContentValues();
    values.put(BrowserContract.Bookmarks.DATE_MODIFIED, modified);

    return context.getContentResolver().update(
            withLocalVersionIncrement(getUri()),
            values, where, selectionArgs);
  }

  protected void updateParentAndPosition(String guid, long newParentId, long position) {
    ContentValues cv = new ContentValues();
    cv.put(BrowserContract.Bookmarks.PARENT, newParentId);
    if (position >= 0) {
      cv.put(BrowserContract.Bookmarks.POSITION, position);
    }
    updateByGuid(guid, cv);
  }

  protected Map<String, Long> idsForGUIDs(String[] guids) throws NullCursorException {
    final String where = RepoUtils.computeSQLInClause(guids.length, BrowserContract.Bookmarks.GUID);
    Cursor c = queryHelper.safeQuery(".idsForGUIDs", GUID_AND_ID, where, guids, null);
    try {
      HashMap<String, Long> out = new HashMap<String, Long>();
      if (!c.moveToFirst()) {
        return out;
      }
      final int guidIndex = c.getColumnIndexOrThrow(BrowserContract.Bookmarks.GUID);
      final int idIndex = c.getColumnIndexOrThrow(BrowserContract.Bookmarks._ID);
      while (!c.isAfterLast()) {
        out.put(c.getString(guidIndex), c.getLong(idIndex));
        c.moveToNext();
      }
      return out;
    } finally {
      c.close();
    }
  }

  /**
   * Move the children of each source folder to the destination folder.
   * Bump the modified time of each child.
   * The caller should bump the modified time of the destination if desired.
   *
   * @param fromIDs the Android IDs of the source folders.
   * @param to the Android ID of the destination folder.
   * @return the number of updated rows.
   */
  protected int moveChildren(String[] fromIDs, long to) {
    long now = System.currentTimeMillis();
    long pos = -1;

    ContentValues cv = new ContentValues();
    cv.put(BrowserContract.Bookmarks.PARENT, to);
    cv.put(BrowserContract.Bookmarks.DATE_MODIFIED, now);
    cv.put(BrowserContract.Bookmarks.POSITION, pos);

    final String where = RepoUtils.computeSQLInClause(fromIDs.length, BrowserContract.Bookmarks.PARENT);
    return context.getContentResolver().update(withLocalVersionIncrement(getUri()), cv, where, fromIDs);
  }

  /*
   * Verify that all special GUIDs are present and that they aren't marked as deleted.
   * Insert them if they aren't there.
   */
  public void checkAndBuildSpecialGuids() throws NullCursorException {
    final String[] specialGUIDs = BookmarksRepositorySession.SPECIAL_GUIDS;
    Cursor cur = fetch(specialGUIDs);
    long placesRoot = 0;

    // Map from GUID to whether deleted. Non-presence implies just that.
    HashMap<String, Boolean> statuses = new HashMap<String, Boolean>(specialGUIDs.length);
    try {
      if (cur.moveToFirst()) {
        while (!cur.isAfterLast()) {
          String guid = RepoUtils.getStringFromCursor(cur, BrowserContract.SyncColumns.GUID);
          if ("places".equals(guid)) {
            placesRoot = RepoUtils.getLongFromCursor(cur, BrowserContract.CommonColumns._ID);
          }
          // Make sure none of these folders are marked as deleted.
          boolean deleted = RepoUtils.getLongFromCursor(cur, BrowserContract.SyncColumns.IS_DELETED) == 1;
          statuses.put(guid, deleted);
          cur.moveToNext();
        }
      }
    } finally {
      cur.close();
    }

    // Insert or undelete them if missing.
    for (String guid : specialGUIDs) {
      if (statuses.containsKey(guid)) {
        if (statuses.get(guid)) {
          // Undelete.
          Logger.info(LOG_TAG, "Undeleting special GUID " + guid);
          ContentValues cv = new ContentValues();
          cv.put(BrowserContract.SyncColumns.IS_DELETED, 0);
          updateByGuid(guid, cv);
        }
      } else {
        // Insert.
        if (guid.equals("places")) {
          // This is awkward.
          Logger.info(LOG_TAG, "No places root. Inserting one.");
          placesRoot = insertSpecialFolder("places", 0);
        } else if (guid.equals("mobile")) {
          Logger.info(LOG_TAG, "No mobile folder. Inserting one under the places root.");
          insertSpecialFolder("mobile", placesRoot);
        } else {
          // unfiled, menu, toolbar.
          Logger.info(LOG_TAG, "No " + guid + " root. Inserting one under places (" + placesRoot + ").");
          insertSpecialFolder(guid, placesRoot);
        }
      }
    }
  }

  private long insertSpecialFolder(String guid, long parentId) {
    BookmarkRecord record = new BookmarkRecord(guid);
    record.title = BookmarksRepositorySession.SPECIAL_GUIDS_MAP.get(guid);
    record.type = "folder";
    record.androidParentID = parentId;
    return ContentUris.parseId(insert(record));
  }

  @Override
  protected ContentValues getContentValues(Record record) {
    BookmarkRecord rec = (BookmarkRecord) record;

    if (rec.deleted) {
      ContentValues cv = new ContentValues();
      cv.put(BrowserContract.SyncColumns.GUID,     rec.guid);
      cv.put(BrowserContract.Bookmarks.IS_DELETED, 1);
      return cv;
    }

    final int recordType = BrowserContractHelpers.typeCodeForString(rec.type);
    if (recordType == -1) {
      throw new IllegalStateException("Unexpected record type " + rec.type);
    }

    ContentValues cv = new ContentValues();
    cv.put(BrowserContract.SyncColumns.GUID,      rec.guid);
    cv.put(BrowserContract.Bookmarks.TYPE,        recordType);
    cv.put(BrowserContract.Bookmarks.TITLE,       rec.title);
    cv.put(BrowserContract.Bookmarks.URL,         rec.bookmarkURI);
    cv.put(BrowserContract.Bookmarks.DESCRIPTION, rec.description);

    if (rec.dateAdded != null) {
      cv.put(BrowserContract.Bookmarks.DATE_CREATED, rec.dateAdded);
    }

    if (rec.tags == null) {
      rec.tags = new JSONArray();
    }

    // We might want to indicate that this record is to be inserted as "modified". Incoming records
    // might be modified as we're processing them for insertion, and so should be re-uploaded.
    // Our data provider layer manages versions, so we don't pass in localVersion explicitly.
    if (rec.modifiedBySync) {
      cv.put(BrowserContract.Bookmarks.PARAM_INSERT_FROM_SYNC_AS_MODIFIED, true);
    }

    cv.put(BrowserContract.Bookmarks.TAGS,        rec.tags.toJSONString());
    cv.put(BrowserContract.Bookmarks.KEYWORD,     rec.keyword);
    cv.put(BrowserContract.Bookmarks.PARENT,      rec.androidParentID);
    cv.put(BrowserContract.Bookmarks.POSITION,    rec.androidPosition);

    // Note that we don't set the modified timestamp: we allow the
    // content provider to do that for us.
    return cv;
  }

  /**
   * Returns a cursor over non-deleted records that list the given androidID as a parent.
   */
  public Cursor getChildren(long androidID) throws NullCursorException {
    return getChildren(androidID, false);
  }

  /**
   * Returns a cursor with any records that list the given androidID as a parent.
   * Excludes 'places', and optionally any deleted records.
   */
  public Cursor getChildren(long androidID, boolean includeDeleted) throws NullCursorException {
    final String where = BrowserContract.Bookmarks.PARENT + " = ? AND " +
                         BrowserContract.SyncColumns.GUID + " <> ? " +
                         (!includeDeleted ? ("AND " + BrowserContract.SyncColumns.IS_DELETED + " = 0") : "");

    final String[] args = new String[] { String.valueOf(androidID), "places" };

    // Order by position, falling back on creation date and ID.
    final String order = BrowserContract.Bookmarks.POSITION + ", " +
                         BrowserContract.SyncColumns.DATE_CREATED + ", " +
                         BrowserContract.Bookmarks._ID;
    return queryHelper.safeQuery(".getChildren", getAllColumns(), where, args, order);
  }

  
  @Override
  protected String[] getAllColumns() {
    return BrowserContractHelpers.BookmarkColumns;
  }
}
