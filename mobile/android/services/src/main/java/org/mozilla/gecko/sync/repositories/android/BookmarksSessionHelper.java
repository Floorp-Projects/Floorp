/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import android.content.ContentUris;
import android.database.Cursor;
import android.net.Uri;
import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;

import org.json.simple.JSONArray;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.NoGuidForIdException;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.ParentNotFoundException;
import org.mozilla.gecko.sync.repositories.RecordFilter;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.StoreTrackingRepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;
import org.mozilla.gecko.sync.repositories.domain.BookmarkRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.TreeMap;
import java.util.concurrent.ConcurrentHashMap;

/* package-private */ class BookmarksSessionHelper extends SessionHelper implements BookmarksInsertionManager.BookmarkInserter {
    private static final String LOG_TAG = "BookmarksSessionHelper";

    private final BookmarksDataAccessor dbAccessor;

    // We are primarily concerned with thread safety guarantees of ConcurrentHashMap, and not its
    // synchronization details. Note that this isn't fully thought out, and is a marginal improvement
    // to use of a simple HashMap prior to Bug 1364644.
    private final ConcurrentHashMap<String, Long> parentGuidToIDMap = new ConcurrentHashMap<>();
    private final ConcurrentHashMap<Long, String> parentIDToGuidMap = new ConcurrentHashMap<>();

    // TODO: can we guarantee serial access to these?
    private final HashMap<String, JSONArray> parentToChildArray = new HashMap<>();
    private final HashMap<String, ArrayList<String>> missingParentToChildren = new HashMap<>();

    private int needsReparenting = 0;

    private BookmarksDeletionManager deletionManager;
    private BookmarksInsertionManager insertionManager;

    private static final int DEFAULT_DELETION_FLUSH_THRESHOLD = 50;
    private static final int DEFAULT_INSERTION_FLUSH_THRESHOLD = 50;

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
    private static final Map<String, String> SPECIAL_GUID_PARENTS;
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

    /* package-private */ BookmarksSessionHelper(StoreTrackingRepositorySession session, BookmarksDataAccessor dbAccessor) {
        super(session, dbAccessor);
        this.dbAccessor = dbAccessor;
    }

    @Override
    protected Record retrieveDuringFetch(Cursor cur) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
        final Record retrievedRecord = retrieveRecord(cur, true);

        // In addition to the regular bookmark fields, we will need to know local and sync versions of
        // this record. They're used by VersionedMiddlewareRepository to track sync state of records,
        // and during record merging to detect conflicts and drive record reconciliation.
        return populateVersionFields(retrievedRecord, cur);
    }

    @Override
    /* package-private */ Runnable getWipeRunnable(RepositorySessionWipeDelegate delegate) {
        return new BookmarkWipeRunnable(delegate);
    }

    @Override
    /* package-private */ boolean shouldIgnore(Record record) {
        return shouldIgnoreStatic(record);
    }

    @VisibleForTesting
    /* package-private */ static boolean shouldIgnoreStatic(Record record) {
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
    /* package-private */ Record retrieveDuringStore(Cursor cursor) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
        // During storing of a retrieved record, we never care about the children
        // array that's already present in the database -- we don't use it for
        // reconciling. Skip all that effort for now.
        return populateVersionFields(retrieveRecord(cursor, false), cursor);
    }

    @Override
    protected void storeRecordDeletion(RepositorySessionStoreDelegate storeDelegate, final Record record, final Record existingRecord) {
        if (BookmarksRepositorySession.SPECIAL_GUIDS_MAP.containsKey(record.guid)) {
            Logger.debug(LOG_TAG, "Told to delete record " + record.guid + ". Ignoring.");
            return;
        }
        final BookmarkRecord bookmarkRecord = (BookmarkRecord) record;
        final BookmarkRecord existingBookmark = (BookmarkRecord) existingRecord;
        final boolean isFolder = existingBookmark.isFolder();
        final String parentGUID = existingBookmark.parentID;
        deletionManager.deleteRecord(storeDelegate, bookmarkRecord.guid, isFolder, parentGUID);
    }

    /**
     * Rename mobile folders to "mobile", both in and out. The other half of
     * this logic lives in {@link #computeParentFields(BookmarkRecord, String, String)}, where
     * the parent name of a record is set from {@link BookmarksRepositorySession#SPECIAL_GUIDS_MAP} rather than
     * from source data.
     *
     * Apply this approach generally for symmetry.
     */
    @Override
    protected void fixupRecord(Record record) {
        final BookmarkRecord r = (BookmarkRecord) record;
        final String parentName = BookmarksRepositorySession.SPECIAL_GUIDS_MAP.get(r.parentID);
        if (parentName == null) {
            return;
        }
        if (Logger.shouldLogVerbose(LOG_TAG)) {
            Logger.trace(LOG_TAG, "Replacing parent name \"" + r.parentName + "\" with \"" + parentName + "\".");
        }
        r.parentName = parentName;
    }

    @Override
    /* package-private */ String buildRecordString(Record record) {
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

    @Override
    /* package-private */ void updateBookkeeping(Record record) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
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
                dbAccessor.updateParentAndPosition(child, bmk.androidID, position);
                needsReparenting--;
            }
            missingParentToChildren.remove(bmk.guid);
        }
    }

    /**
     * Incoming bookmark records might be missing dateAdded field, because clients started to sync it
     * only in version 55. However, record's lastModified value is a good upper bound. In order to
     * encourage modern clients to re-upload the record with an earlier local dateAdded value,
     * we bump record's lastModified value if we perform any substitutions.
     *
     * This function is only called while inserting a record which doesn't exist locally.
     */
    @Override
    protected Record processBeforeInsertion(Record toProcess) {
        // If incoming record is missing dateAdded, use its lastModified value instead.
        if (((BookmarkRecord) toProcess).dateAdded == null) {
            ((BookmarkRecord) toProcess).dateAdded = toProcess.lastModified;
            toProcess.lastModified = RepositorySession.now();
            toProcess.modifiedBySync = true;
            return toProcess;
        }

        // If both are present, use the lowest value. We trust server's monotonously increasing timestamps
        // more than clients' potentially bogus ones.
        if (toProcess.lastModified < ((BookmarkRecord) toProcess).dateAdded) {
            ((BookmarkRecord) toProcess).dateAdded = toProcess.lastModified;
            toProcess.lastModified = RepositorySession.now();
            toProcess.modifiedBySync = true;
            return toProcess;
        }

        return toProcess;
    }

    /**
     * Implement method of BookmarksInsertionManager.BookmarkInserter.
     */
    @Override
    public boolean insertFolder(RepositorySessionStoreDelegate delegate, BookmarkRecord record) {
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
        session.trackRecord(toStore);
        delegate.onRecordStoreSucceeded(1);
        return true;
    }

    /**
     * Implement method of BookmarksInsertionManager.BookmarkInserter.
     */
    @Override
    public void bulkInsertNonFolders(RepositorySessionStoreDelegate delegate, Collection<BookmarkRecord> records) {
        // All of these records are *not* deleted and *not* folders, so we don't
        // need to update androidID at all!
        // TODO: persist records that fail to insert for later retry.
        ArrayList<Record> toStores = new ArrayList<Record>(records.size());
        for (Record record : records) {
            toStores.add(prepareRecord(record));
        }

        try {
            int stored = dbAccessor.bulkInsert(toStores);
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
            session.trackRecord(succeeded);
        }
        delegate.onRecordStoreSucceeded(toStores.size());
    }

    @Override
    /* package-private */ void doBegin() throws NullCursorException {
        // To deal with parent mapping of bookmarks we have to do some
        // hairy stuff. Here's the setup for it.
        Cursor cur = dbAccessor.getGuidsIDsForFolders();

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
        deletionManager = new BookmarksDeletionManager(dbAccessor, DEFAULT_DELETION_FLUSH_THRESHOLD);

        // We just crawled the database enumerating all folders; we'll start the
        // insertion manager with exactly these folders as the known parents (the
        // collection is copied) in the manager constructor.
        insertionManager = new BookmarksInsertionManager(DEFAULT_INSERTION_FLUSH_THRESHOLD, parentGuidToIDMap.keySet(), this);

        Logger.debug(LOG_TAG, "Done with initial setup of bookmarks session.");
    }

    @Override
    /* package-private */ Record transformRecord(Record record) {
        return record;
    }

    @Override
    /* package-private */ Runnable getStoreDoneRunnable(final RepositorySessionStoreDelegate delegate) {
        return new Runnable() {
            @Override
            public void run() {
                try {
                    finishUp(delegate);
                } catch (Exception e) {
                    Logger.error(LOG_TAG, "Error finishing up store: " + e);
                }
            }
        };
    }

    @Override
    /* package-private */ boolean doReplaceRecord(Record toStore, Record existingRecord, RepositorySessionStoreDelegate delegate) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
        // Request that localVersion is bumped if an incoming record has been modified upstream, or
        // if we're working with a record that also changed locally since we last synced.
        // We do this so that any merged changes are uploaded.
        // We simply replace and don't subsequently upload records which didn't change locally
        // since our last sync.
        boolean shouldIncrementLocalVersion = toStore.modifiedBySync || (existingRecord.localVersion > existingRecord.syncVersion);
        Record replaced = replaceWithAssertion(
                toStore,
                existingRecord,
                shouldIncrementLocalVersion
        );
        if (replaced == null) {
            return false;
        }

        // Note that we don't track records here; deciding that is the job
        // of reconcileRecords.
        Logger.debug(LOG_TAG, "Calling delegate callback with guid " + replaced.guid +
                "(" + replaced.androidID + ")");

        // There's book-keeping which needs to happen with versions, and so we need to pass
        // along the new localVersion.
        delegate.onRecordStoreReconciled(replaced.guid, existingRecord.guid, replaced.localVersion);
        delegate.onRecordStoreSucceeded(1);
        return true;
    }

    @Override
    /* package-private */ boolean isLocallyModified(Record record) {
        if (record.localVersion == null || record.syncVersion == null) {
            throw new IllegalArgumentException("Bookmark session helper received non-versioned record");
        }
        return record.localVersion > record.syncVersion;
    }

    @Override
    /* package-private */ void insert(RepositorySessionStoreDelegate delegate, Record record) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
        try {
            insertionManager.enqueueRecord(delegate, (BookmarkRecord) record);
        } catch (Exception e) {
            throw new NullCursorException(e);
        }
    }

    @Override
    /* package-private */ Record reconcileRecords(Record remoteRecord, Record localRecord, long lastRemoteRetrieval, long lastLocalRetrieval) {
        // If localVersion==syncVersion, we should be able to safely take the remote record as is.
        // if localVersion>syncVersion, records changed both locally and remotely; merge changes.

        // In spirit of "no worse than before", while also setting a path forward, for now we
        // ignore relationship of localVersion to syncVersion, and always reconcile instead.

        // See Bug 814801 for a larger story around improving bookmark sync generally and record
        // merging in particular.
        final BookmarkRecord reconciled = (BookmarkRecord) session.reconcileRecords(
                remoteRecord, localRecord, lastRemoteRetrieval, lastLocalRetrieval);

        final BookmarkRecord remote = (BookmarkRecord) remoteRecord;
        final BookmarkRecord local = (BookmarkRecord) localRecord;

        // For now we *always* use the remote record's children array as a starting point.
        // We won't write it into the database yet; we'll record it and process as we go.
        reconciled.children = remote.children;

        // *Always* track folders, though: if we decide we need to reposition items, we'll
        // untrack later.
        if (reconciled.isFolder()) {
            session.trackRecord(reconciled);
        }

        // We should always have:
        // - local dateAdded
        // - lastModified values for both records
        // We might not have the remote dateAdded.
        // We always pick the lowest value out of what is available.
        long lowest = remote.lastModified;

        // During a similar operation, desktop clients consider dates before Jan 23, 1993 to be invalid.
        // We do the same here out of a desire to be consistent.
        final long releaseOfNCSAMosaicMillis = 727747200000L;

        if (local.dateAdded != null && local.dateAdded < lowest && local.dateAdded > releaseOfNCSAMosaicMillis) {
            reconciled.modifiedBySync = true;
            lowest = local.dateAdded;
        }

        if (remote.dateAdded != null && remote.dateAdded < lowest && remote.dateAdded > releaseOfNCSAMosaicMillis) {
            reconciled.modifiedBySync = true;
            lowest = remote.dateAdded;
        }

        reconciled.dateAdded = lowest;

        return reconciled;
    }

    @Override
    /* package-private */ Record prepareRecord(Record record) {
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

    /* package-private */ void finish() {
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
    }

    @Nullable
    private Record populateVersionFields(@Nullable Record record, Cursor cur) {
        if (record == null) {
            return null;
        }
        final int localVersionCol = cur.getColumnIndexOrThrow(BrowserContract.VersionColumns.LOCAL_VERSION);
        final int syncVersionCol = cur.getColumnIndexOrThrow(BrowserContract.VersionColumns.SYNC_VERSION);
        final int localVersion = cur.getInt(localVersionCol);
        // localVersion starts off at 1, and goes up. If we reset versions, localVersion is reset
        // back to 1 or 2, depending on reset type. Value of 0 means that either this logic is faulty,
        // or that we encountered a null value in the localVersion column, which also indicates that
        // something went really wrong.
        if (localVersion == 0) {
            throw new IllegalStateException("Unexpected localVersion value of 0 while fetching bookmark record");
        }
        record.localVersion = cur.getInt(localVersionCol);
        record.syncVersion = cur.getInt(syncVersionCol);
        return record;
    }

    @Nullable
    private Record replaceWithAssertion(Record newRecord, Record existingRecord, boolean shouldIncrementLocalVersion) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
        Record toStore = prepareRecord(newRecord);

        if (existingRecord.localVersion == null || existingRecord.syncVersion == null) {
            throw new IllegalStateException("Missing versions for a versioned bookmark record.");
        }

        // newRecord should already have suitable androidID and guid.
        boolean didUpdate = ((BookmarksDataAccessor) dbHelper).updateAssertingLocalVersion(
                existingRecord.guid,
                existingRecord.localVersion,
                shouldIncrementLocalVersion,
                toStore
        );
        if (!didUpdate) {
            return null;
        }
        if (shouldIncrementLocalVersion) {
            // Our replacement counts as a single modification to the record.
            toStore.localVersion = existingRecord.localVersion + 1;
        } else {
            toStore.localVersion = existingRecord.localVersion;
        }
        updateBookkeeping(toStore);
        Logger.debug(LOG_TAG, "replace() returning record " + toStore.guid);
        return toStore;
    }

    @SuppressWarnings("unchecked")
    private void finishUp(RepositorySessionStoreDelegate delegate) {
        flushQueues(delegate);
        Logger.debug(LOG_TAG, "Have " + parentToChildArray.size() + " folders whose children might need repositioning.");
        for (Map.Entry<String, JSONArray> entry : parentToChildArray.entrySet()) {
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
                    dbAccessor.bumpModified(folderID, RepositorySession.now());
                    session.untrackGUID(guid);
                }

                // If the arrays are different, or they're the same but not flushed to disk,
                // write them out now.
                if (!sameArrays || !clean) {
                    final ArrayList<String> list = (ArrayList<String>) (ArrayList) onServer;
                    dbAccessor.updatePositions(new ArrayList<>(list));
                }
            } catch (Exception e) {
                Logger.warn(LOG_TAG, "Error repositioning children for " + guid, e);
            }
        }
    }

    private void flushQueues(RepositorySessionStoreDelegate delegate) {
        long now = RepositorySession.now();
        Logger.debug(LOG_TAG, "Applying remaining insertions.");
        try {
            insertionManager.finishUp(delegate);
            Logger.debug(LOG_TAG, "Done applying remaining insertions.");
        } catch (Exception e) {
            Logger.warn(LOG_TAG, "Unable to apply remaining insertions.", e);
        }

        Logger.debug(LOG_TAG, "Applying deletions.");
        try {
            session.untrackGUIDs(deletionManager.flushAll(delegate, getIDForGUID("unfiled"), now));
            Logger.debug(LOG_TAG, "Done applying deletions.");
        } catch (Exception e) {
            Logger.error(LOG_TAG, "Unable to apply deletions.", e);
        }
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
    private static boolean forbiddenParent(final String parentGUID) {
        return parentGUID == null ||
                BrowserContract.Bookmarks.PINNED_FOLDER_GUID.equals(parentGUID);
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
                children = new ArrayList<>();
            }
            children.add(bmk.guid);
            needsReparenting++;
            missingParentToChildren.put(bmk.parentID, children);
        }
    }

    /**
     * Hook into the deletion manager on wipe.
     */
    private class BookmarkWipeRunnable extends WipeRunnable {
        /* package-private */ BookmarkWipeRunnable(RepositorySessionWipeDelegate delegate) {
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
            }
        }
    }

    /**
     * Build a record from a cursor, with a flag to dictate whether the
     * children array should be computed and written back into the database.
     */
    @Nullable
    private BookmarkRecord retrieveRecord(Cursor cur, boolean computeAndPersistChildren) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
        String recordGUID = getGUID(cur);
        Logger.trace(LOG_TAG, "Record from mirror cursor: " + recordGUID);

        if (forbiddenGUID(recordGUID)) {
            Logger.debug(LOG_TAG, "Ignoring " + recordGUID + " record in recordFromMirrorCursor.");
            return null;
        }

        // Short-cut for deleted items.
        if (isDeleted(cur)) {
            return bookmarkFromMirrorCursor(cur, null, null, null);
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
        BookmarkRecord bookmark = bookmarkFromMirrorCursor(cur, androidParentGUID, parentName, childArray);

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

    private String getParentName(String parentGUID) throws ParentNotFoundException, NullCursorException {
        if (parentGUID == null) {
            return "";
        }
        if (BookmarksRepositorySession.SPECIAL_GUIDS_MAP.containsKey(parentGUID)) {
            return BookmarksRepositorySession.SPECIAL_GUIDS_MAP.get(parentGUID);
        }

        // Get parent name from database.
        String parentName = "";
        Cursor name = dbAccessor.fetch(new String[] { parentGUID });
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


    private long getIDForGUID(String guid) {
        Long id = parentGuidToIDMap.get(guid);
        if (id == null) {
            Logger.warn(LOG_TAG, "Couldn't find local ID for GUID " + guid);
            return -1;
        }
        return id;
    }

    /**
     * Ensure that the local database row for the provided bookmark
     * reflects this record's parent information.
     *
     * @param bookmark
     */
    private void relocateBookmark(BookmarkRecord bookmark) {
        dbAccessor.updateParentAndPosition(bookmark.guid, bookmark.androidParentID, bookmark.androidPosition);
    }

    // Create a BookmarkRecord object from a cursor on a row containing a Fennec bookmark.
    private static BookmarkRecord bookmarkFromMirrorCursor(Cursor cur, String parentGUID, String parentName, JSONArray children) {
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
        }

        Logger.trace(LOG_TAG, "Record " + guid + " has type " + typeString);

        rec.type = typeString;
        rec.title = RepoUtils.getStringFromCursor(cur, BrowserContract.Bookmarks.TITLE);
        rec.bookmarkURI = RepoUtils.getStringFromCursor(cur, BrowserContract.Bookmarks.URL);
        rec.description = RepoUtils.getStringFromCursor(cur, BrowserContract.Bookmarks.DESCRIPTION);
        rec.tags = RepoUtils.getJSONArrayFromCursor(cur, BrowserContract.Bookmarks.TAGS);
        rec.keyword = RepoUtils.getStringFromCursor(cur, BrowserContract.Bookmarks.KEYWORD);
        rec.dateAdded = RepoUtils.getLongFromCursor(cur, BrowserContract.SyncColumns.DATE_CREATED);

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

    private static boolean isDeleted(Cursor cur) {
        return RepoUtils.getLongFromCursor(cur, BrowserContract.SyncColumns.IS_DELETED) != 0;
    }

    private String getGUID(Cursor cur) {
        return RepoUtils.getStringFromCursor(cur, "guid");
    }

    /**
     * Return true if the provided record GUID should be skipped
     * in child lists or fetch results.
     *
     * @param recordGUID the GUID of the record to check.
     * @return true if the record should be skipped.
     */
    private static boolean forbiddenGUID(final String recordGUID) {
        return recordGUID == null ||
                BrowserContract.Bookmarks.PINNED_FOLDER_GUID.equals(recordGUID) ||
                BrowserContract.Bookmarks.PLACES_FOLDER_GUID.equals(recordGUID) ||
                BrowserContract.Bookmarks.TAGS_FOLDER_GUID.equals(recordGUID);
    }

    private long getParentID(Cursor cur) {
        return RepoUtils.getLongFromCursor(cur, BrowserContract.Bookmarks.PARENT);
    }

    private String getGUIDForID(long androidID) {
        String guid = parentIDToGuidMap.get(androidID);
        trace("  " + androidID + " => " + guid);
        return guid;
    }

    private static BookmarkRecord computeParentFields(BookmarkRecord rec, String suggestedParentGUID, String suggestedParentName) {
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
        String parentName = BookmarksRepositorySession.SPECIAL_GUIDS_MAP.get(realParent);
        if (parentName == null) {
            parentName = suggestedParentName;
        }

        rec.parentID = realParent;
        rec.parentName = parentName;
        return rec;
    }

    private JSONArray getChildrenArrayForRecordCursor(Cursor cur, String recordGUID, boolean persist) throws NullCursorException {
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

    private static boolean rowIsFolder(Cursor cur) {
        return getTypeFromCursor(cur) == BrowserContract.Bookmarks.TYPE_FOLDER;
    }

    private static int getTypeFromCursor(Cursor cur) {
        return RepoUtils.getIntFromCursor(cur, BrowserContract.Bookmarks.TYPE);
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
        Cursor children = dbAccessor.getChildren(folderID);
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
            for (Map.Entry<Long, ArrayList<String>> entry : guids.entrySet()) {
                long pos = entry.getKey();
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
            final long time = RepositorySession.now();
            final ArrayList<String> cArray = (ArrayList<String>) (ArrayList) childArray;
            if (0 < dbAccessor.updatePositions(cArray)) {
                Logger.debug(LOG_TAG, "Bumping parent time to " + time + ".");
                dbAccessor.bumpModified(folderID, time);
            }
            return true;
        } finally {
            children.close();
        }
    }

    // More efficient for bulk operations.
    private long getPosition(Cursor cur, int positionIndex) {
        return cur.getLong(positionIndex);
    }
    private long getPosition(Cursor cur) {
        return RepoUtils.getLongFromCursor(cur, BrowserContract.Bookmarks.POSITION);
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

    /* package-private */ Runnable getFetchModifiedRunnable(long end,
                                      RecordFilter filter,
                                      RepositorySessionFetchRecordsDelegate delegate) {
        return new FetchModifiedRunnable(end, filter, delegate);
    }

    private boolean isSpecialRecord(Record record) {
        return SPECIAL_GUID_PARENTS.containsKey(record.guid);
    }

    private class FetchModifiedRunnable extends FetchingRunnable {
        private final long end;
        private final RecordFilter filter;

        /* package-private */ FetchModifiedRunnable(long end,
                                                    RecordFilter filter,
                                                    RepositorySessionFetchRecordsDelegate delegate) {
            super(delegate);
            this.end    = end;
            this.filter = filter;
        }

        @Override
        public void run() {
            if (!session.isActive()) {
                delegate.onFetchFailed(new InactiveSessionException());
                return;
            }

            try {
                Cursor cursor = ((BookmarksDataAccessor) dbHelper).fetchModified();
                this.fetchFromCursor(cursor, filter, end);
            } catch (NullCursorException e) {
                delegate.onFetchFailed(e);
                return;
            }
        }
    }
}
