/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Set;

import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.domain.BookmarkRecord;

/**
 * Queue up insertions:
 * <ul>
 * <li>Folder inserts where the parent is known. Do these immediately, because
 * they allow other records to be inserted. Requires bookkeeping updates. On
 * insert, flush the next set.</li>
 * <li>Regular inserts where the parent is known. These can happen whenever.
 * Batch for speed.</li>
 * <li>Records where the parent is not known. These can be flushed out when the
 * parent is known, or entered as orphans. This can be a queue earlier in the
 * process, so they don't get assigned to Unsorted. Feed into the main batch
 * when the parent arrives.</li>
 * </ul>
 * <p>
 * Deletions are always done at the end so that orphaning is minimized, and
 * that's why we are batching folders and non-folders separately.
 * <p>
 * Updates are always applied as they arrive.
 * <p>
 * Note that this class is not thread safe. This should be fine: call it only
 * from within a store runnable.
 */
public class BookmarksInsertionManager {
  public static final String LOG_TAG = "BookmarkInsert";
  public static boolean DEBUG = false;

  protected final int flushThreshold;
  protected final BookmarkInserter inserter;

  /**
   * Folders that have been successfully inserted.
   */
  private final Set<String> insertedFolders = new HashSet<String>();

  /**
   * Non-folders waiting for bulk insertion.
   * <p>
   * We write in insertion order to keep things easy to debug.
   */
  private final Set<BookmarkRecord> nonFoldersToWrite = new LinkedHashSet<BookmarkRecord>();

  /**
   * Map from parent folder GUID to child records (folders and non-folders)
   * waiting to be enqueued after parent folder is inserted.
   */
  private final Map<String, Set<BookmarkRecord>> recordsWaitingForParent = new HashMap<String, Set<BookmarkRecord>>();

  /**
   * Create an instance to be used for tracking insertions in a bookmarks
   * repository session.
   *
   * @param flushThreshold
   *        When this many non-folder records have been stored for insertion,
   *        an incremental flush occurs.
   * @param insertedFolders
   *        The GUIDs of all the folders already inserted into the database.
   * @param inserter
   *        The <code>BookmarkInsert</code> to use.
   */
  public BookmarksInsertionManager(int flushThreshold, Collection<String> insertedFolders, BookmarkInserter inserter) {
    this.flushThreshold = flushThreshold;
    this.insertedFolders.addAll(insertedFolders);
    this.inserter = inserter;
  }

  protected void addRecordWithUnwrittenParent(BookmarkRecord record) {
    Set<BookmarkRecord> destination = recordsWaitingForParent.get(record.parentID);
    if (destination == null) {
      destination = new LinkedHashSet<BookmarkRecord>();
      recordsWaitingForParent.put(record.parentID, destination);
    }
    destination.add(record);
  }

  /**
   * If <code>record</code> is a folder, insert it immediately; if it is a
   * non-folder, enqueue it. Then do the same for any records waiting for this record.
   *
   * @param record
   *          the <code>BookmarkRecord</code> to enqueue.
   */
  protected void recursivelyEnqueueRecordAndChildren(BookmarkRecord record) {
    if (record.isFolder()) {
      if (!inserter.insertFolder(record)) {
        Logger.warn(LOG_TAG, "Folder with known parent with guid " + record.parentID + " failed to insert!");
        return;
      }
      Logger.debug(LOG_TAG, "Folder with known parent with guid " + record.parentID + " inserted; adding to inserted folders.");
      insertedFolders.add(record.guid);
    } else {
      Logger.debug(LOG_TAG, "Non-folder has known parent with guid " + record.parentID + "; adding to insertion queue.");
      nonFoldersToWrite.add(record);
    }

    // Now process record's children.
    Set<BookmarkRecord> waiting = recordsWaitingForParent.remove(record.guid);
    if (waiting == null) {
      return;
    }
    for (BookmarkRecord waiter : waiting) {
      recursivelyEnqueueRecordAndChildren(waiter);
    }
  }

  /**
   * Enqueue a folder.
   *
   * @param record
   *          the folder to enqueue.
   */
  protected void enqueueFolder(BookmarkRecord record) {
    Logger.debug(LOG_TAG, "Inserting folder with guid " + record.guid);

    if (!insertedFolders.contains(record.parentID)) {
      Logger.debug(LOG_TAG, "Folder has unknown parent with guid " + record.parentID + "; keeping until we see the parent.");
      addRecordWithUnwrittenParent(record);
      return;
    }

    // Parent is known; add as much of the tree as this roots.
    recursivelyEnqueueRecordAndChildren(record);
    flushNonFoldersIfNecessary();
  }

  /**
   * Enqueue a non-folder.
   *
   * @param record
   *          the non-folder to enqueue.
   */
  protected void enqueueNonFolder(BookmarkRecord record) {
    Logger.debug(LOG_TAG, "Inserting non-folder with guid " + record.guid);

    if (!insertedFolders.contains(record.parentID)) {
      Logger.debug(LOG_TAG, "Non-folder has unknown parent with guid " + record.parentID + "; keeping until we see the parent.");
      addRecordWithUnwrittenParent(record);
      return;
    }

    // Parent is known; add to insertion queue and maybe write.
    Logger.debug(LOG_TAG, "Non-folder has known parent with guid " + record.parentID + "; adding to insertion queue.");
    nonFoldersToWrite.add(record);
    flushNonFoldersIfNecessary();
  }

  /**
   * Enqueue a bookmark record for eventual insertion.
   *
   * @param record
   *          the <code>BookmarkRecord</code> to enqueue.
   */
  public void enqueueRecord(BookmarkRecord record) {
    if (record.isFolder()) {
      enqueueFolder(record);
    } else {
      enqueueNonFolder(record);
    }
    if (DEBUG) {
      dumpState();
    }
  }

  /**
   * Flush non-folders; empties the insertion queue entirely.
   */
  protected void flushNonFolders() {
    inserter.bulkInsertNonFolders(nonFoldersToWrite); // All errors are handled in bulkInsertNonFolders.
    nonFoldersToWrite.clear();
  }

  /**
   * Flush non-folder insertions if there are many of them; empties the
   * insertion queue entirely.
   */
  protected void flushNonFoldersIfNecessary() {
    int num = nonFoldersToWrite.size();
    if (num < flushThreshold) {
      Logger.debug(LOG_TAG, "Incremental flush called with " + num + " < " + flushThreshold + " non-folders; not flushing.");
      return;
    }
    Logger.debug(LOG_TAG, "Incremental flush called with " + num + " non-folders; flushing.");
    flushNonFolders();
  }

  /**
   * Insert all remaining folders followed by all remaining non-folders,
   * regardless of whether parent records have been successfully inserted.
   */
  public void finishUp() {
    // Iterate through all waiting records, writing the folders and collecting
    // the non-folders for bulk insertion.
    int numFolders = 0;
    int numNonFolders = 0;
    for (Set<BookmarkRecord> records : recordsWaitingForParent.values()) {
      for (BookmarkRecord record : records) {
        if (!record.isFolder()) {
          numNonFolders += 1;
          nonFoldersToWrite.add(record);
          continue;
        }

        numFolders += 1;
        if (!inserter.insertFolder(record)) {
          Logger.warn(LOG_TAG, "Folder with known parent with guid " + record.parentID + " failed to insert!");
          continue;
        }

        Logger.debug(LOG_TAG, "Folder with known parent with guid " + record.parentID + " inserted; adding to inserted folders.");
        insertedFolders.add(record.guid);
      }
    }
    recordsWaitingForParent.clear();
    flushNonFolders();

    Logger.debug(LOG_TAG, "finishUp inserted " +
        numFolders + " folders without known parents and " +
        numNonFolders + " non-folders without known parents.");
    if (DEBUG) {
      dumpState();
    }
  }

  public void clear() {
    this.insertedFolders.clear();
    this.nonFoldersToWrite.clear();
    this.recordsWaitingForParent.clear();
  }

  // For debugging.
  public boolean isClear() {
    return nonFoldersToWrite.isEmpty() && recordsWaitingForParent.isEmpty();
  }

  // For debugging.
  public void dumpState() {
    ArrayList<String> readies = new ArrayList<String>();
    for (BookmarkRecord record : nonFoldersToWrite) {
      readies.add(record.guid);
    }
    String ready = Utils.toCommaSeparatedString(new ArrayList<String>(readies));

    ArrayList<String> waits = new ArrayList<String>();
    for (Set<BookmarkRecord> recs : recordsWaitingForParent.values()) {
      for (BookmarkRecord rec : recs) {
        waits.add(rec.guid);
      }
    }
    String waiting = Utils.toCommaSeparatedString(waits);
    String known = Utils.toCommaSeparatedString(insertedFolders);

    Logger.debug(LOG_TAG, "Q=(" + ready + "), W = (" + waiting + "), P=(" + known + ")");
  }

  public interface BookmarkInserter {
    /**
     * Insert a single folder.
     * <p>
     * All exceptions should be caught and all delegate callbacks invoked here.
     *
     * @param record
     *          the record to insert.
     * @return
     *          <code>true</code> if the folder was inserted; <code>false</code> otherwise.
     */
    public boolean insertFolder(BookmarkRecord record);

    /**
     * Insert many non-folders. Each non-folder's parent was already present in
     * the database before this <code>BookmarkInsertionsManager</code> was
     * created, or had <code>insertFolder</code> called with it as argument (and
     * possibly was not inserted).
     * <p>
     * All exceptions should be caught and all delegate callbacks invoked here.
     *
     * @param record
     *          the record to insert.
     */
    public void bulkInsertNonFolders(Collection<BookmarkRecord> records);
  }
}
