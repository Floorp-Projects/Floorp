/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.ArrayList;
import java.util.HashSet;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.Element;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.R;
import org.mozilla.gecko.RobocopUtils;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;

import android.app.Activity;
import android.content.ContentValues;
import android.content.pm.ActivityInfo;
import android.content.res.AssetManager;
import android.database.Cursor;
import android.os.Build;
import android.os.SystemClock;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.text.TextUtils;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListAdapter;
import android.widget.TextView;

import com.jayway.android.robotium.solo.Condition;
import com.jayway.android.robotium.solo.Solo;
import com.jayway.android.robotium.solo.Timeout;

/**
 *  A convenient base class suitable for most Robocop tests.
 */
@SuppressWarnings("unchecked")
abstract class BaseTest extends BaseRobocopTest {
    private static final int VERIFY_URL_TIMEOUT = 2000;
    private static final int MAX_WAIT_ENABLED_TEXT_MS = 15000;
    private static final int MAX_WAIT_HOME_PAGER_HIDDEN_MS = 15000;
    private static final int MAX_WAIT_VERIFY_PAGE_TITLE_MS = 15000;
    public static final int MAX_WAIT_MS = 4500;
    public static final int LONG_PRESS_TIME = 6000;
    private static final int GECKO_READY_WAIT_MS = 180000;

    protected static final String URL_HTTP_PREFIX = "http://";

    public Device mDevice;
    protected DatabaseHelper mDatabaseHelper;
    protected int mScreenMidWidth;
    protected int mScreenMidHeight;
    private final HashSet<Integer> mKnownTabIDs = new HashSet<Integer>();

    protected void blockForDelayedStartup() {
        try {
            Actions.EventExpecter delayedStartupExpector = mActions.expectGeckoEvent("Gecko:DelayedStartup");
            delayedStartupExpector.blockForEvent(GECKO_READY_WAIT_MS, true);
            delayedStartupExpector.unregisterListener();
        } catch (Exception e) {
            mAsserter.dumpLog("Exception in blockForDelayedStartup", e);
        }
    }

    protected void blockForGeckoReady() {
        try {
            Actions.EventExpecter geckoReadyExpector = mActions.expectGeckoEvent("Gecko:Ready");
            if (!GeckoThread.isRunning()) {
                geckoReadyExpector.blockForEvent(GECKO_READY_WAIT_MS, true);
            }
            geckoReadyExpector.unregisterListener();
        } catch (Exception e) {
            mAsserter.dumpLog("Exception in blockForGeckoReady", e);
        }
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        mDevice = new Device();
        mDatabaseHelper = new DatabaseHelper(getActivity(), mAsserter);

        // Ensure Robocop tests have access to network, and are run with Display powered on.
        throwIfHttpGetFails();
        throwIfScreenNotOn();
    }

    protected GeckoProfile getTestProfile() {
        if (mProfile.startsWith("/")) {
            return GeckoProfile.get(getActivity(), "default", mProfile);
        }

        return GeckoProfile.get(getActivity(), mProfile);
    }

    protected void initializeProfile() {
        final GeckoProfile profile = getTestProfile();

        // In Robocop tests, we typically don't get initialized correctly, because
        // GeckoProfile doesn't create the profile directory.
        profile.enqueueInitialization(profile.getDir());
    }

    /**
     * Click on the URL bar to focus it and enter editing mode.
     */
    protected final void focusUrlBar() {
        // Click on the browser toolbar to enter editing mode
        mSolo.waitForView(R.id.browser_toolbar);
        final View toolbarView = mSolo.getView(R.id.browser_toolbar);
        mSolo.clickOnView(toolbarView);

        // Wait for highlighed text to gain focus
        boolean success = waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                mSolo.waitForView(R.id.url_edit_text);
                EditText urlEditText = (EditText) mSolo.getView(R.id.url_edit_text);
                if (urlEditText.isInputMethodTarget()) {
                    return true;
                }
                return false;
            }
        }, MAX_WAIT_ENABLED_TEXT_MS);

        mAsserter.ok(success, "waiting for urlbar text to gain focus", "urlbar text gained focus");
    }

    protected final void enterUrl(String url) {
        focusUrlBar();

        final EditText urlEditView = (EditText) mSolo.getView(R.id.url_edit_text);

        // Send the keys for the URL we want to enter
        mSolo.clearEditText(urlEditView);
        mSolo.typeText(urlEditView, url);

        // Get the URL text from the URL bar EditText view
        final String urlBarText = urlEditView.getText().toString();
        mAsserter.is(url, urlBarText, "URL typed properly");
    }

    protected final Fragment getBrowserSearch() {
        final FragmentManager fm = ((FragmentActivity) getActivity()).getSupportFragmentManager();
        return fm.findFragmentByTag("browser_search");
    }

    protected final void hitEnterAndWait() {
        Actions.EventExpecter contentEventExpecter = mActions.expectGeckoEvent("DOMContentLoaded");
        mActions.sendSpecialKey(Actions.SpecialKey.ENTER);
        // wait for screen to load
        contentEventExpecter.blockForEvent();
        contentEventExpecter.unregisterListener();
    }

    /**
     * Load <code>url</code> by sending key strokes to the URL bar UI.
     *
     * This method waits synchronously for the <code>DOMContentLoaded</code>
     * message from Gecko before returning.
     *
     * Unless you need to test text entry in the url bar, consider using loadUrl
     * instead -- it loads pages more directly and quickly.
     */
    protected final void inputAndLoadUrl(String url) {
        enterUrl(url);
        hitEnterAndWait();
    }

    /**
     * Load <code>url</code> using the internal
     * <code>org.mozilla.gecko.Tabs</code> API.
     *
     * This method does not wait for any confirmation from Gecko before
     * returning -- consider using verifyUrlBarTitle or a similar approach
     * to wait for the page to load, or at least use loadUrlAndWait.
     */
    protected final void loadUrl(final String url) {
        try {
            Tabs.getInstance().loadUrl(url);
        } catch (Exception e) {
            mAsserter.dumpLog("Exception in loadUrl", e);
            throw new RuntimeException(e);
        }
    }

    /**
     * Load <code>url</code> using the internal
     * <code>org.mozilla.gecko.Tabs</code> API and wait for DOMContentLoaded.
     */
    protected final void loadUrlAndWait(final String url) {
        Actions.EventExpecter contentEventExpecter = mActions.expectGeckoEvent("DOMContentLoaded");
        loadUrl(url);
        contentEventExpecter.blockForEvent();
        contentEventExpecter.unregisterListener();
    }

    protected final void closeTab(int tabId) {
        Tabs tabs = Tabs.getInstance();
        Tab tab = tabs.getTab(tabId);
        tabs.closeTab(tab);
    }

    public final void verifyUrl(String url) {
        final EditText urlEditText = (EditText) mSolo.getView(R.id.url_edit_text);
        String urlBarText = null;
        if (urlEditText != null) {
            // wait for a short time for the expected text, in case there is a delay
            // in updating the view
            waitForCondition(new VerifyTextViewText(urlEditText, url), VERIFY_URL_TIMEOUT);
            urlBarText = urlEditText.getText().toString();

        }
        mAsserter.is(urlBarText, url, "Browser toolbar URL stayed the same");
    }

    class VerifyTextViewText implements Condition {
        private final TextView mTextView;
        private final String mExpected;
        public VerifyTextViewText(TextView textView, String expected) {
            mTextView = textView;
            mExpected = expected;
        }

        @Override
        public boolean isSatisfied() {
            String textValue = mTextView.getText().toString();
            return mExpected.equals(textValue);
        }
    }

    class VerifyContentDescription implements Condition {
        private final View view;
        private final String expected;

        public VerifyContentDescription(View view, String expected) {
            this.view = view;
            this.expected = expected;
        }

        @Override
        public boolean isSatisfied() {
            final CharSequence actual = view.getContentDescription();
            return TextUtils.equals(actual, expected);
        }
    }

    protected final String getAbsoluteUrl(String url) {
        return mBaseHostnameUrl + "/" + url.replaceAll("(^/)", "");
    }

    protected final String getAbsoluteRawUrl(String url) {
        return mBaseIpUrl + "/" + url.replaceAll("(^/)", "");
    }

    /*
     * Wrapper method for mSolo.waitForCondition with additional logging.
     */
    protected final boolean waitForCondition(Condition condition, int timeout) {
        boolean result = mSolo.waitForCondition(condition, timeout);
        if (!result) {
            // Log timeout failure for diagnostic purposes only; a failed wait may
            // be normal and does not necessarily warrant a test assertion/failure.
            mAsserter.dumpLog("waitForCondition timeout after " + timeout + " ms.");
        }
        return result;
    }

    public void SqliteCompare(String dbName, String sqlCommand, ContentValues[] cvs) {
        File profile = new File(mProfile);
        String dbPath = new File(profile, dbName).getPath();

        Cursor c = mActions.querySql(dbPath, sqlCommand);
        SqliteCompare(c, cvs);
    }

    public void SqliteCompare(Cursor c, ContentValues[] cvs) {
        mAsserter.is(c.getCount(), cvs.length, "List is correct length");
        if (c.moveToFirst()) {
            do {
                boolean found = false;
                for (int i = 0; !found && i < cvs.length; i++) {
                    if (CursorMatches(c, cvs[i])) {
                        found = true;
                    }
                }
                mAsserter.is(found, true, "Password was found");
            } while (c.moveToNext());
        }
    }

    public boolean CursorMatches(Cursor c, ContentValues cv) {
        for (int i = 0; i < c.getColumnCount(); i++) {
            String column = c.getColumnName(i);
            if (cv.containsKey(column)) {
                mAsserter.info("Comparing", "Column values for: " + column);
                Object value = cv.get(column);
                if (value == null) {
                    if (!c.isNull(i)) {
                        return false;
                    }
                } else {
                    if (c.isNull(i) || !value.toString().equals(c.getString(i))) {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    public InputStream getAsset(String filename) throws IOException {
        AssetManager assets = getInstrumentation().getContext().getAssets();
        return assets.open(filename);
    }

    public boolean waitForText(final String text) {
        // false is the default value for finding only
        // visible views in `Solo.waitForText(String)`.
        return waitForText(text, false);
    }

    public boolean waitForText(final String text, final boolean onlyVisibleViews) {
        // We use the default robotium values from
        // `Waiter.waitForText(String)` for unspecified arguments.
        final boolean rc =
                mSolo.waitForText(text, 0, Timeout.getLargeTimeout(), true, onlyVisibleViews);
        if (!rc) {
            // log out failed wait for diagnostic purposes only;
            // waitForText failures are sometimes expected/normal
            mAsserter.dumpLog("waitForText timeout on "+text);
        }
        return rc;
    }

    // waitForText usually scrolls down in a view when text is not visible.
    // For PreferenceScreens and dialogs, Solo.waitForText scrolling does not
    // work, so we use this hack to do the same thing.
    protected boolean waitForPreferencesText(String txt) {
        boolean foundText = waitForText(txt);
        if (!foundText) {
            if ((mScreenMidWidth == 0) || (mScreenMidHeight == 0)) {
                mScreenMidWidth = mDriver.getGeckoWidth()/2;
                mScreenMidHeight = mDriver.getGeckoHeight()/2;
            }

            // If we don't see the item, scroll down once in case it's off-screen.
            // Hacky way to scroll down.  solo.scroll* does not work in dialogs.
            MotionEventHelper meh = new MotionEventHelper(getInstrumentation(), mDriver.getGeckoLeft(), mDriver.getGeckoTop());
            meh.dragSync(mScreenMidWidth, mScreenMidHeight+100, mScreenMidWidth, mScreenMidHeight-100);

            foundText = mSolo.waitForText(txt);
        }
        return foundText;
    }

    /**
     * Wait for <text> to be visible and also be enabled/clickable.
     */
    public boolean waitForEnabledText(String text) {
        final String testText = text;
        boolean rc = waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                // Solo.getText() could be used here, except that it sometimes
                // hits an assertion when the requested text is not found.
                ArrayList<View> views = mSolo.getCurrentViews();
                for (View view : views) {
                    if (view instanceof TextView) {
                        TextView tv = (TextView)view;
                        String viewText = tv.getText().toString();
                        if (tv.isEnabled() && viewText != null && viewText.matches(testText)) {
                            return true;
                        }
                    }
                }
                return false;
            }
        }, MAX_WAIT_ENABLED_TEXT_MS);
        if (!rc) {
            // log out failed wait for diagnostic purposes only;
            // failures are sometimes expected/normal
            mAsserter.dumpLog("waitForEnabledText timeout on "+text);
        }
        return rc;
    }


    /**
     * Select <item> from Menu > "Settings" > <section>.
     */
    public void selectSettingsItem(String section, String item) {
        String[] itemPath = { "Settings", section, item };
        selectMenuItemByPath(itemPath);
    }

    /**
     * Traverses the items in listItems in order in the menu.
     */
    public void selectMenuItemByPath(String[] listItems) {
        int listLength = listItems.length;
        if (listLength > 0) {
            selectMenuItem(listItems[0]);
        }
        if (listLength > 1) {
            for (int i = 1; i < listLength; i++) {
                String itemName = "^" + listItems[i] + "$";
                mAsserter.ok(waitForPreferencesText(itemName), "Waiting for and scrolling once to find item " + itemName, itemName + " found");
                mAsserter.ok(waitForEnabledText(itemName), "Waiting for enabled text " + itemName, itemName + " option is present and enabled");
                mSolo.clickOnText(itemName);
            }
        }
    }

    public final void selectMenuItem(String menuItemName) {
        // build the item name ready to be used
        String itemName = "^" + menuItemName + "$";
        final View menuView = mSolo.getView(R.id.menu);
        mAsserter.isnot(menuView, null, "Menu view is not null");
        mSolo.clickOnView(menuView, true);
        mAsserter.ok(waitForEnabledText(itemName), "Waiting for menu item " + itemName, itemName + " is present and enabled");
        mSolo.clickOnText(itemName);
    }

    public final void verifyHomePagerHidden() {
        final View homePagerContainer = mSolo.getView(R.id.home_pager_container);

        boolean rc = waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                return homePagerContainer.getVisibility() != View.VISIBLE;
            }
        }, MAX_WAIT_HOME_PAGER_HIDDEN_MS);

        if (!rc) {
            mAsserter.ok(rc, "Verify HomePager is hidden", "HomePager is hidden");
        }
    }

    public final void verifyUrlBarTitle(String url) {
        mAsserter.isnot(url, null, "The url argument is not null");

        final String expected;
        if (mStringHelper.ABOUT_HOME_URL.equals(url)) {
            expected = mStringHelper.ABOUT_HOME_TITLE;
        } else if (url.startsWith(URL_HTTP_PREFIX)) {
            expected = url.substring(URL_HTTP_PREFIX.length());
        } else {
            expected = url;
        }

        final TextView urlBarTitle = (TextView) mSolo.getView(R.id.url_bar_title);
        String pageTitle = null;
        if (urlBarTitle != null) {
            // Wait for the title to make sure it has been displayed in case the view
            // does not update fast enough
            waitForCondition(new VerifyTextViewText(urlBarTitle, expected), MAX_WAIT_VERIFY_PAGE_TITLE_MS);
            pageTitle = urlBarTitle.getText().toString();
        }
        mAsserter.is(pageTitle, expected, "Page title is correct");
    }

    public final void verifyUrlInContentDescription(String url) {
        mAsserter.isnot(url, null, "The url argument is not null");

        final String expected;
        if (mStringHelper.ABOUT_HOME_URL.equals(url)) {
            expected = mStringHelper.ABOUT_HOME_TITLE;
        } else if (url.startsWith(URL_HTTP_PREFIX)) {
            expected = url.substring(URL_HTTP_PREFIX.length());
        } else {
            expected = url;
        }

        final View urlDisplayLayout = mSolo.getView(R.id.display_layout);
        assertNotNull("ToolbarDisplayLayout is not null", urlDisplayLayout);

        String actualUrl = null;

        // Wait for the title to make sure it has been displayed in case the view
        // does not update fast enough
        waitForCondition(new VerifyContentDescription(urlDisplayLayout, expected), MAX_WAIT_VERIFY_PAGE_TITLE_MS);
        if (urlDisplayLayout.getContentDescription() != null) {
            actualUrl = urlDisplayLayout.getContentDescription().toString();
        }

        mAsserter.is(actualUrl, expected, "Url is correct");
    }

    public final void verifyTabCount(int expectedTabCount) {
        Element tabCount = mDriver.findElement(getActivity(), R.id.tabs_counter);
        String tabCountText = tabCount.getText();
        int tabCountInt = Integer.parseInt(tabCountText);
        mAsserter.is(tabCountInt, expectedTabCount, "The correct number of tabs are opened");
    }

    public void verifyPinned(final boolean isPinned, final String gridItemTitle) {
        boolean viewFound = waitForText(gridItemTitle);
        mAsserter.ok(viewFound, "Found top site title: " + gridItemTitle, null);

        boolean success = waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                // We set the left compound drawable (index 0) to the pin icon.
                final TextView gridItemTextView = mSolo.getText(gridItemTitle);
                return isPinned == (gridItemTextView.getCompoundDrawables()[0] != null);
            }
        }, MAX_WAIT_MS);
        mAsserter.ok(success, "Top site item was pinned: " + isPinned, null);
    }

    public void pinTopSite(String gridItemTitle) {
        verifyPinned(false, gridItemTitle);
        mSolo.clickLongOnText(gridItemTitle);
        boolean dialogOpened = mSolo.waitForDialogToOpen();
        mAsserter.ok(dialogOpened, "Pin site dialog opened: " + gridItemTitle, null);
        boolean pinSiteFound = waitForText(mStringHelper.CONTEXT_MENU_PIN_SITE);
        mAsserter.ok(pinSiteFound, "Found pin site menu item", null);
        mSolo.clickOnText(mStringHelper.CONTEXT_MENU_PIN_SITE);
        verifyPinned(true, gridItemTitle);
    }

    public void unpinTopSite(String gridItemTitle) {
        verifyPinned(true, gridItemTitle);
        mSolo.clickLongOnText(gridItemTitle);
        boolean dialogOpened = mSolo.waitForDialogToOpen();
        mAsserter.ok(dialogOpened, "Pin site dialog opened: " + gridItemTitle, null);
        boolean unpinSiteFound = waitForText(mStringHelper.CONTEXT_MENU_UNPIN_SITE);
        mAsserter.ok(unpinSiteFound, "Found unpin site menu item", null);
        mSolo.clickOnText(mStringHelper.CONTEXT_MENU_UNPIN_SITE);
        verifyPinned(false, gridItemTitle);
    }

    // Used to perform clicks on pop-up buttons without having to close the virtual keyboard
    public void clickOnButton(String label) {
        final Button button = mSolo.getButton(label);
        try {
            runTestOnUiThread(new Runnable() {
                @Override
                public void run() {
                    button.performClick();
                }
            });
       } catch (Throwable throwable) {
           mAsserter.ok(false, "Unable to click the button","Was unable to click button ");
       }
    }

    private void waitForAnimationsToFinish() {
        // Ideally we'd actually wait for animations to finish but since we have
        // no good way of doing that, we just wait an arbitrary unit of time.
        mSolo.sleep(3500);
    }

    public void addTab() {
        mSolo.clickOnView(mSolo.getView(R.id.tabs));
        waitForAnimationsToFinish();

        // wait for addTab to appear (this is usually immediate)
        boolean success = waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                View addTabView = mSolo.getView(R.id.add_tab);
                if (addTabView == null) {
                    return false;
                }
                return true;
            }
        }, MAX_WAIT_MS);
        mAsserter.ok(success, "waiting for add tab view", "add tab view available");
        final Actions.RepeatedEventExpecter pageShowExpecter = mActions.expectGeckoEvent("Content:PageShow");
        mSolo.clickOnView(mSolo.getView(R.id.add_tab));
        waitForAnimationsToFinish();

        // Wait until we get a PageShow event for a new tab ID
        for(;;) {
            try {
                JSONObject data = new JSONObject(pageShowExpecter.blockForEventData());
                int tabID = data.getInt("tabID");
                if (tabID == 0) {
                    mAsserter.dumpLog("addTab ignoring PageShow for tab 0");
                    continue;
                }
                if (!mKnownTabIDs.contains(tabID)) {
                    mKnownTabIDs.add(tabID);
                    break;
                }
            } catch(JSONException e) {
                mAsserter.ok(false, "Exception in addTab", getStackTraceString(e));
            }
        }
        pageShowExpecter.unregisterListener();
    }

    public void addTab(String url) {
        addTab();

        // Adding a new tab opens about:home, so now we just need to load the url in it.
        loadUrlAndWait(url);
    }

    public void closeAddedTabs() {
        for(int tabID : mKnownTabIDs) {
            closeTab(tabID);
        }
    }

    /**
     * Gets the AdapterView of the tabs list.
     *
     * @return List view in the tabs panel
     */
    private final AdapterView<ListAdapter> getTabsLayout() {
        Element tabs = mDriver.findElement(getActivity(), R.id.tabs);
        tabs.click();
        return (AdapterView<ListAdapter>) getActivity().findViewById(R.id.normal_tabs);
    }

    /**
     * Gets the view in the tabs panel at the specified index.
     *
     * @return View at index
     */
    private View getTabViewAt(final int index) {
        final View[] childView = { null };

        final AdapterView<ListAdapter> view = getTabsLayout();

        runOnUiThreadSync(new Runnable() {
            @Override
            public void run() {
                view.setSelection(index);

                // The selection isn't updated synchronously; posting a
                // runnable to the view's queue guarantees we'll run after the
                // layout pass.
                view.post(new Runnable() {
                    @Override
                    public void run() {
                        // getChildAt() is relative to the list of visible
                        // views, but our index is relative to all views in the
                        // list. Subtract the first visible list position for
                        // the correct offset.
                        childView[0] = view.getChildAt(index - view.getFirstVisiblePosition());
                    }
                });
            }
        });

        boolean result = waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                return childView[0] != null;
            }
        }, MAX_WAIT_MS);

        mAsserter.ok(result, "list item at index " + index + " exists", null);

        return childView[0];
    }

    /**
     * Selects the tab at the specified index.
     *
     * @param index Index of tab to select
     */
    public void selectTabAt(final int index) {
        mSolo.clickOnView(getTabViewAt(index));
    }

    public final void runOnUiThreadSync(Runnable runnable) {
        RobocopUtils.runOnUiThreadSync(getActivity(), runnable);
    }

    /* Tap the "star" (bookmark) button to bookmark or un-bookmark the current page */
    public void toggleBookmark() {
        mActions.sendSpecialKey(Actions.SpecialKey.MENU);
        waitForText("Settings");

        // On ICS+ phones, there is no button labeled "Bookmarks"
        // instead we have to just dig through every button on the screen
        ArrayList<View> images = mSolo.getCurrentViews();
        for (int i = 0; i < images.size(); i++) {
            final View view = images.get(i);
            boolean found = false;
            found = "Bookmark".equals(view.getContentDescription());

            // on older android versions, try looking at the button's text
            if (!found) {
                if (view instanceof TextView) {
                    found = "Bookmark".equals(((TextView)view).getText());
                }
            }

            if (found) {
                int[] xy = new int[2];
                view.getLocationOnScreen(xy);

                final int viewWidth = view.getWidth();
                final int viewHeight = view.getHeight();
                final float x = xy[0] + (viewWidth / 2.0f);
                float y = xy[1] + (viewHeight / 2.0f);

                mSolo.clickOnScreen(x, y);
            }
        }
    }

    class Device {
        public final String version; // 2.x or 3.x or 4.x
        public String type; // "tablet" or "phone"
        public final int width;
        public final int height;
        public final float density;

        public Device() {
            // Determine device version
            int sdk = Build.VERSION.SDK_INT;
            if (sdk < Build.VERSION_CODES.HONEYCOMB) {
                version = "2.x";
            } else {
                if (sdk > Build.VERSION_CODES.HONEYCOMB_MR2) {
                    version = "4.x";
                } else {
                    version = "3.x";
                }
            }
            // Determine with and height
            DisplayMetrics dm = new DisplayMetrics();
            getActivity().getWindowManager().getDefaultDisplay().getMetrics(dm);
            height = dm.heightPixels;
            width = dm.widthPixels;
            density = dm.density;
            // Determine device type
            type = "phone";
            try {
                if (GeckoAppShell.isTablet()) {
                    type = "tablet";
                }
            } catch (Exception e) {
                mAsserter.dumpLog("Exception in detectDevice", e);
            }
        }
    }

    class Navigation {
        private final String devType;
        private final String osVersion;

        public Navigation(Device mDevice) {
            devType = mDevice.type;
            osVersion = mDevice.version;
        }

        public void back() {
            Actions.EventExpecter pageShowExpecter = mActions.expectGeckoEvent("Content:PageShow");

            if (devType.equals("tablet")) {
                Element backBtn = mDriver.findElement(getActivity(), R.id.back);
                backBtn.click();
            } else {
                mSolo.goBack();
            }

            pageShowExpecter.blockForEvent();
            pageShowExpecter.unregisterListener();
        }

        public void forward() {
            Actions.EventExpecter pageShowExpecter = mActions.expectGeckoEvent("Content:PageShow");

            if (devType.equals("tablet")) {
                mSolo.waitForView(R.id.forward);
                mSolo.clickOnView(mSolo.getView(R.id.forward));
            } else {
                mActions.sendSpecialKey(Actions.SpecialKey.MENU);
                waitForText("^New Tab$");
                if (!osVersion.equals("2.x")) {
                    mSolo.waitForView(R.id.forward);
                    mSolo.clickOnView(mSolo.getView(R.id.forward));
                } else {
                    mSolo.clickOnText("^Forward$");
                }
                ensureMenuClosed();
            }

            pageShowExpecter.blockForEvent();
            pageShowExpecter.unregisterListener();
        }

        // DEPRECATED!
        // Use BaseTest.toggleBookmark() in new code.
        public void bookmark() {
            mActions.sendSpecialKey(Actions.SpecialKey.MENU);
            waitForText("^New Tab$");
            if (mSolo.searchText("^Bookmark$")) {
                // This is the Android 2.x so the button has text
                mSolo.clickOnText("^Bookmark$");
            } else {
                Element bookmarkBtn = mDriver.findElement(getActivity(), R.id.bookmark);
                if (bookmarkBtn != null) {
                    // We are on Android 4.x so the button is an image button
                    bookmarkBtn.click();
                }
            }
            ensureMenuClosed();
        }

        // On some devices, the menu may not be dismissed after clicking on an
        // item. Close it here.
        private void ensureMenuClosed() {
            if (mSolo.searchText("^New Tab$")) {
                mSolo.goBack();
            }
         }
    }

    /**
     * Gets the string representation of a stack trace.
     *
     * @param t Throwable to get stack trace for
     * @return Stack trace as a string
     */
    public static String getStackTraceString(Throwable t) {
        StringWriter sw = new StringWriter();
        t.printStackTrace(new PrintWriter(sw));
        return sw.toString();
    }

    /**
     * Condition class that waits for a view, and allows callers access it when done.
     */
    private class DescriptionCondition<T extends View> implements Condition {
        public T mView;
        private final String mDescr;
        private final Class<T> mCls;

        public DescriptionCondition(Class<T> cls, String descr) {
            mDescr = descr;
            mCls = cls;
        }

        @Override
        public boolean isSatisfied() {
            mView = findViewWithContentDescription(mCls, mDescr);
            return (mView != null);
        }
    }

    /**
     * Wait for a view with the specified description .
     */
    public <T extends View> T waitForViewWithDescription(Class<T> cls, String description) {
        DescriptionCondition<T> c = new DescriptionCondition<T>(cls, description);
        waitForCondition(c, MAX_WAIT_ENABLED_TEXT_MS);
        return c.mView;
    }

    /**
     * Get an active view with the specified description .
     */
    public <T extends View> T findViewWithContentDescription(Class<T> cls, String description) {
        for (T view : mSolo.getCurrentViews(cls)) {
            final String descr = (String) view.getContentDescription();
            if (TextUtils.isEmpty(descr)) {
                continue;
            }

            if (TextUtils.equals(description, descr)) {
                return view;
            }
        }

        return null;
    }

    /**
     * Abstract class for running small test cases within a BaseTest.
     */
    abstract class TestCase implements Runnable {
        /**
         * Implement tests here. setUp and tearDown for the test case
         * should be handled by the parent test. This is so we can avoid the
         * overhead of starting Gecko and creating profiles.
         */
        protected abstract void test() throws Exception;

        @Override
        public void run() {
            try {
                test();
            } catch (Exception e) {
                mAsserter.ok(false,
                             "Test " + this.getClass().getName() + " threw exception: " + e,
                             "");
            }
        }
    }

    /**
     * Set the preference and wait for it to change before proceeding with the test.
     */
    public void setPreferenceAndWaitForChange(final String name, final Object value) {
        blockForGeckoReady();
        mActions.setPref(name, value, /* flush */ false);

        // Wait for confirmation of the pref change before proceeding with the test.
        mActions.getPrefs(new String[] { name }, new Actions.PrefHandlerBase() {

            @Override // Actions.PrefHandlerBase
            public void prefValue(String pref, boolean changedValue) {
                mAsserter.is(pref, name, "Expecting correct pref name");
                mAsserter.ok(value instanceof Boolean, "Expecting boolean pref", "");
                mAsserter.is(changedValue, value, "Expecting matching pref value");
            }

            @Override // Actions.PrefHandlerBase
            public void prefValue(String pref, int changedValue) {
                mAsserter.is(pref, name, "Expecting correct pref name");
                mAsserter.ok(value instanceof Integer, "Expecting int pref", "");
                mAsserter.is(changedValue, value, "Expecting matching pref value");
            }

            @Override // Actions.PrefHandlerBase
            public void prefValue(String pref, String changedValue) {
                mAsserter.is(pref, name, "Expecting correct pref name");
                mAsserter.ok(value instanceof CharSequence, "Expecting string pref", "");
                mAsserter.is(changedValue, value, "Expecting matching pref value");
            }

        }).waitForFinish();
    }
}
