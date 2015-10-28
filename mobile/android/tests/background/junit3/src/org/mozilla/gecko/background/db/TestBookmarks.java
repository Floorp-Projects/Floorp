/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.db;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;

import org.json.simple.JSONArray;
import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.helpers.AndroidSyncTestCase;
import org.mozilla.gecko.background.sync.helpers.BookmarkHelpers;
import org.mozilla.gecko.background.sync.helpers.SimpleSuccessBeginDelegate;
import org.mozilla.gecko.background.sync.helpers.SimpleSuccessCreationDelegate;
import org.mozilla.gecko.background.sync.helpers.SimpleSuccessFetchDelegate;
import org.mozilla.gecko.background.sync.helpers.SimpleSuccessFinishDelegate;
import org.mozilla.gecko.background.sync.helpers.SimpleSuccessStoreDelegate;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;
import org.mozilla.gecko.sync.repositories.NoStoreDelegateException;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.RepositorySessionBundle;
import org.mozilla.gecko.sync.repositories.android.AndroidBrowserBookmarksDataAccessor;
import org.mozilla.gecko.sync.repositories.android.AndroidBrowserBookmarksRepository;
import org.mozilla.gecko.sync.repositories.android.AndroidBrowserBookmarksRepositorySession;
import org.mozilla.gecko.sync.repositories.android.BrowserContractHelpers;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionBeginDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.domain.BookmarkRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;

public class TestBookmarks extends AndroidSyncTestCase {

  protected static final String LOG_TAG = "BookmarksTest";

  /**
   * Trivial test that forbidden records such as pinned items
   * will be ignored if processed.
   */
  public void testForbiddenItemsAreIgnored() {
    final AndroidBrowserBookmarksRepository repo = new AndroidBrowserBookmarksRepository();
    final long now = System.currentTimeMillis();
    final String bookmarksCollection = "bookmarks";

    final BookmarkRecord pinned = new BookmarkRecord("pinpinpinpin", "bookmarks", now - 1, false);
    final BookmarkRecord normal = new BookmarkRecord("baaaaaaaaaaa", "bookmarks", now - 2, false);

    final BookmarkRecord pinnedItems  = new BookmarkRecord(Bookmarks.PINNED_FOLDER_GUID,
                                                           bookmarksCollection, now - 4, false);

    normal.type = "bookmark";
    pinned.type = "bookmark";
    pinnedItems.type = "folder";

    pinned.parentID = Bookmarks.PINNED_FOLDER_GUID;
    normal.parentID = Bookmarks.TOOLBAR_FOLDER_GUID;

    pinnedItems.parentID = Bookmarks.PLACES_FOLDER_GUID;

    inBegunSession(repo, new SimpleSuccessBeginDelegate() {
      @Override
      public void onBeginSucceeded(RepositorySession session) {
        assertTrue(((AndroidBrowserBookmarksRepositorySession) session).shouldIgnore(pinned));
        assertTrue(((AndroidBrowserBookmarksRepositorySession) session).shouldIgnore(pinnedItems));
        assertFalse(((AndroidBrowserBookmarksRepositorySession) session).shouldIgnore(normal));
        finishAndNotify(session);
      }
    });
  }

  /**
   * Trivial test that pinned items will be skipped if present in the DB.
   */
  public void testPinnedItemsAreNotRetrieved() {
    final AndroidBrowserBookmarksRepository repo = new AndroidBrowserBookmarksRepository();

    // Ensure that they exist.
    setUpFennecPinnedItemsRecord();

    // They're there in the DB…
    final ArrayList<String> roots = fetchChildrenDirect(Bookmarks.FIXED_ROOT_ID);
    Logger.info(LOG_TAG, "Roots: " + roots);
    assertTrue(roots.contains(Bookmarks.PINNED_FOLDER_GUID));

    final ArrayList<String> pinned = fetchChildrenDirect(Bookmarks.FIXED_PINNED_LIST_ID);
    Logger.info(LOG_TAG, "Pinned: " + pinned);
    assertTrue(pinned.contains("dapinneditem"));

    // … but not when we fetch.
    final ArrayList<String> guids = fetchGUIDs(repo);
    assertFalse(guids.contains(Bookmarks.PINNED_FOLDER_GUID));
    assertFalse(guids.contains("dapinneditem"));
  }

  public void testRetrieveFolderHasAccurateChildren() {
    AndroidBrowserBookmarksRepository repo = new AndroidBrowserBookmarksRepository();

    final long now = System.currentTimeMillis();

    final String folderGUID = "eaaaaaaaafff";
    BookmarkRecord folder    = new BookmarkRecord(folderGUID,     "bookmarks", now - 5, false);
    BookmarkRecord bookmarkA = new BookmarkRecord("daaaaaaaaaaa", "bookmarks", now - 1, false);
    BookmarkRecord bookmarkB = new BookmarkRecord("baaaaaaaabbb", "bookmarks", now - 3, false);
    BookmarkRecord bookmarkC = new BookmarkRecord("aaaaaaaaaccc", "bookmarks", now - 2, false);

    folder.children   = childrenFromRecords(bookmarkA, bookmarkB, bookmarkC);
    folder.sortIndex  = 150;
    folder.title      = "Test items";
    folder.parentID   = "toolbar";
    folder.parentName = "Bookmarks Toolbar";
    folder.type       = "folder";

    bookmarkA.parentID    = folderGUID;
    bookmarkA.bookmarkURI = "http://example.com/A";
    bookmarkA.title       = "Title A";
    bookmarkA.type        = "bookmark";

    bookmarkB.parentID    = folderGUID;
    bookmarkB.bookmarkURI = "http://example.com/B";
    bookmarkB.title       = "Title B";
    bookmarkB.type        = "bookmark";

    bookmarkC.parentID    = folderGUID;
    bookmarkC.bookmarkURI = "http://example.com/C";
    bookmarkC.title       = "Title C";
    bookmarkC.type        = "bookmark";

    BookmarkRecord[] folderOnly = new BookmarkRecord[1];
    BookmarkRecord[] children   = new BookmarkRecord[3];

    folderOnly[0] = folder;

    children[0] = bookmarkA;
    children[1] = bookmarkB;
    children[2] = bookmarkC;

    wipe();
    Logger.debug(getName(), "Storing just folder...");
    storeRecordsInSession(repo, folderOnly, null);

    // We don't have any children, despite our insistence upon storing.
    assertChildrenAreOrdered(repo, folderGUID, new Record[] {});

    // Now store the children.
    Logger.debug(getName(), "Storing children...");
    storeRecordsInSession(repo, children, null);

    // Now we have children, but their order is not determined, because
    // they were stored out-of-session with the original folder.
    assertChildrenAreUnordered(repo, folderGUID, children);

    // Now if we store the folder record again, they'll be put in the
    // right place.
    folder.lastModified++;
    Logger.debug(getName(), "Storing just folder again...");
    storeRecordsInSession(repo, folderOnly, null);
    Logger.debug(getName(), "Fetching children yet again...");
    assertChildrenAreOrdered(repo, folderGUID, children);

    // Now let's see what happens when we see records in the same session.
    BookmarkRecord[] parentMixed = new BookmarkRecord[4];
    BookmarkRecord[] parentFirst = new BookmarkRecord[4];
    BookmarkRecord[] parentLast  = new BookmarkRecord[4];

    // None of our records have a position set.
    assertTrue(bookmarkA.androidPosition <= 0);
    assertTrue(bookmarkB.androidPosition <= 0);
    assertTrue(bookmarkC.androidPosition <= 0);

    parentMixed[1] = folder;
    parentMixed[0] = bookmarkA;
    parentMixed[2] = bookmarkC;
    parentMixed[3] = bookmarkB;

    parentFirst[0] = folder;
    parentFirst[1] = bookmarkC;
    parentFirst[2] = bookmarkA;
    parentFirst[3] = bookmarkB;

    parentLast[3]  = folder;
    parentLast[0]  = bookmarkB;
    parentLast[1]  = bookmarkA;
    parentLast[2]  = bookmarkC;

    wipe();
    storeRecordsInSession(repo, parentMixed, null);
    assertChildrenAreOrdered(repo, folderGUID, children);

    wipe();
    storeRecordsInSession(repo, parentFirst, null);
    assertChildrenAreOrdered(repo, folderGUID, children);

    wipe();
    storeRecordsInSession(repo, parentLast, null);
    assertChildrenAreOrdered(repo, folderGUID, children);

    // Ensure that records are ordered even if we re-process the folder.
    wipe();
    storeRecordsInSession(repo, parentLast, null);
    folder.lastModified++;
    storeRecordsInSession(repo, folderOnly, null);
    assertChildrenAreOrdered(repo, folderGUID, children);
  }

  public void testMergeFoldersPreservesSaneOrder() {
    AndroidBrowserBookmarksRepository repo = new AndroidBrowserBookmarksRepository();

    final long now = System.currentTimeMillis();
    final String folderGUID = "mobile";

    wipe();
    final long mobile = setUpFennecMobileRecord();

    // No children.
    assertChildrenAreUnordered(repo, folderGUID, new Record[] {});

    // Add some, as Fennec would.
    fennecAddBookmark("Bookmark One", "http://example.com/fennec/One");
    fennecAddBookmark("Bookmark Two", "http://example.com/fennec/Two");

    Logger.debug(getName(), "Fetching children...");
    JSONArray folderChildren = fetchChildrenForGUID(repo, folderGUID);

    assertTrue(folderChildren != null);
    Logger.debug(getName(), "Children are " + folderChildren.toJSONString());
    assertEquals(2, folderChildren.size());
    String guidOne = (String) folderChildren.get(0);
    String guidTwo = (String) folderChildren.get(1);

    // Make sure positions were saved.
    assertChildrenAreDirect(mobile, new String[] {
        guidOne,
        guidTwo
    });

    // Add some through Sync.
    BookmarkRecord folder    = new BookmarkRecord(folderGUID,     "bookmarks", now, false);
    BookmarkRecord bookmarkA = new BookmarkRecord("daaaaaaaaaaa", "bookmarks", now, false);
    BookmarkRecord bookmarkB = new BookmarkRecord("baaaaaaaabbb", "bookmarks", now, false);

    folder.children   = childrenFromRecords(bookmarkA, bookmarkB);
    folder.sortIndex  = 150;
    folder.title      = "Mobile Bookmarks";
    folder.parentID   = "places";
    folder.parentName = "";
    folder.type       = "folder";

    bookmarkA.parentID    = folderGUID;
    bookmarkA.parentName  = "Mobile Bookmarks";       // Using this title exercises Bug 748898.
    bookmarkA.bookmarkURI = "http://example.com/A";
    bookmarkA.title       = "Title A";
    bookmarkA.type        = "bookmark";

    bookmarkB.parentID    = folderGUID;
    bookmarkB.parentName  = "mobile";
    bookmarkB.bookmarkURI = "http://example.com/B";
    bookmarkB.title       = "Title B";
    bookmarkB.type        = "bookmark";

    BookmarkRecord[] parentMixed = new BookmarkRecord[3];
    parentMixed[0] = bookmarkA;
    parentMixed[1] = folder;
    parentMixed[2] = bookmarkB;

    storeRecordsInSession(repo, parentMixed, null);

    BookmarkRecord expectedOne = new BookmarkRecord(guidOne, "bookmarks", now - 10, false);
    BookmarkRecord expectedTwo = new BookmarkRecord(guidTwo, "bookmarks", now - 10, false);

    // We want the server to win in this case, and otherwise to preserve order.
    // TODO
    assertChildrenAreOrdered(repo, folderGUID, new Record[] {
        bookmarkA,
        bookmarkB,
        expectedOne,
        expectedTwo
    });

    // Furthermore, the children of that folder should be correct in the DB.
    ContentResolver cr = getApplicationContext().getContentResolver();
    final long folderId = fennecGetFolderId(cr, folderGUID);
    Logger.debug(getName(), "Folder " + folderGUID + " => " + folderId);

    assertChildrenAreDirect(folderId, new String[] {
        bookmarkA.guid,
        bookmarkB.guid,
        expectedOne.guid,
        expectedTwo.guid
    });
  }

  /**
   * Apply a folder record whose children array is already accurately
   * stored in the database. Verify that the parent folder is not flagged
   * for reupload (i.e., that its modified time is *ahem* unmodified).
   */
  public void testNoReorderingMeansNoReupload() {
    AndroidBrowserBookmarksRepository repo = new AndroidBrowserBookmarksRepository();

    final long now = System.currentTimeMillis();

    final String folderGUID = "eaaaaaaaafff";
    BookmarkRecord folder    = new BookmarkRecord(folderGUID,     "bookmarks", now -5, false);
    BookmarkRecord bookmarkA = new BookmarkRecord("daaaaaaaaaaa", "bookmarks", now -1, false);
    BookmarkRecord bookmarkB = new BookmarkRecord("baaaaaaaabbb", "bookmarks", now -3, false);

    folder.children   = childrenFromRecords(bookmarkA, bookmarkB);
    folder.sortIndex  = 150;
    folder.title      = "Test items";
    folder.parentID   = "toolbar";
    folder.parentName = "Bookmarks Toolbar";
    folder.type       = "folder";

    bookmarkA.parentID    = folderGUID;
    bookmarkA.bookmarkURI = "http://example.com/A";
    bookmarkA.title       = "Title A";
    bookmarkA.type        = "bookmark";

    bookmarkB.parentID    = folderGUID;
    bookmarkB.bookmarkURI = "http://example.com/B";
    bookmarkB.title       = "Title B";
    bookmarkB.type        = "bookmark";

    BookmarkRecord[] abf = new BookmarkRecord[3];
    BookmarkRecord[] justFolder = new BookmarkRecord[1];

    abf[0] = bookmarkA;
    abf[1] = bookmarkB;
    abf[2] = folder;

    justFolder[0] = folder;

    final String[] abGUIDs   = new String[] { bookmarkA.guid, bookmarkB.guid };
    final Record[] abRecords = new Record[] { bookmarkA, bookmarkB };
    final String[] baGUIDs   = new String[] { bookmarkB.guid, bookmarkA.guid };
    final Record[] baRecords = new Record[] { bookmarkB, bookmarkA };

    wipe();
    Logger.debug(getName(), "Storing A, B, folder...");
    storeRecordsInSession(repo, abf, null);

    ContentResolver cr = getApplicationContext().getContentResolver();
    final long folderID = fennecGetFolderId(cr, folderGUID);
    assertChildrenAreOrdered(repo, folderGUID, abRecords);
    assertChildrenAreDirect(folderID, abGUIDs);

    // To ensure an interval.
    try {
      Thread.sleep(100);
    } catch (InterruptedException e) {
    }

    // Store the same folder record again, and check the tracking.
    // Because the folder array didn't change,
    // the item is still tracked to not be uploaded.
    folder.lastModified = System.currentTimeMillis() + 1;
    HashSet<String> tracked = new HashSet<String>();
    storeRecordsInSession(repo, justFolder, tracked);
    assertChildrenAreOrdered(repo, folderGUID, abRecords);
    assertChildrenAreDirect(folderID, abGUIDs);

    assertTrue(tracked.contains(folderGUID));

    // Store again, but with a different order.
    tracked = new HashSet<String>();
    folder.children = childrenFromRecords(bookmarkB, bookmarkA);
    folder.lastModified = System.currentTimeMillis() + 1;
    storeRecordsInSession(repo, justFolder, tracked);
    assertChildrenAreOrdered(repo, folderGUID, baRecords);
    assertChildrenAreDirect(folderID, baGUIDs);

    // Now it's going to be reuploaded.
    assertFalse(tracked.contains(folderGUID));
  }

  /**
   * Exercise the deletion of folders when their children have not been
   * marked as deleted. In a database with constraints, this would fail
   * if we simply deleted the records, so we move them first.
   */
  public void testFolderDeletionOrphansChildren() {
    AndroidBrowserBookmarksRepository repo = new AndroidBrowserBookmarksRepository();

    long now = System.currentTimeMillis();

    // Add a folder and four children.
    final String folderGUID = "eaaaaaaaafff";
    BookmarkRecord folder    = new BookmarkRecord(folderGUID,     "bookmarks", now -5, false);
    BookmarkRecord bookmarkA = new BookmarkRecord("daaaaaaaaaaa", "bookmarks", now -1, false);
    BookmarkRecord bookmarkB = new BookmarkRecord("baaaaaaaabbb", "bookmarks", now -3, false);
    BookmarkRecord bookmarkC = new BookmarkRecord("daaaaaaaaccc", "bookmarks", now -7, false);
    BookmarkRecord bookmarkD = new BookmarkRecord("baaaaaaaaddd", "bookmarks", now -4, false);

    folder.children   = childrenFromRecords(bookmarkA, bookmarkB, bookmarkC, bookmarkD);
    folder.sortIndex  = 150;
    folder.title      = "Test items";
    folder.parentID   = "toolbar";
    folder.parentName = "Bookmarks Toolbar";
    folder.type       = "folder";

    bookmarkA.parentID    = folderGUID;
    bookmarkA.bookmarkURI = "http://example.com/A";
    bookmarkA.title       = "Title A";
    bookmarkA.type        = "bookmark";

    bookmarkB.parentID    = folderGUID;
    bookmarkB.bookmarkURI = "http://example.com/B";
    bookmarkB.title       = "Title B";
    bookmarkB.type        = "bookmark";

    bookmarkC.parentID    = folderGUID;
    bookmarkC.bookmarkURI = "http://example.com/C";
    bookmarkC.title       = "Title C";
    bookmarkC.type        = "bookmark";

    bookmarkD.parentID    = folderGUID;
    bookmarkD.bookmarkURI = "http://example.com/D";
    bookmarkD.title       = "Title D";
    bookmarkD.type        = "bookmark";

    BookmarkRecord[] abfcd = new BookmarkRecord[5];
    BookmarkRecord[] justFolder = new BookmarkRecord[1];
    abfcd[0] = bookmarkA;
    abfcd[1] = bookmarkB;
    abfcd[2] = folder;
    abfcd[3] = bookmarkC;
    abfcd[4] = bookmarkD;

    justFolder[0] = folder;

    final String[] abcdGUIDs   = new String[] { bookmarkA.guid, bookmarkB.guid, bookmarkC.guid, bookmarkD.guid };
    final Record[] abcdRecords = new Record[] { bookmarkA, bookmarkB, bookmarkC, bookmarkD };

    wipe();
    Logger.debug(getName(), "Storing A, B, folder, C, D...");
    storeRecordsInSession(repo, abfcd, null);

    // Verify that it worked.
    ContentResolver cr = getApplicationContext().getContentResolver();
    final long folderID = fennecGetFolderId(cr, folderGUID);
    assertChildrenAreOrdered(repo, folderGUID, abcdRecords);
    assertChildrenAreDirect(folderID, abcdGUIDs);

    now = System.currentTimeMillis();

    // Add one child to unsorted bookmarks.
    BookmarkRecord unsortedA = new BookmarkRecord("yiamunsorted", "bookmarks", now, false);
    unsortedA.parentID = "unfiled";
    unsortedA.title    = "Unsorted A";
    unsortedA.type     = "bookmark";
    unsortedA.androidPosition = 0;

    BookmarkRecord[] ua = new BookmarkRecord[1];
    ua[0] = unsortedA;

    storeRecordsInSession(repo, ua, null);

    // Ensure that the database is in this state.
    assertChildrenAreOrdered(repo, "unfiled", ua);

    // Delete the second child, the folder, and then the third child.
    bookmarkB.bookmarkURI  = bookmarkC.bookmarkURI  = folder.bookmarkURI  = null;
    bookmarkB.deleted      = bookmarkC.deleted      = folder.deleted      = true;
    bookmarkB.title        = bookmarkC.title        = folder.title        = null;

    // Nulling the type of folder is very important: it verifies
    // that the session can behave correctly according to local type.
    bookmarkB.type = bookmarkC.type = folder.type = null;

    bookmarkB.lastModified = bookmarkC.lastModified = folder.lastModified = now = System.currentTimeMillis();

    BookmarkRecord[] deletions = new BookmarkRecord[] { bookmarkB, folder, bookmarkC };
    storeRecordsInSession(repo, deletions, null);

    // Verify that the unsorted bookmarks folder contains its child and the
    // first and fourth children of the now-deleted folder.
    // Also verify that the folder is gone.
    long unsortedID = fennecGetFolderId(cr, "unfiled");
    long toolbarID  = fennecGetFolderId(cr, "toolbar");
    String[] expected = new String[] { unsortedA.guid, bookmarkA.guid, bookmarkD.guid };

    // This will trigger positioning.
    assertChildrenAreUnordered(repo, "unfiled", new Record[] { unsortedA, bookmarkA, bookmarkD });
    assertChildrenAreDirect(unsortedID, expected);
    assertChildrenAreDirect(toolbarID, new String[] {});
  }

  /**
   * A test where we expect to replace a local folder with a new folder (with a
   * new GUID), whilst adding children to it. Verifies that replace and insert
   * co-operate.
   */
  public void testInsertAndReplaceGuid() {
    AndroidBrowserBookmarksRepository repo = new AndroidBrowserBookmarksRepository();
    wipe();

    BookmarkRecord folder1 = BookmarkHelpers.createFolder1();
    BookmarkRecord folder2 = BookmarkHelpers.createFolder2(); // child of folder1
    BookmarkRecord folder3 = BookmarkHelpers.createFolder3(); // child of folder2
    BookmarkRecord bmk1 = BookmarkHelpers.createBookmark1();  // child of folder1
    BookmarkRecord bmk2 = BookmarkHelpers.createBookmark2();  // child of folder1
    BookmarkRecord bmk3 = BookmarkHelpers.createBookmark3();  // child of folder2
    BookmarkRecord bmk4 = BookmarkHelpers.createBookmark4();  // child of folder3

    BookmarkRecord[] records = new BookmarkRecord[] {
        folder1, folder2, folder3,
        bmk1, bmk4
    };
    storeRecordsInSession(repo, records, null);

    assertChildrenAreUnordered(repo, folder1.guid, new Record[] { bmk1, folder2 });
    assertChildrenAreUnordered(repo, folder2.guid, new Record[] { folder3 });
    assertChildrenAreUnordered(repo, folder3.guid, new Record[] { bmk4 });

    // Replace folder3 with a record with a new GUID, and add bmk4 as folder3's child.
    final long now = System.currentTimeMillis();
    folder3.guid = Utils.generateGuid();
    folder3.lastModified = now;
    bmk4.title = bmk4.title + "/NEW";
    bmk4.parentID = folder3.guid; // Incoming child knows its parent.
    bmk4.parentName = folder3.title;
    bmk4.lastModified = now;

    // Order of store should not matter.
    ArrayList<BookmarkRecord> changedRecords = new ArrayList<BookmarkRecord>();
    changedRecords.add(bmk2); changedRecords.add(bmk3); changedRecords.add(bmk4); changedRecords.add(folder3);
    Collections.shuffle(changedRecords);
    storeRecordsInSession(repo, changedRecords.toArray(new BookmarkRecord[changedRecords.size()]), null);

    assertChildrenAreUnordered(repo, folder1.guid, new Record[] { bmk1, bmk2, folder2 });
    assertChildrenAreUnordered(repo, folder2.guid, new Record[] { bmk3, folder3 });
    assertChildrenAreUnordered(repo, folder3.guid, new Record[] { bmk4 });

    assertNotNull(fetchGUID(repo, folder3.guid));
    assertEquals(bmk4.title, fetchGUID(repo, bmk4.guid).title);
  }

  /**
   * A test where we expect to replace a local folder with a new folder (with a
   * new title but the same GUID), whilst adding children to it. Verifies that
   * replace and insert co-operate.
   */
  public void testInsertAndReplaceTitle() {
    AndroidBrowserBookmarksRepository repo = new AndroidBrowserBookmarksRepository();
    wipe();

    BookmarkRecord folder1 = BookmarkHelpers.createFolder1();
    BookmarkRecord folder2 = BookmarkHelpers.createFolder2(); // child of folder1
    BookmarkRecord folder3 = BookmarkHelpers.createFolder3(); // child of folder2
    BookmarkRecord bmk1 = BookmarkHelpers.createBookmark1();  // child of folder1
    BookmarkRecord bmk2 = BookmarkHelpers.createBookmark2();  // child of folder1
    BookmarkRecord bmk3 = BookmarkHelpers.createBookmark3();  // child of folder2
    BookmarkRecord bmk4 = BookmarkHelpers.createBookmark4();  // child of folder3

    BookmarkRecord[] records = new BookmarkRecord[] {
        folder1, folder2, folder3,
        bmk1, bmk4
    };
    storeRecordsInSession(repo, records, null);

    assertChildrenAreUnordered(repo, folder1.guid, new Record[] { bmk1, folder2 });
    assertChildrenAreUnordered(repo, folder2.guid, new Record[] { folder3 });
    assertChildrenAreUnordered(repo, folder3.guid, new Record[] { bmk4 });

    // Rename folder1, and add bmk2 as folder1's child.
    final long now = System.currentTimeMillis();
    folder1.title = folder1.title + "/NEW";
    folder1.lastModified = now;
    bmk2.title = bmk2.title + "/NEW";
    bmk2.parentID = folder1.guid; // Incoming child knows its parent.
    bmk2.parentName = folder1.title;
    bmk2.lastModified = now;

    // Order of store should not matter.
    ArrayList<BookmarkRecord> changedRecords = new ArrayList<BookmarkRecord>();
    changedRecords.add(bmk2); changedRecords.add(bmk3); changedRecords.add(folder1);
    Collections.shuffle(changedRecords);
    storeRecordsInSession(repo, changedRecords.toArray(new BookmarkRecord[changedRecords.size()]), null);

    assertChildrenAreUnordered(repo, folder1.guid, new Record[] { bmk1, bmk2, folder2 });
    assertChildrenAreUnordered(repo, folder2.guid, new Record[] { bmk3, folder3 });
    assertChildrenAreUnordered(repo, folder3.guid, new Record[] { bmk4 });

    assertEquals(folder1.title, fetchGUID(repo, folder1.guid).title);
    assertEquals(bmk2.title, fetchGUID(repo, bmk2.guid).title);
  }

  /**
   * Create and begin a new session, handing control to the delegate when started.
   * Returns when the delegate has notified.
   */
  public void inBegunSession(final AndroidBrowserBookmarksRepository repo,
                             final RepositorySessionBeginDelegate beginDelegate) {
    Runnable go = new Runnable() {
      @Override
      public void run() {
        RepositorySessionCreationDelegate delegate = new SimpleSuccessCreationDelegate() {
          @Override
          public void onSessionCreated(final RepositorySession session) {
            try {
              session.begin(beginDelegate);
            } catch (InvalidSessionTransitionException e) {
              performNotify(e);
            }
          }
        };
        repo.createSession(delegate, getApplicationContext());
      }
    };
    performWait(go);
  }

  /**
   * Finish the provided session, notifying on success.
   *
   * @param session
   */
  public void finishAndNotify(final RepositorySession session) {
    try {
      session.finish(new SimpleSuccessFinishDelegate() {
        @Override
        public void onFinishSucceeded(RepositorySession session,
                                      RepositorySessionBundle bundle) {
          performNotify();
        }
      });
    } catch (InactiveSessionException e) {
      performNotify(e);
    }
  }

  /**
   * Simple helper class for fetching all records.
   * The fetched records' GUIDs are stored in `fetchedGUIDs`.
   */
  public class SimpleFetchAllBeginDelegate extends SimpleSuccessBeginDelegate {
    public final ArrayList<String> fetchedGUIDs = new ArrayList<String>();

    @Override
    public void onBeginSucceeded(final RepositorySession session) {
      RepositorySessionFetchRecordsDelegate fetchDelegate = new SimpleSuccessFetchDelegate() {

        @Override
        public void onFetchedRecord(Record record) {
          fetchedGUIDs.add(record.guid);
        }

        @Override
        public void onFetchCompleted(long end) {
          finishAndNotify(session);
        }
      };
      session.fetchSince(0, fetchDelegate);
    }
  }

  /**
   * Simple helper class for fetching a single record by GUID.
   * The fetched record is stored in `fetchedRecord`.
   */
  public class SimpleFetchOneBeginDelegate extends SimpleSuccessBeginDelegate {
    public final String guid;
    public Record fetchedRecord = null;

    public SimpleFetchOneBeginDelegate(String guid) {
      this.guid = guid;
    }

    @Override
    public void onBeginSucceeded(final RepositorySession session) {
      RepositorySessionFetchRecordsDelegate fetchDelegate = new SimpleSuccessFetchDelegate() {

        @Override
        public void onFetchedRecord(Record record) {
          fetchedRecord = record;
        }

        @Override
        public void onFetchCompleted(long end) {
          finishAndNotify(session);
        }
      };
      try {
        session.fetch(new String[] { guid }, fetchDelegate);
      } catch (InactiveSessionException e) {
        performNotify("Session is inactive.", e);
      }
    }
  }

  /**
   * Create a new session for the given repository, storing each record
   * from the provided array. Notifies on failure or success.
   *
   * Optionally populates a provided Collection with tracked items.
   * @param repo
   * @param records
   * @param tracked
   */
  public void storeRecordsInSession(AndroidBrowserBookmarksRepository repo,
                                    final BookmarkRecord[] records,
                                    final Collection<String> tracked) {
    SimpleSuccessBeginDelegate beginDelegate = new SimpleSuccessBeginDelegate() {
      @Override
      public void onBeginSucceeded(final RepositorySession session) {
        RepositorySessionStoreDelegate storeDelegate = new SimpleSuccessStoreDelegate() {

          @Override
          public void onStoreCompleted(final long storeEnd) {
            // Pass back whatever we tracked.
            if (tracked != null) {
              Iterator<String> iter = session.getTrackedRecordIDs();
              while (iter.hasNext()) {
                tracked.add(iter.next());
              }
            }
            finishAndNotify(session);
          }

          @Override
          public void onRecordStoreSucceeded(String guid) {
          }
        };
        session.setStoreDelegate(storeDelegate);
        for (BookmarkRecord record : records) {
          try {
            session.store(record);
          } catch (NoStoreDelegateException e) {
            // Never happens.
          }
        }
        session.storeDone();
      }
    };
    inBegunSession(repo, beginDelegate);
  }

  public ArrayList<String> fetchGUIDs(AndroidBrowserBookmarksRepository repo) {
    SimpleFetchAllBeginDelegate beginDelegate = new SimpleFetchAllBeginDelegate();
    inBegunSession(repo, beginDelegate);
    return beginDelegate.fetchedGUIDs;
  }

  public BookmarkRecord fetchGUID(AndroidBrowserBookmarksRepository repo,
                                  final String guid) {
    Logger.info(LOG_TAG, "Fetching for " + guid);
    SimpleFetchOneBeginDelegate beginDelegate = new SimpleFetchOneBeginDelegate(guid);
    inBegunSession(repo, beginDelegate);
    Logger.info(LOG_TAG, "Fetched " + beginDelegate.fetchedRecord);
    assertTrue(beginDelegate.fetchedRecord != null);
    return (BookmarkRecord) beginDelegate.fetchedRecord;
  }

  public JSONArray fetchChildrenForGUID(AndroidBrowserBookmarksRepository repo,
      final String guid) {
    return fetchGUID(repo, guid).children;
  }

  @SuppressWarnings("unchecked")
  protected static JSONArray childrenFromRecords(BookmarkRecord... records) {
    JSONArray children = new JSONArray();
    for (BookmarkRecord record : records) {
      children.add(record.guid);
    }
    return children;
  }


  protected void updateRow(ContentValues values) {
    Uri uri = BrowserContractHelpers.BOOKMARKS_CONTENT_URI;
    final String where = BrowserContract.Bookmarks.GUID + " = ?";
    final String[] args = new String[] { values.getAsString(BrowserContract.Bookmarks.GUID) };
    getApplicationContext().getContentResolver().update(uri, values, where, args);
  }

  protected Uri insertRow(ContentValues values) {
    Uri uri = BrowserContractHelpers.BOOKMARKS_CONTENT_URI;
    return getApplicationContext().getContentResolver().insert(uri, values);
  }

  protected static ContentValues specialFolder() {
    ContentValues values = new ContentValues();

    final long now = System.currentTimeMillis();
    values.put(Bookmarks.DATE_CREATED, now);
    values.put(Bookmarks.DATE_MODIFIED, now);
    values.put(Bookmarks.TYPE, BrowserContract.Bookmarks.TYPE_FOLDER);

    return values;
  }

  protected static ContentValues fennecMobileRecordWithoutTitle() {
    ContentValues values = specialFolder();
    values.put(BrowserContract.SyncColumns.GUID, "mobile");
    values.putNull(BrowserContract.Bookmarks.TITLE);

    return values;
  }

  protected ContentValues fennecPinnedItemsRecord() {
    final ContentValues values = specialFolder();
    final String title = getApplicationContext().getResources().getString(R.string.bookmarks_folder_pinned);

    values.put(BrowserContract.SyncColumns.GUID, Bookmarks.PINNED_FOLDER_GUID);
    values.put(Bookmarks._ID, Bookmarks.FIXED_PINNED_LIST_ID);
    values.put(Bookmarks.PARENT, Bookmarks.FIXED_ROOT_ID);
    values.put(Bookmarks.TITLE, title);
    return values;
  }

  protected static ContentValues fennecPinnedChildItemRecord() {
    ContentValues values = new ContentValues();

    final long now = System.currentTimeMillis();

    values.put(BrowserContract.SyncColumns.GUID, "dapinneditem");
    values.put(Bookmarks.DATE_CREATED, now);
    values.put(Bookmarks.DATE_MODIFIED, now);
    values.put(Bookmarks.TYPE, BrowserContract.Bookmarks.TYPE_BOOKMARK);
    values.put(Bookmarks.URL, "user-entered:foobar");
    values.put(Bookmarks.PARENT, Bookmarks.FIXED_PINNED_LIST_ID);
    values.put(Bookmarks.TITLE, "Foobar");
    return values;
  }

  protected long setUpFennecMobileRecordWithoutTitle() {
    ContentResolver cr = getApplicationContext().getContentResolver();
    ContentValues values = fennecMobileRecordWithoutTitle();
    updateRow(values);
    return fennecGetMobileBookmarksFolderId(cr);
  }

  protected long setUpFennecMobileRecord() {
    ContentResolver cr = getApplicationContext().getContentResolver();
    ContentValues values = fennecMobileRecordWithoutTitle();
    values.put(BrowserContract.Bookmarks.PARENT, BrowserContract.Bookmarks.FIXED_ROOT_ID);
    String title = getApplicationContext().getResources().getString(R.string.bookmarks_folder_mobile);
    values.put(BrowserContract.Bookmarks.TITLE, title);
    updateRow(values);
    return fennecGetMobileBookmarksFolderId(cr);
  }

  protected void setUpFennecPinnedItemsRecord() {
    insertRow(fennecPinnedItemsRecord());
    insertRow(fennecPinnedChildItemRecord());
  }

  //
  // Fennec fake layer.
  //
  private Uri appendProfile(Uri uri) {
    final String defaultProfile = "default";     // Fennec constant removed in Bug 715307.
    return uri.buildUpon().appendQueryParameter(BrowserContract.PARAM_PROFILE, defaultProfile).build();
  }

  private long fennecGetFolderId(ContentResolver cr, String guid) {
    Cursor c = null;
    try {
      c = cr.query(appendProfile(BrowserContractHelpers.BOOKMARKS_CONTENT_URI),
          new String[] { BrowserContract.Bookmarks._ID },
          BrowserContract.Bookmarks.GUID + " = ?",
          new String[] { guid },
          null);

      if (c.moveToFirst()) {
        return c.getLong(c.getColumnIndexOrThrow(BrowserContract.Bookmarks._ID));
      }
      return -1;
    } finally {
      if (c != null) {
        c.close();
      }
    }
  }

  private long fennecGetMobileBookmarksFolderId(ContentResolver cr) {
    return fennecGetFolderId(cr, BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);
  }

  public void fennecAddBookmark(String title, String uri) {
    ContentResolver cr = getApplicationContext().getContentResolver();

    long folderId = fennecGetMobileBookmarksFolderId(cr);
    if (folderId < 0) {
      return;
    }

    ContentValues values = new ContentValues();
    values.put(BrowserContract.Bookmarks.TITLE, title);
    values.put(BrowserContract.Bookmarks.URL, uri);
    values.put(BrowserContract.Bookmarks.PARENT, folderId);

    // Restore deleted record if possible
    values.put(BrowserContract.Bookmarks.IS_DELETED, 0);

    Logger.debug(getName(), "Adding bookmark " + title + ", " + uri + " in " + folderId);
    int updated = cr.update(appendProfile(BrowserContractHelpers.BOOKMARKS_CONTENT_URI),
        values,
        BrowserContract.Bookmarks.URL + " = ?",
            new String[] { uri });

    if (updated == 0) {
      Uri insert = cr.insert(appendProfile(BrowserContractHelpers.BOOKMARKS_CONTENT_URI), values);
      long idFromUri = ContentUris.parseId(insert);
      Logger.debug(getName(), "Inserted " + uri + " as " + idFromUri);
      Logger.debug(getName(), "Position is " + getPosition(idFromUri));
    }
  }

  private long getPosition(long idFromUri) {
    ContentResolver cr = getApplicationContext().getContentResolver();
    Cursor c = cr.query(appendProfile(BrowserContractHelpers.BOOKMARKS_CONTENT_URI),
                        new String[] { BrowserContract.Bookmarks.POSITION },
                        BrowserContract.Bookmarks._ID + " = ?",
                        new String[] { String.valueOf(idFromUri) },
                        null);
    if (!c.moveToFirst()) {
      return -2;
    }
    return c.getLong(0);
  }

  protected AndroidBrowserBookmarksDataAccessor dataAccessor = null;
  protected AndroidBrowserBookmarksDataAccessor getDataAccessor() {
    if (dataAccessor == null) {
      dataAccessor = new AndroidBrowserBookmarksDataAccessor(getApplicationContext());
    }
    return dataAccessor;
  }

  protected void wipe() {
    Logger.debug(getName(), "Wiping.");
    getDataAccessor().wipe();
  }

  protected void assertChildrenAreOrdered(AndroidBrowserBookmarksRepository repo, String guid, Record[] expected) {
    Logger.debug(getName(), "Fetching children...");
    JSONArray folderChildren = fetchChildrenForGUID(repo, guid);

    assertTrue(folderChildren != null);
    Logger.debug(getName(), "Children are " + folderChildren.toJSONString());
    assertEquals(expected.length, folderChildren.size());
    for (int i = 0; i < expected.length; ++i) {
      assertEquals(expected[i].guid, ((String) folderChildren.get(i)));
    }
  }

  protected void assertChildrenAreUnordered(AndroidBrowserBookmarksRepository repo, String guid, Record[] expected) {
    Logger.debug(getName(), "Fetching children...");
    JSONArray folderChildren = fetchChildrenForGUID(repo, guid);

    assertTrue(folderChildren != null);
    Logger.debug(getName(), "Children are " + folderChildren.toJSONString());
    assertEquals(expected.length, folderChildren.size());
    for (Record record : expected) {
      folderChildren.contains(record.guid);
    }
  }

  /**
   * Return a sequence of children GUIDs for the provided folder ID.
   */
  protected ArrayList<String> fetchChildrenDirect(long id) {
    Logger.debug(getName(), "Fetching children directly from DB...");
    final ArrayList<String> out = new ArrayList<String>();
    final AndroidBrowserBookmarksDataAccessor accessor = new AndroidBrowserBookmarksDataAccessor(getApplicationContext());
    Cursor cur = null;
    try {
      cur = accessor.getChildren(id);
    } catch (NullCursorException e) {
      fail("Got null cursor.");
    }
    try {
      if (!cur.moveToFirst()) {
        return out;
      }
      final int guidCol = cur.getColumnIndex(BrowserContract.SyncColumns.GUID);
      while (!cur.isAfterLast()) {
        out.add(cur.getString(guidCol));
        cur.moveToNext();
      }
    } finally {
      cur.close();
    }
    return out;
  }

  /**
   * Assert that the children of the provided ID are correct and positioned in the database.
   * @param id
   * @param guids
   */
  protected void assertChildrenAreDirect(long id, String[] guids) {
    Logger.debug(getName(), "Fetching children directly from DB...");
    AndroidBrowserBookmarksDataAccessor accessor = new AndroidBrowserBookmarksDataAccessor(getApplicationContext());
    Cursor cur = null;
    try {
      cur = accessor.getChildren(id);
    } catch (NullCursorException e) {
      fail("Got null cursor.");
    }
    try {
      if (guids == null || guids.length == 0) {
        assertFalse(cur.moveToFirst());
        return;
      }

      assertTrue(cur.moveToFirst());
      int i = 0;
      final int guidCol = cur.getColumnIndex(BrowserContract.SyncColumns.GUID);
      final int posCol = cur.getColumnIndex(BrowserContract.Bookmarks.POSITION);
      while (!cur.isAfterLast()) {
        assertTrue(i < guids.length);
        final String guid = cur.getString(guidCol);
        final int pos = cur.getInt(posCol);
        Logger.debug(getName(), "Fetched child: " + guid + " has position " + pos);
        assertEquals(guids[i], guid);
        assertEquals(i,        pos);

        ++i;
        cur.moveToNext();
      }
      assertEquals(guids.length, i);
    } finally {
      cur.close();
    }
  }
}

/**
TODO

Test for storing a record that will reconcile to mobile; postcondition is
that there's still a directory called mobile that includes all the items that
it used to.

mobile folder created without title.
Unsorted put in mobile???
Tests for children retrieval
Tests for children merge
Tests for modify retrieve parent when child added, removed, reordered (oh, reorder is hard! Any change, then.)
Safety mode?
Test storing folder first, contents first.
Store folder in next session. Verify order recovery.


*/
