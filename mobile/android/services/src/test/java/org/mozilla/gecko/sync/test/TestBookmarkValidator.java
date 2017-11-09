/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
package org.mozilla.gecko.sync.test;

import org.json.simple.JSONArray;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.repositories.domain.BookmarkRecord;
import org.mozilla.gecko.sync.validation.BookmarkValidationResults;
import org.mozilla.gecko.sync.validation.BookmarkValidator;

import static org.junit.Assert.*;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

@RunWith(TestRunner.class)
public class TestBookmarkValidator {

    private List<BookmarkRecord> getDummyRecords() {
        List<BookmarkRecord> l = new ArrayList<>();
        {
            BookmarkRecord r = new BookmarkRecord();
            r.guid = "menu";
            r.parentID = "places";
            r.type = "folder";
            r.title = "foo";
            r.children = childrenFromGUIDs("aaaaaaaaaaaa", "bbbbbbbbbbbb", "cccccccccccc");
            l.add(r);
        }
        {
            BookmarkRecord r = new BookmarkRecord();
            r.guid = "aaaaaaaaaaaa";
            r.parentID = "menu";
            r.type = "folder";
            r.title = "stuff";
            r.children = new JSONArray();
            l.add(r);
        }
        {
            BookmarkRecord r = new BookmarkRecord();
            r.guid = "bbbbbbbbbbbb";
            r.parentID = "menu";
            r.type = "bookmark";
            r.title = "bar";
            r.bookmarkURI = "http://baz.com";
            l.add(r);
        }
        {
            BookmarkRecord r = new BookmarkRecord();
            r.guid = "cccccccccccc";
            r.parentID = "menu";
            r.type = "query";
            r.title = "";
            r.bookmarkURI = "place:type=6&sort=14&maxResults=10";
            l.add(r);
        }
        return l;
    }

    @Test
    public void testMatching() {
        List<BookmarkRecord> client = getDummyRecords();
        List<BookmarkRecord> server = getDummyRecords();
        BookmarkValidationResults v = BookmarkValidator.validateClientAgainstServer(client, server);
        assertFalse(v.anyProblemsExist());
    }

    @Test
    public void testClientMissing() {
        List<BookmarkRecord> client = getDummyRecords();
        List<BookmarkRecord> server = getDummyRecords();
        BookmarkRecord removed = client.remove(client.size() - 1);
        BookmarkValidationResults v = BookmarkValidator.validateClientAgainstServer(client, server);
        assertTrue(v.anyProblemsExist());
        assertEquals(v.clientMissing.size(), 1);
        assertTrue(v.clientMissing.contains(removed.guid));
    }

    @Test
    public void testServerMissing() {
        // Also tests some other server validation
        List<BookmarkRecord> client = getDummyRecords();
        List<BookmarkRecord> server = getDummyRecords();
        BookmarkRecord removed = server.remove(server.size() - 1);
        BookmarkValidationResults v = BookmarkValidator.validateClientAgainstServer(client, server);
        assertTrue(v.anyProblemsExist());
        assertEquals(v.serverMissing.size(), 1);
        assertTrue(v.serverMissing.contains(removed.guid));
        assertEquals(v.missingChildren.size(), 1);
        assertTrue(v.missingChildren.contains(
                new BookmarkValidationResults.ParentChildPair("menu", removed.guid)));
    }

    @Test
    public void testOrphans() {
        List<BookmarkRecord> server = getDummyRecords();
        BookmarkRecord last = server.get(server.size() - 1);
        last.parentID = "asdfasdfasdf";
        BookmarkValidationResults v = BookmarkValidator.validateServer(server);
        assertTrue(v.anyProblemsExist());

        assertEquals(v.orphans.size(), 1);
        assertTrue(v.orphans.contains(
                new BookmarkValidationResults.ParentChildPair("asdfasdfasdf", last.guid)));

        assertEquals(v.parentChildMismatches.size(), 1);
        assertTrue(v.parentChildMismatches.contains(
                new BookmarkValidationResults.ParentChildPair("menu", last.guid)));
    }

    @Test
    public void testServerDeleted() {
        List<BookmarkRecord> client = getDummyRecords();
        List<BookmarkRecord> server = getDummyRecords();
        BookmarkRecord removed = server.remove(server.size() - 1);
        server.add(createTombstone(removed.guid));
        BookmarkValidationResults v = BookmarkValidator.validateClientAgainstServer(client, server);
        assertTrue(v.anyProblemsExist());

        assertEquals(v.serverDeleted.size(), 1);
        assertTrue(v.serverDeleted.contains(removed.guid));
        assertEquals(v.serverMissing.size(), 0);

        assertEquals(v.deletedChildren.size(), 1);
        assertTrue(v.deletedChildren.contains(
                new BookmarkValidationResults.ParentChildPair(removed.parentID, removed.guid)));

        assertEquals(v.missingChildren.size(), 0);
    }

    @Test
    public void testMultipleParents() {
        List<BookmarkRecord> server = getDummyRecords();
        BookmarkRecord otherFolder = server.get(1);
        assertEquals(otherFolder.type, "folder");
        otherFolder.children = childrenFromGUIDs("bbbbbbbbbbbb");

        BookmarkValidationResults v = BookmarkValidator.validateServer(server);
        assertTrue(v.anyProblemsExist());

        assertEquals(v.parentChildMismatches.size(), 1);
        assertTrue(v.parentChildMismatches.contains(
                new BookmarkValidationResults.ParentChildPair(otherFolder.guid, "bbbbbbbbbbbb")));

        assertEquals(v.multipleParents.size(), 1);
        assertTrue(v.multipleParents.containsKey("bbbbbbbbbbbb"));
        List<String> parentGUIDs = v.multipleParents.get("bbbbbbbbbbbb");
        assertEquals(parentGUIDs.size(), 2);
        assertTrue(parentGUIDs.contains("menu"));
        assertTrue(parentGUIDs.contains("aaaaaaaaaaaa"));
    }

    private BookmarkRecord createTombstone(String guid) {
        return new BookmarkRecord(guid, BookmarkRecord.COLLECTION_NAME, 0, true);
    }

    @SuppressWarnings("unchecked")
    private JSONArray childrenFromGUIDs(String... guids) {
        JSONArray children = new JSONArray();
        children.addAll(Arrays.asList(guids));
        return children;
    }
}
