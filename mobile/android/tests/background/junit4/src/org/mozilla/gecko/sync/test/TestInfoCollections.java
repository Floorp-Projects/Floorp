/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.InfoCollections;
import org.mozilla.gecko.sync.InfoCounts;
import org.mozilla.gecko.sync.Utils;

/**
 * Test both info/collections and info/collection_counts.
 */
public class TestInfoCollections {
  public static final String TEST_COLLECTIONS_JSON =
      "{\"history\":1.3319567131E9, "    +
      " \"bookmarks\":1.33195669592E9, " +
      " \"prefs\":1.33115408641E9, "     +
      " \"crypto\":1.32046063664E9, "    +
      " \"meta\":1.321E9, "              +
      " \"forms\":1.33136685374E9, "     +
      " \"clients\":1.3313667619E9, "    +
      " \"tabs\":1.35E9"                 +
      "}";


  public static final String TEST_COUNTS_JSON =
      "{\"passwords\": 390, " +
      " \"clients\": 2, "     +
      " \"crypto\": 1, "      +
      " \"forms\": 1019, "    +
      " \"bookmarks\": 766, " +
      " \"prefs\": 1, "       +
      " \"history\": 9278"    +
      "}";

  @SuppressWarnings("static-method")
  @Test
  public void testSetCountsFromRecord() throws Exception {
    InfoCounts infoCountsEmpty = new InfoCounts(new ExtendedJSONObject("{}"));
    assertEquals(null, infoCountsEmpty.getCount("bookmarks"));

    ExtendedJSONObject record = ExtendedJSONObject.parseJSONObject(TEST_COUNTS_JSON);
    InfoCounts infoCountsFull = new InfoCounts(record);
    assertEquals(Integer.valueOf(766), infoCountsFull.getCount("bookmarks"));
    assertEquals(null, infoCountsFull.getCount("notpresent"));
  }


  @SuppressWarnings("static-method")
  @Test
  public void testSetCollectionsFromRecord() throws Exception {
    ExtendedJSONObject record = ExtendedJSONObject.parseJSONObject(TEST_COLLECTIONS_JSON);
    InfoCollections infoCollections = new InfoCollections(record);

    assertEquals(Utils.decimalSecondsToMilliseconds(1.3319567131E9), infoCollections.getTimestamp("history").longValue());
    assertEquals(Utils.decimalSecondsToMilliseconds(1.321E9), infoCollections.getTimestamp("meta").longValue());
    assertEquals(Utils.decimalSecondsToMilliseconds(1.35E9), infoCollections.getTimestamp("tabs").longValue());
    assertNull(infoCollections.getTimestamp("missing"));
  }

  @SuppressWarnings("static-method")
  @Test
  public void testUpdateNeeded() throws Exception {
    ExtendedJSONObject record = ExtendedJSONObject.parseJSONObject(TEST_COLLECTIONS_JSON);
    InfoCollections infoCollections = new InfoCollections(record);

    long none = -1;
    long past = Utils.decimalSecondsToMilliseconds(1.3E9);
    long same = Utils.decimalSecondsToMilliseconds(1.35E9);
    long future = Utils.decimalSecondsToMilliseconds(1.4E9);


    // Test with no local timestamp set.
    assertTrue(infoCollections.updateNeeded("tabs", none));

    // Test with local timestamp set in the past.
    assertTrue(infoCollections.updateNeeded("tabs", past));

    // Test with same timestamp.
    assertFalse(infoCollections.updateNeeded("tabs", same));

    // Test with local timestamp set in the future.
    assertFalse(infoCollections.updateNeeded("tabs", future));

    // Test with no collection.
    assertTrue(infoCollections.updateNeeded("missing", none));
    assertTrue(infoCollections.updateNeeded("missing", past));
    assertTrue(infoCollections.updateNeeded("missing", same));
    assertTrue(infoCollections.updateNeeded("missing", future));
  }
}
