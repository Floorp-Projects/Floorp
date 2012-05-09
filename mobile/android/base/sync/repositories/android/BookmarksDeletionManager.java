/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.domain.BookmarkRecord;

/**
 * Queue up deletions. Process them at the end.
 *
 * Algorithm:
 *
 * * Collect GUIDs as we go. For convenience we partition these into
 *   folders and non-folders.
 *
 * * Non-folders can be deleted in batches as we go.
 *
 * * At the end of the sync:
 *   * Delete all that aren't folders.
 *   * Move the remaining children of any that are folders to an "Orphans" folder.
 *     - We do this even for children that are _marked_ as deleted -- we still want
 *       to upload them, and their parent is irrelevant.
 *   * Delete all the folders.
 *
 * * Any outstanding records -- the ones we moved to "Orphans" -- are true orphans.
 *   These should be reuploaded (because their parent has changed), as should their
 *   new parent (because its children array has changed).
 *   We achieve the former by moving them without tracking (but we don't make any
 *   special effort here -- warning! Lurking bug!).
 *   We achieve the latter by bumping its mtime. The caller should take care of untracking it.
 *
 * Note that we make no particular effort to handle repositioning or reparenting:
 * batching deletes at the end should be handled seamlessly by existing code,
 * because the deleted records could have arrived in a batch at the end regardless.
 *
 * Note that this class is not thread safe. This should be fine: call it only
 * from within a store runnable.
 *
 */
public class BookmarksDeletionManager {
  private static final String LOG_TAG = "BookmarkDelete";

  private final AndroidBrowserBookmarksDataAccessor dataAccessor;
  private RepositorySessionStoreDelegate delegate;

  private final int flushThreshold;

  private final HashSet<String> folders    = new HashSet<String>();
  private final HashSet<String> nonFolders = new HashSet<String>();
  private int nonFolderCount = 0;

  // Records that we need to touch once we've deleted the non-folders.
  private HashSet<String> nonFolderParents = new HashSet<String>();
  private HashSet<String> folderParents    = new HashSet<String>();

  /**
   * Create an instance to be used for tracking deletions in a bookmarks
   * repository session.
   *
   * @param dataAccessor
   *        Used to effect database changes.
   *
   * @param flushThreshold
   *        When this many non-folder records have been stored for deletion,
   *        an incremental flush occurs.
   */
  public BookmarksDeletionManager(AndroidBrowserBookmarksDataAccessor dataAccessor, int flushThreshold) {
    this.dataAccessor = dataAccessor;
    this.flushThreshold = flushThreshold;
  }

  /**
   * Set the delegate to use for callbacks.
   * If not invoked, no callbacks will be submitted.
   *
   * @param delegate a delegate, which should already be a delayed delegate.
   */
  public void setDelegate(RepositorySessionStoreDelegate delegate) {
    this.delegate = delegate;
  }

  public void deleteRecord(String guid, boolean isFolder, String parentGUID) {
    if (guid == null) {
      Logger.warn(LOG_TAG, "Cannot queue deletion of record with no GUID.");
      return;
    }
    Logger.debug(LOG_TAG, "Queuing deletion of " + guid);

    if (isFolder) {
      folders.add(guid);
      if (!folders.contains(parentGUID)) {
        // We're not going to delete its parent; will need to bump it.
        folderParents.add(parentGUID);
      }

      nonFolderParents.remove(guid);
      folderParents.remove(guid);
      return;
    }

    if (!folders.contains(parentGUID)) {
      // We're not going to delete its parent; will need to bump it.
      nonFolderParents.add(parentGUID);
    }

    if (nonFolders.add(guid)) {
      if (++nonFolderCount >= flushThreshold) {
        deleteNonFolders();
      }
    }
  }

  /**
   * Flush deletions that can be easily taken care of right now.
   */
  public void incrementalFlush() {
    // Yes, this means we only bump when we finish, not during an incremental flush.
    deleteNonFolders();
  }

  /**
   * Apply all pending deletions and reset state for the next batch of stores.
   *
   * @param orphanDestination the ID of the folder to which orphaned children
   *                          should be moved.
   *
   * @throws NullCursorException
   * @return a set of IDs to untrack. Will not be null.
   */
  public Set<String> flushAll(long orphanDestination, long now) throws NullCursorException {
    Logger.debug(LOG_TAG, "Doing complete flush of deleted items. Moving orphans to " + orphanDestination);
    deleteNonFolders();

    // Find out which parents *won't* be deleted, and thus need to have their
    // modified times bumped.
    nonFolderParents.removeAll(folders);

    Logger.debug(LOG_TAG, "Bumping modified times for " + nonFolderParents.size() +
                          " parents of deleted non-folders.");
    dataAccessor.bumpModifiedByGUID(nonFolderParents, now);

    if (folders.size() > 0) {
      final String[] folderGUIDs = folders.toArray(new String[folders.size()]);
      final String[] folderIDs = getIDs(folderGUIDs);   // Throws if any don't exist.
      int moved = dataAccessor.moveChildren(folderIDs, orphanDestination);
      if (moved > 0) {
        dataAccessor.bumpModified(orphanDestination, now);
      }

      // We've deleted or moved anything that might be under these folders.
      // Just delete them.
      final String folderWhere = RepoUtils.computeSQLInClause(folders.size(), BrowserContract.Bookmarks.GUID);
      dataAccessor.delete(folderWhere, folderGUIDs);
      invokeCallbacks(delegate, folderGUIDs);

      folderParents.removeAll(folders);
      Logger.debug(LOG_TAG, "Bumping modified times for " + folderParents.size() +
                            " parents of deleted folders.");
      dataAccessor.bumpModifiedByGUID(folderParents, now);

      // Clean up.
      folders.clear();
    }

    HashSet<String> ret = nonFolderParents;
    ret.addAll(folderParents);

    nonFolderParents = new HashSet<String>();
    folderParents    = new HashSet<String>();
    return ret;
  }

  private String[] getIDs(String[] guids) throws NullCursorException {
    // Convert GUIDs to numeric IDs.
    String[] ids = new String[guids.length];
    Map<String, Long> guidsToIDs = dataAccessor.idsForGUIDs(guids);
    for (int i = 0; i < guids.length; ++i) {
      String guid = guids[i];
      Long id =  guidsToIDs.get(guid);
      if (id == null) {
        throw new IllegalArgumentException("Can't get ID for unknown record " + guid);
      }
      ids[i] = id.toString();
    }
    return ids;
  }

  /**
   * Flush non-folder deletions. This can be called at any time.
   */
  private void deleteNonFolders() {
    if (nonFolderCount == 0) {
      Logger.debug(LOG_TAG, "No non-folders to delete.");
      return;
    }

    Logger.debug(LOG_TAG, "Applying deletion of " + nonFolderCount + " non-folders.");
    final String[] nonFolderGUIDs = nonFolders.toArray(new String[nonFolderCount]);
    final String nonFolderWhere = RepoUtils.computeSQLInClause(nonFolderCount, BrowserContract.Bookmarks.GUID);
    dataAccessor.delete(nonFolderWhere, nonFolderGUIDs);

    invokeCallbacks(delegate, nonFolderGUIDs);

    // Discard these.
    // Note that we maintain folderParents and nonFolderParents; we need them later.
    nonFolders.clear();
    nonFolderCount = 0;
  }

  private void invokeCallbacks(RepositorySessionStoreDelegate delegate,
                               String[] nonFolderGUIDs) {
    if (delegate == null) {
      return;
    }
    Logger.trace(LOG_TAG, "Invoking store callback for " + nonFolderGUIDs.length + " GUIDs.");
    final long now = System.currentTimeMillis();
    BookmarkRecord r = new BookmarkRecord(null, "bookmarks", now, true);
    for (String guid : nonFolderGUIDs) {
      r.guid = guid;
      delegate.onRecordStoreSucceeded(r);
    }
  }

  /**
   * Clear state in case of redundancy (e.g., wipe).
   */
  public void clear() {
    nonFolders.clear();
    nonFolderCount = 0;
    folders.clear();
    nonFolderParents.clear();
    folderParents.clear();
  }
}
