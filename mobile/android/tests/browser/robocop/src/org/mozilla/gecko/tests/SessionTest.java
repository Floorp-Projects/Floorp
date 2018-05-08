/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.util.Log;

import com.robotium.solo.Condition;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.Assert;
import org.mozilla.gecko.FennecMochitestAssert;
import org.mozilla.gecko.tests.helpers.NavigationHelper;

import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;

import static org.mozilla.gecko.GeckoApp.PREFS_ALLOW_STATE_BUNDLE;
import static org.mozilla.gecko.tests.components.AppMenuComponent.MenuItem;

public abstract class SessionTest extends UITest {
    private static final String PREFS_NAME = "GeckoApp";
    protected final static int SESSION_TIMEOUT = 25000;

    /**
     * A generic session object representing a collection of items that has a
     * selected index.
     */
    protected abstract class SessionObject<T> {
        private final int mIndex;
        private final T[] mItems;

        @SuppressWarnings({"unchecked", "varargs"})
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
        private final String triggeringPrincipal_base64;

        public PageInfo(String key) {
            if (key.startsWith("about:")) {
                url = key;
            } else {
                url = getPage(key);
            }
            title = key;
            triggeringPrincipal_base64 =
              "SmIS26zLEdO3ZQBgsLbOywAAAAAAAAAAwAAAAAAAAEY=";
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

    protected Session createTestSession(int selectedTabIndex) {
        PageInfo home = new PageInfo("about:home");
        PageInfo page1 = new PageInfo("page1");
        PageInfo page2 = new PageInfo("page2");
        PageInfo page3 = new PageInfo("page3");
        PageInfo page4 = new PageInfo("page4");
        PageInfo page5 = new PageInfo("page5");
        PageInfo page6 = new PageInfo("page6");

        SessionTab tab1 = new SessionTab(0, home, page1, page2);
        SessionTab tab2 = new SessionTab(1, home, page3, page4);
        SessionTab tab3 = new SessionTab(2, home, page5, page6);

        return new Session(selectedTabIndex, tab1, tab2, tab3);
    }

    protected void injectSessionToRestore(final Intent intent, Session session) {
        String sessionString = buildSessionJSON(session);
        writeProfileFile("sessionstore.js", sessionString);

        // This feature is pref-protected to prevent other apps from injecting
        // a state bundle, so enable it here.
        SharedPreferences prefs = getInstrumentation().getTargetContext()
                .getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        prefs.edit().putBoolean(PREFS_ALLOW_STATE_BUNDLE, true).apply();

        Bundle bundle = new Bundle();
        bundle.putString("privateSession", null);
        intent.putExtra("stateBundle", bundle);
    }

    /**
     * Loads a set of tabs in the browser specified by the given session.
     *
     * @param session Session to load
     */
    protected void loadSessionTabs(Session session) {
        // Verify initial about:home tab
        mToolbar.assertTabCount(1);
        mToolbar.assertTitle(mStringHelper.ABOUT_HOME_URL);

        SessionTab[] tabs = session.getItems();
        for (int i = 0; i < tabs.length; i++) {
            final SessionTab tab = tabs[i];
            final PageInfo[] pages = tab.getItems();

            // New tabs always start with about:home, so make sure about:home
            // is always the first entry.
            mAsserter.is(pages[0].url, mStringHelper.ABOUT_HOME_URL, "first page in tab is " +
                    mStringHelper.ABOUT_HOME_URL);

            // If this is the first tab, the tab already exists, so no need to
            // create a new one. Otherwise, create a new tab if we're loading
            // the first the first page in the set.
            if (i > 0) {
                mAppMenu.pressMenuItem(MenuItem.NEW_TAB);
            }

            for (int j = 1; j < pages.length; j++) {
                NavigationHelper.enterAndLoadUrl(pages[j].url);
            }

            final int index = tab.getIndex();
            for (int j = pages.length - 1; j > index; j--) {
                NavigationHelper.goBack();
            }
        }

        mTabsPanel.selectTabAt(session.getIndex());
    }

    /**
     * Verifies that the set of open tabs matches the given session.
     *
     * @param session Session to verify
     */
    protected void verifySessionTabs(Session session) {
        mToolbar.assertTabCount(session.getItems().length);

        (new NavigationWalker<SessionTab>(session) {
            boolean mFirstTabVisited;

            @Override
            public void onItem(SessionTab tab, int currentIndex) {
                // The first tab to check should already be selected at startup
                if (mFirstTabVisited) {
                    mTabsPanel.selectTabAt(currentIndex);
                } else {
                    mFirstTabVisited = true;
                }

                (new NavigationWalker<PageInfo>(tab) {
                    @Override
                    public void onItem(PageInfo page, int currentIndex) {
                        mToolbar.assertTitle(page.url);
                    }

                    @Override
                    public void goBack() {
                        NavigationHelper.goBack();
                    }

                    @Override
                    public void goForward() {
                        NavigationHelper.goForward();
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
                    entry.put("triggeringPrincipal_base64",
                              page.triggeringPrincipal_base64);
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
            mAsserter.ok(false, "JSON exception", Log.getStackTraceString(e));
        }

        return sessionString;
    }

    protected class VerifyJSONCondition implements Condition {
        private AssertException mLastException;
        private final NonFatalAsserter mAsserter = new NonFatalAsserter();
        private final Session mSession;

        public VerifyJSONCondition(Session session) {
            mSession = session;
        }

        @Override
        public boolean isSatisfied() {
            try {
                String sessionString = readProfileFile("sessionstore.js");
                if (sessionString == null) {
                    mLastException = new AssertException("Could not read sessionstore.js");
                    return false;
                }

                verifySessionJSON(mSession, sessionString, mAsserter);
            } catch (AssertException e) {
                mLastException = e;
                return false;
            }
            return true;
        }

        /**
         * Gets the last AssertException thrown by verifySessionJSON().
         *
         * This is useful to get the stack trace if the test fails.
         */
        public AssertException getLastException() {
            return mLastException;
        }
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

            asserter.is(tabs.length(), sessionTabs.length, "number of saved tabs matches");
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
                    final String principal =
                      entry.getString("triggeringPrincipal_base64");
                    final PageInfo page = pages[j];

                    asserter.is(url, page.url, "URL in JSON matches session URL");
                    if (!page.url.startsWith("about:")) {
                        asserter.is(title, page.title, "title in JSON matches session title");
                    }

                    asserter.is(principal, page.triggeringPrincipal_base64,
                                "principal in JSON matches session principal");
                }
            }
        } catch (JSONException e) {
            asserter.ok(false, "JSON exception", Log.getStackTraceString(e));
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
        return getAbsoluteHostnameUrl("/robocop/robocop_dynamic.sjs?id=" + id);
    }

    protected String readProfileFile(String filename) {
        try {
            return readFile(new File(mProfile, filename));
        } catch (IOException e) {
            mAsserter.ok(false, "Error reading" + filename, Log.getStackTraceString(e));
        }
        return null;
    }

    protected void writeProfileFile(String filename, String data) {
        try {
            writeFile(new File(mProfile, filename), data);
        } catch (IOException e) {
            mAsserter.ok(false, "Error writing to " + filename, Log.getStackTraceString(e));
        }
    }

    protected void deleteProfileFile(String filename) {
        File fileToDelete = new File(mProfile, filename);
        if (!fileToDelete.delete()) {
            mAsserter.ok(false, "Error deleting " + filename, null);
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
