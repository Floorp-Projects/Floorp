/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.db;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import static org.junit.Assert.*;

@RunWith(TestRunner.class)
public class BrowserContractTest {
    @Test
    /**
     * Test that bookmark and sorting order clauses are set correctly
     */
    public void testGetCombinedFrecencySortOrder() throws Exception {
        String sqlNoBookmarksDesc = BrowserContract.getCombinedFrecencySortOrder(false, false);
        String sqlNoBookmarksAsc = BrowserContract.getCombinedFrecencySortOrder(false, true);
        String sqlBookmarksDesc = BrowserContract.getCombinedFrecencySortOrder(true, false);
        String sqlBookmarksAsc = BrowserContract.getCombinedFrecencySortOrder(true, true);

        assertTrue(sqlBookmarksAsc.endsWith(" ASC"));
        assertTrue(sqlBookmarksDesc.endsWith(" DESC"));
        assertTrue(sqlNoBookmarksAsc.endsWith(" ASC"));
        assertTrue(sqlNoBookmarksDesc.endsWith(" DESC"));

        assertTrue(sqlBookmarksAsc.startsWith("(CASE WHEN bookmark_id > -1 THEN 100 ELSE 0 END) + "));
        assertTrue(sqlBookmarksDesc.startsWith("(CASE WHEN bookmark_id > -1 THEN 100 ELSE 0 END) + "));
    }

    @Test
    /**
     * Test that calculation string is correct for remote visits
     * maxFrecency=1, scaleConst=110, correct sql params for visit count and last date
     * and that time is converted to microseconds.
     */
    public void testGetRemoteFrecencySQL() throws Exception {
        long now = 1;
        String sql = BrowserContract.getRemoteFrecencySQL(now);
        String ageExpr = "(" + now * 1000 + " - remoteDateLastVisited) / 86400000000";

        assertEquals(
                "remoteVisitCount * MAX(1, 100 * 110 / (" + ageExpr + " * " + ageExpr + " + 110))",
                sql
        );
    }

    @Test
    /**
     * Test that calculation string is correct for remote visits
     * maxFrecency=2, scaleConst=225, correct sql params for visit count and last date
     * and that time is converted to microseconds.
     */
    public void testGetLocalFrecencySQL() throws Exception {
        long now = 1;
        String sql = BrowserContract.getLocalFrecencySQL(now);
        String ageExpr = "(" + now * 1000 + " - localDateLastVisited) / 86400000000";
        String visitCountExpr = "(localVisitCount + 2) * (localVisitCount + 2)";

        assertEquals(
                visitCountExpr + " * MAX(2, 100 * 225 / (" + ageExpr + " * " + ageExpr + " + 225))",
                sql
        );
    }
}