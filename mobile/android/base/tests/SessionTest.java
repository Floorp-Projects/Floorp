package org.mozilla.gecko.tests;

import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.Actions;
import org.mozilla.gecko.Assert;
import org.mozilla.gecko.FennecMochitestAssert;

public abstract class SessionTest extends BaseTest {
    protected Navigation mNavigation;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        mNavigation = new Navigation(mDevice);
    }

    /**
     * A generic session object representing a collection of items that has a
     * selected index.
     */
    protected abstract class SessionObject<T> {
        private final int mIndex;
        private final T[] mItems;

        public SessionObject(int index, T... items) {
            mIndex = index;
            mItems = items;
        }

        public int getIndex() {
            return mIndex;
        }

        public T[] getItems() {
            return mItems;
        }
    }

    protected class PageInfo {
        private final String url;
        private final String title;

        public PageInfo(String key) {
            if (key.startsWith("about:")) {
                url = key;
            } else {
                url = getPage(key);
            }
            title = key;
        }
    }

    protected class SessionTab extends SessionObject<PageInfo> {
        public SessionTab(int index, PageInfo... items) {
            super(index, items);
        }
    }

    protected class Session extends SessionObject<SessionTab> {
        public Session(int index, SessionTab... items) {
            super(index, items);
        }
    }

    /**
     * Walker for visiting items in a browser-like navigation order.
     */
    protected abstract class NavigationWalker<T> {
        private final T[] mItems;
        private final int mIndex;

        public NavigationWalker(SessionObject<T> obj) {
            mItems = obj.getItems();
            mIndex = obj.getIndex();
        }

        /**
         * Walks over the list of items, calling the onItem() callback for each.
         *
         * The selected item is the first item visited. Each item after the
         * selected item is then visited in ascending index order. Finally, the
         * list is iterated in reverse, and each item before the selected item
         * is visited in descending index order.
         */
        public void walk() {
            onItem(mItems[mIndex], mIndex);
            for (int i = mIndex + 1; i < mItems.length; i++) {
                goForward();
                onItem(mItems[i], i);
            }
            if (mIndex > 0) {
                for (int i = mItems.length - 2; i >= 0; i--) {
                    goBack();
                    if (i < mIndex) {
                        onItem(mItems[i], i);
                    }
                }
            }
        }

        /**
         * Callback when an item is visited during a walk.
         *
         * Only one callback is executed per item.
         */
        public abstract void onItem(T item, int currentIndex);

        /**
         * Callback executed for each back step of the walk.
         */
        public void goBack() {}

        /**
         * Callback executed for each forward step of the walk.
         */
        public void goForward() {}
    }

    /**
     * Loads a set of tabs in the browser specified by the given session.
     *
     * @param session Session to load
     */
    protected void loadSessionTabs(Session session) {
        // Verify initial about:home tab
        verifyTabCount(1);
        verifyUrl(StringHelper.ABOUT_HOME_URL);

        SessionTab[] tabs = session.getItems();
        for (int i = 0; i < tabs.length; i++) {
            final SessionTab tab = tabs[i];
            final PageInfo[] pages = tab.getItems();

            // New tabs always start with about:home, so make sure about:home
            // is always the first entry.
            mAsserter.is(pages[0].url, StringHelper.ABOUT_HOME_URL, "first page in tab is " +
                    StringHelper.ABOUT_HOME_URL);

            // If this is the first tab, the tab already exists, so no need to
            // create a new one. Otherwise, create a new tab if we're loading
            // the first the first page in the set.
            if (i > 0) {
                addTab();
            }

            for (int j = 1; j < pages.length; j++) {
                Actions.EventExpecter pageShowExpecter = mActions.expectGeckoEvent("Content:PageShow");

                loadUrl(pages[j].url);

                pageShowExpecter.blockForEvent();
                pageShowExpecter.unregisterListener();
            }

            final int index = tab.getIndex();
            for (int j = pages.length - 1; j > index; j--) {
                mNavigation.back();
            }
        }

        selectTabAt(session.getIndex());
    }

    /**
     * Verifies that the set of open tabs matches the given session.
     *
     * @param session Session to verify
     */
    protected void verifySessionTabs(Session session) {
        verifyTabCount(session.getItems().length);

        (new NavigationWalker<SessionTab>(session) {
            boolean mFirstTabVisited;

            @Override
            public void onItem(SessionTab tab, int currentIndex) {
                // The first tab to check should already be selected at startup
                if (mFirstTabVisited) {
                    selectTabAt(currentIndex);
                } else {
                    mFirstTabVisited = true;
                }

                (new NavigationWalker<PageInfo>(tab) {
                    @Override
                    public void onItem(PageInfo page, int currentIndex) {
                        if (page.url.equals(StringHelper.ABOUT_HOME_URL)) {
                            waitForText("Enter Search or Address");
                            verifyUrl(page.url);
                        } else {
                            waitForText(page.title);
                            verifyPageTitle(page.title, page.url);
                        }
                    }

                    @Override
                    public void goBack() {
                        mNavigation.back();
                    }

                    @Override
                    public void goForward() {
                        mNavigation.forward();
                    }
                }).walk();
            }
        }).walk();
    }

    /**
     * Gets session restore JSON corresponding to the open session.
     *
     * The JSON format follows the format used in Gecko for session restore and
     * should be interchangeable with the Gecko's generated sessionstore.js.
     *
     * @param session Session to serialize
     * @return JSON string of session
     */
    protected String buildSessionJSON(Session session) {
        final SessionTab[] sessionTabs = session.getItems();
        String sessionString = null;

        try {
            final JSONArray tabs = new JSONArray();

            for (int i = 0; i < sessionTabs.length; i++) {
                final JSONObject tab = new JSONObject();
                final JSONArray entries = new JSONArray();
                final SessionTab sessionTab = sessionTabs[i];
                final PageInfo[] pages = sessionTab.getItems();

                for (int j = 0; j < pages.length; j++) {
                    final PageInfo page = pages[j];
                    final JSONObject entry = new JSONObject();
                    entry.put("url", page.url);
                    entry.put("title", page.title);
                    entries.put(entry);
                }

                tab.put("entries", entries);
                tab.put("index", sessionTab.getIndex() + 1);
                tabs.put(tab);
            }

            JSONObject window = new JSONObject();
            window.put("tabs", tabs);
            window.put("selected", session.getIndex() + 1);
            sessionString = new JSONObject().put("windows", new JSONArray().put(window)).toString();
        } catch (JSONException e) {
            mAsserter.ok(false, "JSON exception", getStackTraceString(e));
        }

        return sessionString;
    }

    /**
     * @see SessionTest#verifySessionJSON(Session, String, Assert)
     */
    protected void verifySessionJSON(Session session, String sessionString) {
        verifySessionJSON(session, sessionString, mAsserter);
    }

    /**
     * Verifies a session JSON string against the given session.
     *
     * @param session       Session to verify against
     * @param sessionString JSON string to verify
     * @param asserter      Assert class to use during verification
     */
    protected void verifySessionJSON(Session session, String sessionString, Assert asserter) {
        final SessionTab[] sessionTabs = session.getItems();

        try {
            final JSONObject window = new JSONObject(sessionString).getJSONArray("windows").getJSONObject(0);
            final JSONArray tabs = window.getJSONArray("tabs");
            final int optSelected = window.optInt("selected", -1);

            asserter.is(optSelected, session.getIndex() + 1, "selected tab matches");

            for (int i = 0; i < tabs.length(); i++) {
                final JSONObject tab = tabs.getJSONObject(i);
                final int index = tab.getInt("index");
                final JSONArray entries = tab.getJSONArray("entries");
                final SessionTab sessionTab = sessionTabs[i];
                final PageInfo[] pages = sessionTab.getItems();

                asserter.is(index, sessionTab.getIndex() + 1, "selected page index matches");

                for (int j = 0; j < entries.length(); j++) {
                    final JSONObject entry = entries.getJSONObject(j);
                    final String url = entry.getString("url");
                    final String title = entry.optString("title");
                    final PageInfo page = pages[j];

                    asserter.is(url, page.url, "URL in JSON matches session URL");
                    if (!page.url.startsWith("about:")) {
                        asserter.is(title, page.title, "title in JSON matches session title");
                    }
                }
            }
        } catch (JSONException e) {
            asserter.ok(false, "JSON exception", getStackTraceString(e));
        }
    }

    /**
     * Exception thrown by NonFatalAsserter for assertion failures.
     */
    public static class AssertException extends RuntimeException {
        public AssertException(String msg) {
            super(msg);
        }
    }

    /**
     * Asserter that throws an AssertException on failure instead of aborting
     * the test.
     *
     * This can be used in methods called via waitForCondition() where an assertion
     * might not immediately succeed.
     */
    public class NonFatalAsserter extends FennecMochitestAssert {
        @Override
        public void ok(boolean condition, String name, String diag) {
            if (!condition) {
                String details = (diag == null ? "" : " | " + diag);
                throw new AssertException("Assertion failed: " + name + details);
            }
            mAsserter.ok(condition, name, diag);
        }
    }

    /**
     * Gets a URL for a dynamically-generated page.
     *
     * The page will have a URL unique to the given ID, and the page's title
     * will match the given ID.
     *
     * @param id ID used to generate page URL
     * @return URL of the page
     */
    protected String getPage(String id) {
        return getAbsoluteUrl("/robocop/robocop_dynamic.sjs?id=" + id);
    }

    protected String readProfileFile(String filename) {
        try {
            return readFile(new File(mProfile, filename));
        } catch (IOException e) {
            mAsserter.ok(false, "Error reading" + filename, getStackTraceString(e));
        }
        return null;
    }

    protected void writeProfileFile(String filename, String data) {
        try {
            writeFile(new File(mProfile, filename), data);
        } catch (IOException e) {
            mAsserter.ok(false, "Error writing to " + filename, getStackTraceString(e));
        }
    }

    private String readFile(File target) throws IOException {
        if (!target.exists()) {
            return null;
        }

        FileReader fr = new FileReader(target);
        try {
            StringBuffer sb = new StringBuffer();
            char[] buf = new char[8192];
            int read = fr.read(buf);
            while (read >= 0) {
                sb.append(buf, 0, read);
                read = fr.read(buf);
            }
            return sb.toString();
        } finally {
            fr.close();
        }
    }

    private void writeFile(File target, String data) throws IOException {
        FileWriter writer = new FileWriter(target);
        try {
            writer.write(data);
        } finally {
            writer.close();
        }
    }
}
