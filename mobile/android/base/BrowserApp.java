/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.util.GeckoAsyncTask;
import org.mozilla.gecko.util.GeckoBackgroundThread;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.content.Intent;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
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
import android.widget.Toast;

import dalvik.system.DexClassLoader;

import java.io.File;
import java.io.InputStream;
import java.net.URL;
import java.util.EnumSet;
import java.util.Vector;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

abstract public class BrowserApp extends GeckoApp
                                 implements TabsPanel.TabsLayoutChangeListener,
                                            PropertyAnimator.PropertyAnimationListener {
    private static final String LOGTAG = "GeckoBrowserApp";

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
        public float getInterpolation(float t) {
            t -= 1.0f;
            return t * t * t * t * t + 1.0f;
        }
    };

    private FindInPageBar mFindInPageBar;

    // We'll ask for feedback after the user launches the app this many times.
    private static final int FEEDBACK_LAUNCH_COUNT = 15;

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
                    if ("about:home".equals(tab.getURL()))
                        showAboutHome();
                    else
                        hideAboutHome();
                }
                break;
            case LOAD_ERROR:
            case START:
            case STOP:
            case MENU_UPDATED:
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    invalidateOptionsMenu();
                }
                break;
        }
        super.onTabChanged(tab, msg, data);
    }

    @Override
    void handlePageShow(final int tabId) {
        super.handlePageShow(tabId);
        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        mMainHandler.post(new Runnable() {
            public void run() {
                loadFavicon(tab);
            }
        });
    }

    @Override
    void handleLinkAdded(final int tabId, String rel, final String href, int size) {
        super.handleLinkAdded(tabId, rel, href, size);
        if (rel.indexOf("[icon]") == -1)
            return;

        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        // If tab is not loading and the favicon is updated, we
        // want to load the image straight away. If tab is still
        // loading, we only load the favicon once the page's content
        // is fully loaded (see handleContentLoaded()).
        if (tab.getState() != Tab.STATE_LOADING) {
            mMainHandler.post(new Runnable() {
                public void run() {
                    loadFavicon(tab);
                }
            });
        }
    }

    @Override
    void handleClearHistory() {
        super.handleClearHistory();
        updateAboutHomeTopSites();
    }

    @Override
    void handleSecurityChange(final int tabId, final JSONObject identityData) {
        super.handleSecurityChange(tabId, identityData);
        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        mMainHandler.post(new Runnable() { 
            public void run() {
                if (Tabs.getInstance().isSelectedTab(tab))
                    mBrowserToolbar.setSecurityMode(tab.getSecurityMode());
            }
        });
    }

    void handleReaderEnabled(final int tabId) {
        super.handleReaderEnabled(tabId);
        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        mMainHandler.post(new Runnable() {
            public void run() {
                if (Tabs.getInstance().isSelectedTab(tab))
                    mBrowserToolbar.setReaderMode(tab.getReaderEnabled());
            }
        });
    }

    @Override
    void onStatePurged() {
        mMainHandler.post(new Runnable() {
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
        mMainLayout.addView(actionBar, 0);

        ((GeckoApp.MainLayout) mMainLayout).setOnInterceptTouchListener(new HideTabsTouchListener());

        mBrowserToolbar = new BrowserToolbar(this);
        mBrowserToolbar.from(actionBar);

        if (mTabsPanel != null)
            mTabsPanel.setTabsLayoutChangeListener(this);

        mFindInPageBar = (FindInPageBar) findViewById(R.id.find_in_page);

        registerEventListener("CharEncoding:Data");
        registerEventListener("CharEncoding:State");
        registerEventListener("Feedback:LastUrl");
        registerEventListener("Feedback:OpenPlayStore");
        registerEventListener("Feedback:MaybeLater");
        registerEventListener("Dex:Load");
        registerEventListener("Telemetry:Gather");
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mAboutHomeContent != null)
            mAboutHomeContent.onDestroy();

        unregisterEventListener("CharEncoding:Data");
        unregisterEventListener("CharEncoding:State");
        unregisterEventListener("Feedback:LastUrl");
        unregisterEventListener("Feedback:OpenPlayStore");
        unregisterEventListener("Feedback:MaybeLater");
        unregisterEventListener("Dex:Load");
        unregisterEventListener("Telemetry:Gather");
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
    }

    @Override
    void toggleChrome(final boolean aShow) {
        mMainHandler.post(new Runnable() {
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
        mMainHandler.post(new Runnable() {
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
        mTabsPanel.refresh();

        if (mAboutHomeContent != null)
            mAboutHomeContent.refresh();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        mBrowserToolbar.fromAwesomeBarSearch();
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
                info.checkable = false;
                try {
                    info.checkable = message.getBoolean("checkable");
                } catch (Exception ex) { }
                try { // parent is optional
                    info.parent = message.getInt("parent") + ADDON_MENU_OFFSET;
                } catch (Exception ex) { }
                final MenuItemInfo menuItemInfo = info;
                mMainHandler.post(new Runnable() {
                    public void run() {
                        addAddonMenuItem(menuItemInfo);
                    }
                });
            } else if (event.equals("Menu:Remove")) {
                final int id = message.getInt("id") + ADDON_MENU_OFFSET;
                mMainHandler.post(new Runnable() {
                    public void run() {
                        removeAddonMenuItem(id);
                    }
                });
            } else if (event.equals("Menu:Update")) {
                final int id = message.getInt("id") + ADDON_MENU_OFFSET;
                final JSONObject options = message.getJSONObject("options");
                mMainHandler.post(new Runnable() {
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
                    public void onClick(DialogInterface dialog, int which) {
                        dialog.dismiss();
                    }
                });
                mMainHandler.post(new Runnable() {
                    public void run() {
                        dialogBuilder.show();
                    }
                });
            } else if (event.equals("CharEncoding:State")) {
                final boolean visible = message.getString("visible").equals("true");
                GeckoPreferences.setCharEncodingState(visible);
                final Menu menu = mMenu;
                mMainHandler.post(new Runnable() {
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
                mMainHandler.post(new Runnable() {
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
            } else if (event.equals("Dex:Load")) {
                String zipFile = message.getString("zipfile");
                String implClass = message.getString("impl");
                Log.d(LOGTAG, "Attempting to load classes.dex file from " + zipFile + " and instantiate " + implClass);
                try {
                    File tmpDir = getDir("dex", 0);
                    DexClassLoader loader = new DexClassLoader(zipFile, tmpDir.getAbsolutePath(), null, ClassLoader.getSystemClassLoader());
                    Class<?> c = loader.loadClass(implClass);
                    c.newInstance();
                } catch (Exception e) {
                    Log.e(LOGTAG, "Unable to initialize addon", e);
                }
            } else {
                super.handleMessage(event, message);
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception handling message \"" + event + "\":", e);
        }
    }

    void addTab() {
        showAwesomebar(AwesomeBar.Target.NEW_TAB);
    }

    public void showLocalTabs() {
        showTabs(TabsPanel.Panel.LOCAL_TABS);
    }

    public void showRemoteTabs() {
        showTabs(TabsPanel.Panel.REMOTE_TABS);
    }

    private void showTabs(TabsPanel.Panel panel) {
        if (Tabs.getInstance().getCount() == 0)
            return;

        mTabsPanel.show(panel);
    }

    public void hideTabs() {
        mTabsPanel.hide();
    }

    public boolean autoHideTabs() {
        if (!hasTabsSideBar() && areTabsShown()) {
            hideTabs();
            return true;
        }
        return false;
    }

    public boolean areTabsShown() {
        return mTabsPanel.isShown();
    }

    @Override
    public void onTabsLayoutChange(int width, int height) {
        if (mMainLayoutAnimator != null)
            mMainLayoutAnimator.stop();

        if (mTabsPanel.isShown())
            mTabsPanel.setDescendantFocusability(ViewGroup.FOCUS_AFTER_DESCENDANTS);

        mMainLayoutAnimator = new PropertyAnimator(450, sTabsInterpolator);
        mMainLayoutAnimator.setPropertyAnimationListener(this);

        boolean usingTextureView = mLayerView.shouldUseTextureView();
        mMainLayoutAnimator.setUseHardwareLayer(usingTextureView);

        if (hasTabsSideBar()) {
            mBrowserToolbar.prepareTabsAnimation(mMainLayoutAnimator, width);

            // Set the gecko layout for sliding.
            if (!mTabsPanel.isShown()) {
                ((LinearLayout.LayoutParams) mGeckoLayout.getLayoutParams()).setMargins(0, 0, 0, 0);
                if (!usingTextureView)
                    mGeckoLayout.scrollTo(mTabsPanel.getWidth() * -1, 0);
                mGeckoLayout.requestLayout();
            }

            mMainLayoutAnimator.attach(mGeckoLayout,
                                       usingTextureView ? PropertyAnimator.Property.TRANSLATION_X :
                                                          PropertyAnimator.Property.SCROLL_X,
                                       usingTextureView ? width : -width);
        } else {
            mMainLayoutAnimator.attach(mMainLayout,
                                       usingTextureView ? PropertyAnimator.Property.TRANSLATION_Y :
                                                          PropertyAnimator.Property.SCROLL_Y,
                                       usingTextureView ? height : -height);
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
                boolean usingTextureView = mLayerView.shouldUseTextureView();

                int leftMargin = (usingTextureView ? 0 : mTabsPanel.getWidth());
                int rightMargin = (usingTextureView ? mTabsPanel.getWidth() : 0);
                ((LinearLayout.LayoutParams) mGeckoLayout.getLayoutParams()).setMargins(leftMargin, 0, rightMargin, 0);

                if (!usingTextureView)
                    mGeckoLayout.scrollTo(0, 0);
            }

            mGeckoLayout.requestLayout();
        } else {
            mBrowserToolbar.updateTabs(false);
            mBrowserToolbar.finishTabsAnimation();
            mTabsPanel.setDescendantFocusability(ViewGroup.FOCUS_BLOCK_DESCENDANTS);
        }

        mBrowserToolbar.refreshBackground();

        if (hasTabsSideBar())
            mBrowserToolbar.adjustTabsAnimation(true);
    }

    /* Favicon methods */
    private void loadFavicon(final Tab tab) {
        maybeCancelFaviconLoad(tab);

        long id = Favicons.getInstance().loadFavicon(tab.getURL(), tab.getFaviconURL(), !tab.isPrivate(),
                        new Favicons.OnFaviconLoadedListener() {

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

                if (Tabs.getInstance().isSelectedTab(tab))
                    mBrowserToolbar.setFavicon(tab.getFavicon());

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
        mMainHandler.postAtFrontOfQueue(r);
    }

    private void hideAboutHome() {
        // If hideAboutHome gets called before showAboutHome, we still need
        // to create an AboutHomeRunnable to hide the about:home content.
        if (mAboutHomeShowing != null && !mAboutHomeShowing)
            return;

        mBrowserToolbar.setShadowVisibility(true);
        mAboutHomeShowing = false;
        Runnable r = new AboutHomeRunnable(false);
        mMainHandler.postAtFrontOfQueue(r);
    }

    private class AboutHomeRunnable implements Runnable {
        boolean mShow;
        AboutHomeRunnable(boolean show) {
            mShow = show;
        }

        public void run() {
            if (mShow) {
                if (mAboutHomeContent == null) {
                    mAboutHomeContent = (AboutHomeContent) findViewById(R.id.abouthome_content);
                    mAboutHomeContent.init();
                    mAboutHomeContent.update(AboutHomeContent.UpdateFlags.ALL);
                    mAboutHomeContent.setUriLoadCallback(new AboutHomeContent.UriLoadCallback() {
                        public void callback(String url) {
                            mBrowserToolbar.setProgressVisibility(true);
                            Tabs.getInstance().loadUrl(url);
                        }
                    });
                    mAboutHomeContent.setLoadCompleteCallback(new AboutHomeContent.VoidCallback() {
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
        } 
    }

    private class HideTabsTouchListener implements OnInterceptTouchListener {
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
                GeckoAppShell.getHandler().post(new Runnable() {
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
            }
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
    }

    @Override
    public void closeOptionsMenu() {
        if (!mBrowserToolbar.closeOptionsMenu())
            super.closeOptionsMenu();
    }

    @Override
    public void setFullScreen(final boolean fullscreen) {
      super.setFullScreen(fullscreen);
      mMainHandler.post(new Runnable() {
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

        if (!checkLaunchState(LaunchState.GeckoRunning))
            aMenu.findItem(R.id.settings).setEnabled(false);

        Tab tab = Tabs.getInstance().getSelectedTab();
        MenuItem bookmark = aMenu.findItem(R.id.bookmark);
        MenuItem forward = aMenu.findItem(R.id.forward);
        MenuItem share = aMenu.findItem(R.id.share);
        MenuItem saveAsPDF = aMenu.findItem(R.id.save_as_pdf);
        MenuItem charEncoding = aMenu.findItem(R.id.char_encoding);
        MenuItem findInPage = aMenu.findItem(R.id.find_in_page);
        MenuItem desktopMode = aMenu.findItem(R.id.desktop_mode);

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
                Tabs.getInstance().loadUrl("about:home", Tabs.LOADURL_NEW_TAB | Tabs.LOADURL_PRIVATE);
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    /*
     * If the app has been launched a certain number of times, and we haven't asked for feedback before,
     * open a new tab with about:feedback when launching the app from the icon shortcut.
     */ 
    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);

        if (!Intent.ACTION_MAIN.equals(intent.getAction()) || !mInitialized)
            return;

        (new GeckoAsyncTask<Void, Void, Boolean>(mAppContext, GeckoAppShell.getHandler()) {
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

    private void resetFeedbackLaunchCount() {
        GeckoBackgroundThread.post(new Runnable() {
            @Override
            public synchronized void run() {
                SharedPreferences settings = getPreferences(Activity.MODE_PRIVATE);
                settings.edit().putInt(getPackageName() + ".feedback_launch_count", 0).commit();
            }
        });
    }

    private void getLastUrl() {
        (new GeckoAsyncTask<Void, Void, String>(mAppContext, GeckoAppShell.getHandler()) {
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
}
