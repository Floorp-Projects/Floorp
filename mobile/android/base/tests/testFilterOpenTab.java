package org.mozilla.gecko.tests;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;

import org.mozilla.gecko.PrivateTab;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.TabsAccessor;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.TabsProvider;

import android.content.ContentProvider;
import android.content.Context;
import android.database.Cursor;

/**
 * Tests that local tabs are filtered prior to upload.
 * - create a set of tabs and persists them through TabsAccessor.
 * - verifies that tabs are filtered by querying.
 */
public class testFilterOpenTab extends ContentProviderTest {
    private static final String[] TABS_PROJECTION_COLUMNS = new String[] {
                                                                BrowserContract.Tabs.TITLE,
                                                                BrowserContract.Tabs.URL,
                                                                BrowserContract.Clients.GUID,
                                                                BrowserContract.Clients.NAME
                                                            };

    private static final String LOCAL_TABS_SELECTION = BrowserContract.Tabs.CLIENT_GUID + " IS NULL";

    /**
     * Factory function that makes new ContentProvider instances.
     * <p>
     * We want a fresh provider each test, so this should be invoked in
     * <code>setUp</code> before each individual test.
     */
    protected static Callable<ContentProvider> sTabProviderCallable = new Callable<ContentProvider>() {
        @Override
        public ContentProvider call() {
            return new TabsProvider();
        }
    };

    private Cursor getTabsFromLocalClient() throws Exception {
        return mProvider.query(BrowserContract.Tabs.CONTENT_URI,
                               TABS_PROJECTION_COLUMNS,
                               LOCAL_TABS_SELECTION,
                               null,
                               null);
    }

    private Tab createTab(int id, String url, boolean external, int parentId, String title) {
        return new Tab((Context) getActivity(), id, url, external, parentId, title);
    }

    private Tab createPrivateTab(int id, String url, boolean external, int parentId, String title) {
        return new PrivateTab((Context) getActivity(), id, url, external, parentId, title);
    }

    @Override
    public void setUp() throws Exception {
        super.setUp(sTabProviderCallable, BrowserContract.TABS_AUTHORITY, "tabs.db");
        mTests.add(new TestInsertLocalTabs());
    }

    public void testFilterOpenTab() throws Exception {
        for (int i = 0; i < mTests.size(); i++) {
            Runnable test = mTests.get(i);

            setTestName(test.getClass().getSimpleName());
            test.run();
        }
    }

    private class TestInsertLocalTabs extends TestCase  {
        @Override
        public void test() throws Exception {
            final String TITLE1 = "Google";
            final String URL1 = "http://www.google.com/";
            final String TITLE2 = "Mozilla Start Page";
            final String URL2 = "about:home";
            final String TITLE3 = "Chrome Weave URL";
            final String URL3 = "chrome://weave/";
            final String TITLE4 = "What You Cache Is What You Get";
            final String URL4 = "wyciwyg://1/test.com";
            final String TITLE5 = "Root Folder";
            final String URL5 = "file:///";

            // Create a list of local tabs.
            List<Tab> tabs = new ArrayList<Tab>(6);
            Tab tab1 = createTab(1, URL1, false, 0, TITLE1);
            Tab tab2 = createTab(2, URL2, false, 0, TITLE2);
            Tab tab3 = createTab(3, URL3, false, 0, TITLE3);
            Tab tab4 = createTab(4, URL4, false, 0, TITLE4);
            Tab tab5 = createTab(5, URL5, false, 0, TITLE5);
            Tab tab6 = createPrivateTab(6, URL1, false, 0, TITLE1);
            tabs.add(tab1);
            tabs.add(tab2);
            tabs.add(tab3);
            tabs.add(tab4);
            tabs.add(tab5);
            tabs.add(tab6);

            // Persist the created tabs.
            TabsAccessor.persistLocalTabs(mResolver, tabs);

            // Get the persisted tab and check if urls are filtered.
            Cursor c = getTabsFromLocalClient();
            assertCountIsAndClose(c, 1, 1 + " tabs entries found");
        }
    }

    /**
     * Assert that the provided cursor has the expected number of rows,
     * closing the cursor afterwards.
     */
    private void assertCountIsAndClose(Cursor c, int expectedCount, String message) {
        try {
            mAsserter.is(c.getCount(), expectedCount, message);
        } finally {
            c.close();
        }
    }
}
