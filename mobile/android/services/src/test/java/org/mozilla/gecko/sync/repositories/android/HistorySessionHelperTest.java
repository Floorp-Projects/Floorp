/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.repositories.android;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.domain.HistoryRecord;

import static org.junit.Assert.*;

@RunWith(TestRunner.class)
public class HistorySessionHelperTest {
    @Test
    public void testShouldIgnore() throws Exception {
        final HistoryRecord aboutURL = new HistoryRecord(Utils.generateGuid(), "history", System.currentTimeMillis(), false);

        // And we'd ignore about:home if we downloaded it.
        assertTrue(HistorySessionHelper.shouldIgnoreStatic(aboutURL));
    }

    @Test
    public void testBuildRecordString() throws Exception {
        final long now = RepositorySession.now();

        final HistoryRecord record0 = new HistoryRecord(null, "history", now + 1, false);
        final HistoryRecord record1 = new HistoryRecord(null, "history", now + 2, false);
        final HistoryRecord record2 = new HistoryRecord(null, "history", now + 3, false);

        record0.histURI = "http://example.com/foo";
        record1.histURI = "http://example.com/foo";
        record2.histURI = "http://example.com/bar";
        record0.title = "Foo 0";
        record1.title = "Foo 1";
        record2.title = "Foo 2";

        // Ensure that two records with the same URI produce the same record string,
        // and two records with different URIs do not.
        assertEquals(
                HistorySessionHelper.buildRecordStringStatic(record0),
                HistorySessionHelper.buildRecordStringStatic(record1)
        );
        assertNotEquals(
                HistorySessionHelper.buildRecordStringStatic(record0),
                HistorySessionHelper.buildRecordStringStatic(record2)
        );
    }
}