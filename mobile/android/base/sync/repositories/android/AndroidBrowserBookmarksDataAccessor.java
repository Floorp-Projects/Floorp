/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import java.util.ArrayList;
import java.util.HashMap;

import org.json.simple.JSONArray;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.domain.BookmarkRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;

public class AndroidBrowserBookmarksDataAccessor extends AndroidBrowserRepositoryDataAccessor {

  private static final String LOG_TAG = "BookmarksDataAccessor";

  /*
   * Fragments of SQL to make our lives easier.
   */
  private static final String BOOKMARK_IS_FOLDER = BrowserContract.Bookmarks.IS_FOLDER + " = 1";
  private static final String GUID_NOT_TAGS_OR_PLACES = BrowserContract.SyncColumns.GUID + " NOT IN ('" +
                                                        BrowserContract.Bookmarks.TAGS_FOLDER_GUID + "', '" +
                                                        BrowserContract.Bookmarks.PLACES_FOLDER_GUID + "')";

  private static final String EXCLUDE_SPECIAL_GUIDS_WHERE_CLAUSE;
  static {
    if (AndroidBrowserBookmarksRepositorySession.SPECIAL_GUIDS.length > 0) {
      StringBuilder b = new StringBuilder(BrowserContract.SyncColumns.GUID + " NOT IN (");

      int remaining = AndroidBrowserBookmarksRepositorySession.SPECIAL_GUIDS.length - 1;
      for (String specialGuid : AndroidBrowserBookmarksRepositorySession.SPECIAL_GUIDS) {
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

  public AndroidBrowserBookmarksDataAccessor(Context context) {
    super(context);
    this.queryHelper = new RepoUtils.QueryHelper(context, getUri(), LOG_TAG);
  }

  @Override
  protected Uri getUri() {
    return BrowserContractHelpers.BOOKMARKS_CONTENT_URI;
  }

  protected Uri getPositionsUri() {
    return BrowserContractHelpers.BOOKMARKS_POSITIONS_CONTENT_URI;
  }

  @Override
  public void wipe() {
    Uri uri = getUri();
    Logger.info(LOG_TAG, "wiping (except for special guids): " + uri);
    context.getContentResolver().delete(uri, EXCLUDE_SPECIAL_GUIDS_WHERE_CLAUSE, null);
  }

  protected Cursor getGuidsIDsForFolders() throws NullCursorException {
    // Exclude "places" and "tags", in case they've ended up in the DB.
    String where = BOOKMARK_IS_FOLDER + " AND " + GUID_NOT_TAGS_OR_PLACES;
    return queryHelper.safeQuery(".getGuidsIDsForFolders", null, where, null, null);
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
    Logger.debug(LOG_TAG, "Updating positions for " + childArray.size() + " items.");
    String[] args = childArray.toArray(new String[childArray.size()]);
    return context.getContentResolver().update(getPositionsUri(), new ContentValues(), null, args);
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

    return context.getContentResolver().update(getUri(), values, where, selectionArgs);
  }

  protected void updateParentAndPosition(String guid, long newParentId, long position) {
    ContentValues cv = new ContentValues();
    cv.put(BrowserContract.Bookmarks.PARENT, newParentId);
    if (position >= 0) {
      cv.put(BrowserContract.Bookmarks.POSITION, position);
    }
    updateByGuid(guid, cv);
  } 
  
  /*
   * Verify that all special GUIDs are present and that they aren't marked as deleted.
   * Insert them if they aren't there.
   */
  public void checkAndBuildSpecialGuids() throws NullCursorException {
    final String[] specialGUIDs = AndroidBrowserBookmarksRepositorySession.SPECIAL_GUIDS;
    Cursor cur = fetch(specialGUIDs);
    long mobileRoot  = 0;
    long desktopRoot = 0;

    // Map from GUID to whether deleted. Non-presence implies just that.
    HashMap<String, Boolean> statuses = new HashMap<String, Boolean>(specialGUIDs.length);
    try {
      if (cur.moveToFirst()) {
        while (!cur.isAfterLast()) {
          String guid = RepoUtils.getStringFromCursor(cur, BrowserContract.SyncColumns.GUID);
          if (guid.equals("mobile")) {
            mobileRoot = RepoUtils.getLongFromCursor(cur, BrowserContract.CommonColumns._ID);
          }
          if (guid.equals("desktop")) {
            desktopRoot = RepoUtils.getLongFromCursor(cur, BrowserContract.CommonColumns._ID);
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
        if (guid.equals("mobile")) {
          Logger.info(LOG_TAG, "No mobile folder. Inserting one.");
          mobileRoot = insertSpecialFolder("mobile", 0);
        } else if (guid.equals("places")) {
          // This is awkward.
          Logger.info(LOG_TAG, "No places root. Inserting one under mobile (" + mobileRoot + ").");
          desktopRoot = insertSpecialFolder("places", mobileRoot);
        } else {
          // unfiled, menu, toolbar.
          Logger.info(LOG_TAG, "No " + guid + " root. Inserting one under places (" + desktopRoot + ").");
          insertSpecialFolder(guid, desktopRoot);
        }
      }
    }
  }

  private long insertSpecialFolder(String guid, long parentId) {
    BookmarkRecord record = new BookmarkRecord(guid);
    record.title = AndroidBrowserBookmarksRepositorySession.SPECIAL_GUIDS_MAP.get(guid);
    record.type = "folder";
    record.androidParentID = parentId;
    return(RepoUtils.getAndroidIdFromUri(insert(record)));
  }

  @Override
  protected ContentValues getContentValues(Record record) {
    ContentValues cv = new ContentValues();
    BookmarkRecord rec = (BookmarkRecord) record;
    cv.put(BrowserContract.SyncColumns.GUID,      rec.guid);
    cv.put(BrowserContract.Bookmarks.TITLE,       rec.title);
    cv.put(BrowserContract.Bookmarks.URL,         rec.bookmarkURI);
    cv.put(BrowserContract.Bookmarks.DESCRIPTION, rec.description);
    if (rec.tags == null) {
      rec.tags = new JSONArray();
    }
    cv.put(BrowserContract.Bookmarks.TAGS,        rec.tags.toJSONString());
    cv.put(BrowserContract.Bookmarks.KEYWORD,     rec.keyword);
    cv.put(BrowserContract.Bookmarks.PARENT,      rec.androidParentID);
    cv.put(BrowserContract.Bookmarks.POSITION,    rec.androidPosition);

    // Only bookmark and folder types should make it this far.
    // Other types should be filtered out and dropped.
    cv.put(BrowserContract.Bookmarks.IS_FOLDER,   rec.type.equalsIgnoreCase(TYPE_FOLDER) ? 1 : 0);

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
