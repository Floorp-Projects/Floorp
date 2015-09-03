/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.repositories.android.test;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.Set;

import org.junit.Before;
import org.junit.Test;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.android.BookmarksInsertionManager;
import org.mozilla.gecko.sync.repositories.domain.BookmarkRecord;

public class TestBookmarksInsertionManager {
  public BookmarksInsertionManager manager;
  public ArrayList<String[]> insertions;

  @Before
  public void setUp() {
    insertions = new ArrayList<String[]>();
    Set<String> writtenFolders = new HashSet<String>();
    writtenFolders.add("mobile");

    BookmarksInsertionManager.BookmarkInserter inserter = new BookmarksInsertionManager.BookmarkInserter() {
      @Override
      public boolean insertFolder(BookmarkRecord record) {
        if (record.guid == "fail") {
          return false;
        }
        Logger.debug(BookmarksInsertionManager.LOG_TAG, "Inserted folder (" + record.guid + ").");
        insertions.add(new String[] { record.guid });
        return true;
      }

      @Override
      public void bulkInsertNonFolders(Collection<BookmarkRecord> records) {
        ArrayList<String> guids = new ArrayList<String>();
        for (BookmarkRecord record : records) {
          guids.add(record.guid);
        }
        String[] guidList = guids.toArray(new String[guids.size()]);
        insertions.add(guidList);
        Logger.debug(BookmarksInsertionManager.LOG_TAG, "Inserted non-folders (" + Utils.toCommaSeparatedString(guids) + ").");
      }
    };
    manager = new BookmarksInsertionManager(3, writtenFolders, inserter);
    BookmarksInsertionManager.DEBUG = true;
  }

  protected static BookmarkRecord bookmark(String guid, String parent) {
    BookmarkRecord bookmark = new BookmarkRecord(guid);
    bookmark.type = "bookmark";
    bookmark.parentID = parent;
    return bookmark;
  }

  protected static BookmarkRecord folder(String guid, String parent) {
    BookmarkRecord bookmark = new BookmarkRecord(guid);
    bookmark.type = "folder";
    bookmark.parentID = parent;
    return bookmark;
  }

  @Test
  public void testChildrenBeforeFolder() {
    BookmarkRecord folder = folder("folder", "mobile");
    BookmarkRecord child1 = bookmark("child1", "folder");
    BookmarkRecord child2 = bookmark("child2", "folder");

    manager.enqueueRecord(child1);
    assertTrue(insertions.isEmpty());
    manager.enqueueRecord(child2);
    assertTrue(insertions.isEmpty());
    manager.enqueueRecord(folder);
    assertEquals(1, insertions.size());
    manager.finishUp();
    assertTrue(manager.isClear());
    assertEquals(2, insertions.size());
    assertArrayEquals(new String[] { "folder" }, insertions.get(0));
    assertArrayEquals(new String[] { "child1", "child2" }, insertions.get(1));
  }

  @Test
  public void testChildAfterFolder() {
    BookmarkRecord folder = folder("folder", "mobile");
    BookmarkRecord child1 = bookmark("child1", "folder");
    BookmarkRecord child2 = bookmark("child2", "folder");

    manager.enqueueRecord(child1);
    assertTrue(insertions.isEmpty());
    manager.enqueueRecord(folder);
    assertEquals(1, insertions.size());
    manager.enqueueRecord(child2);
    assertEquals(1, insertions.size());
    manager.finishUp();
    assertTrue(manager.isClear());
    assertEquals(2, insertions.size());
    assertArrayEquals(new String[] { "folder" }, insertions.get(0));
    assertArrayEquals(new String[] { "child1", "child2" }, insertions.get(1));
  }

  @Test
  public void testFolderAfterFolder() {
    manager.enqueueRecord(bookmark("child1", "folder1"));
    assertEquals(0, insertions.size());
    manager.enqueueRecord(folder("folder1", "mobile"));
    assertEquals(1, insertions.size());
    manager.enqueueRecord(bookmark("child2", "folder2"));
    assertEquals(1, insertions.size());
    manager.enqueueRecord(folder("folder2", "folder1"));
    assertEquals(2, insertions.size());
    manager.enqueueRecord(bookmark("child3", "folder1"));
    manager.enqueueRecord(bookmark("child4", "folder2"));
    assertEquals(3, insertions.size());

    manager.finishUp();
    assertTrue(manager.isClear());
    assertEquals(4, insertions.size());
    assertArrayEquals(new String[] { "folder1" }, insertions.get(0));
    assertArrayEquals(new String[] { "folder2" }, insertions.get(1));
    assertArrayEquals(new String[] { "child1", "child2", "child3" }, insertions.get(2));
    assertArrayEquals(new String[] { "child4" }, insertions.get(3));
  }

  @Test
  public void testFolderRecursion() {
    manager.enqueueRecord(folder("1", "mobile"));
    manager.enqueueRecord(folder("2", "1"));
    assertEquals(2, insertions.size());
    manager.enqueueRecord(bookmark("3a", "3"));
    manager.enqueueRecord(bookmark("3b", "3"));
    manager.enqueueRecord(bookmark("3c", "3"));
    manager.enqueueRecord(bookmark("4a", "4"));
    manager.enqueueRecord(bookmark("4b", "4"));
    manager.enqueueRecord(bookmark("4c", "4"));
    assertEquals(2, insertions.size());
    manager.enqueueRecord(folder("3", "2"));
    assertEquals(4, insertions.size());
    manager.enqueueRecord(folder("4", "2"));
    assertEquals(6, insertions.size());

    assertTrue(manager.isClear());
    manager.finishUp();
    assertTrue(manager.isClear());
    // Folders in order.
    assertArrayEquals(new String[] { "1" }, insertions.get(0));
    assertArrayEquals(new String[] { "2" }, insertions.get(1));
    assertArrayEquals(new String[] { "3" }, insertions.get(2));
    // Then children in batches of 3.
    assertArrayEquals(new String[] { "3a", "3b", "3c" }, insertions.get(3));
    // Then last folder.
    assertArrayEquals(new String[] { "4" }, insertions.get(4));
    assertArrayEquals(new String[] { "4a", "4b", "4c" }, insertions.get(5));
  }

  @Test
  public void testFailedFolderInsertion() {
    manager.enqueueRecord(bookmark("failA", "fail"));
    manager.enqueueRecord(bookmark("failB", "fail"));
    assertEquals(0, insertions.size());
    manager.enqueueRecord(folder("fail", "mobile"));
    assertEquals(0, insertions.size());
    manager.enqueueRecord(bookmark("failC", "fail"));
    assertEquals(0, insertions.size());
    manager.finishUp(); // Children inserted at the end; they will be treated as orphans.
    assertTrue(manager.isClear());
    assertEquals(1, insertions.size());
    assertArrayEquals(new String[] { "failA", "failB", "failC" }, insertions.get(0));
  }

  @Test
  public void testIncrementalFlush() {
    manager.enqueueRecord(bookmark("a", "1"));
    manager.enqueueRecord(bookmark("b", "1"));
    manager.enqueueRecord(folder("1", "mobile"));
    assertEquals(1, insertions.size());
    manager.enqueueRecord(bookmark("c", "1"));
    assertEquals(2, insertions.size());
    manager.enqueueRecord(bookmark("d", "1"));
    manager.enqueueRecord(bookmark("e", "1"));
    manager.enqueueRecord(bookmark("f", "1"));
    assertEquals(3, insertions.size());
    manager.enqueueRecord(bookmark("g", "1")); // Start of new batch.
    assertEquals(3, insertions.size());
    manager.finishUp(); // Children inserted at the end; they will be treated as orphans.
    assertTrue(manager.isClear());
    assertEquals(4, insertions.size());
    assertArrayEquals(new String[] { "1" }, insertions.get(0));
    assertArrayEquals(new String[] { "a", "b", "c"}, insertions.get(1));
    assertArrayEquals(new String[] { "d", "e", "f"}, insertions.get(2));
    assertArrayEquals(new String[] { "g" }, insertions.get(3));
  }

  @Test
  public void testFinishUp() {
    manager.enqueueRecord(bookmark("a", "1"));
    manager.enqueueRecord(bookmark("b", "1"));
    manager.enqueueRecord(folder("2", "1"));
    manager.enqueueRecord(bookmark("c", "1"));
    manager.enqueueRecord(bookmark("d", "1"));
    manager.enqueueRecord(folder("3", "1"));
    assertEquals(0, insertions.size());
    manager.finishUp(); // Children inserted at the end; they will be treated as orphans.
    assertTrue(manager.isClear());
    assertEquals(3, insertions.size());
    assertArrayEquals(new String[] { "2" }, insertions.get(0));
    assertArrayEquals(new String[] { "3" }, insertions.get(1));
    assertArrayEquals(new String[] { "a", "b", "c", "d" }, insertions.get(2)); // Last insertion could be big.
  }
}
