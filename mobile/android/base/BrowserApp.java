/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.gfx.ImmutableViewportMetrics;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.util.FloatUtils;
import org.mozilla.gecko.util.GamepadUtils;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UiAsyncTask;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.content.Intent;
import android.content.res.Configuration;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.PointF;
import android.graphics.Rect;
import android.net.Uri;
import android.nfc.NdefMessage;
import android.nfc.NdefRecord;
import android.nfc.NfcAdapter;
import android.nfc.NfcEvent;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.SubMenu;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.Interpolator;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.Toast;

import java.io.InputStream;
import java.net.URL;
import java.util.EnumSet;
import java.util.Vector;

abstract public class BrowserApp extends GeckoApp
                                 implements TabsPanel.TabsLayoutChangeListener,
                                            PropertyAnimator.PropertyAnimationListener,
                                            View.OnKeyListener {
    private static final String LOGTAG = "GeckoBrowserApp";

    private static final String PREF_CHROME_DYNAMICTOOLBAR = "browser.chrome.dynamictoolbar";

    private static final int TABS_ANIMATION_DURATION = 450;

    public static BrowserToolbar mBrowserToolbar;
    private AboutHomeContent mAboutHomeContent;
    private Boolean mAboutHomeShowing = null;
    protected Telemetry.Timer mAboutHomeStartupTimer = null;

    private static final int ADDON_MENU_OFFSET = 1000;
    private class MenuItemInfo {
        public int id;
        public String label;
        public String icon;
        public boolean checkable;
        public boolean checked;
        public boolean enabled;
        public boolean visible;
        public int parent;
    }

    private Vector<MenuItemInfo> mAddonMenuItemsCache;

    private PropertyAnimator mMainLayoutAnimator;

    private static final Interpolator sTabsInterpolator = new Interpolator() {
        @Override
        public float getInterpolation(float t) {
            t -= 1.0f;
            return t * t * t * t * t + 1.0f;
        }
    };

    private FindInPageBar mFindInPageBar;

    // We'll ask for feedback after the user launches the app this many times.
    private static final int FEEDBACK_LAUNCH_COUNT = 15;

    // Variables used for scrolling the toolbar on/off the page;

    // A drag has to move this amount multiplied by the height of the toolbar
    // before the toolbar will appear or disappear.
    private static final float TOOLBAR_MOVEMENT_THRESHOLD = 0.3f;

    // Whether the dynamic toolbar pref is enabled.
    private boolean mDynamicToolbarEnabled = false;

    // The last recorded touch event from onInterceptTouchEvent. These are
    // not updated until the movement threshold has been exceeded.
    private float mLastTouchX = 0.0f;
    private float mLastTouchY = 0.0f;

    // Because we can only scroll by integer amounts, we store the fractional
    // amounts to be applied here.
    private float mToolbarSubpixelAccumulation = 0.0f;

    // Used by onInterceptTouchEvent to lock the toolbar into an off or on
    // position.
    private boolean mToolbarLocked = false;

    // Whether the toolbar movement threshold has been passed by the current
    // drag.
    private boolean mToolbarThresholdPassed = false;

    // Toggled when the tabs tray is made visible to disable toolbar movement.
    private boolean mToolbarPinned = false;

    // Stored value of the toolbar height, so we know when it's changed.
    private int mToolbarHeight = 0;

    private Integer mPrefObserverId;

    @Override
    public void onTabChanged(Tab tab, Tabs.TabEvents msg, Object data) {
        switch(msg) {
            case LOCATION_CHANGE:
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    maybeCancelFaviconLoad(tab);
                }
                // fall through
            case SELECTED:
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    if ("about:home".equals(tab.getURL())) {
                        showAboutHome();

                        if (mDynamicToolbarEnabled) {
                            // Show the toolbar.
                            mBrowserToolbar.animateVisibility(true);
                        }
                    } else {
                        hideAboutHome();
                    }

                    // Dismiss any SiteIdentity Popup
                    SiteIdentityPopup.getInstance().dismiss();

                    final TabsPanel.Panel panel = tab.isPrivate()
                                                ? TabsPanel.Panel.PRIVATE_TABS
                                                : TabsPanel.Panel.NORMAL_TABS;
                    // Delay calling showTabs so that it does not modify the mTabsChangedListeners
                    // array while we are still iterating through the array.
                    ThreadUtils.postToUiThread(new Runnable() {
                        @Override
                        public void run() {
                            if (areTabsShown() && mTabsPanel.getCurrentPanel() != panel)
                                showTabs(panel);
                        }
                    });
                }
                break;
            case START:
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    invalidateOptionsMenu();

                    if (mDynamicToolbarEnabled) {
                        // Show the toolbar.
                        mBrowserToolbar.animateVisibility(true);
                    }
                }
                break;
            case LOAD_ERROR:
            case STOP:
            case MENU_UPDATED:
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    invalidateOptionsMenu();
                }
                break;
            case PAGE_SHOW:
                loadFavicon(tab);
                break;
            case LINK_ADDED:
                // If tab is not loading and the favicon is updated, we
                // want to load the image straight away. If tab is still
                // loading, we only load the favicon once the page's content
                // is fully loaded.
                if (tab.getState() != Tab.STATE_LOADING) {
                    loadFavicon(tab);
                }
                break;
        }
        super.onTabChanged(tab, msg, data);
    }

    @Override
    void handleClearHistory() {
        super.handleClearHistory();
        updateAboutHomeTopSites();
    }

    @Override
    public boolean onInterceptTouchEvent(View view, MotionEvent event) {
        if (!mDynamicToolbarEnabled || mToolbarPinned) {
            return super.onInterceptTouchEvent(view, event);
        }

        // Don't let the toolbar scroll at all if the page is shorter than
        // the screen height.
        ImmutableViewportMetrics metrics =
            mLayerView.getLayerClient().getViewportMetrics();
        if (metrics.getPageHeight() < metrics.getHeight()) {
            return super.onInterceptTouchEvent(view, event);
        }

        int action = event.getActionMasked();
        int pointerCount = event.getPointerCount();

        // Whenever there are no pointers left on the screen, tell the page
        // to clamp the viewport on fixed layer margin changes. This lets the
        // toolbar scrolling off the top of the page make the page scroll up
        // if it'd cause the page to go into overscroll, but only when there
        // are no pointers held down.
        mLayerView.getLayerClient().setClampOnFixedLayerMarginsChange(
            pointerCount == 0 || action == MotionEvent.ACTION_CANCEL ||
            action == MotionEvent.ACTION_UP);

        View toolbarView = mBrowserToolbar.getLayout();
        if (action == MotionEvent.ACTION_DOWN ||
            action == MotionEvent.ACTION_POINTER_DOWN) {
            if (pointerCount == 1) {
                mToolbarLocked = mToolbarThresholdPassed = false;
                mToolbarSubpixelAccumulation = 0.0f;
                mLastTouchX = event.getX();
                mLastTouchY = event.getY();
                return super.onInterceptTouchEvent(view, event);
            }

            // Animate the toolbar to the fully on/off position.
            mBrowserToolbar.animateVisibility(
                toolbarView.getScrollY() > toolbarView.getHeight() / 2 ?
                    false : true);
        }

        // If more than one pointer has been tracked, or we've locked the
        // toolbar movement, let the event pass through and be handled by the
        // PanZoomController for zooming.
        if (pointerCount > 1 || mToolbarLocked) {
            return super.onInterceptTouchEvent(view, event);
        }

        // If a pointer has been lifted so that there's only one pointer left,
        // unlock the toolbar and track that remaining pointer.
        if (pointerCount == 1 && action == MotionEvent.ACTION_POINTER_UP) {
            mLastTouchY = event.getY(1 - event.getActionIndex());
            return super.onInterceptTouchEvent(view, event);
        }

        // Handle scrolling the toolbar
        float eventX = event.getX();
        float eventY = event.getY();
        float deltaX = mLastTouchX - eventX;
        float deltaY = mLastTouchY - eventY;
        int toolbarY = toolbarView.getScrollY();
        int toolbarHeight = toolbarView.getHeight();

        // Check if we've passed the toolbar movement threshold
        if (!mToolbarThresholdPassed) {
            float threshold = toolbarHeight * TOOLBAR_MOVEMENT_THRESHOLD;
            if (Math.abs(deltaY) > threshold) {
                mToolbarThresholdPassed = true;
                // If we're scrolling downwards and the toolbar was hidden
                // when we started scrolling, lock it.
                if (deltaY > 0 && toolbarY == toolbarHeight) {
                    mToolbarLocked = true;
                    return super.onInterceptTouchEvent(view, event);
                }
            } else if (Math.abs(deltaX) > threshold) {
                // Any horizontal scrolling past the threshold should
                // initiate toolbar lock.
                mToolbarLocked = true;
                mToolbarThresholdPassed = true;
                return super.onInterceptTouchEvent(view, event);
            } else {
                // The threshold hasn't been passed. We don't want to update
                // the stored last touch position, so return here.
                return super.onInterceptTouchEvent(view, event);
            }
        } else if (action == MotionEvent.ACTION_MOVE) {
            // Cancel any ongoing animation before we start moving the toolbar.
            mBrowserToolbar.cancelVisibilityAnimation();

            // Move the toolbar by the amount the touch event has moved,
            // clamping to fully visible or fully hidden.

            // Don't let the toolbar scroll off the top if it's just exposing
            // overscroll area.
            float toolbarMaxY = Math.min(toolbarHeight,
                Math.max(0, toolbarHeight - (metrics.pageRectTop -
                                             metrics.viewportRectTop)));

            float newToolbarYf = Math.max(0, Math.min(toolbarMaxY,
                toolbarY + deltaY + mToolbarSubpixelAccumulation));
            int newToolbarY = Math.round(newToolbarYf);
            mToolbarSubpixelAccumulation = (newToolbarYf - newToolbarY);

            toolbarView.scrollTo(0, newToolbarY);

            // Reset tracking when the toolbar is fully visible or hidden.
            if (newToolbarY == 0 || newToolbarY == toolbarHeight) {
                mLastTouchY = eventY;
            }
        } else if (action == MotionEvent.ACTION_UP ||
                   action == MotionEvent.ACTION_CANCEL) {
            // Animate the toolbar to fully on or off, depending on how much
            // of it is hidden and the current swipe velocity.
            mBrowserToolbar.animateVisibilityWithVelocityBias(
                toolbarY > toolbarHeight / 2 ? false : true,
                mLayerView.getPanZoomController().getVelocityVector().y);
        }

        // Update the last recorded position.
        mLastTouchX = eventX;
        mLastTouchY = eventY;

        return super.onInterceptTouchEvent(view, event);
    }

    @Override
    public boolean onKey(View v, int keyCode, KeyEvent event) {
        // Global onKey handler. This is called if the focused UI doesn't
        // handle the key event, and before Gecko swallows the events.
        if (event.getAction() != KeyEvent.ACTION_DOWN) {
            return false;
        }

        // Gamepad support only exists in API-level >= 9
        if (Build.VERSION.SDK_INT >= 9 &&
            (event.getSource() & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD) {
            switch (keyCode) {
                case KeyEvent.KEYCODE_BUTTON_Y:
                    // Toggle/focus the address bar on gamepad-y button.
                    if (mBrowserToolbar.isVisible()) {
                        if (mDynamicToolbarEnabled &&
                            Boolean.FALSE.equals(mAboutHomeShowing)) {
                            mBrowserToolbar.animateVisibility(false);
                            mLayerView.requestFocus();
                        } else {
                            // Just focus the address bar when about:home is visible
                            // or when the dynamic toolbar isn't enabled.
                            mBrowserToolbar.requestFocusFromTouch();
                        }
                    } else {
                        mBrowserToolbar.animateVisibility(true);
                        mBrowserToolbar.requestFocusFromTouch();
                    }
                    return true;
                case KeyEvent.KEYCODE_BUTTON_L1:
                    // Go back on L1
                    Tabs.getInstance().getSelectedTab().doBack();
                    return true;
                case KeyEvent.KEYCODE_BUTTON_R1:
                    // Go forward on R1
                    Tabs.getInstance().getSelectedTab().doForward();
                    return true;
            }
        }

        return false;
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (onKey(null, keyCode, event)) {
            return true;
        }

        return super.onKeyDown(keyCode, event);
    }

    void handleReaderAdded(boolean success, final String title, final String url) {
        if (!success) {
            showToast(R.string.reading_list_failed, Toast.LENGTH_SHORT);
            return;
        }

        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                BrowserDB.addReadingListItem(getContentResolver(), title, url);
                showToast(R.string.reading_list_added, Toast.LENGTH_SHORT);
            }
        });
    }

    void handleReaderRemoved(final String url) {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                BrowserDB.removeReadingListItemWithURL(getContentResolver(), url);
                showToast(R.string.reading_list_removed, Toast.LENGTH_SHORT);
            }
        });
    }

    @Override
    void onStatePurged() {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                if (mAboutHomeContent != null)
                    mAboutHomeContent.setLastTabsVisibility(false);
            }
        });

        super.onStatePurged();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        mAboutHomeStartupTimer = new Telemetry.Timer("FENNEC_STARTUP_TIME_ABOUTHOME");

        super.onCreate(savedInstanceState);

        LinearLayout actionBar = (LinearLayout) getActionBarLayout();
        mMainLayout.addView(actionBar, 2);

        ((GeckoApp.MainLayout) mMainLayout).setTouchEventInterceptor(new HideTabsTouchListener());
        ((GeckoApp.MainLayout) mMainLayout).setMotionEventInterceptor(new MotionEventInterceptor() {
            @Override
            public boolean onInterceptMotionEvent(View view, MotionEvent event) {
                // If we get a gamepad panning MotionEvent while the focus is not on the layerview,
                // put the focus on the layerview and carry on
                LayerView layerView = mLayerView;
                if (layerView != null && !layerView.hasFocus() && GamepadUtils.isPanningControl(event)) {
                    layerView.requestFocus();
                }
                return false;
            }
        });


        mBrowserToolbar = new BrowserToolbar(this);
        mBrowserToolbar.from(actionBar);

        // Intercept key events for gamepad shortcuts
        actionBar.setOnKeyListener(this);

        if (mTabsPanel != null) {
            mTabsPanel.setTabsLayoutChangeListener(this);
            updateSideBarState();
        }

        mFindInPageBar = (FindInPageBar) findViewById(R.id.find_in_page);

        registerEventListener("CharEncoding:Data");
        registerEventListener("CharEncoding:State");
        registerEventListener("Feedback:LastUrl");
        registerEventListener("Feedback:OpenPlayStore");
        registerEventListener("Feedback:MaybeLater");
        registerEventListener("Telemetry:Gather");

        Distribution.init(this, getPackageResourcePath());
        JavaAddonManager.getInstance().init(getApplicationContext());

        if (Build.VERSION.SDK_INT >= 14) {
            NfcAdapter nfc = NfcAdapter.getDefaultAdapter(this);
            if (nfc != null) {
                nfc.setNdefPushMessageCallback(new NfcAdapter.CreateNdefMessageCallback() {
                    @Override
                    public NdefMessage createNdefMessage(NfcEvent event) {
                        Tab tab = Tabs.getInstance().getSelectedTab();
                        if (tab == null || tab.isPrivate()) {
                            return null;
                        }
                        return new NdefMessage(new NdefRecord[] { NdefRecord.createUri(tab.getURL()) });
                    }
                }, this);
            }
        }

        // Listen to the dynamic toolbar pref
        mPrefObserverId = PrefsHelper.getPref(PREF_CHROME_DYNAMICTOOLBAR, new PrefsHelper.PrefHandlerBase() {
            @Override
            public void prefValue(String pref, boolean value) {
                if (value == mDynamicToolbarEnabled) {
                    return;
                }
                mDynamicToolbarEnabled = value;

                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (mDynamicToolbarEnabled) {
                            setToolbarMargin(0);
                        } else {
                            // Immediately show the toolbar when disabling the dynamic
                            // toolbar.
                            mAboutHomeContent.setPadding(0, 0, 0, 0);
                            mBrowserToolbar.cancelVisibilityAnimation();
                            mBrowserToolbar.getLayout().scrollTo(0, 0);
                        }

                        // Refresh the margins to reset the padding on the spacer and
                        // make sure that Gecko is in sync.
                        ((BrowserToolbarLayout)mBrowserToolbar.getLayout()).refreshMargins();
                    }
                });
            }

            @Override
            public boolean isObserver() {
                // We want to be notified of changes to be able to switch mode
                // without restarting.
                return true;
            }
        });
    }

    @Override
    public void onDestroy() {
        if (mPrefObserverId != null) {
            PrefsHelper.removeObserver(mPrefObserverId);
            mPrefObserverId = null;
        }
        if (mAboutHomeContent != null)
            mAboutHomeContent.onDestroy();
        if (mBrowserToolbar != null)
            mBrowserToolbar.onDestroy();

        unregisterEventListener("CharEncoding:Data");
        unregisterEventListener("CharEncoding:State");
        unregisterEventListener("Feedback:LastUrl");
        unregisterEventListener("Feedback:OpenPlayStore");
        unregisterEventListener("Feedback:MaybeLater");
        unregisterEventListener("Telemetry:Gather");

        if (Build.VERSION.SDK_INT >= 14) {
            NfcAdapter nfc = NfcAdapter.getDefaultAdapter(this);
            if (nfc != null) {
                // null this out even though the docs say it's not needed,
                // because the source code looks like it will only do this
                // automatically on API 14+
                nfc.setNdefPushMessageCallback(null, this);
            }
        }

        super.onDestroy();
    }

    @Override
    public void onContentChanged() {
        super.onContentChanged();
        if (mAboutHomeContent != null)
            mAboutHomeContent.onActivityContentChanged();
    }

    @Override
    protected void finishProfileMigration() {
        // Update about:home with the new information.
        updateAboutHomeTopSites();

        super.finishProfileMigration();
    }

    @Override
    protected void initializeChrome(String uri, boolean isExternalURL) {
        super.initializeChrome(uri, isExternalURL);

        mBrowserToolbar.updateBackButton(false);
        mBrowserToolbar.updateForwardButton(false);
        ((BrowserToolbarLayout)mBrowserToolbar.getLayout()).refreshMargins();

        mDoorHangerPopup.setAnchor(mBrowserToolbar.mFavicon);

        if (isExternalURL || mRestoreMode != RESTORE_NONE) {
            mAboutHomeStartupTimer.cancel();
        }

        if (!mIsRestoringActivity) {
            if (!isExternalURL) {
                // show about:home if we aren't restoring previous session
                if (mRestoreMode == RESTORE_NONE) {
                    Tab tab = Tabs.getInstance().loadUrl("about:home", Tabs.LOADURL_NEW_TAB);
                } else {
                    hideAboutHome();
                }
            } else {
                int flags = Tabs.LOADURL_NEW_TAB | Tabs.LOADURL_USER_ENTERED;
                Tabs.getInstance().loadUrl(uri, flags);
            }
        }

        // Intercept key events for gamepad shortcuts
        mLayerView.setOnKeyListener(this);
    }

    private void setSidebarMargin(int margin) {
        ((RelativeLayout.LayoutParams) mGeckoLayout.getLayoutParams()).leftMargin = margin;
        mGeckoLayout.requestLayout();
    }

    private void setToolbarMargin(int margin) {
        ((RelativeLayout.LayoutParams) mGeckoLayout.getLayoutParams()).topMargin = margin;
        mGeckoLayout.requestLayout();
    }

    public void setToolbarHeight(int aHeight, int aVisibleHeight) {
        if (!mDynamicToolbarEnabled || Boolean.TRUE.equals(mAboutHomeShowing)) {
            // Use aVisibleHeight here so that when the dynamic toolbar is
            // enabled, the padding will animate with the toolbar becoming
            // visible.
            if (mDynamicToolbarEnabled) {
                // When the dynamic toolbar is enabled, set the padding on the
                // about:home widget directly - this is to avoid resizing the
                // LayerView, which can cause visible artifacts.
                mAboutHomeContent.setPadding(0, aVisibleHeight, 0, 0);
            } else {
                setToolbarMargin(aVisibleHeight);
            }
            aHeight = aVisibleHeight = 0;
        } else {
            setToolbarMargin(0);
        }

        // Update the Gecko-side global for fixed viewport margins.
        if (aHeight != mToolbarHeight) {
            mToolbarHeight = aHeight;

            // In the current UI, this is the only place we have need of
            // viewport margins (to stop the toolbar from obscuring fixed-pos
            // content).
            GeckoAppShell.sendEventToGecko(
                GeckoEvent.createBroadcastEvent("Viewport:FixedMarginsChanged",
                    "{ \"top\" : " + aHeight + ", \"right\" : 0, \"bottom\" : 0, \"left\" : 0 }"));
        }

        if (mLayerView != null) {
            mLayerView.getLayerClient().setFixedLayerMargins(0, aVisibleHeight, 0, 0);

            // Force a redraw when the view isn't moving and the toolbar is
            // fully visible or fully hidden. This is to make sure that the
            // Gecko-side fixed viewport margins are in sync when the view and
            // bar aren't animating.
            PointF velocityVector = mLayerView.getPanZoomController().getVelocityVector();
            if ((aVisibleHeight == 0 || aVisibleHeight == aHeight) &&
                FloatUtils.fuzzyEquals(velocityVector.x, 0.0f) &&
                FloatUtils.fuzzyEquals(velocityVector.y, 0.0f)) {
                mLayerView.getLayerClient().forceRedraw();
            }
        }
    }

    @Override
    void toggleChrome(final boolean aShow) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                if (aShow) {
                    mBrowserToolbar.show();
                } else {
                    mBrowserToolbar.hide();
                    if (hasTabsSideBar()) {
                        hideTabs();
                    }
                }
            }
        });

        super.toggleChrome(aShow);
    }

    @Override
    void focusChrome() {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                mBrowserToolbar.show();
                mBrowserToolbar.requestFocusFromTouch();
            }
        });
    }

    @Override
    public void refreshChrome() {
        // Only ICS phones use a smaller action-bar in landscape mode.
        if (Build.VERSION.SDK_INT >= 14 && !isTablet()) {
            int index = mMainLayout.indexOfChild(mBrowserToolbar.getLayout());
            mMainLayout.removeViewAt(index);

            LinearLayout actionBar = (LinearLayout) getActionBarLayout();
            mMainLayout.addView(actionBar, index);
            mBrowserToolbar.from(actionBar);
            mBrowserToolbar.refresh();

            // The favicon view is different now, so we need to update the DoorHangerPopup anchor view.
            if (mDoorHangerPopup != null)
                mDoorHangerPopup.setAnchor(mBrowserToolbar.mFavicon);
        }

        invalidateOptionsMenu();
        updateSideBarState();
        mTabsPanel.refresh();

        if (mAboutHomeContent != null)
            mAboutHomeContent.refresh();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        String url = resultCode == Activity.RESULT_OK ? data.getStringExtra(AwesomeBar.URL_KEY) : null;
        mBrowserToolbar.fromAwesomeBarSearch(url);
    }

    public View getActionBarLayout() {
        int actionBarRes;

        if (!hasPermanentMenuKey() || isTablet())
           actionBarRes = R.layout.browser_toolbar_menu;
        else
           actionBarRes = R.layout.browser_toolbar;

        LinearLayout actionBar = (LinearLayout) LayoutInflater.from(this).inflate(actionBarRes, null);
        actionBar.setLayoutParams(new LinearLayout.LayoutParams(LinearLayout.LayoutParams.FILL_PARENT,
                                                                (int) getResources().getDimension(R.dimen.browser_toolbar_height)));
        return actionBar;
    }

    @Override
    public boolean hasTabsSideBar() {
        return (mTabsPanel != null && mTabsPanel.isSideBar());
    }

    private void updateSideBarState() {
        if (mMainLayoutAnimator != null)
            mMainLayoutAnimator.stop();

        boolean isSideBar = (GeckoAppShell.isTablet() && mOrientation == Configuration.ORIENTATION_LANDSCAPE);

        ViewGroup.LayoutParams lp = mTabsPanel.getLayoutParams();
        if (isSideBar) {
            lp.width = getResources().getDimensionPixelSize(R.dimen.tabs_sidebar_width);
        } else {
            lp.width = ViewGroup.LayoutParams.FILL_PARENT;
        }
        mTabsPanel.requestLayout();

        final boolean changed = (mTabsPanel.isSideBar() != isSideBar);
        final boolean needsRelayout = (changed && mTabsPanel.isShown());

        if (needsRelayout) {
            final int width;
            final int scrollY;

            if (isSideBar) {
                width = lp.width;
                mMainLayout.scrollTo(0, 0);
            } else {
                width = 0;
            }

            mBrowserToolbar.adjustForTabsLayout(width);
            setSidebarMargin(width);
        }

        if (changed) {
            // Cancel state of previous sidebar state
            mBrowserToolbar.updateTabs(false);

            mTabsPanel.setIsSideBar(isSideBar);
            mBrowserToolbar.setIsSideBar(isSideBar);

            // Update with new sidebar state
            mBrowserToolbar.updateTabs(mTabsPanel.isShown());
        }
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        try {
            if (event.equals("Menu:Add")) {
                MenuItemInfo info = new MenuItemInfo();
                info.label = message.getString("name");
                info.id = message.getInt("id") + ADDON_MENU_OFFSET;
                info.checkable = false;
                info.checked = false;
                info.enabled = true;
                info.visible = true;
                String iconRes = null;
                try { // icon is optional
                    iconRes = message.getString("icon");
                } catch (Exception ex) { }
                info.icon = iconRes;
                try {
                    info.checkable = message.getBoolean("checkable");
                } catch (Exception ex) { }
                try { // parent is optional
                    info.parent = message.getInt("parent") + ADDON_MENU_OFFSET;
                } catch (Exception ex) { }
                final MenuItemInfo menuItemInfo = info;
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        addAddonMenuItem(menuItemInfo);
                    }
                });
            } else if (event.equals("Menu:Remove")) {
                final int id = message.getInt("id") + ADDON_MENU_OFFSET;
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        removeAddonMenuItem(id);
                    }
                });
            } else if (event.equals("Menu:Update")) {
                final int id = message.getInt("id") + ADDON_MENU_OFFSET;
                final JSONObject options = message.getJSONObject("options");
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        updateAddonMenuItem(id, options);
                    }
                });
            } else if (event.equals("CharEncoding:Data")) {
                final JSONArray charsets = message.getJSONArray("charsets");
                int selected = message.getInt("selected");

                final int len = charsets.length();
                final String[] titleArray = new String[len];
                for (int i = 0; i < len; i++) {
                    JSONObject charset = charsets.getJSONObject(i);
                    titleArray[i] = charset.getString("title");
                }

                final AlertDialog.Builder dialogBuilder = new AlertDialog.Builder(this);
                dialogBuilder.setSingleChoiceItems(titleArray, selected, new AlertDialog.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        try {
                            JSONObject charset = charsets.getJSONObject(which);
                            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("CharEncoding:Set", charset.getString("code")));
                            dialog.dismiss();
                        } catch (JSONException e) {
                            Log.e(LOGTAG, "error parsing json", e);
                        }
                    }
                });
                dialogBuilder.setNegativeButton(R.string.button_cancel, new AlertDialog.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        dialog.dismiss();
                    }
                });
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        dialogBuilder.show();
                    }
                });
            } else if (event.equals("CharEncoding:State")) {
                final boolean visible = message.getString("visible").equals("true");
                GeckoPreferences.setCharEncodingState(visible);
                final Menu menu = mMenu;
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (menu != null)
                            menu.findItem(R.id.char_encoding).setVisible(visible);
                    }
                });
            } else if (event.equals("Feedback:OpenPlayStore")) {
                Intent intent = new Intent(Intent.ACTION_VIEW);
                intent.setData(Uri.parse("market://details?id=" + getPackageName()));
                startActivity(intent);
            } else if (event.equals("Feedback:MaybeLater")) {
                resetFeedbackLaunchCount();
            } else if (event.equals("Feedback:LastUrl")) {
                getLastUrl();
            } else if (event.equals("Gecko:Ready")) {
                // Handle this message in GeckoApp, but also enable the Settings
                // menuitem, which is specific to BrowserApp.
                super.handleMessage(event, message);
                final Menu menu = mMenu;
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (menu != null)
                            menu.findItem(R.id.settings).setEnabled(true);
                    }
                });
            } else if (event.equals("Telemetry:Gather")) {
                Telemetry.HistogramAdd("PLACES_PAGES_COUNT", BrowserDB.getCount(getContentResolver(), "history"));
                Telemetry.HistogramAdd("PLACES_BOOKMARKS_COUNT", BrowserDB.getCount(getContentResolver(), "bookmarks"));
                Telemetry.HistogramAdd("FENNEC_FAVICONS_COUNT", BrowserDB.getCount(getContentResolver(), "favicons"));
                Telemetry.HistogramAdd("FENNEC_THUMBNAILS_COUNT", BrowserDB.getCount(getContentResolver(), "thumbnails"));
            } else if (event.equals("Reader:Added")) {
                final boolean success = message.getBoolean("success");
                final String title = message.getString("title");
                final String url = message.getString("url");
                handleReaderAdded(success, title, url);
            } else if (event.equals("Reader:Removed")) {
                final String url = message.getString("url");
                handleReaderRemoved(url);
            } else if (event.equals("Reader:Share")) {
                final String title = message.getString("title");
                final String url = message.getString("url");
                GeckoAppShell.openUriExternal(url, "text/plain", "", "",
                                              Intent.ACTION_SEND, title);
            } else {
                super.handleMessage(event, message);
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception handling message \"" + event + "\":", e);
        }
    }

    @Override
    public void addTab() {
        showAwesomebar(AwesomeBar.Target.NEW_TAB);
    }

    @Override
    public void addPrivateTab() {
        Tabs.getInstance().loadUrl("about:privatebrowsing", Tabs.LOADURL_NEW_TAB | Tabs.LOADURL_PRIVATE);
    }

    @Override
    public void showNormalTabs() {
        showTabs(TabsPanel.Panel.NORMAL_TABS);
    }

    @Override
    public void showPrivateTabs() {
        showTabs(TabsPanel.Panel.PRIVATE_TABS);
    }

    @Override
    public void showRemoteTabs() {
        showTabs(TabsPanel.Panel.REMOTE_TABS);
    }

    private void showTabs(TabsPanel.Panel panel) {
        if (Tabs.getInstance().getDisplayCount() == 0)
            return;

        mTabsPanel.show(panel);
    }

    @Override
    public void hideTabs() {
        mTabsPanel.hide();
    }

    @Override
    public boolean autoHideTabs() {
        if (!hasTabsSideBar() && areTabsShown()) {
            hideTabs();
            return true;
        }
        return false;
    }

    @Override
    public boolean areTabsShown() {
        return mTabsPanel.isShown();
    }

    @Override
    public void onTabsLayoutChange(int width, int height) {
        int animationLength = TABS_ANIMATION_DURATION;

        if (mMainLayoutAnimator != null) {
            animationLength = Math.max(1, animationLength - (int)mMainLayoutAnimator.getRemainingTime());
            mMainLayoutAnimator.stop(false);
        }

        if (mTabsPanel.isShown())
            mTabsPanel.setDescendantFocusability(ViewGroup.FOCUS_AFTER_DESCENDANTS);

        mMainLayoutAnimator = new PropertyAnimator(animationLength, sTabsInterpolator);
        mMainLayoutAnimator.setPropertyAnimationListener(this);

        boolean usingTextureView = mLayerView.shouldUseTextureView();
        mMainLayoutAnimator.setUseHardwareLayer(usingTextureView);

        if (hasTabsSideBar()) {
            mBrowserToolbar.prepareTabsAnimation(mMainLayoutAnimator, width);

            // Set the gecko layout for sliding.
            if (!mTabsPanel.isShown()) {
                mGeckoLayout.scrollTo(mTabsPanel.getWidth() * -1, 0);
                setSidebarMargin(0);
            }

            mMainLayoutAnimator.attach(mGeckoLayout,
                                       PropertyAnimator.Property.SCROLL_X,
                                       -width);
        } else {
            mMainLayoutAnimator.attach(mMainLayout,
                                       PropertyAnimator.Property.SCROLL_Y,
                                       -height);
        }

        // If the tabs layout is animating onto the screen, pin the dynamic
        // toolbar.
        if (width > 0 && height > 0) {
            mToolbarPinned = true;
            mBrowserToolbar.animateVisibility(true);
        } else {
            mToolbarPinned = false;
        }

        mMainLayoutAnimator.start();
    }

    @Override
    public void onPropertyAnimationStart() {
        mBrowserToolbar.updateTabs(true);

        // Although the tabs panel is not animating per se, it will be re-drawn several
        // times while the main/gecko layout slides to left/top. Adding a hardware layer
        // here considerably improves the frame rate of the animation.
        if (Build.VERSION.SDK_INT >= 11)
            mTabsPanel.setLayerType(View.LAYER_TYPE_HARDWARE, null);
        else
            mTabsPanel.setDrawingCacheEnabled(true);
    }

    @Override
    public void onPropertyAnimationEnd() {
        // Destroy the hardware layer used during the animation
        if (Build.VERSION.SDK_INT >= 11)
            mTabsPanel.setLayerType(View.LAYER_TYPE_NONE, null);
        else
            mTabsPanel.setDrawingCacheEnabled(false);

        if (mTabsPanel.isShown()) {
            if (hasTabsSideBar()) {
                setSidebarMargin(mTabsPanel.getWidth());
                mGeckoLayout.scrollTo(0, 0);
            }

            mGeckoLayout.requestLayout();
        } else {
            mTabsPanel.setVisibility(View.INVISIBLE);
            mBrowserToolbar.updateTabs(false);
            mBrowserToolbar.finishTabsAnimation();
            mTabsPanel.setDescendantFocusability(ViewGroup.FOCUS_BLOCK_DESCENDANTS);
        }

        mBrowserToolbar.refreshBackground();

        if (hasTabsSideBar())
            mBrowserToolbar.adjustTabsAnimation(true);

        mMainLayoutAnimator = null;
    }

    /* Favicon methods */
    private void loadFavicon(final Tab tab) {
        maybeCancelFaviconLoad(tab);

        long id = Favicons.getInstance().loadFavicon(tab.getURL(), tab.getFaviconURL(), !tab.isPrivate(),
                        new Favicons.OnFaviconLoadedListener() {

            @Override
            public void onFaviconLoaded(String pageUrl, Bitmap favicon) {
                // Leave favicon UI untouched if we failed to load the image
                // for some reason.
                if (favicon == null)
                    return;

                // The tab might be pointing to another URL by the time the
                // favicon is finally loaded, in which case we simply ignore it.
                if (!tab.getURL().equals(pageUrl))
                    return;

                tab.updateFavicon(favicon);
                tab.setFaviconLoadId(Favicons.NOT_LOADING);

                Tabs.getInstance().notifyListeners(tab, Tabs.TabEvents.FAVICON);
            }
        });

        tab.setFaviconLoadId(id);
    }

    private void maybeCancelFaviconLoad(Tab tab) {
        long faviconLoadId = tab.getFaviconLoadId();

        if (faviconLoadId == Favicons.NOT_LOADING)
            return;

        // Cancel pending favicon load task
        Favicons.getInstance().cancelFaviconLoad(faviconLoadId);

        // Reset favicon load state
        tab.setFaviconLoadId(Favicons.NOT_LOADING);
    }


    /* About:home UI */
    void updateAboutHomeTopSites() {
        if (mAboutHomeContent == null)
            return;

        mAboutHomeContent.update(EnumSet.of(AboutHomeContent.UpdateFlags.TOP_SITES));
    }

    private void showAboutHome() {
        // Don't create an additional AboutHomeRunnable if about:home
        // is already visible.
        if (mAboutHomeShowing != null && mAboutHomeShowing)
            return;

        mAboutHomeShowing = true;
        Runnable r = new AboutHomeRunnable(true);
        ThreadUtils.getUiHandler().postAtFrontOfQueue(r);
    }

    private void hideAboutHome() {
        // If hideAboutHome gets called before showAboutHome, we still need
        // to create an AboutHomeRunnable to hide the about:home content.
        if (mAboutHomeShowing != null && !mAboutHomeShowing)
            return;

        mBrowserToolbar.setShadowVisibility(true);
        mAboutHomeShowing = false;
        Runnable r = new AboutHomeRunnable(false);
        ThreadUtils.getUiHandler().postAtFrontOfQueue(r);
    }

    private class AboutHomeRunnable implements Runnable {
        boolean mShow;
        AboutHomeRunnable(boolean show) {
            mShow = show;
        }

        @Override
        public void run() {
            if (mShow) {
                if (mAboutHomeContent == null) {
                    mAboutHomeContent = (AboutHomeContent) findViewById(R.id.abouthome_content);
                    mAboutHomeContent.init();
                    mAboutHomeContent.update(AboutHomeContent.UpdateFlags.ALL);
                    mAboutHomeContent.setUriLoadCallback(new AboutHomeContent.UriLoadCallback() {
                        @Override
                        public void callback(String url) {
                            mBrowserToolbar.setProgressVisibility(true);
                            Tabs.getInstance().loadUrl(url);
                        }
                    });
                    mAboutHomeContent.setLoadCompleteCallback(new AboutHomeContent.VoidCallback() {
                        @Override
                        public void callback() {
                            mAboutHomeStartupTimer.stop();
                        }
                    });
                } else {
                    mAboutHomeContent.update(EnumSet.of(AboutHomeContent.UpdateFlags.TOP_SITES,
                                                        AboutHomeContent.UpdateFlags.REMOTE_TABS));
                }
                mAboutHomeContent.setVisibility(View.VISIBLE);
            } else {
                findViewById(R.id.abouthome_content).setVisibility(View.GONE);
            }

            // Refresh margins to possibly restore the toolbar padding
            ((BrowserToolbarLayout)mBrowserToolbar.getLayout()).refreshMargins();
        }
    }

    private class HideTabsTouchListener implements TouchEventInterceptor {
        private boolean mIsHidingTabs = false;

        @Override
        public boolean onInterceptTouchEvent(View view, MotionEvent event) {
            // We need to account for scroll state for the touched view otherwise
            // tapping on an "empty" part of the view will still be considered a
            // valid touch event.
            if (view.getScrollX() != 0 || view.getScrollY() != 0) {
                Rect rect = new Rect();
                view.getHitRect(rect);
                rect.offset(-view.getScrollX(), -view.getScrollY());

                int[] viewCoords = new int[2];
                view.getLocationOnScreen(viewCoords);

                int x = (int) event.getRawX() - viewCoords[0];
                int y = (int) event.getRawY() - viewCoords[1];

                if (!rect.contains(x, y))
                    return false;
            }

            // If the tab tray is showing, hide the tab tray and don't send the event to content.
            if (event.getActionMasked() == MotionEvent.ACTION_DOWN && autoHideTabs()) {
                mIsHidingTabs = true;
                return true;
            }
            return false;
        }

        @Override
        public boolean onTouch(View view, MotionEvent event) {
            if (mIsHidingTabs) {
                // Keep consuming events until the gesture finishes.
                int action = event.getActionMasked();
                if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
                    mIsHidingTabs = false;
                }
                return true;
            }
            return false;
        }
    }

    private void addAddonMenuItem(final MenuItemInfo info) {
        if (mMenu == null) {
            if (mAddonMenuItemsCache == null)
                mAddonMenuItemsCache = new Vector<MenuItemInfo>();

            mAddonMenuItemsCache.add(info);
            return;
        }

        Menu menu;
        if (info.parent == 0) {
            menu = mMenu;
        } else {
            MenuItem parent = mMenu.findItem(info.parent);
            if (parent == null)
                return;

            if (!parent.hasSubMenu()) {
                mMenu.removeItem(parent.getItemId());
                menu = mMenu.addSubMenu(Menu.NONE, parent.getItemId(), Menu.NONE, parent.getTitle());
                if (parent.getIcon() != null)
                    ((SubMenu) menu).getItem().setIcon(parent.getIcon());
            } else {
                menu = parent.getSubMenu();
            }
        }

        final MenuItem item = menu.add(Menu.NONE, info.id, Menu.NONE, info.label);
        item.setOnMenuItemClickListener(new MenuItem.OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem item) {
                Log.i(LOGTAG, "menu item clicked");
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Menu:Clicked", Integer.toString(info.id - ADDON_MENU_OFFSET)));
                return true;
            }
        });

        if (info.icon != null) {
            if (info.icon.startsWith("data")) {
                BitmapDrawable drawable = new BitmapDrawable(BitmapUtils.getBitmapFromDataURI(info.icon));
                item.setIcon(drawable);
            }
            else if (info.icon.startsWith("jar:") || info.icon.startsWith("file://")) {
                ThreadUtils.postToBackgroundThread(new Runnable() {
                    @Override
                    public void run() {
                        try {
                            URL url = new URL(info.icon);
                            InputStream is = (InputStream) url.getContent();
                            try {
                                Drawable drawable = Drawable.createFromStream(is, "src");
                                item.setIcon(drawable);
                            } finally {
                                is.close();
                            }
                        } catch (Exception e) {
                            Log.w(LOGTAG, "Unable to set icon", e);
                        }
                    }
                });
            } else {
                item.setIcon(R.drawable.ic_menu_addons_filler);
            }
        } else {
            item.setIcon(R.drawable.ic_menu_addons_filler);
        }

        item.setCheckable(info.checkable);
        item.setChecked(info.checked);
        item.setEnabled(info.enabled);
        item.setVisible(info.visible);
    }

    private void removeAddonMenuItem(int id) {
        // Remove add-on menu item from cache, if available.
        if (mAddonMenuItemsCache != null && !mAddonMenuItemsCache.isEmpty()) {
            for (MenuItemInfo item : mAddonMenuItemsCache) {
                 if (item.id == id) {
                     mAddonMenuItemsCache.remove(item);
                     break;
                 }
            }
        }

        if (mMenu == null)
            return;

        MenuItem menuItem = mMenu.findItem(id);
        if (menuItem != null)
            mMenu.removeItem(id);
    }

    private void updateAddonMenuItem(int id, JSONObject options) {
        // Set attribute for the menu item in cache, if available
        if (mAddonMenuItemsCache != null && !mAddonMenuItemsCache.isEmpty()) {
            for (MenuItemInfo item : mAddonMenuItemsCache) {
                 if (item.id == id) {
                     try {
                        item.checkable = options.getBoolean("checkable");
                     } catch (JSONException e) {}

                     try {
                        item.checked = options.getBoolean("checked");
                     } catch (JSONException e) {}

                     try {
                        item.enabled = options.getBoolean("enabled");
                     } catch (JSONException e) {}

                     try {
                        item.visible = options.getBoolean("visible");
                     } catch (JSONException e) {}
                     break;
                 }
            }
        }

        if (mMenu == null)
            return;

        MenuItem menuItem = mMenu.findItem(id);
        if (menuItem != null) {
            try {
               menuItem.setCheckable(options.getBoolean("checkable"));
            } catch (JSONException e) {}

            try {
               menuItem.setChecked(options.getBoolean("checked"));
            } catch (JSONException e) {}

            try {
               menuItem.setEnabled(options.getBoolean("enabled"));
            } catch (JSONException e) {}

            try {
               menuItem.setVisible(options.getBoolean("visible"));
            } catch (JSONException e) {}
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        super.onCreateOptionsMenu(menu);

        // Inform the menu about the action-items bar. 
        if (menu instanceof GeckoMenu && isTablet())
            ((GeckoMenu) menu).setActionItemBarPresenter(mBrowserToolbar);

        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.browser_app_menu, mMenu);

        // Add add-on menu items if any.
        if (mAddonMenuItemsCache != null && !mAddonMenuItemsCache.isEmpty()) {
            for (MenuItemInfo item : mAddonMenuItemsCache) {
                 addAddonMenuItem(item);
            }

            mAddonMenuItemsCache.clear();
        }

        return true;
    }

    @Override
    public void openOptionsMenu() {
        if (!hasTabsSideBar() && areTabsShown())
            return;

        // Scroll custom menu to the top
        if (mMenuPanel != null)
            mMenuPanel.scrollTo(0, 0);

        if (!mBrowserToolbar.openOptionsMenu())
            super.openOptionsMenu();

        if (mDynamicToolbarEnabled)
            mBrowserToolbar.animateVisibility(true);
    }

    @Override
    public void closeOptionsMenu() {
        if (!mBrowserToolbar.closeOptionsMenu())
            super.closeOptionsMenu();
    }

    @Override
    public void setFullScreen(final boolean fullscreen) {
        super.setFullScreen(fullscreen);
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                if (fullscreen)
                    mBrowserToolbar.hide();
                else
                    mBrowserToolbar.show();
            }
        });
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu aMenu) {
        if (aMenu == null)
            return false;

        if (!GeckoThread.checkLaunchState(GeckoThread.LaunchState.GeckoRunning))
            aMenu.findItem(R.id.settings).setEnabled(false);

        Tab tab = Tabs.getInstance().getSelectedTab();
        MenuItem bookmark = aMenu.findItem(R.id.bookmark);
        MenuItem forward = aMenu.findItem(R.id.forward);
        MenuItem share = aMenu.findItem(R.id.share);
        MenuItem saveAsPDF = aMenu.findItem(R.id.save_as_pdf);
        MenuItem charEncoding = aMenu.findItem(R.id.char_encoding);
        MenuItem findInPage = aMenu.findItem(R.id.find_in_page);
        MenuItem desktopMode = aMenu.findItem(R.id.desktop_mode);

        // Only show the "Quit" menu item on pre-ICS. In ICS+, it's easy to
        // kill an app through the task switcher.
        aMenu.findItem(R.id.quit).setVisible(Build.VERSION.SDK_INT < 14 || !isTouchDevice());

        if (tab == null || tab.getURL() == null) {
            bookmark.setEnabled(false);
            forward.setEnabled(false);
            share.setEnabled(false);
            saveAsPDF.setEnabled(false);
            findInPage.setEnabled(false);
            return true;
        }

        bookmark.setEnabled(!tab.getURL().startsWith("about:reader"));
        bookmark.setCheckable(true);
        bookmark.setChecked(tab.isBookmark());
        bookmark.setIcon(tab.isBookmark() ? R.drawable.ic_menu_bookmark_remove : R.drawable.ic_menu_bookmark_add);

        forward.setEnabled(tab.canDoForward());
        desktopMode.setChecked(tab.getDesktopMode());
        desktopMode.setIcon(tab.getDesktopMode() ? R.drawable.ic_menu_desktop_mode_on : R.drawable.ic_menu_desktop_mode_off);

        String url = tab.getURL();
        if (ReaderModeUtils.isAboutReader(url)) {
            String urlFromReader = ReaderModeUtils.getUrlFromAboutReader(url);
            if (urlFromReader != null)
                url = urlFromReader;
        }

        // Disable share menuitem for about:, chrome:, file:, and resource: URIs
        String scheme = Uri.parse(url).getScheme();
        share.setEnabled(!(scheme.equals("about") || scheme.equals("chrome") ||
                           scheme.equals("file") || scheme.equals("resource")));

        // Disable save as PDF for about:home and xul pages
        saveAsPDF.setEnabled(!(tab.getURL().equals("about:home") ||
                               tab.getContentType().equals("application/vnd.mozilla.xul+xml")));

        // Disable find in page for about:home, since it won't work on Java content
        findInPage.setEnabled(!tab.getURL().equals("about:home"));

        charEncoding.setVisible(GeckoPreferences.getCharEncodingState());

        return true;
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.abouthome_topsites_edit:
                mAboutHomeContent.editSite();
                return true;

            case R.id.abouthome_topsites_unpin:
                mAboutHomeContent.unpinSite(AboutHomeContent.UnpinFlags.REMOVE_PIN);
                return true;

            case R.id.abouthome_topsites_pin:
                mAboutHomeContent.pinSite();
                return true;

            case R.id.abouthome_topsites_remove:
                mAboutHomeContent.unpinSite(AboutHomeContent.UnpinFlags.REMOVE_HISTORY);
                return true;

        }
        return super.onContextItemSelected(item);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        Tab tab = null;
        Intent intent = null;
        switch (item.getItemId()) {
            case R.id.bookmark:
                tab = Tabs.getInstance().getSelectedTab();
                if (tab != null) {
                    if (item.isChecked()) {
                        tab.removeBookmark();
                        Toast.makeText(this, R.string.bookmark_removed, Toast.LENGTH_SHORT).show();
                        item.setIcon(R.drawable.ic_menu_bookmark_add);
                    } else {
                        tab.addBookmark();
                        Toast.makeText(this, R.string.bookmark_added, Toast.LENGTH_SHORT).show();
                        item.setIcon(R.drawable.ic_menu_bookmark_remove);
                    }
                }
                return true;
            case R.id.share:
                shareCurrentUrl();
                return true;
            case R.id.reload:
                tab = Tabs.getInstance().getSelectedTab();
                if (tab != null)
                    tab.doReload();
                return true;
            case R.id.forward:
                tab = Tabs.getInstance().getSelectedTab();
                if (tab != null)
                    tab.doForward();
                return true;
            case R.id.save_as_pdf:
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("SaveAs:PDF", null));
                return true;
            case R.id.settings:
                intent = new Intent(this, GeckoPreferences.class);
                startActivity(intent);
                return true;
            case R.id.addons:
                Tabs.getInstance().loadUrlInTab("about:addons");
                return true;
            case R.id.downloads:
                Tabs.getInstance().loadUrlInTab("about:downloads");
                return true;
            case R.id.apps:
                Tabs.getInstance().loadUrlInTab("about:apps");
                return true;
            case R.id.char_encoding:
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("CharEncoding:Get", null));
                return true;
            case R.id.find_in_page:
                mFindInPageBar.show();
                return true;
            case R.id.desktop_mode:
                Tab selectedTab = Tabs.getInstance().getSelectedTab();
                if (selectedTab == null)
                    return true;
                JSONObject args = new JSONObject();
                try {
                    args.put("desktopMode", !item.isChecked());
                    args.put("tabId", selectedTab.getId());
                } catch (JSONException e) {
                    Log.e(LOGTAG, "error building json arguments");
                }
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("DesktopMode:Change", args.toString()));
                return true;
            case R.id.new_tab:
                addTab();
                return true;
            case R.id.new_private_tab:
                addPrivateTab();
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    /**
     * This will detect if the key pressed is back. If so, will show the history.
     */
    @Override
    public boolean onKeyLongPress(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            Tab tab = Tabs.getInstance().getSelectedTab();
            if (tab != null) {
                return tab.showAllHistory();
            }
        }
        return super.onKeyLongPress(keyCode, event);
    }

    /*
     * If the app has been launched a certain number of times, and we haven't asked for feedback before,
     * open a new tab with about:feedback when launching the app from the icon shortcut.
     */ 
    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);

        String action = intent.getAction();

        if (Build.VERSION.SDK_INT >= 10 && NfcAdapter.ACTION_NDEF_DISCOVERED.equals(action)) {
            String uri = intent.getDataString();
            GeckoAppShell.sendEventToGecko(GeckoEvent.createURILoadEvent(uri));
        }

        if (!Intent.ACTION_MAIN.equals(action) || !mInitialized) {
            return;
        }

        (new UiAsyncTask<Void, Void, Boolean>(ThreadUtils.getBackgroundHandler()) {
            @Override
            public synchronized Boolean doInBackground(Void... params) {
                // Check to see how many times the app has been launched.
                SharedPreferences settings = getPreferences(Activity.MODE_PRIVATE);
                String keyName = getPackageName() + ".feedback_launch_count";
                int launchCount = settings.getInt(keyName, 0);
                if (launchCount >= FEEDBACK_LAUNCH_COUNT)
                    return false;

                // Increment the launch count and store the new value.
                launchCount++;
                settings.edit().putInt(keyName, launchCount).commit();

                // If we've reached our magic number, show the feedback page.
                return launchCount == FEEDBACK_LAUNCH_COUNT;
            }

            @Override
            public void onPostExecute(Boolean shouldShowFeedbackPage) {
                if (shouldShowFeedbackPage)
                    Tabs.getInstance().loadUrlInTab("about:feedback");
            }
        }).execute();
    }

    @Override
    protected NotificationClient makeNotificationClient() {
        // The service is local to Fennec, so we can use it to keep
        // Fennec alive during downloads.
        return new ServiceNotificationClient(getApplicationContext());
    }

    private void resetFeedbackLaunchCount() {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public synchronized void run() {
                SharedPreferences settings = getPreferences(Activity.MODE_PRIVATE);
                settings.edit().putInt(getPackageName() + ".feedback_launch_count", 0).commit();
            }
        });
    }

    private void getLastUrl() {
        (new UiAsyncTask<Void, Void, String>(ThreadUtils.getBackgroundHandler()) {
            @Override
            public synchronized String doInBackground(Void... params) {
                // Get the most recent URL stored in browser history.
                String url = "";
                Cursor c = BrowserDB.getRecentHistory(getContentResolver(), 1);
                if (c.moveToFirst()) {
                    url = c.getString(c.getColumnIndexOrThrow(Combined.URL));
                }
                c.close();
                return url;
            }

            @Override
            public void onPostExecute(String url) {
                // Don't bother sending a message if there is no URL.
                if (url.length() > 0)
                    GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Feedback:LastUrl", url));
            }
        }).execute();
    }

    @Override
    public void onResume()
    {
        super.onResume();
        if (mAboutHomeContent != null) {
            mAboutHomeContent.refresh();
        }

    }
}
