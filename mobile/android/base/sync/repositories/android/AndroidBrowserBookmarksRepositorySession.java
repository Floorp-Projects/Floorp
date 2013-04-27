/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;
import java.util.TreeMap;

import org.json.simple.JSONArray;
import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;
import org.mozilla.gecko.sync.repositories.NoGuidForIdException;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.ParentNotFoundException;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionBeginDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFinishDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;
import org.mozilla.gecko.sync.repositories.domain.BookmarkRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.content.ContentUris;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;

public class AndroidBrowserBookmarksRepositorySession extends AndroidBrowserRepositorySession
  implements BookmarksInsertionManager.BookmarkInserter {

  public static final int DEFAULT_DELETION_FLUSH_THRESHOLD = 50;
  public static final int DEFAULT_INSERTION_FLUSH_THRESHOLD = 50;

  // TODO: synchronization for these.
  private HashMap<String, Long> parentGuidToIDMap = new HashMap<String, Long>();
  private HashMap<Long, String> parentIDToGuidMap = new HashMap<Long, String>();

  /**
   * Some notes on reparenting/reordering.
   *
   * Fennec stores new items with a high-negative position, because it doesn't care.
   * On the other hand, it also doesn't give us any help managing positions.
   *
   * We can process records and folders in any order, though we'll usually see folders
   * first because their sortindex is larger.
   *
   * We can also see folders that refer to children we haven't seen, and children we
   * won't see (perhaps due to a TTL, perhaps due to a limit on our fetch).
   *
   * And of course folders can refer to local children (including ones that might
   * be reconciled into oblivion!), or local children in other folders. And the local
   * version of a folder -- which might be a reconciling target, or might not -- can
   * have local additions or removals. (That causes complications with on-the-fly
   * reordering: we don't know in advance which records will even exist by the end
   * of the sync.)
   *
   * We opt to leave records in a reasonable state as we go, applying reordering/
   * reparenting operations whenever possible. A final sequence is applied after all
   * incoming records have been handled.
   *
   * As such, we need to track a bunch of stuff as we go:
   *
   * • For each downloaded folder, the array of children. These will be server GUIDs,
   *   but not necessarily identical to the remote list: if we download a record and
   *   it's been locally moved, it must be removed from this child array.
   *
   *   This mapping can be discarded when final reordering has occurred, either on
   *   store completion or when every child has been seen within this session.
   *
   * • A list of orphans: records whose parent folder does not yet exist. This can be
   *   trimmed as orphans are reparented.
   *
   * • Mappings from folder GUIDs to folder IDs, so that we can parent items without
   *   having to look in the DB. Of course, this must be kept up-to-date as we
   *   reconcile.
   *
   * Reordering also needs to occur during fetch. That is, a folder might have been
   * created locally, or modified locally without any remote changes. An order must
   * be generated for the folder's children array, and it must be persisted into the
   * database to act as a starting point for future changes. But of course we don't
   * want to incur a database write if the children already have a satisfactory order.
   *
   * Do we also need a list of "adopters", parents that are still waiting for children?
   * As items get picked out of the orphans list, we can do on-the-fly ordering, until
   * we're left with lonely records at the end.
   *
   * As we modify local folders, perhaps by moving children out of their purview, we
   * must bump their modification time so as to cause them to be uploaded on the next
   * stage of syncing. The same applies to simple reordering.
   */

  // TODO: can we guarantee serial access to these?
  private HashMap<String, ArrayList<String>> missingParentToChildren = new HashMap<String, ArrayList<String>>();
  private HashMap<String, JSONArray>         parentToChildArray      = new HashMap<String, JSONArray>();
  private int needsReparenting = 0;

  private AndroidBrowserBookmarksDataAccessor dataAccessor;

  protected BookmarksDeletionManager deletionManager;
  protected BookmarksInsertionManager insertionManager;

  /**
   * An array of known-special GUIDs.
   */
  public static String[] SPECIAL_GUIDS = new String[] {
    // Mobile and desktop places roots have to come first.
    "places",
    "mobile",
    "toolbar",
    "menu",
    "unfiled"
  };

  /**
   * = A note about folder mapping =
   *
   * Note that _none_ of Places's folders actually have a special GUID. They're all
   * randomly generated. Special folders are indicated by membership in the
   * moz_bookmarks_roots table, and by having the parent `1`.
   *
   * Additionally, the mobile root is annotated. In Firefox Sync, PlacesUtils is
   * used to find the IDs of these special folders.
   *
   * We need to consume records with these various GUIDs, producing a local
   * representation which we are able to stably map upstream.
   *
   * Android Sync skips over the contents of some special GUIDs -- `places`, `tags`,
   * etc. -- when finding IDs.
   * Some of these special GUIDs are part of desktop structure (places, tags). Some
   * are part of Fennec's custom data (readinglist, pinned).
   *
   * We don't want to upload or apply these records.
   *
   * That is:
   *
   * * We should not upload a `places`,`tags`, `readinglist`, or `pinned` record.
   * * We can stably _store_ menu/toolbar/unfiled/mobile as special GUIDs, and set
     * their parent ID as appropriate on upload.
   *
   * Fortunately, Fennec stores our representation of the data, not Places: that is,
   * there's a "places" root, containing "mobile", "menu", "toolbar", etc.
   *
   * These are guaranteed to exist when the database is created.
   *
   * = Places folders =
   *
   * guid        root_name   folder_id   parent
   * ----------  ----------  ----------  ----------
   * ?           places      1           0
   * ?           menu        2           1
   * ?           toolbar     3           1
   * ?           tags        4           1
   * ?           unfiled     5           1
   *
   * ?           mobile*     474         1
   *
   *
   * = Fennec folders =
   *
   * guid        folder_id   parent
   * ----------  ----------  ----------
   * places      0           0
   * mobile      1           0
   * menu        2           0
   * etc.
   *
  */
  public static final Map<String, String> SPECIAL_GUID_PARENTS;
  static {
    HashMap<String, String> m = new HashMap<String, String>();
    m.put("places",  null);
    m.put("menu",    "places");
    m.put("toolbar", "places");
    m.put("tags",    "places");
    m.put("unfiled", "places");
    m.put("mobile",  "places");
    SPECIAL_GUID_PARENTS = Collections.unmodifiableMap(m);
  }


  /**
   * A map of guids to their localized name strings.
   */
  // Oh, if only we could make this final and initialize it in the static initializer.
  public static Map<String, String> SPECIAL_GUIDS_MAP;

  /**
   * Return true if the provided record GUID should be skipped
   * in child lists or fetch results.
   *
   * @param recordGUID the GUID of the record to check.
   * @return true if the record should be skipped.
   */
  public static boolean forbiddenGUID(final String recordGUID) {
    return recordGUID == null ||
           // Temporarily exclude reading list items (Bug 762118; re-enable in Bug 762109.)
           BrowserContract.Bookmarks.READING_LIST_FOLDER_GUID.equals(recordGUID) ||
           BrowserContract.Bookmarks.PINNED_FOLDER_GUID.equals(recordGUID) ||
           BrowserContract.Bookmarks.PLACES_FOLDER_GUID.equals(recordGUID) ||
           BrowserContract.Bookmarks.TAGS_FOLDER_GUID.equals(recordGUID);
  }

  /**
   * Return true if the provided parent GUID's children should
   * be skipped in child lists or fetch results.
   * This differs from {@link #forbiddenGUID(String)} in that we're skipping
   * part of the hierarchy.
   *
   * @param parentGUID the GUID of parent of the record to check.
   * @return true if the record should be skipped.
   */
  public static boolean forbiddenParent(final String parentGUID) {
    return parentGUID == null ||
           // Temporarily exclude reading list items (Bug 762118; re-enable in Bug 762109.)
           BrowserContract.Bookmarks.READING_LIST_FOLDER_GUID.equals(parentGUID) ||
           BrowserContract.Bookmarks.PINNED_FOLDER_GUID.equals(parentGUID);
  }

  public AndroidBrowserBookmarksRepositorySession(Repository repository, Context context) {
    super(repository);

    if (SPECIAL_GUIDS_MAP == null) {
      HashMap<String, String> m = new HashMap<String, String>();

      // Note that we always use the literal name "mobile" for the Mobile Bookmarks
      // folder, regardless of its actual name in the database or the Fennec UI.
      // This is to match desktop (working around Bug 747699) and to avoid a similar
      // issue locally. See Bug 748898.
      m.put("mobile",  "mobile");

      // Other folders use their contextualized names, and we simply rely on
      // these not changing, matching desktop, and such to avoid issues.
      m.put("menu",    context.getString(R.string.bookmarks_folder_menu));
      m.put("places",  context.getString(R.string.bookmarks_folder_places));
      m.put("toolbar", context.getString(R.string.bookmarks_folder_toolbar));
      m.put("unfiled", context.getString(R.string.bookmarks_folder_unfiled));

      SPECIAL_GUIDS_MAP = Collections.unmodifiableMap(m);
    }

    dbHelper = new AndroidBrowserBookmarksDataAccessor(context);
    dataAccessor = (AndroidBrowserBookmarksDataAccessor) dbHelper;
  }

  private static int getTypeFromCursor(Cursor cur) {
    return RepoUtils.getIntFromCursor(cur, BrowserContract.Bookmarks.TYPE);
  }

  private static boolean rowIsFolder(Cursor cur) {
    return getTypeFromCursor(cur) == BrowserContract.Bookmarks.TYPE_FOLDER;
  }

  private String getGUIDForID(long androidID) {
    String guid = parentIDToGuidMap.get(androidID);
    trace("  " + androidID + " => " + guid);
    return guid;
  }

  private long getIDForGUID(String guid) {
    Long id = parentGuidToIDMap.get(guid);
    if (id == null) {
      Logger.warn(LOG_TAG, "Couldn't find local ID for GUID " + guid);
      return -1;
    }
    return id.longValue();
  }

  private String getGUID(Cursor cur) {
    return RepoUtils.getStringFromCursor(cur, "guid");
  }

  private long getParentID(Cursor cur) {
    return RepoUtils.getLongFromCursor(cur, BrowserContract.Bookmarks.PARENT);
  }

  // More efficient for bulk operations.
  private long getPosition(Cursor cur, int positionIndex) {
    return cur.getLong(positionIndex);
  }
  private long getPosition(Cursor cur) {
    return RepoUtils.getLongFromCursor(cur, BrowserContract.Bookmarks.POSITION);
  }

  private String getParentName(String parentGUID) throws ParentNotFoundException, NullCursorException {
    if (parentGUID == null) {
      return "";
    }
    if (SPECIAL_GUIDS_MAP.containsKey(parentGUID)) {
      return SPECIAL_GUIDS_MAP.get(parentGUID);
    }

    // Get parent name from database.
    String parentName = "";
    Cursor name = dataAccessor.fetch(new String[] { parentGUID });
    try {
      name.moveToFirst();
      if (!name.isAfterLast()) {
        parentName = RepoUtils.getStringFromCursor(name, BrowserContract.Bookmarks.TITLE);
      }
      else {
        Logger.error(LOG_TAG, "Couldn't find record with guid '" + parentGUID + "' when looking for parent name.");
        throw new ParentNotFoundException(null);
      }
    } finally {
      name.close();
    }
    return parentName;
  }

  /**
   * Retrieve the child array for a record, repositioning and updating the database as necessary.
   *
   * @param folderID
   *        The database ID of the folder.
   * @param persist
   *        True if generated positions should be written to the database. The modified
   *        time of the parent folder is only bumped if this is true.
   * @param childArray
   *        A new, empty JSONArray which will be populated with an array of GUIDs.
   * @return
   *        True if the resulting array is "clean" (i.e., reflects the content of the database).
   * @throws NullCursorException
   */
  @SuppressWarnings("unchecked")
  private boolean getChildrenArray(long folderID, boolean persist, JSONArray childArray) throws NullCursorException {
    trace("Calling getChildren for androidID " + folderID);
    Cursor children = dataAccessor.getChildren(folderID);
    try {
      if (!children.moveToFirst()) {
        trace("No children: empty cursor.");
        return true;
      }
      final int positionIndex = children.getColumnIndex(BrowserContract.Bookmarks.POSITION);
      final int count = children.getCount();
      Logger.debug(LOG_TAG, "Expecting " + count + " children.");

      // Sorted by requested position.
      TreeMap<Long, ArrayList<String>> guids = new TreeMap<Long, ArrayList<String>>();

      while (!children.isAfterLast()) {
        final String childGuid   = getGUID(children);
        final long childPosition = getPosition(children, positionIndex);
        trace("  Child GUID: " + childGuid);
        trace("  Child position: " + childPosition);
        Utils.addToIndexBucketMap(guids, Math.abs(childPosition), childGuid);
        children.moveToNext();
      }

      // This will suffice for taking a jumble of records and indices and
      // producing a sorted sequence that preserves some kind of order --
      // from the abs of the position, falling back on cursor order (that
      // is, creation time and ID).
      // Note that this code is not intended to merge values from two sources!
      boolean changed = false;
      int i = 0;
      for (Entry<Long, ArrayList<String>> entry : guids.entrySet()) {
        long pos = entry.getKey().longValue();
        int atPos = entry.getValue().size();

        // If every element has a different index, and the indices are
        // in strict natural order, then changed will be false.
        if (atPos > 1 || pos != i) {
          changed = true;
        }

        ++i;

        for (String guid : entry.getValue()) {
          if (!forbiddenGUID(guid)) {
            childArray.add(guid);
          }
        }
      }

      if (Logger.shouldLogVerbose(LOG_TAG)) {
        // Don't JSON-encode unless we're logging.
        Logger.trace(LOG_TAG, "Output child array: " + childArray.toJSONString());
      }

      if (!changed) {
        Logger.debug(LOG_TAG, "Nothing moved! Database reflects child array.");
        return true;
      }

      if (!persist) {
        Logger.debug(LOG_TAG, "Returned array does not match database, and not persisting.");
        return false;
      }

      Logger.debug(LOG_TAG, "Generating child array required moving records. Updating DB.");
      final long time = now();
      if (0 < dataAccessor.updatePositions(childArray)) {
        Logger.debug(LOG_TAG, "Bumping parent time to " + time + ".");
        dataAccessor.bumpModified(folderID, time);
      }
      return true;
    } finally {
      children.close();
    }
  }

  protected static boolean isDeleted(Cursor cur) {
    return RepoUtils.getLongFromCursor(cur, BrowserContract.SyncColumns.IS_DELETED) != 0;
  }

  @Override
  protected Record retrieveDuringStore(Cursor cur) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
    // During storing of a retrieved record, we never care about the children
    // array that's already present in the database -- we don't use it for
    // reconciling. Skip all that effort for now.
    return retrieveRecord(cur, false);
  }

  @Override
  protected Record retrieveDuringFetch(Cursor cur) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
    return retrieveRecord(cur, true);
  }

  /**
   * Build a record from a cursor, with a flag to dictate whether the
   * children array should be computed and written back into the database.
   */
  protected BookmarkRecord retrieveRecord(Cursor cur, boolean computeAndPersistChildren) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
    String recordGUID = getGUID(cur);
    Logger.trace(LOG_TAG, "Record from mirror cursor: " + recordGUID);

    if (forbiddenGUID(recordGUID)) {
      Logger.debug(LOG_TAG, "Ignoring " + recordGUID + " record in recordFromMirrorCursor.");
      return null;
    }

    // Short-cut for deleted items.
    if (isDeleted(cur)) {
      return AndroidBrowserBookmarksRepositorySession.bookmarkFromMirrorCursor(cur, null, null, null);
    }

    long androidParentID = getParentID(cur);

    // Ensure special folders stay in the right place.
    String androidParentGUID = SPECIAL_GUID_PARENTS.get(recordGUID);
    if (androidParentGUID == null) {
      androidParentGUID = getGUIDForID(androidParentID);
    }

    boolean needsReparenting = false;

    if (androidParentGUID == null) {
      Logger.debug(LOG_TAG, "No parent GUID for record " + recordGUID + " with parent " + androidParentID);
      // If the parent has been stored and somehow has a null GUID, throw an error.
      if (parentIDToGuidMap.containsKey(androidParentID)) {
        Logger.error(LOG_TAG, "Have the parent android ID for the record but the parent's GUID wasn't found.");
        throw new NoGuidForIdException(null);
      }

      // We have a parent ID but it's wrong. If the record is deleted,
      // we'll just say that it was in the Unsorted Bookmarks folder.
      // If not, we'll move it into Mobile Bookmarks.
      needsReparenting = true;
    }

    // If record is a folder, and we want to see children at this time, then build out the children array.
    final JSONArray childArray;
    if (computeAndPersistChildren) {
      childArray = getChildrenArrayForRecordCursor(cur, recordGUID, true);
    } else {
      childArray = null;
    }
    String parentName = getParentName(androidParentGUID);
    BookmarkRecord bookmark = AndroidBrowserBookmarksRepositorySession.bookmarkFromMirrorCursor(cur, androidParentGUID, parentName, childArray);

    if (bookmark == null) {
      Logger.warn(LOG_TAG, "Unable to extract bookmark from cursor. Record GUID " + recordGUID +
                           ", parent " + androidParentGUID + "/" + androidParentID);
      return null;
    }

    if (needsReparenting) {
      Logger.warn(LOG_TAG, "Bookmark record " + recordGUID + " has a bad parent pointer. Reparenting now.");

      String destination = bookmark.deleted ? "unfiled" : "mobile";
      bookmark.androidParentID = getIDForGUID(destination);
      bookmark.androidPosition = getPosition(cur);
      bookmark.parentID        = destination;
      bookmark.parentName      = getParentName(destination);
      if (!bookmark.deleted) {
        // Actually move it.
        // TODO: compute position. Persist.
        relocateBookmark(bookmark);
      }
    }

    return bookmark;
  }

  /**
   * Ensure that the local database row for the provided bookmark
   * reflects this record's parent information.
   *
   * @param bookmark
   */
  private void relocateBookmark(BookmarkRecord bookmark) {
    dataAccessor.updateParentAndPosition(bookmark.guid, bookmark.androidParentID, bookmark.androidPosition);
  }

  protected JSONArray getChildrenArrayForRecordCursor(Cursor cur, String recordGUID, boolean persist) throws NullCursorException {
    boolean isFolder = rowIsFolder(cur);
    if (!isFolder) {
      return null;
    }

    long androidID = parentGuidToIDMap.get(recordGUID);
    JSONArray childArray = new JSONArray();
    getChildrenArray(androidID, persist, childArray);

    Logger.debug(LOG_TAG, "Fetched " + childArray.size() + " children for " + recordGUID);
    return childArray;
  }

  @Override
  public boolean shouldIgnore(Record record) {
    if (!(record instanceof BookmarkRecord)) {
      return true;
    }
    if (record.deleted) {
      return false;
    }

    BookmarkRecord bmk = (BookmarkRecord) record;

    if (forbiddenGUID(bmk.guid)) {
      Logger.debug(LOG_TAG, "Ignoring forbidden record with guid: " + bmk.guid);
      return true;
    }

    if (forbiddenParent(bmk.parentID)) {
      Logger.debug(LOG_TAG,  "Ignoring child " + bmk.guid + " of forbidden parent folder " + bmk.parentID);
      return true;
    }

    if (BrowserContractHelpers.isSupportedType(bmk.type)) {
      return false;
    }

    Logger.debug(LOG_TAG, "Ignoring record with guid: " + bmk.guid + " and type: " + bmk.type);
    return true;
  }

  @Override
  public void begin(RepositorySessionBeginDelegate delegate) throws InvalidSessionTransitionException {
    // Check for the existence of special folders
    // and insert them if they don't exist.
    Cursor cur;
    try {
      Logger.debug(LOG_TAG, "Check and build special GUIDs.");
      dataAccessor.checkAndBuildSpecialGuids();
      cur = dataAccessor.getGuidsIDsForFolders();
      Logger.debug(LOG_TAG, "Got GUIDs for folders.");
    } catch (android.database.sqlite.SQLiteConstraintException e) {
      Logger.error(LOG_TAG, "Got sqlite constraint exception working with Fennec bookmark DB.", e);
      delegate.onBeginFailed(e);
      return;
    } catch (NullCursorException e) {
      delegate.onBeginFailed(e);
      return;
    } catch (Exception e) {
      delegate.onBeginFailed(e);
      return;
    }

    // To deal with parent mapping of bookmarks we have to do some
    // hairy stuff. Here's the setup for it.

    Logger.debug(LOG_TAG, "Preparing folder ID mappings.");

    // Fake our root.
    Logger.debug(LOG_TAG, "Tracking places root as ID 0.");
    parentIDToGuidMap.put(0L, "places");
    parentGuidToIDMap.put("places", 0L);
    try {
      cur.moveToFirst();
      while (!cur.isAfterLast()) {
        String guid = getGUID(cur);
        long id = RepoUtils.getLongFromCursor(cur, BrowserContract.Bookmarks._ID);
        parentGuidToIDMap.put(guid, id);
        parentIDToGuidMap.put(id, guid);
        Logger.debug(LOG_TAG, "GUID " + guid + " maps to " + id);
        cur.moveToNext();
      }
    } finally {
      cur.close();
    }
    deletionManager = new BookmarksDeletionManager(dataAccessor, DEFAULT_DELETION_FLUSH_THRESHOLD);

    // We just crawled the database enumerating all folders; we'll start the
    // insertion manager with exactly these folders as the known parents (the
    // collection is copied) in the manager constructor.
    insertionManager = new BookmarksInsertionManager(DEFAULT_INSERTION_FLUSH_THRESHOLD, parentGuidToIDMap.keySet(), this);

    Logger.debug(LOG_TAG, "Done with initial setup of bookmarks session.");
    super.begin(delegate);
  }

  /**
   * Implement method of BookmarksInsertionManager.BookmarkInserter.
   */
  @Override
  public boolean insertFolder(BookmarkRecord record) {
    // A folder that is *not* deleted needs its androidID updated, so that
    // updateBookkeeping can re-parent, etc.
    Record toStore = prepareRecord(record);
    try {
      Uri recordURI = dbHelper.insert(toStore);
      if (recordURI == null) {
        delegate.onRecordStoreFailed(new RuntimeException("Got null URI inserting folder with guid " + toStore.guid + "."), record.guid);
        return false;
      }
      toStore.androidID = ContentUris.parseId(recordURI);
      Logger.debug(LOG_TAG, "Inserted folder with guid " + toStore.guid + " as androidID " + toStore.androidID);

      updateBookkeeping(toStore);
    } catch (Exception e) {
      delegate.onRecordStoreFailed(e, record.guid);
      return false;
    }
    trackRecord(toStore);
    delegate.onRecordStoreSucceeded(record.guid);
    return true;
  }

  /**
   * Implement method of BookmarksInsertionManager.BookmarkInserter.
   */
  @Override
  public void bulkInsertNonFolders(Collection<BookmarkRecord> records) {
    // All of these records are *not* deleted and *not* folders, so we don't
    // need to update androidID at all!
    // TODO: persist records that fail to insert for later retry.
    ArrayList<Record> toStores = new ArrayList<Record>(records.size());
    for (Record record : records) {
      toStores.add(prepareRecord(record));
    }

    try {
      int stored = dataAccessor.bulkInsert(toStores);
      if (stored != toStores.size()) {
        // Something failed; most pessimistic action is to declare that all insertions failed.
        // TODO: perform the bulkInsert in a transaction and rollback unless all insertions succeed?
        for (Record failed : toStores) {
          delegate.onRecordStoreFailed(new RuntimeException("Possibly failed to bulkInsert non-folder with guid " + failed.guid + "."), failed.guid);
        }
        return;
      }
    } catch (NullCursorException e) {
      for (Record failed : toStores) {
        delegate.onRecordStoreFailed(e, failed.guid);
      }
      return;
    }

    // Success For All!
    for (Record succeeded : toStores) {
      try {
        updateBookkeeping(succeeded);
      } catch (Exception e) {
        Logger.warn(LOG_TAG, "Got exception updating bookkeeping of non-folder with guid " + succeeded.guid + ".", e);
      }
      trackRecord(succeeded);
      delegate.onRecordStoreSucceeded(succeeded.guid);
    }
  }

  @Override
  public void finish(RepositorySessionFinishDelegate delegate) throws InactiveSessionException {
    // Allow these to be GCed.
    deletionManager = null;
    insertionManager = null;

    // Override finish to do this check; make sure all records
    // needing re-parenting have been re-parented.
    if (needsReparenting != 0) {
      Logger.error(LOG_TAG, "Finish called but " + needsReparenting +
                            " bookmark(s) have been placed in unsorted bookmarks and not been reparented.");

      // TODO: handling of failed reparenting.
      // E.g., delegate.onFinishFailed(new BookmarkNeedsReparentingException(null));
    }
    super.finish(delegate);
  };

  @Override
  public void setStoreDelegate(RepositorySessionStoreDelegate delegate) {
    super.setStoreDelegate(delegate);

    if (deletionManager != null) {
      deletionManager.setDelegate(delegate);
    }
  }

  @Override
  protected Record reconcileRecords(Record remoteRecord, Record localRecord,
                                    long lastRemoteRetrieval,
                                    long lastLocalRetrieval) {

    BookmarkRecord reconciled = (BookmarkRecord) super.reconcileRecords(remoteRecord, localRecord,
                                                                        lastRemoteRetrieval,
                                                                        lastLocalRetrieval);

    // For now we *always* use the remote record's children array as a starting point.
    // We won't write it into the database yet; we'll record it and process as we go.
    reconciled.children = ((BookmarkRecord) remoteRecord).children;

    // *Always* track folders, though: if we decide we need to reposition items, we'll
    // untrack later.
    if (reconciled.isFolder()) {
      trackRecord(reconciled);
    }
    return reconciled;
  }

  /**
   * Rename mobile folders to "mobile", both in and out. The other half of
   * this logic lives in {@link #computeParentFields(BookmarkRecord, String, String)}, where
   * the parent name of a record is set from {@link #SPECIAL_GUIDS_MAP} rather than
   * from source data.
   *
   * Apply this approach generally for symmetry.
   */
  @Override
  protected void fixupRecord(Record record) {
    final BookmarkRecord r = (BookmarkRecord) record;
    final String parentName = SPECIAL_GUIDS_MAP.get(r.parentID);
    if (parentName == null) {
      return;
    }
    if (Logger.shouldLogVerbose(LOG_TAG)) {
      Logger.trace(LOG_TAG, "Replacing parent name \"" + r.parentName + "\" with \"" + parentName + "\".");
    }
    r.parentName = parentName;
  }

  @Override
  protected Record prepareRecord(Record record) {
    if (record.deleted) {
      Logger.debug(LOG_TAG, "No need to prepare deleted record " + record.guid);
      return record;
    }

    BookmarkRecord bmk = (BookmarkRecord) record;

    if (!isSpecialRecord(record)) {
      // We never want to reparent special records.
      handleParenting(bmk);
    }

    if (Logger.LOG_PERSONAL_INFORMATION) {
      if (bmk.isFolder()) {
        Logger.pii(LOG_TAG, "Inserting folder " + bmk.guid + ", " + bmk.title +
                            " with parent " + bmk.androidParentID +
                            " (" + bmk.parentID + ", " + bmk.parentName +
                            ", " + bmk.androidPosition + ")");
      } else {
        Logger.pii(LOG_TAG, "Inserting bookmark " + bmk.guid + ", " + bmk.title + ", " +
                            bmk.bookmarkURI + " with parent " + bmk.androidParentID +
                            " (" + bmk.parentID + ", " + bmk.parentName +
                            ", " + bmk.androidPosition + ")");
      }
    } else {
      if (bmk.isFolder()) {
        Logger.debug(LOG_TAG, "Inserting folder " + bmk.guid +  ", parent " +
                              bmk.androidParentID +
                              " (" + bmk.parentID + ", " + bmk.androidPosition + ")");
      } else {
        Logger.debug(LOG_TAG, "Inserting bookmark " + bmk.guid + " with parent " +
                              bmk.androidParentID +
                              " (" + bmk.parentID + ", " + ", " + bmk.androidPosition + ")");
      }
    }
    return bmk;
  }

  /**
   * If the provided record doesn't have correct parent information,
   * update appropriate bookkeeping to improve the situation.
   *
   * @param bmk
   */
  private void handleParenting(BookmarkRecord bmk) {
    if (parentGuidToIDMap.containsKey(bmk.parentID)) {
      bmk.androidParentID = parentGuidToIDMap.get(bmk.parentID);

      // Might as well set a basic position from the downloaded children array.
      JSONArray children = parentToChildArray.get(bmk.parentID);
      if (children != null) {
        int index = children.indexOf(bmk.guid);
        if (index >= 0) {
          bmk.androidPosition = index;
        }
      }
    }
    else {
      bmk.androidParentID = parentGuidToIDMap.get("unfiled");
      ArrayList<String> children;
      if (missingParentToChildren.containsKey(bmk.parentID)) {
        children = missingParentToChildren.get(bmk.parentID);
      } else {
        children = new ArrayList<String>();
      }
      children.add(bmk.guid);
      needsReparenting++;
      missingParentToChildren.put(bmk.parentID, children);
    }
  }

  private boolean isSpecialRecord(Record record) {
    return SPECIAL_GUID_PARENTS.containsKey(record.guid);
  }

  @Override
  protected void updateBookkeeping(Record record) throws NoGuidForIdException,
                                                         NullCursorException,
                                                         ParentNotFoundException {
    super.updateBookkeeping(record);
    BookmarkRecord bmk = (BookmarkRecord) record;

    // If record is folder, update maps and re-parent children if necessary.
    if (!bmk.isFolder()) {
      Logger.debug(LOG_TAG, "Not a folder. No bookkeeping.");
      return;
    }

    Logger.debug(LOG_TAG, "Updating bookkeeping for folder " + record.guid);

    // Mappings between ID and GUID.
    // TODO: update our persisted children arrays!
    // TODO: if our Android ID just changed, replace parents for all of our children.
    parentGuidToIDMap.put(bmk.guid,      bmk.androidID);
    parentIDToGuidMap.put(bmk.androidID, bmk.guid);

    JSONArray childArray = bmk.children;

    if (Logger.shouldLogVerbose(LOG_TAG)) {
      Logger.trace(LOG_TAG, bmk.guid + " has children " + childArray.toJSONString());
    }
    parentToChildArray.put(bmk.guid, childArray);

    // Re-parent.
    if (missingParentToChildren.containsKey(bmk.guid)) {
      for (String child : missingParentToChildren.get(bmk.guid)) {
        // This might return -1; that's OK, the bookmark will
        // be properly repositioned later.
        long position = childArray.indexOf(child);
        dataAccessor.updateParentAndPosition(child, bmk.androidID, position);
        needsReparenting--;
      }
      missingParentToChildren.remove(bmk.guid);
    }
  }

  @Override
  protected void insert(Record record) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
    try {
      insertionManager.enqueueRecord((BookmarkRecord) record);
    } catch (Exception e) {
      throw new NullCursorException(e);
    }
  }

  @Override
  protected void storeRecordDeletion(final Record record, final Record existingRecord) {
    if (SPECIAL_GUIDS_MAP.containsKey(record.guid)) {
      Logger.debug(LOG_TAG, "Told to delete record " + record.guid + ". Ignoring.");
      return;
    }
    final BookmarkRecord bookmarkRecord = (BookmarkRecord) record;
    final BookmarkRecord existingBookmark = (BookmarkRecord) existingRecord;
    final boolean isFolder = existingBookmark.isFolder();
    final String parentGUID = existingBookmark.parentID;
    deletionManager.deleteRecord(bookmarkRecord.guid, isFolder, parentGUID);
  }

  protected void flushQueues() {
    long now = now();
    Logger.debug(LOG_TAG, "Applying remaining insertions.");
    try {
      insertionManager.finishUp();
      Logger.debug(LOG_TAG, "Done applying remaining insertions.");
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Unable to apply remaining insertions.", e);
    }

    Logger.debug(LOG_TAG, "Applying deletions.");
    try {
      untrackGUIDs(deletionManager.flushAll(getIDForGUID("unfiled"), now));
      Logger.debug(LOG_TAG, "Done applying deletions.");
    } catch (Exception e) {
      Logger.error(LOG_TAG, "Unable to apply deletions.", e);
    }
  }

  @SuppressWarnings("unchecked")
  private void finishUp() {
    try {
      flushQueues();
      Logger.debug(LOG_TAG, "Have " + parentToChildArray.size() + " folders whose children might need repositioning.");
      for (Entry<String, JSONArray> entry : parentToChildArray.entrySet()) {
        String guid = entry.getKey();
        JSONArray onServer = entry.getValue();
        try {
          final long folderID = getIDForGUID(guid);
          final JSONArray inDB = new JSONArray();
          final boolean clean = getChildrenArray(folderID, false, inDB);
          final boolean sameArrays = Utils.sameArrays(onServer, inDB);

          // If the local children and the remote children are already
          // the same, then we don't need to bump the modified time of the
          // parent: we wouldn't upload a different record, so avoid the cycle.
          if (!sameArrays) {
            int added = 0;
            for (Object o : inDB) {
              if (!onServer.contains(o)) {
                onServer.add(o);
                added++;
              }
            }
            Logger.debug(LOG_TAG, "Added " + added + " items locally.");
            Logger.debug(LOG_TAG, "Untracking and bumping " + guid + "(" + folderID + ")");
            dataAccessor.bumpModified(folderID, now());
            untrackGUID(guid);
          }

          // If the arrays are different, or they're the same but not flushed to disk,
          // write them out now.
          if (!sameArrays || !clean) {
            dataAccessor.updatePositions(new ArrayList<String>(onServer));
          }
        } catch (Exception e) {
          Logger.warn(LOG_TAG, "Error repositioning children for " + guid, e);
        }
      }
    } finally {
      super.storeDone();
    }
  }

  /**
   * Hook into the deletion manager on wipe.
   */
  class BookmarkWipeRunnable extends WipeRunnable {
    public BookmarkWipeRunnable(RepositorySessionWipeDelegate delegate) {
      super(delegate);
    }

    @Override
    public void run() {
      try {
        // Clear our queued deletions.
        deletionManager.clear();
        insertionManager.clear();
        super.run();
      } catch (Exception ex) {
        delegate.onWipeFailed(ex);
        return;
      }
    }
  }

  @Override
  protected WipeRunnable getWipeRunnable(RepositorySessionWipeDelegate delegate) {
    return new BookmarkWipeRunnable(delegate);
  }

  @Override
  public void storeDone() {
    Runnable command = new Runnable() {
      @Override
      public void run() {
        finishUp();
      }
    };
    storeWorkQueue.execute(command);
  }

  @Override
  protected String buildRecordString(Record record) {
    BookmarkRecord bmk = (BookmarkRecord) record;
    String parent = bmk.parentName + "/";
    if (bmk.isBookmark()) {
      return "b" + parent + bmk.bookmarkURI + ":" + bmk.title;
    }
    if (bmk.isFolder()) {
      return "f" + parent + bmk.title;
    }
    if (bmk.isSeparator()) {
      return "s" + parent + bmk.androidPosition;
    }
    if (bmk.isQuery()) {
      return "q" + parent + bmk.bookmarkURI;
    }
    return null;
  }

  public static BookmarkRecord computeParentFields(BookmarkRecord rec, String suggestedParentGUID, String suggestedParentName) {
    final String guid = rec.guid;
    if (guid == null) {
      // Oh dear.
      Logger.error(LOG_TAG, "No guid in computeParentFields!");
      return null;
    }

    String realParent = SPECIAL_GUID_PARENTS.get(guid);
    if (realParent == null) {
      // No magic parent. Use whatever the caller suggests.
      realParent = suggestedParentGUID;
    } else {
      Logger.debug(LOG_TAG, "Ignoring suggested parent ID " + suggestedParentGUID +
                            " for " + guid + "; using " + realParent);
    }

    if (realParent == null) {
      // Oh dear.
      Logger.error(LOG_TAG, "No parent for record " + guid);
      return null;
    }

    // Always set the parent name for special folders back to default.
    String parentName = SPECIAL_GUIDS_MAP.get(realParent);
    if (parentName == null) {
      parentName = suggestedParentName;
    }

    rec.parentID = realParent;
    rec.parentName = parentName;
    return rec;
  }

  private static BookmarkRecord logBookmark(BookmarkRecord rec) {
    try {
      Logger.debug(LOG_TAG, "Returning " + (rec.deleted ? "deleted " : "") +
                            "bookmark record " + rec.guid + " (" + rec.androidID +
                           ", parent " + rec.parentID + ")");
      if (!rec.deleted && Logger.LOG_PERSONAL_INFORMATION) {
        Logger.pii(LOG_TAG, "> Parent name:      " + rec.parentName);
        Logger.pii(LOG_TAG, "> Title:            " + rec.title);
        Logger.pii(LOG_TAG, "> Type:             " + rec.type);
        Logger.pii(LOG_TAG, "> URI:              " + rec.bookmarkURI);
        Logger.pii(LOG_TAG, "> Position:         " + rec.androidPosition);
        if (rec.isFolder()) {
          Logger.pii(LOG_TAG, "FOLDER: Children are " +
                             (rec.children == null ?
                                 "null" :
                                 rec.children.toJSONString()));
        }
      }
    } catch (Exception e) {
      Logger.debug(LOG_TAG, "Exception logging bookmark record " + rec, e);
    }
    return rec;
  }

  // Create a BookmarkRecord object from a cursor on a row containing a Fennec bookmark.
  public static BookmarkRecord bookmarkFromMirrorCursor(Cursor cur, String parentGUID, String parentName, JSONArray children) {
    final String collection = "bookmarks";
    final String guid       = RepoUtils.getStringFromCursor(cur, BrowserContract.SyncColumns.GUID);
    final long lastModified = RepoUtils.getLongFromCursor(cur,   BrowserContract.SyncColumns.DATE_MODIFIED);
    final boolean deleted   = isDeleted(cur);
    BookmarkRecord rec = new BookmarkRecord(guid, collection, lastModified, deleted);

    // No point in populating it.
    if (deleted) {
      return logBookmark(rec);
    }

    int rowType = getTypeFromCursor(cur);
    String typeString = BrowserContractHelpers.typeStringForCode(rowType);

    if (typeString == null) {
      Logger.warn(LOG_TAG, "Unsupported type code " + rowType);
      return null;
    } else {
      Logger.trace(LOG_TAG, "Record " + guid + " has type " + typeString);
    }

    rec.type = typeString;
    rec.title = RepoUtils.getStringFromCursor(cur, BrowserContract.Bookmarks.TITLE);
    rec.bookmarkURI = RepoUtils.getStringFromCursor(cur, BrowserContract.Bookmarks.URL);
    rec.description = RepoUtils.getStringFromCursor(cur, BrowserContract.Bookmarks.DESCRIPTION);
    rec.tags = RepoUtils.getJSONArrayFromCursor(cur, BrowserContract.Bookmarks.TAGS);
    rec.keyword = RepoUtils.getStringFromCursor(cur, BrowserContract.Bookmarks.KEYWORD);

    rec.androidID = RepoUtils.getLongFromCursor(cur, BrowserContract.Bookmarks._ID);
    rec.androidPosition = RepoUtils.getLongFromCursor(cur, BrowserContract.Bookmarks.POSITION);
    rec.children = children;

    // Need to restore the parentId since it isn't stored in content provider.
    // We also take this opportunity to fix up parents for special folders,
    // allowing us to map between the hierarchies used by Fennec and Places.
    BookmarkRecord withParentFields = computeParentFields(rec, parentGUID, parentName);
    if (withParentFields == null) {
      // Oh dear. Something went wrong.
      return null;
    }
    return logBookmark(withParentFields);
  }
}
