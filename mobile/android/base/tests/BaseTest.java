package org.mozilla.gecko.tests;

import com.jayway.android.robotium.solo.Condition;
import com.jayway.android.robotium.solo.Solo;

import org.mozilla.gecko.*;
import org.mozilla.gecko.GeckoThread.LaunchState;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.ContentUris;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.res.AssetManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.SystemClock;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.text.InputType;
import android.text.TextUtils;
import android.test.ActivityInstrumentationTestCase2;
import android.util.DisplayMetrics;
import android.view.inputmethod.InputMethodManager;
import android.view.KeyEvent;
import android.view.View;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListAdapter;
import android.widget.ListView;
import android.widget.TextView;

import java.io.File;
import java.io.InputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.ArrayList;
import java.util.HashMap;

/**
 *  A convenient base class suitable for most Robocop tests.
 */
abstract class BaseTest extends ActivityInstrumentationTestCase2<Activity> {
    public static final int TEST_MOCHITEST = 0;
    public static final int TEST_TALOS = 1;

    private static final String TARGET_PACKAGE_ID = "org.mozilla.gecko";
    private static final String LAUNCH_ACTIVITY_FULL_CLASSNAME = TestConstants.ANDROID_PACKAGE_NAME + ".App";
    private static final int VERIFY_URL_TIMEOUT = 2000;
    private static final int MAX_LIST_ATTEMPTS = 3;
    private static final int MAX_WAIT_ENABLED_TEXT_MS = 10000;
    private static final int MAX_WAIT_HOME_PAGER_HIDDEN_MS = 15000;
    public static final int MAX_WAIT_MS = 4500;
    public static final int LONG_PRESS_TIME = 6000;
    private static final int GECKO_READY_WAIT_MS = 180000;
    public static final int MAX_WAIT_BLOCK_FOR_EVENT_DATA_MS = 90000;

    private static Class<Activity> mLauncherActivityClass;
    private Activity mActivity;
    private int mPreferenceRequestID = 0;
    protected Solo mSolo;
    protected Driver mDriver;
    protected Assert mAsserter;
    protected Actions mActions;
    protected String mBaseUrl;
    protected String mRawBaseUrl;
    private String mLogFile;
    protected String mProfile;
    public Device mDevice;
    protected DatabaseHelper mDatabaseHelper;
    protected StringHelper mStringHelper;

    protected void blockForGeckoReady() {
        try {
            Actions.EventExpecter geckoReadyExpector = mActions.expectGeckoEvent("Gecko:Ready");
            if (!GeckoThread.checkLaunchState(LaunchState.GeckoRunning)) {
                geckoReadyExpector.blockForEvent(GECKO_READY_WAIT_MS, true);
            }
            geckoReadyExpector.unregisterListener();
        } catch (Exception e) {
            mAsserter.dumpLog("Exception in blockForGeckoReady", e);
        }
    }

    static {
        try {
            mLauncherActivityClass = (Class<Activity>)Class.forName(LAUNCH_ACTIVITY_FULL_CLASSNAME);
        } catch (ClassNotFoundException e) {
            throw new RuntimeException(e);
        }
    }

    public BaseTest() {
        super(TARGET_PACKAGE_ID, mLauncherActivityClass);
    }

    protected abstract int getTestType();

    @Override
    protected void setUp() throws Exception {
        // Load config file from root path (setup by python script)
        String rootPath = FennecInstrumentationTestRunner.getFennecArguments().getString("deviceroot");
        String configFile = FennecNativeDriver.getFile(rootPath + "/robotium.config");
        HashMap config = FennecNativeDriver.convertTextToTable(configFile);
        mLogFile = (String)config.get("logfile");
        mBaseUrl = ((String)config.get("host")).replaceAll("(/$)", "");
        mRawBaseUrl = ((String)config.get("rawhost")).replaceAll("(/$)", "");
        // Initialize the asserter
        if (getTestType() == TEST_TALOS) {
            mAsserter = new FennecTalosAssert();
        } else {
            mAsserter = new FennecMochitestAssert();
        }
        mAsserter.setLogFile(mLogFile);
        mAsserter.setTestName(this.getClass().getName());
        // Create the intent to be used with all the important arguments.
        Intent i = new Intent(Intent.ACTION_MAIN);
        mProfile = (String)config.get("profile");
        i.putExtra("args", "-no-remote -profile " + mProfile);
        String envString = (String)config.get("envvars");
        if (envString != "") {
            String[] envStrings = envString.split(",");
            for (int iter = 0; iter < envStrings.length; iter++) {
                i.putExtra("env" + iter, envStrings[iter]);
            }
        }
        // Start the activity
        setActivityIntent(i);
        mActivity = getActivity();
        // Set up Robotium.solo and Driver objects
        mSolo = new Solo(getInstrumentation(), mActivity);
        mDriver = new FennecNativeDriver(mActivity, mSolo, rootPath);
        mActions = new FennecNativeActions(mActivity, mSolo, getInstrumentation(), mAsserter);
        mDevice = new Device();
        mDatabaseHelper = new DatabaseHelper(mActivity, mAsserter);
        mStringHelper = new StringHelper();
    }

    @Override
    protected void runTest() throws Throwable {
        try {
            super.runTest();
        } catch (Throwable t) {
            // save screenshot -- written to /mnt/sdcard/Robotium-Screenshots
            // as <filename>.jpg
            mSolo.takeScreenshot("robocop-screenshot");
            if (mAsserter != null) {
                mAsserter.dumpLog("Exception caught during test!", t);
                mAsserter.ok(false, "Exception caught", t.toString());
            }
            // re-throw to continue bail-out
            throw t;
        }
    }

    @Override
    public void tearDown() throws Exception {
        try {
            mAsserter.endTest();
            mSolo.finishOpenedActivities();
        } catch (Throwable e) {
            e.printStackTrace();
        }
        super.tearDown();
    }

    public void assertMatches(String value, String regex, String name) {
        if (value == null) {
            mAsserter.ok(false, name, "Expected /" + regex + "/, got null");
            return;
        }
        mAsserter.ok(value.matches(regex), name, "Expected /" + regex +"/, got \"" + value + "\"");
    }

    /**
     * Click on the URL bar to focus it and enter editing mode.
     */
    protected final void focusUrlBar() {
        // Click on the browser toolbar to enter editing mode
        final View toolbarView = mSolo.getView(R.id.browser_toolbar);
        mSolo.clickOnView(toolbarView);

        // Wait for highlighed text to gain focus
        boolean success = waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                EditText urlEditText = (EditText) mSolo.getView(R.id.url_edit_text);
                if (urlEditText.isInputMethodTarget()) {
                    return true;
                } else {
                    mSolo.clickOnView(urlEditText);
                    return false;
                }
            }
        }, MAX_WAIT_ENABLED_TEXT_MS);

        mAsserter.ok(success, "waiting for urlbar text to gain focus", "urlbar text gained focus");
    }

    protected final void enterUrl(String url) {
        final EditText urlEditView = (EditText) mSolo.getView(R.id.url_edit_text);

        focusUrlBar();

        // Send the keys for the URL we want to enter
        mSolo.clearEditText(urlEditView);
        mSolo.enterText(urlEditView, url);

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
     */
    protected final void inputAndLoadUrl(String url) {
        enterUrl(url);
        hitEnterAndWait();
    }

    /**
     * Load <code>url</code> using reflection and the internal
     * <code>org.mozilla.gecko.Tabs</code> API.
     *
     * This method does not wait for any confirmation from Gecko before
     * returning.
     */
    protected final void loadUrl(final String url) {
        try {
            Tabs.getInstance().loadUrl(url);
        } catch (Exception e) {
            mAsserter.dumpLog("Exception in loadUrl", e);
            throw new RuntimeException(e);
        }
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
        private TextView mTextView;
        private String mExpected;
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

    protected final String getAbsoluteUrl(String url) {
        return mBaseUrl + "/" + url.replaceAll("(^/)", "");
    }

    protected final String getAbsoluteRawUrl(String url) {
        return mRawBaseUrl + "/" + url.replaceAll("(^/)", "");
    }

    /*
     * Wrapper method for mSolo.waitForCondition with additional logging.
     */
    protected final boolean waitForCondition(Condition condition, int timeout) {
        boolean result = mSolo.waitForCondition(condition, timeout);
        if (!result) {
            // Log timeout failure for diagnostic purposes only; a failed wait may
            // be normal and does not necessarily warrant a test asssertion/failure.
            mAsserter.dumpLog("waitForCondition timeout after " + timeout + " ms.");
        }
        return result;
    }

    // TODO: With Robotium 4.2, we should use Condition and waitForCondition instead.
    // Future boolean tests should not use this method.
    protected final boolean waitForTest(BooleanTest t, int timeout) {
        long end = SystemClock.uptimeMillis() + timeout;
        while (SystemClock.uptimeMillis() < end) {
            if (t.test()) {
                return true;
            }
            mSolo.sleep(100);
        }
        // log out wait failure for diagnostic purposes only;
        // a failed wait may be normal and does not necessarily
        // warrant a test assertion/failure
        mAsserter.dumpLog("waitForTest timeout after "+timeout+" ms");
        return false;
    }

    // TODO: With Robotium 4.2, we should use Condition and waitForCondition instead.
    // Future boolean tests should not implement this interface.
    protected interface BooleanTest {
        public boolean test();
    }

    @SuppressWarnings({"unchecked", "non-varargs"})
    public void SqliteCompare(String dbName, String sqlCommand, ContentValues[] cvs) {
        File profile = new File(mProfile);
        String dbPath = new File(profile, dbName).getPath();

        Cursor c = mActions.querySql(dbPath, sqlCommand);
        SqliteCompare(c, cvs);
    }

    private boolean CursorMatches(Cursor c, String[] columns, ContentValues cv) {
        for (int i = 0; i < columns.length; i++) {
            String column = columns[i];
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

    @SuppressWarnings({"unchecked", "non-varargs"})
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
            } while(c.moveToNext());
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

    public boolean waitForText(String text) {
        boolean rc = mSolo.waitForText(text);
        if (!rc) {
            // log out failed wait for diagnostic purposes only;
            // waitForText failures are sometimes expected/normal
            mAsserter.dumpLog("waitForText timeout on "+text);
        }
        return rc;
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
                if (!waitForEnabledText(itemName)) {
                    mSolo.scrollDown();
                }
                mSolo.clickOnText(itemName);
            }
        }
    }

    public final void selectMenuItem(String menuItemName) {
        // build the item name ready to be used
        String itemName = "^" + menuItemName + "$";
        mActions.sendSpecialKey(Actions.SpecialKey.MENU);
        if (waitForText(itemName)) {
            mSolo.clickOnText(itemName);
        } else {
            // Older versions of Android have additional settings under "More",
            // including settings that newer versions have under "Tools."
            if (mSolo.searchText("(^More$|^Tools$)")) {
                mSolo.clickOnText("(^More$|^Tools$)");
            }
            waitForText(itemName);
            mSolo.clickOnText(itemName);
        }
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

    public final void verifyPageTitle(String title) {
        final TextView urlBarTitle = (TextView) mSolo.getView(R.id.url_bar_title);
        String pageTitle = null;
        if (urlBarTitle != null) {
            // Wait for the title to make sure it has been displayed in case the view
            // does not update fast enough
            waitForCondition(new VerifyTextViewText(urlBarTitle, title), MAX_WAIT_MS);
            pageTitle = urlBarTitle.getText().toString();
        }
        mAsserter.is(pageTitle, title, "Page title is correct");
    }

    public final void verifyTabCount(int expectedTabCount) {
        Element tabCount = mDriver.findElement(getActivity(), R.id.tabs_counter);
        String tabCountText = tabCount.getText();
        int tabCountInt = Integer.parseInt(tabCountText);
        mAsserter.is(tabCountInt, expectedTabCount, "The correct number of tabs are opened");
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

    // Used to hide/show the virtual keyboard
    public void toggleVKB() {
        InputMethodManager imm = (InputMethodManager) getActivity().getSystemService(Activity.INPUT_METHOD_SERVICE);
        imm.toggleSoftInput(InputMethodManager.HIDE_IMPLICIT_ONLY, 0);
    }

    public void addTab() {
        mSolo.clickOnView(mSolo.getView(R.id.tabs));
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
        mSolo.clickOnView(mSolo.getView(R.id.add_tab));
    }

    public void addTab(String url) {
        addTab();

        // Adding a new tab opens about:home, so now we just need to load the url in it.
        inputAndLoadUrl(url);
    }

    /**
     * Gets the AdapterView of the tabs list.
     *
     * @return List view in the tabs tray
     */
    private final AdapterView<ListAdapter> getTabsList() {
        Element tabs = mDriver.findElement(getActivity(), R.id.tabs);
        tabs.click();
        return (AdapterView<ListAdapter>) getActivity().findViewById(R.id.normal_tabs);
    }

    /**
     * Gets the view in the tabs tray at the specified index.
     *
     * @return View at index
     */
    private View getTabViewAt(final int index) {
        final View[] childView = { null };

        final AdapterView<ListAdapter> view = getTabsList();

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

    /**
     * Closes the tab at the specified index.
     *
     * @param index Index of tab to close
     */
    public void closeTabAt(final int index) {
        View closeButton = getTabViewAt(index).findViewById(R.id.close);

        mSolo.clickOnView(closeButton);
    }

    public final void runOnUiThreadSync(Runnable runnable) {
        RobocopUtils.runOnUiThreadSync(mActivity, runnable);
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

    public void clearPrivateData() {
        selectSettingsItem(StringHelper.PRIVACY_SECTION_LABEL, StringHelper.CLEAR_PRIVATE_DATA_LABEL);
        Actions.EventExpecter clearData = mActions.expectGeckoEvent("Sanitize:Finished");
        mSolo.clickOnText("Clear data");
        clearData.blockForEvent();
        clearData.unregisterListener();
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

        public void rotate() {
            if (getActivity().getRequestedOrientation () == ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE) {
                mSolo.setActivityOrientation(Solo.PORTRAIT);
            } else {
                mSolo.setActivityOrientation(Solo.LANDSCAPE);
            }
        }
    }

    class Navigation {
        private String devType;
        private String osVersion;

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
                mActions.sendSpecialKey(Actions.SpecialKey.BACK);
            }

            pageShowExpecter.blockForEvent();
            pageShowExpecter.unregisterListener();
        }

        public void forward() {
            Actions.EventExpecter pageShowExpecter = mActions.expectGeckoEvent("Content:PageShow");

            if (devType.equals("tablet")) {
                Element fwdBtn = mDriver.findElement(getActivity(), R.id.forward);
                fwdBtn.click();
            } else {
                mActions.sendSpecialKey(Actions.SpecialKey.MENU);
                waitForText("^New Tab$");
                if (!osVersion.equals("2.x")) {
                    Element fwdBtn = mDriver.findElement(getActivity(), R.id.forward);
                    fwdBtn.click();
                } else {
                    mSolo.clickOnText("^Forward$");
                }
                ensureMenuClosed();
            }

            pageShowExpecter.blockForEvent();
            pageShowExpecter.unregisterListener();
        }

        public void reload() {
            if (devType.equals("tablet")) {
                Element reloadBtn = mDriver.findElement(getActivity(), R.id.reload);
                reloadBtn.click();
            } else {
                mActions.sendSpecialKey(Actions.SpecialKey.MENU);
                waitForText("^New Tab$");
                if (!osVersion.equals("2.x")) {
                    Element reloadBtn = mDriver.findElement(getActivity(), R.id.reload);
                    reloadBtn.click();
                } else {
                    mSolo.clickOnText("^Reload$");
                }
                ensureMenuClosed();
            }
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
                mActions.sendSpecialKey(Actions.SpecialKey.BACK);
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
        private String mDescr;
        private Class<T> mCls;

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
    public void setPreferenceAndWaitForChange(final JSONObject jsonPref) {
        mActions.sendGeckoEvent("Preferences:Set", jsonPref.toString());

        // Get the preference name from the json and store it in an array. This array 
        // will be used later while fetching the preference data.
        String[] prefNames = new String[1];
        try {
            prefNames[0] = jsonPref.getString("name");
        } catch (JSONException e) {
            mAsserter.ok(false, "Exception in setPreferenceAndWaitForChange", getStackTraceString(e));
        }

        // Wait for confirmation of the pref change before proceeding with the test.
        final int ourRequestID = mPreferenceRequestID--;
        final Actions.RepeatedEventExpecter eventExpecter = mActions.expectGeckoEvent("Preferences:Data");
        mActions.sendPreferencesGetEvent(ourRequestID, prefNames);

        // Wait until we get the correct "Preferences:Data" event
        waitForCondition(new Condition() {
            final long endTime = SystemClock.elapsedRealtime() + MAX_WAIT_BLOCK_FOR_EVENT_DATA_MS;

            @Override
            public boolean isSatisfied() {
                try {
                    long timeout = endTime - SystemClock.elapsedRealtime();
                    if (timeout < 0) {
                        timeout = 0;
                    }

                    JSONObject data = new JSONObject(eventExpecter.blockForEventDataWithTimeout(timeout));
                    int requestID = data.getInt("requestId");
                    if (requestID != ourRequestID) {
                        return false;
                    }

                    JSONArray preferences = data.getJSONArray("preferences");
                    mAsserter.is(preferences.length(), 1, "Expecting preference array to have one element");
                    JSONObject prefs = (JSONObject) preferences.get(0);
                    mAsserter.is(prefs.getString("name"), jsonPref.getString("name"),
                            "Expecting returned preference name to be the same as the set name");
                    mAsserter.is(prefs.getString("type"), jsonPref.getString("type"),
                            "Expecting returned preference type to be the same as the set type");
                    mAsserter.is(prefs.get("value"), jsonPref.get("value"),
                            "Expecting returned preference value to be the same as the set value");
                    return true;
                } catch(JSONException e) {
                    mAsserter.ok(false, "Exception in setPreferenceAndWaitForChange", getStackTraceString(e));
                    // Please the java compiler
                    return false;
                }
            }
        }, MAX_WAIT_BLOCK_FOR_EVENT_DATA_MS);

        eventExpecter.unregisterListener();
    }
}
