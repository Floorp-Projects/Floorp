/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.DataReportingNotification;
import org.mozilla.gecko.background.announcements.AnnouncementsBroadcastService;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.gfx.Layer;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.gfx.PanZoomController;
import org.mozilla.gecko.gfx.PluginLayer;
import org.mozilla.gecko.gfx.PointUtils;
import org.mozilla.gecko.menu.GeckoMenu;
import org.mozilla.gecko.menu.GeckoMenuInflater;
import org.mozilla.gecko.menu.MenuPanel;
import org.mozilla.gecko.health.BrowserHealthRecorder;
import org.mozilla.gecko.updater.UpdateService;
import org.mozilla.gecko.updater.UpdateServiceHelper;
import org.mozilla.gecko.util.EventDispatcher;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.GeckoEventResponder;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UiAsyncTask;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.Notification;
import android.app.PendingIntent;
import android.app.WallpaperManager;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.location.Location;
import android.location.LocationListener;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.PowerManager;
import android.os.StrictMode;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Base64;
import android.util.Log;
import android.util.SparseBooleanArray;
import android.view.Display;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.AbsoluteLayout;
import android.widget.FrameLayout;
import android.widget.ListView;
import android.widget.RelativeLayout;
import android.widget.SimpleAdapter;
import android.widget.TextView;
import android.widget.Toast;

import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

abstract public class GeckoApp
                extends GeckoActivity 
    implements GeckoEventListener, SensorEventListener, LocationListener,
                           Tabs.OnTabsChangedListener, GeckoEventResponder,
                           GeckoMenu.Callback, GeckoMenu.MenuPresenter,
                           TouchEventInterceptor, ContextGetter, GeckoAppShell.GeckoInterface
{
    private static final String LOGTAG = "GeckoApp";

    private static enum StartupAction {
        NORMAL,     /* normal application start */
        URL,        /* launched with a passed URL */
        PREFETCH    /* launched with a passed URL that we prefetch */
    }

    public static final String ACTION_ALERT_CALLBACK = "org.mozilla.gecko.ACTION_ALERT_CALLBACK";
    public static final String ACTION_WEBAPP_PREFIX = "org.mozilla.gecko.WEBAPP";
    public static final String ACTION_DEBUG         = "org.mozilla.gecko.DEBUG";
    public static final String ACTION_BOOKMARK      = "org.mozilla.gecko.BOOKMARK";
    public static final String ACTION_LOAD          = "org.mozilla.gecko.LOAD";
    public static final String ACTION_INIT_PW       = "org.mozilla.gecko.INIT_PW";
    public static final String SAVED_STATE_IN_BACKGROUND = "inBackground";
    public static final String SAVED_STATE_PRIVATE_SESSION = "privateSession";

    public static final String PREFS_NAME          = "GeckoApp";
    public static final String PREFS_OOM_EXCEPTION = "OOMException";
    public static final String PREFS_WAS_STOPPED   = "wasStopped";
    public static final String PREFS_CRASHED       = "crashed";

    static public final int RESTORE_NONE = 0;
    static public final int RESTORE_OOM = 1;
    static public final int RESTORE_CRASH = 2;

    protected RelativeLayout mMainLayout;
    protected RelativeLayout mGeckoLayout;
    public View getView() { return mGeckoLayout; }
    public SurfaceView cameraView;
    private static GeckoApp sAppContext;
    protected MenuPanel mMenuPanel;
    protected Menu mMenu;
    private static GeckoThread sGeckoThread;
    private GeckoProfile mProfile;
    public static int mOrientation;
    protected boolean mIsRestoringActivity;
    private String mCurrentResponse = "";
    public static boolean sIsUsingCustomProfile = false;

    private PromptService mPromptService;
    private TextSelection mTextSelection;

    protected DoorHangerPopup mDoorHangerPopup;
    protected FormAssistPopup mFormAssistPopup;
    protected TabsPanel mTabsPanel;

    protected LayerView mLayerView;
    private AbsoluteLayout mPluginContainer;

    private FullScreenHolder mFullScreenPluginContainer;
    private View mFullScreenPluginView;

    private HashMap<String, PowerManager.WakeLock> mWakeLocks = new HashMap<String, PowerManager.WakeLock>();

    protected int mRestoreMode = RESTORE_NONE;
    protected boolean mInitialized = false;
    private Telemetry.Timer mJavaUiStartupTimer;
    private Telemetry.Timer mGeckoReadyStartupTimer;

    private String mPrivateBrowsingSession;

    private PointF mInitialTouchPoint = null;
    private volatile BrowserHealthRecorder mHealthRecorder = null;

    abstract public int getLayout();
    abstract public boolean hasTabsSideBar();
    abstract protected String getDefaultProfileName();

    private static final String RESTARTER_ACTION = "org.mozilla.gecko.restart";
    private static final String RESTARTER_CLASS = "org.mozilla.gecko.Restarter";

    @SuppressWarnings("serial")
    class SessionRestoreException extends Exception {
        public SessionRestoreException(Exception e) {
            super(e);
        }

        public SessionRestoreException(String message) {
            super(message);
        }
    }

    void toggleChrome(final boolean aShow) { }

    void focusChrome() { }

    public Context getContext() {
        return sAppContext;
    }

    public Activity getActivity() {
        return this;
    }

    public LocationListener getLocationListener() {
        return this;
    }

    public SensorEventListener getSensorEventListener() {
        return this;
    }

    public SurfaceView getCameraView() {
        return cameraView;
    }

    public FormAssistPopup getFormAssistPopup() {
        return mFormAssistPopup;
    }

    @Override
    public void onTabChanged(Tab tab, Tabs.TabEvents msg, Object data) {
        // When a tab is closed, it is always unselected first.
        // When a tab is unselected, another tab is always selected first.
        switch(msg) {
            case UNSELECTED:
                hidePlugins(tab);
                break;

            case LOCATION_CHANGE:
                // We only care about location change for the selected tab.
                if (!Tabs.getInstance().isSelectedTab(tab))
                    break;
                // Fall through...
            case SELECTED:
                invalidateOptionsMenu();
                if (mFormAssistPopup != null)
                    mFormAssistPopup.hide();
                break;

            case LOADED:
                // Sync up the layer view and the tab if the tab is
                // currently displayed.
                LayerView layerView = mLayerView;
                if (layerView != null && Tabs.getInstance().isSelectedTab(tab))
                    layerView.setBackgroundColor(tab.getBackgroundColor());
                break;

            case DESKTOP_MODE_CHANGE:
                if (Tabs.getInstance().isSelectedTab(tab))
                    invalidateOptionsMenu();
                break;
        }
    }

    public void refreshChrome() { }

    @Override
    public void invalidateOptionsMenu() {
        if (mMenu == null)
            return;

        onPrepareOptionsMenu(mMenu);

        if (Build.VERSION.SDK_INT >= 11)
            super.invalidateOptionsMenu();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        mMenu = menu;

        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.gecko_app_menu, mMenu);
        return true;
    }

    @Override
    public MenuInflater getMenuInflater() {
        if (Build.VERSION.SDK_INT >= 11)
            return new GeckoMenuInflater(sAppContext);
        else
            return super.getMenuInflater();
    }

    public MenuPanel getMenuPanel() {
        return mMenuPanel;
    }

    @Override
    public boolean onMenuItemSelected(MenuItem item) {
        return onOptionsItemSelected(item);
    }

    @Override
    public void openMenu() {
        openOptionsMenu();
    }

    @Override
    public void showMenu(View menu) {
        // Hide the menu only if we are showing the MenuPopup.
        if (!HardwareUtils.hasMenuButton())
            closeMenu();

        mMenuPanel.removeAllViews();
        mMenuPanel.addView(menu);

        openOptionsMenu();
    }

    @Override
    public void closeMenu() {
        closeOptionsMenu();
    }

    @Override
    public View onCreatePanelView(int featureId) {
        if (Build.VERSION.SDK_INT >= 11 && featureId == Window.FEATURE_OPTIONS_PANEL) {
            if (mMenuPanel == null) {
                mMenuPanel = new MenuPanel(this, null);
            } else {
                // Prepare the panel everytime before showing the menu.
                onPreparePanel(featureId, mMenuPanel, mMenu);
            }

            return mMenuPanel; 
        }
  
        return super.onCreatePanelView(featureId);
    }

    @Override
    public boolean onCreatePanelMenu(int featureId, Menu menu) {
        if (Build.VERSION.SDK_INT >= 11 && featureId == Window.FEATURE_OPTIONS_PANEL) {
            if (mMenuPanel == null) {
                mMenuPanel = (MenuPanel) onCreatePanelView(featureId);
            }

            GeckoMenu gMenu = new GeckoMenu(this, null);
            gMenu.setCallback(this);
            gMenu.setMenuPresenter(this);
            menu = gMenu;
            mMenuPanel.addView(gMenu);

            return onCreateOptionsMenu(menu);
        }

        return super.onCreatePanelMenu(featureId, menu);
    }

    @Override
    public boolean onPreparePanel(int featureId, View view, Menu menu) {
        if (Build.VERSION.SDK_INT >= 11 && featureId == Window.FEATURE_OPTIONS_PANEL)
            return onPrepareOptionsMenu(menu);

        return super.onPreparePanel(featureId, view, menu);
    }

    @Override
    public boolean onMenuOpened(int featureId, Menu menu) {
        // exit full-screen mode whenever the menu is opened
        if (mLayerView != null && mLayerView.isFullScreen()) {
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("FullScreen:Exit", null));
        }

        if (Build.VERSION.SDK_INT >= 11 && featureId == Window.FEATURE_OPTIONS_PANEL) {
            if (mMenu == null) {
                onCreatePanelMenu(featureId, menu);
                onPreparePanel(featureId, mMenuPanel, mMenu);
            }

            // Scroll custom menu to the top
            if (mMenuPanel != null)
                mMenuPanel.scrollTo(0, 0);

            return true;
        }

        return super.onMenuOpened(featureId, menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.quit:
                if (GeckoThread.checkAndSetLaunchState(GeckoThread.LaunchState.GeckoRunning, GeckoThread.LaunchState.GeckoExiting)) {
                    GeckoAppShell.notifyGeckoOfEvent(GeckoEvent.createBroadcastEvent("Browser:Quit", null));
                } else {
                    System.exit(0);
                }
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    @Override
    public void onOptionsMenuClosed(Menu menu) {
        if (Build.VERSION.SDK_INT >= 11) {
            mMenuPanel.removeAllViews();
            mMenuPanel.addView((GeckoMenu) mMenu);
        }
    }
 
    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        // Custom Menu should be opened when hardware menu key is pressed.
        if (Build.VERSION.SDK_INT >= 11 && keyCode == KeyEvent.KEYCODE_MENU) {
            openOptionsMenu();
            return true;
        }

        return super.onKeyDown(keyCode, event);
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);

        if (outState == null)
            outState = new Bundle();

        outState.putBoolean(SAVED_STATE_IN_BACKGROUND, isApplicationInBackground());
        outState.putString(SAVED_STATE_PRIVATE_SESSION, mPrivateBrowsingSession);
    }

    void handleFaviconRequest(final String url) {
        (new UiAsyncTask<Void, Void, String>(ThreadUtils.getBackgroundHandler()) {
            @Override
            public String doInBackground(Void... params) {
                return Favicons.getInstance().getFaviconUrlForPageUrl(url);
            }

            @Override
            public void onPostExecute(String faviconUrl) {
                JSONObject args = new JSONObject();

                if (faviconUrl != null) {
                    try {
                        args.put("url", url);
                        args.put("faviconUrl", faviconUrl);
                    } catch (JSONException e) {
                        Log.w(LOGTAG, "Error building JSON favicon arguments.", e);
                    }
                }

                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Reader:FaviconReturn", args.toString()));
            }
        }).execute();
    }

    void handleClearHistory() {
        BrowserDB.clearHistory(getContentResolver());
    }

    public void addTab() { }

    public void addPrivateTab() { }

    public void showNormalTabs() { }

    public void showPrivateTabs() { }

    public void showRemoteTabs() { }

    private void showTabs(TabsPanel.Panel panel) { }

    public void hideTabs() { }

    /**
     * Close the tab UI indirectly (not as the result of a direct user
     * action).  This does not force the UI to close; for example in Firefox
     * tablet mode it will remain open unless the user explicitly closes it.
     *
     * @return True if the tab UI was hidden.
     */
    public boolean autoHideTabs() { return false; }

    public boolean areTabsShown() { return false; }

    @Override
    public void handleMessage(String event, JSONObject message) {
        try {
            if (event.equals("Toast:Show")) {
                final String msg = message.getString("message");
                final String duration = message.getString("duration");
                handleShowToast(msg, duration);
            } else if (event.equals("log")) {
                // generic log listener
                final String msg = message.getString("msg");
                Log.d(LOGTAG, "Log: " + msg);
            } else if (event.equals("Reader:FaviconRequest")) {
                final String url = message.getString("url");
                handleFaviconRequest(url);
            } else if (event.equals("Reader:GoToReadingList")) {
                showReadingList();
            } else if (event.equals("Gecko:Ready")) {
                mGeckoReadyStartupTimer.stop();
                connectGeckoLayerClient();
            } else if (event.equals("ToggleChrome:Hide")) {
                toggleChrome(false);
            } else if (event.equals("ToggleChrome:Show")) {
                toggleChrome(true);
            } else if (event.equals("ToggleChrome:Focus")) {
                focusChrome();
            } else if (event.equals("DOMFullScreen:Start")) {
                // Local ref to layerView for thread safety
                LayerView layerView = mLayerView;
                if (layerView != null) {
                    layerView.setFullScreen(true);
                }
            } else if (event.equals("DOMFullScreen:Stop")) {
                // Local ref to layerView for thread safety
                LayerView layerView = mLayerView;
                if (layerView != null) {
                    layerView.setFullScreen(false);
                }
            } else if (event.equals("Permissions:Data")) {
                String host = message.getString("host");
                JSONArray permissions = message.getJSONArray("permissions");
                showSiteSettingsDialog(host, permissions);
            } else if (event.equals("Tab:ViewportMetadata")) {
                int tabId = message.getInt("tabID");
                Tab tab = Tabs.getInstance().getTab(tabId);
                if (tab == null)
                    return;
                tab.setZoomConstraints(new ZoomConstraints(message));
                tab.setIsRTL(message.getBoolean("isRTL"));
                // Sync up the layer view and the tab if the tab is currently displayed.
                LayerView layerView = mLayerView;
                if (layerView != null && Tabs.getInstance().isSelectedTab(tab)) {
                    layerView.setZoomConstraints(tab.getZoomConstraints());
                    layerView.setIsRTL(tab.getIsRTL());
                }
            } else if (event.equals("Session:StatePurged")) {
                onStatePurged();
            } else if (event.equals("Bookmark:Insert")) {
                final String url = message.getString("url");
                final String title = message.getString("title");
                final Context context = this;
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        Toast.makeText(context, R.string.bookmark_added, Toast.LENGTH_SHORT).show();
                        ThreadUtils.postToBackgroundThread(new Runnable() {
                            @Override
                            public void run() {
                                BrowserDB.addBookmark(GeckoApp.sAppContext.getContentResolver(), title, url);
                            }
                        });
                    }
                });
            } else if (event.equals("Accessibility:Event")) {
                GeckoAccessibility.sendAccessibilityEvent(message);
            } else if (event.equals("Accessibility:Ready")) {
                GeckoAccessibility.updateAccessibilitySettings(this);
            } else if (event.equals("Shortcut:Remove")) {
                final String url = message.getString("url");
                final String origin = message.getString("origin");
                final String title = message.getString("title");
                final String type = message.getString("shortcutType");
                GeckoAppShell.removeShortcut(title, url, origin, type);
            } else if (event.equals("WebApps:Open")) {
                String manifestURL = message.getString("manifestURL");
                String origin = message.getString("origin");
                Intent intent = GeckoAppShell.getWebAppIntent(manifestURL, origin, "", null);
                if (intent == null)
                    return;
                startActivity(intent);
            } else if (event.equals("WebApps:Install")) {
                String name = message.getString("name");
                String manifestURL = message.getString("manifestURL");
                String iconURL = message.getString("iconURL");
                String origin = message.getString("origin");
                // preInstallWebapp will return a File object pointing to the profile directory of the webapp
                mCurrentResponse = GeckoAppShell.preInstallWebApp(name, manifestURL, origin).toString();
                GeckoAppShell.postInstallWebApp(name, manifestURL, origin, iconURL);
            } else if (event.equals("WebApps:PreInstall")) {
                String name = message.getString("name");
                String manifestURL = message.getString("manifestURL");
                String origin = message.getString("origin");
                // preInstallWebapp will return a File object pointing to the profile directory of the webapp
                mCurrentResponse = GeckoAppShell.preInstallWebApp(name, manifestURL, origin).toString();
            } else if (event.equals("WebApps:PostInstall")) {
                String name = message.getString("name");
                String manifestURL = message.getString("manifestURL");
                String iconURL = message.getString("iconURL");
                String origin = message.getString("origin");
                GeckoAppShell.postInstallWebApp(name, manifestURL, origin, iconURL);
            } else if (event.equals("WebApps:Uninstall")) {
                String origin = message.getString("origin");
                GeckoAppShell.uninstallWebApp(origin);
            } else if (event.equals("Share:Text")) {
                String text = message.getString("text");
                GeckoAppShell.openUriExternal(text, "text/plain", "", "", Intent.ACTION_SEND, "");
            } else if (event.equals("Share:Image")) {
                String src = message.getString("url");
                String type = message.getString("mime");
                GeckoAppShell.shareImage(src, type);
            } else if (event.equals("Wallpaper:Set")) {
                String src = message.getString("url");
                setImageAsWallpaper(src);
            } else if (event.equals("Sanitize:ClearHistory")) {
                handleClearHistory();
            } else if (event.equals("Update:Check")) {
                startService(new Intent(UpdateServiceHelper.ACTION_CHECK_FOR_UPDATE, null, this, UpdateService.class));
            } else if (event.equals("Update:Download")) {
                startService(new Intent(UpdateServiceHelper.ACTION_DOWNLOAD_UPDATE, null, this, UpdateService.class));
            } else if (event.equals("Update:Install")) {
                startService(new Intent(UpdateServiceHelper.ACTION_APPLY_UPDATE, null, this, UpdateService.class));
            } else if (event.equals("PrivateBrowsing:Data")) {
                // null strings return "null" (http://code.google.com/p/android/issues/detail?id=13830)
                if (message.isNull("session")) {
                    mPrivateBrowsingSession = null;
                } else {
                    mPrivateBrowsingSession = message.getString("session");
                }
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception handling message \"" + event + "\":", e);
        }
    }

    public String getResponse(JSONObject origMessage) {
        String res = mCurrentResponse;
        mCurrentResponse = "";
        return res;
    }

    void onStatePurged() { }

    /**
     * @param aPermissions
     *        Array of JSON objects to represent site permissions.
     *        Example: { type: "offline-app", setting: "Store Offline Data", value: "Allow" }
     */
    private void showSiteSettingsDialog(String aHost, JSONArray aPermissions) {
        final AlertDialog.Builder builder = new AlertDialog.Builder(this);

        View customTitleView = getLayoutInflater().inflate(R.layout.site_setting_title, null);
        ((TextView) customTitleView.findViewById(R.id.title)).setText(R.string.site_settings_title);
        ((TextView) customTitleView.findViewById(R.id.host)).setText(aHost);
        builder.setCustomTitle(customTitleView);

        // If there are no permissions to clear, show the user a message about that.
        // In the future, we want to disable the menu item if there are no permissions to clear.
        if (aPermissions.length() == 0) {
            builder.setMessage(R.string.site_settings_no_settings);
        } else {

            ArrayList <HashMap<String, String>> itemList = new ArrayList <HashMap<String, String>>();
            for (int i = 0; i < aPermissions.length(); i++) {
                try {
                    JSONObject permObj = aPermissions.getJSONObject(i);
                    HashMap<String, String> map = new HashMap<String, String>();
                    map.put("setting", permObj.getString("setting"));
                    map.put("value", permObj.getString("value"));
                    itemList.add(map);
                } catch (JSONException e) {
                    Log.w(LOGTAG, "Exception populating settings items.", e);
                }
            }

            // setMultiChoiceItems doesn't support using an adapter, so we're creating a hack with
            // setSingleChoiceItems and changing the choiceMode below when we create the dialog
            builder.setSingleChoiceItems(new SimpleAdapter(
                GeckoApp.this,
                itemList,
                R.layout.site_setting_item,
                new String[] { "setting", "value" },
                new int[] { R.id.setting, R.id.value }
                ), -1, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) { }
                });

            builder.setPositiveButton(R.string.site_settings_clear, new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int id) {
                    ListView listView = ((AlertDialog) dialog).getListView();
                    SparseBooleanArray checkedItemPositions = listView.getCheckedItemPositions();

                    // An array of the indices of the permissions we want to clear
                    JSONArray permissionsToClear = new JSONArray();
                    for (int i = 0; i < checkedItemPositions.size(); i++)
                        if (checkedItemPositions.get(i))
                            permissionsToClear.put(i);

                    GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent(
                        "Permissions:Clear", permissionsToClear.toString()));
                }
            });
        }

        builder.setNegativeButton(R.string.site_settings_cancel, new DialogInterface.OnClickListener(){
            @Override
            public void onClick(DialogInterface dialog, int id) {
                dialog.cancel();
            }
        });

        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                Dialog dialog = builder.create();
                dialog.show();

                ListView listView = ((AlertDialog) dialog).getListView();
                if (listView != null) {
                    listView.setChoiceMode(ListView.CHOICE_MODE_MULTIPLE);
                    int listSize = listView.getAdapter().getCount();
                    for (int i = 0; i < listSize; i++)
                        listView.setItemChecked(i, true);
                }
            }
        });
    }

    public void showToast(final int resId, final int duration) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(sAppContext, resId, duration).show();
            }
        });
    }

    void handleShowToast(final String message, final String duration) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                Toast toast;
                if (duration.equals("long"))
                    toast = Toast.makeText(sAppContext, message, Toast.LENGTH_LONG);
                else
                    toast = Toast.makeText(sAppContext, message, Toast.LENGTH_SHORT);
                toast.show();
            }
        });
    }

    private void addFullScreenPluginView(View view) {
        if (mFullScreenPluginView != null) {
            Log.w(LOGTAG, "Already have a fullscreen plugin view");
            return;
        }

        setFullScreen(true);

        view.setWillNotDraw(false);
        if (view instanceof SurfaceView) {
            ((SurfaceView) view).setZOrderOnTop(true);
        }

        mFullScreenPluginContainer = new FullScreenHolder(this);

        FrameLayout.LayoutParams layoutParams = new FrameLayout.LayoutParams(
                            ViewGroup.LayoutParams.FILL_PARENT,
                            ViewGroup.LayoutParams.FILL_PARENT,
                            Gravity.CENTER);
        mFullScreenPluginContainer.addView(view, layoutParams);


        FrameLayout decor = (FrameLayout)getWindow().getDecorView();
        decor.addView(mFullScreenPluginContainer, layoutParams);

        mFullScreenPluginView = view;
    }

    public void addPluginView(final View view, final Rect rect, final boolean isFullScreen) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                Tabs tabs = Tabs.getInstance();
                Tab tab = tabs.getSelectedTab();

                if (isFullScreen) {
                    addFullScreenPluginView(view);
                    return;
                }

                PluginLayer layer = (PluginLayer) tab.getPluginLayer(view);
                if (layer == null) {
                    layer = new PluginLayer(view, rect, mLayerView.getRenderer().getMaxTextureSize());
                    tab.addPluginLayer(view, layer);
                } else {
                    layer.reset(rect);
                    layer.setVisible(true);
                }

                mLayerView.addLayer(layer);
            }
        });
    }

    private void removeFullScreenPluginView(View view) {
        if (mFullScreenPluginView == null) {
            Log.w(LOGTAG, "Don't have a fullscreen plugin view");
            return;
        }

        if (mFullScreenPluginView != view) {
            Log.w(LOGTAG, "Passed view is not the current full screen view");
            return;
        }

        mFullScreenPluginContainer.removeView(mFullScreenPluginView);

        // We need do do this on the next iteration in order to avoid
        // a deadlock, see comment below in FullScreenHolder
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                mLayerView.show();
            }
        });

        FrameLayout decor = (FrameLayout)getWindow().getDecorView();
        decor.removeView(mFullScreenPluginContainer);

        mFullScreenPluginView = null;

        GeckoScreenOrientationListener.getInstance().unlockScreenOrientation();
        setFullScreen(false);
    }

    public void removePluginView(final View view, final boolean isFullScreen) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                Tabs tabs = Tabs.getInstance();
                Tab tab = tabs.getSelectedTab();

                if (isFullScreen) {
                    removeFullScreenPluginView(view);
                    return;
                }

                PluginLayer layer = (PluginLayer) tab.removePluginLayer(view);
                if (layer != null) {
                    layer.destroy();
                }
            }
        });
    }

    private void setImageAsWallpaper(final String aSrc) {
        final String progText = sAppContext.getString(R.string.wallpaper_progress);
        final String successText = sAppContext.getString(R.string.wallpaper_success);
        final String failureText = sAppContext.getString(R.string.wallpaper_fail);
        final String fileName = aSrc.substring(aSrc.lastIndexOf("/") + 1);
        final PendingIntent emptyIntent = PendingIntent.getActivity(sAppContext, 0, new Intent(), 0);
        final AlertNotification notification = new AlertNotification(sAppContext, fileName.hashCode(), 
                                R.drawable.alert_download, fileName, progText, System.currentTimeMillis(), null);
        notification.setLatestEventInfo(sAppContext, fileName, progText, emptyIntent );
        notification.flags |= Notification.FLAG_ONGOING_EVENT;
        notification.show();
        new UiAsyncTask<Void, Void, Boolean>(ThreadUtils.getBackgroundHandler()) {

            @Override
            protected Boolean doInBackground(Void... params) {
                WallpaperManager mgr = WallpaperManager.getInstance(sAppContext);
                if (mgr == null) {
                    return false;
                }

                // Determine the ideal width and height of the wallpaper
                // for the device

                int idealWidth = mgr.getDesiredMinimumWidth();
                int idealHeight = mgr.getDesiredMinimumHeight();

                // Sometimes WallpaperManager's getDesiredMinimum*() methods
                // can return 0 if a Remote Exception occurs when calling the
                // Wallpaper Service. So if that fails, we are calculating
                // the ideal width and height from the device's display 
                // resolution (excluding the decorated area)

                if (idealWidth <= 0 || idealHeight <= 0) {
                    int orientation;
                    Display defaultDisplay = getWindowManager().getDefaultDisplay();
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.FROYO) {
                        orientation = defaultDisplay.getRotation();
                    } else {
                        orientation = defaultDisplay.getOrientation();
                    }
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
                        Point size = new Point();
                        defaultDisplay.getSize(size);
                        // The ideal wallpaper width is always twice the size of
                        // display width
                        if (orientation == Surface.ROTATION_0 || orientation == Surface.ROTATION_270) {
                            idealWidth = size.x * 2;
                            idealHeight = size.y;
                        } else {
                            idealWidth = size.y;
                            idealHeight = size.x * 2;
                        }
                    } else {
                        if (orientation == Surface.ROTATION_0 || orientation == Surface.ROTATION_270) {
                            idealWidth = defaultDisplay.getWidth() * 2;
                            idealHeight = defaultDisplay.getHeight();
                        } else {
                            idealWidth = defaultDisplay.getHeight();
                            idealHeight = defaultDisplay.getWidth() * 2;
                        }
                    }
                }

                boolean isDataURI = aSrc.startsWith("data:");
                BitmapFactory.Options options = new BitmapFactory.Options();
                options.inJustDecodeBounds = true;
                Bitmap image = null;
                InputStream is = null;
                ByteArrayOutputStream os = null;
                try{
                    if (isDataURI) {
                        int dataStart = aSrc.indexOf(',');
                        byte[] buf = Base64.decode(aSrc.substring(dataStart+1), Base64.DEFAULT);
                        BitmapUtils.decodeByteArray(buf, options);
                        options.inSampleSize = getBitmapSampleSize(options, idealWidth, idealHeight);
                        options.inJustDecodeBounds = false;
                        image = BitmapUtils.decodeByteArray(buf, options);
                    } else {
                        int byteRead;
                        byte[] buf = new byte[4192];
                        os = new ByteArrayOutputStream();
                        URL url = new URL(aSrc);
                        is = url.openStream();

                        // Cannot read from same stream twice. Also, InputStream from
                        // URL does not support reset. So converting to byte array

                        while((byteRead = is.read(buf)) != -1) {
                            os.write(buf, 0, byteRead);
                        }
                        byte[] imgBuffer = os.toByteArray();
                        BitmapUtils.decodeByteArray(imgBuffer, options);
                        options.inSampleSize = getBitmapSampleSize(options, idealWidth, idealHeight);
                        options.inJustDecodeBounds = false;
                        image = BitmapUtils.decodeByteArray(imgBuffer, options);
                    }
                    if(image != null) {
                        mgr.setBitmap(image);
                        return true;
                    } else {
                        return false;
                    }
                } catch(OutOfMemoryError ome) {
                    Log.e(LOGTAG, "Out of Memory when converting to byte array", ome);
                    return false;
                } catch(IOException ioe) {
                    Log.e(LOGTAG, "I/O Exception while setting wallpaper", ioe);
                    return false;
                } finally {
                    if(is != null) {
                        try {
                            is.close();
                        } catch(IOException ioe) {
                            Log.w(LOGTAG, "I/O Exception while closing stream", ioe);
                        }
                    }
                    if(os != null) {
                        try {
                            os.close();
                        } catch(IOException ioe) {
                            Log.w(LOGTAG, "I/O Exception while closing stream", ioe);
                        }
                    }
                }
            }

            @Override
            protected void onPostExecute(Boolean success) {
                notification.cancel();
                notification.flags = 0;
                notification.flags |= Notification.FLAG_AUTO_CANCEL;
                if(!success) {
                    notification.tickerText = failureText;
                    notification.setLatestEventInfo(sAppContext, fileName, failureText, emptyIntent);
                } else {
                    notification.tickerText = successText;
                    notification.setLatestEventInfo(sAppContext, fileName, successText, emptyIntent);
                }
                notification.show();
            }
        }.execute();
    }

    private int getBitmapSampleSize(BitmapFactory.Options options, int idealWidth, int idealHeight) {
        int width = options.outWidth;
        int height = options.outHeight;
        int inSampleSize = 1;
        if (height > idealHeight || width > idealWidth) {
            if (width > height) {
                inSampleSize = Math.round((float)height / (float)idealHeight);
            } else {
                inSampleSize = Math.round((float)width / (float)idealWidth);
            }
        }
        return inSampleSize;
    }

    private void hidePluginLayer(Layer layer) {
        LayerView layerView = mLayerView;
        layerView.removeLayer(layer);
        layerView.requestRender();
    }

    private void showPluginLayer(Layer layer) {
        LayerView layerView = mLayerView;
        layerView.addLayer(layer);
        layerView.requestRender();
    }

    public void requestRender() {
        mLayerView.requestRender();
    }
    
    public void hidePlugins(Tab tab) {
        for (Layer layer : tab.getPluginLayers()) {
            if (layer instanceof PluginLayer) {
                ((PluginLayer) layer).setVisible(false);
            }

            hidePluginLayer(layer);
        }

        requestRender();
    }

    public void showPlugins() {
        Tabs tabs = Tabs.getInstance();
        Tab tab = tabs.getSelectedTab();

        showPlugins(tab);
    }

    public void showPlugins(Tab tab) {
        for (Layer layer : tab.getPluginLayers()) {
            showPluginLayer(layer);

            if (layer instanceof PluginLayer) {
                ((PluginLayer) layer).setVisible(true);
            }
        }

        requestRender();
    }

    public void setFullScreen(final boolean fullscreen) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                // Hide/show the system notification bar
                Window window = getWindow();
                window.setFlags(fullscreen ?
                                WindowManager.LayoutParams.FLAG_FULLSCREEN : 0,
                                WindowManager.LayoutParams.FLAG_FULLSCREEN);

                if (Build.VERSION.SDK_INT >= 11)
                    window.getDecorView().setSystemUiVisibility(fullscreen ? 1 : 0);
            }
        });
    }

    /**
     * Called when the activity is first created.
     *
     * Here we initialize all of our profile settings, Firefox Health Report,
     * and other one-shot constructions.
     **/
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        GeckoAppShell.registerGlobalExceptionHandler();

        // Enable Android Strict Mode for developers' local builds (the "default" channel).
        if ("default".equals(AppConstants.MOZ_UPDATE_CHANNEL)) {
            enableStrictMode();
        }

        // The clock starts...now. Better hurry!
        mJavaUiStartupTimer = new Telemetry.Timer("FENNEC_STARTUP_TIME_JAVAUI");
        mGeckoReadyStartupTimer = new Telemetry.Timer("FENNEC_STARTUP_TIME_GECKOREADY");

        String args = getIntent().getStringExtra("args");

        String profileName = null;
        String profilePath = null;
        if (args != null) {
            if (args.contains("-P")) {
                Pattern p = Pattern.compile("(?:-P\\s*)(\\w*)(\\s*)");
                Matcher m = p.matcher(args);
                if (m.find()) {
                    profileName = m.group(1);
                }
            }

            if (args.contains("-profile")) {
                Pattern p = Pattern.compile("(?:-profile\\s*)(\\S*)(\\s*)");
                Matcher m = p.matcher(args);
                if (m.find()) {
                    profilePath =  m.group(1);
                }
                if (profileName == null) {
                    profileName = getDefaultProfileName();
                    if (profileName == null)
                        profileName = "default";
                }
                GeckoApp.sIsUsingCustomProfile = true;
            }

            if (profileName != null || profilePath != null) {
                mProfile = GeckoProfile.get(this, profileName, profilePath);
            }
        }

        BrowserDB.initialize(getProfile().getName());
        ((GeckoApplication)getApplication()).initialize();

        sAppContext = this;
        GeckoAppShell.setContextGetter(this);
        GeckoAppShell.setGeckoInterface(this);
        ThreadUtils.setUiThread(Thread.currentThread(), new Handler());

        Tabs.getInstance().attachToActivity(this);
        Favicons.getInstance().attachToContext(this);

        // Check to see if the activity is restarted after configuration change.
        if (getLastCustomNonConfigurationInstance() != null) {
            // Restart the application as a safe way to handle the configuration change.
            doRestart();
            System.exit(0);
            return;
        }

        if (sGeckoThread != null) {
            // This happens when the GeckoApp activity is destroyed by Android
            // without killing the entire application (see Bug 769269).
            mIsRestoringActivity = true;
            Telemetry.HistogramAdd("FENNEC_RESTORING_ACTIVITY", 1);
        }

        // Fix for Bug 830557 on Tegra boards running Froyo.
        // This fix must be done before doing layout.
        // Assume the bug is fixed in Gingerbread and up.
        if (Build.VERSION.SDK_INT < 9) {
            try {
                Class<?> inputBindResultClass =
                    Class.forName("com.android.internal.view.InputBindResult");
                java.lang.reflect.Field creatorField =
                    inputBindResultClass.getField("CREATOR");
                Log.i(LOGTAG, "froyo startup fix: " + String.valueOf(creatorField.get(null)));
            } catch (Exception e) {
                Log.w(LOGTAG, "froyo startup fix failed", e);
            }
        }

        LayoutInflater.from(this).setFactory(this);

        super.onCreate(savedInstanceState);

        mOrientation = getResources().getConfiguration().orientation;

        setContentView(getLayout());

        // Set up Gecko layout.
        mGeckoLayout = (RelativeLayout) findViewById(R.id.gecko_layout);
        mMainLayout = (RelativeLayout) findViewById(R.id.main_layout);

        // Set up tabs panel.
        mTabsPanel = (TabsPanel) findViewById(R.id.tabs_panel);

        // Check if the last run was exited due to a normal kill while
        // we were in the background, or a more harsh kill while we were
        // active.
        mRestoreMode = getSessionRestoreState(savedInstanceState);
        if (mRestoreMode == RESTORE_OOM) {
            boolean wasInBackground =
                savedInstanceState.getBoolean(SAVED_STATE_IN_BACKGROUND, false);

            // Don't log OOM-kills if only one activity was destroyed. (For example
            // from "Don't keep activities" on ICS)
            if (!wasInBackground && !mIsRestoringActivity) {
                Telemetry.HistogramAdd("FENNEC_WAS_KILLED", 1);
            }

            mPrivateBrowsingSession = savedInstanceState.getString(SAVED_STATE_PRIVATE_SESSION);
        }

        // Perform background initialization.
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                SharedPreferences prefs =
                    GeckoApp.sAppContext.getSharedPreferences(PREFS_NAME, 0);

                boolean wasOOM = prefs.getBoolean(PREFS_OOM_EXCEPTION, false);
                boolean wasStopped = prefs.getBoolean(PREFS_WAS_STOPPED, true);
                if (wasOOM || !wasStopped) {
                    Telemetry.HistogramAdd("FENNEC_WAS_KILLED", 1);
                }
                SharedPreferences.Editor editor = prefs.edit();
                editor.putBoolean(GeckoApp.PREFS_OOM_EXCEPTION, false);

                // Put a flag to check if we got a normal onSaveInstanceState
                // on exit, or if we were suddenly killed (crash or native OOM)
                editor.putBoolean(GeckoApp.PREFS_WAS_STOPPED, false);
                editor.commit();

                // The lifecycle of mHealthRecorder is "shortly after onCreate"
                // through "onDestroy" -- essentially the same as the lifecycle
                // of the activity itself.
                final String profilePath = getProfile().getDir().getAbsolutePath();
                final EventDispatcher dispatcher = GeckoAppShell.getEventDispatcher();
                Log.i(LOGTAG, "Creating BrowserHealthRecorder.");
                mHealthRecorder = new BrowserHealthRecorder(sAppContext, profilePath, dispatcher);
            }
        });

        GeckoAppShell.setNotificationClient(makeNotificationClient());
    }

    protected void initializeChrome() {
        mDoorHangerPopup = new DoorHangerPopup(this, null);
        mPluginContainer = (AbsoluteLayout) findViewById(R.id.plugin_container);
        mFormAssistPopup = (FormAssistPopup) findViewById(R.id.form_assist_popup);

        if (cameraView == null) {
            cameraView = new SurfaceView(this);
            cameraView.getHolder().setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
        }

        if (mLayerView == null) {
            LayerView layerView = (LayerView) findViewById(R.id.layer_view);
            layerView.initializeView(GeckoAppShell.getEventDispatcher());
            mLayerView = layerView;
            // bind the GeckoEditable instance to the new LayerView
            GeckoAppShell.notifyIMEContext(GeckoEditableListener.IME_STATE_DISABLED, "", "", "");
        }
    }

    /**
     * Loads the initial tab at Fennec startup.
     *
     * If Fennec was opened with an external URL, that URL will be loaded.
     * Otherwise, unless there was a session restore, the default URL
     * (about:home) be loaded.
     *
     * @param url External URL to load, or null to load the default URL
     */
    protected void loadStartupTab(String url) {
        if (url == null) {
            if (mRestoreMode == RESTORE_NONE) {
                // Show about:home if we aren't restoring previous session and
                // there's no external URL
                Tab tab = Tabs.getInstance().loadUrl("about:home", Tabs.LOADURL_NEW_TAB);
            }
        } else {
            // If given an external URL, load it
            int flags = Tabs.LOADURL_NEW_TAB | Tabs.LOADURL_USER_ENTERED;
            Tabs.getInstance().loadUrl(url, flags);
        }
    }

    private void initialize() {
        mInitialized = true;

        invalidateOptionsMenu();

        Intent intent = getIntent();
        String action = intent.getAction();

        String passedUri = null;
        String uri = getURIFromIntent(intent);
        if (uri != null && uri.length() > 0) {
            passedUri = uri;
        }

        final boolean isExternalURL = passedUri != null && !passedUri.equals("about:home");
        StartupAction startupAction;
        if (isExternalURL) {
            startupAction = StartupAction.URL;
        } else {
            startupAction = StartupAction.NORMAL;
        }

        // Start migrating as early as possible, can do this in
        // parallel with Gecko load.
        checkMigrateProfile();

        Uri data = intent.getData();
        if (data != null && "http".equals(data.getScheme())) {
            startupAction = StartupAction.PREFETCH;
            ThreadUtils.postToBackgroundThread(new PrefetchRunnable(data.toString()));
        }

        Tabs.registerOnTabsChangedListener(this);

        initializeChrome();

        // If we are doing a restore, read the session data and send it to Gecko
        String restoreMessage = null;
        if (mRestoreMode != RESTORE_NONE && !mIsRestoringActivity) {
            try {
                // restoreSessionTabs() will create simple tab stubs with the
                // URL and title for each page, but we also need to restore
                // session history. restoreSessionTabs() will inject the IDs
                // of the tab stubs into the JSON data (which holds the session
                // history). This JSON data is then sent to Gecko so session
                // history can be restored for each tab.
                restoreMessage = restoreSessionTabs(isExternalURL);
            } catch (SessionRestoreException e) {
                // If restore failed, do a normal startup
                Log.e(LOGTAG, "An error occurred during restore", e);
                mRestoreMode = RESTORE_NONE;
            }
        }

        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Session:Restore", restoreMessage));

        if (!mIsRestoringActivity) {
            loadStartupTab(isExternalURL ? passedUri : null);
        }

        if (mRestoreMode == RESTORE_OOM) {
            // If we successfully did an OOM restore, we now have tab stubs
            // from the last session. Any future tabs should be animated.
            Tabs.getInstance().notifyListeners(null, Tabs.TabEvents.RESTORED);
        } else {
            // Move the session file if it exists
            getProfile().moveSessionFile();
        }

        if (mRestoreMode == RESTORE_NONE) {
            Tabs.getInstance().notifyListeners(null, Tabs.TabEvents.RESTORED);
        }

        Telemetry.HistogramAdd("FENNEC_STARTUP_GECKOAPP_ACTION", startupAction.ordinal());

        if (!mIsRestoringActivity) {
            sGeckoThread = new GeckoThread(intent, passedUri);
            ThreadUtils.setGeckoThread(sGeckoThread);
        }
        if (!ACTION_DEBUG.equals(action) &&
            GeckoThread.checkAndSetLaunchState(GeckoThread.LaunchState.Launching, GeckoThread.LaunchState.Launched)) {
            sGeckoThread.start();
        } else if (ACTION_DEBUG.equals(action) &&
            GeckoThread.checkAndSetLaunchState(GeckoThread.LaunchState.Launching, GeckoThread.LaunchState.WaitForDebugger)) {
            ThreadUtils.getUiHandler().postDelayed(new Runnable() {
                @Override
                public void run() {
                    GeckoThread.setLaunchState(GeckoThread.LaunchState.Launching);
                    sGeckoThread.start();
                }
            }, 1000 * 5 /* 5 seconds */);
        }

        //register for events
        registerEventListener("log");
        registerEventListener("Reader:Added");
        registerEventListener("Reader:Removed");
        registerEventListener("Reader:Share");
        registerEventListener("Reader:FaviconRequest");
        registerEventListener("Reader:GoToReadingList");
        registerEventListener("onCameraCapture");
        registerEventListener("Menu:Add");
        registerEventListener("Menu:Remove");
        registerEventListener("Menu:Update");
        registerEventListener("Gecko:Ready");
        registerEventListener("Toast:Show");
        registerEventListener("DOMFullScreen:Start");
        registerEventListener("DOMFullScreen:Stop");
        registerEventListener("ToggleChrome:Hide");
        registerEventListener("ToggleChrome:Show");
        registerEventListener("ToggleChrome:Focus");
        registerEventListener("Permissions:Data");
        registerEventListener("Tab:ViewportMetadata");
        registerEventListener("Session:StatePurged");
        registerEventListener("Bookmark:Insert");
        registerEventListener("Accessibility:Event");
        registerEventListener("Accessibility:Ready");
        registerEventListener("Shortcut:Remove");
        registerEventListener("WebApps:Open");
        registerEventListener("WebApps:PreInstall");
        registerEventListener("WebApps:PostInstall");
        registerEventListener("WebApps:Install");
        registerEventListener("WebApps:Uninstall");
        registerEventListener("Share:Text");
        registerEventListener("Share:Image");
        registerEventListener("Wallpaper:Set");
        registerEventListener("Sanitize:ClearHistory");
        registerEventListener("Update:Check");
        registerEventListener("Update:Download");
        registerEventListener("Update:Install");
        registerEventListener("PrivateBrowsing:Data");

        if (SmsManager.getInstance() != null) {
          SmsManager.getInstance().start();
        }

        mPromptService = new PromptService(this);

        mTextSelection = new TextSelection((TextSelectionHandle) findViewById(R.id.start_handle),
                                           (TextSelectionHandle) findViewById(R.id.middle_handle),
                                           (TextSelectionHandle) findViewById(R.id.end_handle),
                                           GeckoAppShell.getEventDispatcher(),
                                           this);

        PrefsHelper.getPref("app.update.autodownload", new PrefsHelper.PrefHandlerBase() {
            @Override public void prefValue(String pref, String value) {
                UpdateServiceHelper.registerForUpdates(GeckoApp.this, value);
            }
        });

        // End of the startup of our Java App
        mJavaUiStartupTimer.stop();

        ThreadUtils.getBackgroundHandler().postDelayed(new Runnable() {
            @Override
            public void run() {
                // Sync settings need Gecko to be loaded, so
                // no hurry in starting this.
                checkMigrateSync();

                // Record our launch time for the announcements service
                // to use in assessing inactivity.
                final Context context = GeckoApp.sAppContext;
                AnnouncementsBroadcastService.recordLastLaunch(context);

                // Kick off our background service to fetch product announcements.
                // We do this by invoking the broadcast receiver, which uses the
                // system alarm infrastructure to perform tasks at intervals.
                GeckoPreferences.broadcastAnnouncementsPref(context);

                /*
                  XXXX see bug 635342
                   We want to disable this code if possible.  It is about 145ms in runtime
                SharedPreferences settings = getPreferences(Activity.MODE_PRIVATE);
                String localeCode = settings.getString(getPackageName() + ".locale", "");
                if (localeCode != null && localeCode.length() > 0)
                    GeckoAppShell.setSelectedLocale(localeCode);
                */

                if (!GeckoThread.checkLaunchState(GeckoThread.LaunchState.Launched)) {
                    return;
                }
            }
        }, 50);

        if (mIsRestoringActivity) {
            GeckoThread.setLaunchState(GeckoThread.LaunchState.GeckoRunning);
            Tab selectedTab = Tabs.getInstance().getSelectedTab();
            if (selectedTab != null)
                Tabs.getInstance().notifyListeners(selectedTab, Tabs.TabEvents.SELECTED);
            connectGeckoLayerClient();
            GeckoAppShell.setLayerClient(mLayerView.getLayerClient());
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Viewport:Flush", null));
        }

        if (ACTION_ALERT_CALLBACK.equals(action)) {
            processAlertCallback(intent);
        }
    }

    private String restoreSessionTabs(final boolean isExternalURL) throws SessionRestoreException {
        try {
            String sessionString = getProfile().readSessionFile(false);
            if (sessionString == null) {
                throw new SessionRestoreException("Could not read from session file");
            }

            // If we are doing an OOM restore, parse the session data and
            // stub the restored tabs immediately. This allows the UI to be
            // updated before Gecko has restored.
            if (mRestoreMode == RESTORE_OOM) {
                final JSONArray tabs = new JSONArray();
                SessionParser parser = new SessionParser() {
                    @Override
                    public void onTabRead(SessionTab sessionTab) {
                        JSONObject tabObject = sessionTab.getTabObject();

                        int flags = Tabs.LOADURL_NEW_TAB;
                        flags |= ((isExternalURL || !sessionTab.isSelected()) ? Tabs.LOADURL_DELAY_LOAD : 0);
                        flags |= (tabObject.optBoolean("desktopMode") ? Tabs.LOADURL_DESKTOP : 0);
                        flags |= (tabObject.optBoolean("isPrivate") ? Tabs.LOADURL_PRIVATE : 0);

                        Tab tab = Tabs.getInstance().loadUrl(sessionTab.getUrl(), flags);
                        tab.updateTitle(sessionTab.getTitle());

                        try {
                            tabObject.put("tabId", tab.getId());
                        } catch (JSONException e) {
                            Log.e(LOGTAG, "JSON error", e);
                        }
                        tabs.put(tabObject);
                    }
                };

                if (mPrivateBrowsingSession == null) {
                    parser.parse(sessionString);
                } else {
                    parser.parse(sessionString, mPrivateBrowsingSession);
                }

                if (tabs.length() > 0) {
                    sessionString = new JSONObject().put("windows", new JSONArray().put(new JSONObject().put("tabs", tabs))).toString();
                } else {
                    throw new SessionRestoreException("No tabs could be read from session file");
                }
            }

            JSONObject restoreData = new JSONObject();
            restoreData.put("restoringOOM", mRestoreMode == RESTORE_OOM);
            restoreData.put("sessionString", sessionString);
            return restoreData.toString();

        } catch (JSONException e) {
            throw new SessionRestoreException(e);
        }
    }

    public GeckoProfile getProfile() {
        // fall back to default profile if we didn't load a specific one
        if (mProfile == null) {
            mProfile = GeckoProfile.get(this);
        }
        return mProfile;
    }

    protected int getSessionRestoreState(Bundle savedInstanceState) {
        if (savedInstanceState != null) {
            return RESTORE_OOM;
        }

        final SharedPreferences prefs = GeckoApp.sAppContext.getSharedPreferences(PREFS_NAME, 0);

        // We record crashes in the crash reporter. If sessionstore.js
        // exists, but we didn't flag a crash in the crash reporter, we
        // were probably just force killed by the user, so we shouldn't do
        // a restore.
        if (prefs.getBoolean(PREFS_CRASHED, false)) {
            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    prefs.edit()
                         .putBoolean(GeckoApp.PREFS_CRASHED, false)
                         .commit();
                }
            });

            if (getProfile().shouldRestoreSession()) {
                return RESTORE_CRASH;
            }
        }
        return RESTORE_NONE;
    }

    /**
     * Enable Android StrictMode checks (for supported OS versions).
     * http://developer.android.com/reference/android/os/StrictMode.html
     */
    private void enableStrictMode() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.GINGERBREAD) {
            return;
        }

        Log.d(LOGTAG, "Enabling Android StrictMode");

        StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder()
                                  .detectAll()
                                  .penaltyLog()
                                  .build());

        StrictMode.setVmPolicy(new StrictMode.VmPolicy.Builder()
                               .detectAll()
                               .penaltyLog()
                               .build());
    }

    public void enableCameraView() {
        // Some phones (eg. nexus S) need at least a 8x16 preview size
        mMainLayout.addView(cameraView, new AbsoluteLayout.LayoutParams(8, 16, 0, 0));
    }

    public void disableCameraView() {
        mMainLayout.removeView(cameraView);
    }

    public String getDefaultUAString() {
        return HardwareUtils.isTablet() ? AppConstants.USER_AGENT_FENNEC_TABLET :
                                          AppConstants.USER_AGENT_FENNEC_MOBILE;
    }

    public String getUAStringForHost(String host) {
        // With our standard UA String, we get a 200 response code and
        // client-side redirect from t.co. This bot-like UA gives us a
        // 301 response code
        if ("t.co".equals(host)) {
            return AppConstants.USER_AGENT_BOT_LIKE;
        }
        return getDefaultUAString();
    }

    class PrefetchRunnable implements Runnable {
        private String mPrefetchUrl;

        PrefetchRunnable(String prefetchUrl) {
            mPrefetchUrl = prefetchUrl;
        }

        @Override
        public void run() {
            HttpURLConnection connection = null;
            try {
                URL url = new URL(mPrefetchUrl);
                // data url should have an http scheme
                connection = (HttpURLConnection) url.openConnection();
                connection.setRequestProperty("User-Agent", getUAStringForHost(url.getHost()));
                connection.setInstanceFollowRedirects(false);
                connection.setRequestMethod("GET");
                connection.connect();
            } catch (Exception e) {
                Log.e(LOGTAG, "Exception prefetching URL", e);
            } finally {
                if (connection != null)
                    connection.disconnect();
            }
        }
    }

    private void processAlertCallback(Intent intent) {
        String alertName = "";
        String alertCookie = "";
        Uri data = intent.getData();
        if (data != null) {
            alertName = data.getQueryParameter("name");
            if (alertName == null)
                alertName = "";
            alertCookie = data.getQueryParameter("cookie");
            if (alertCookie == null)
                alertCookie = "";
        }
        handleNotification(ACTION_ALERT_CALLBACK, alertName, alertCookie);
    }

    @Override
    protected void onNewIntent(Intent intent) {
        if (GeckoThread.checkLaunchState(GeckoThread.LaunchState.GeckoExiting)) {
            // We're exiting and shouldn't try to do anything else just incase
            // we're hung for some reason we'll force the process to exit
            System.exit(0);
            return;
        }

        // if we were previously OOM killed, we can end up here when launching
        // from external shortcuts, so set this as the intent for initialization
        if (!mInitialized) {
            setIntent(intent);
            return;
        }

        // don't perform any actions if launching from recent apps
        if ((intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY) != 0)
            return;

        final String action = intent.getAction();

        if (Intent.ACTION_MAIN.equals(action)) {
            GeckoAppShell.sendEventToGecko(GeckoEvent.createURILoadEvent(""));
        } else if (ACTION_LOAD.equals(action)) {
            String uri = intent.getDataString();
            Tabs.getInstance().loadUrl(uri);
        } else if (Intent.ACTION_VIEW.equals(action)) {
            String uri = intent.getDataString();
            GeckoAppShell.sendEventToGecko(GeckoEvent.createURILoadEvent(uri));
        } else if (action != null && action.startsWith(ACTION_WEBAPP_PREFIX)) {
            String uri = getURIFromIntent(intent);
            GeckoAppShell.sendEventToGecko(GeckoEvent.createWebappLoadEvent(uri));
        } else if (ACTION_BOOKMARK.equals(action)) {
            String uri = getURIFromIntent(intent);
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBookmarkLoadEvent(uri));
        } else if (Intent.ACTION_SEARCH.equals(action)) {
            String uri = getURIFromIntent(intent);
            GeckoAppShell.sendEventToGecko(GeckoEvent.createURILoadEvent(uri));
        } else if (ACTION_ALERT_CALLBACK.equals(action)) {
            processAlertCallback(intent);
        }
    }

    /*
     * Handles getting a uri from and intent in a way that is backwards
     * compatable with our previous implementations
     */
    protected String getURIFromIntent(Intent intent) {
        final String action = intent.getAction();
        if (ACTION_ALERT_CALLBACK.equals(action))
            return null;

        String uri = intent.getDataString();
        if (uri != null)
            return uri;

        if ((action != null && action.startsWith(ACTION_WEBAPP_PREFIX)) || ACTION_BOOKMARK.equals(action)) {
            uri = intent.getStringExtra("args");
            if (uri != null && uri.startsWith("--url=")) {
                uri.replace("--url=", "");
            }
        }
        return uri;
    }

    @Override
    public void onResume()
    {
        // After an onPause, the activity is back in the foreground.
        // Undo whatever we did in onPause.
        super.onResume();

        SiteIdentityPopup.getInstance().dismiss();

        int newOrientation = getResources().getConfiguration().orientation;

        if (mOrientation != newOrientation) {
            mOrientation = newOrientation;
            refreshChrome();
        }

        GeckoScreenOrientationListener.getInstance().start();

        // User may have enabled/disabled accessibility.
        GeckoAccessibility.updateAccessibilitySettings(this);

        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                SharedPreferences prefs =
                    GeckoApp.sAppContext.getSharedPreferences(GeckoApp.PREFS_NAME, 0);
                SharedPreferences.Editor editor = prefs.edit();
                editor.putBoolean(GeckoApp.PREFS_WAS_STOPPED, false);
                editor.commit();
            }
         });
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);

        if (!mInitialized && hasFocus) {
            initialize();
            getWindow().setBackgroundDrawable(null);
        }
    }

    @Override
    public void onPause()
    {
        // In some way it's sad that Android will trigger StrictMode warnings
        // here as the whole point is to save to disk while the activity is not
        // interacting with the user.
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                SharedPreferences prefs =
                    GeckoApp.sAppContext.getSharedPreferences(GeckoApp.PREFS_NAME, 0);
                SharedPreferences.Editor editor = prefs.edit();
                editor.putBoolean(GeckoApp.PREFS_WAS_STOPPED, true);
                editor.commit();
            }
        });

        GeckoScreenOrientationListener.getInstance().stop();

        super.onPause();
    }

    @Override
    public void onRestart()
    {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                SharedPreferences prefs =
                    GeckoApp.sAppContext.getSharedPreferences(GeckoApp.PREFS_NAME, 0);
                SharedPreferences.Editor editor = prefs.edit();
                editor.putBoolean(GeckoApp.PREFS_WAS_STOPPED, false);
                editor.commit();
            }
        });

        super.onRestart();
    }

    @Override
    public void onDestroy()
    {
        unregisterEventListener("log");
        unregisterEventListener("Reader:Added");
        unregisterEventListener("Reader:Removed");
        unregisterEventListener("Reader:Share");
        unregisterEventListener("Reader:FaviconRequest");
        unregisterEventListener("Reader:GoToReadingList");
        unregisterEventListener("onCameraCapture");
        unregisterEventListener("Menu:Add");
        unregisterEventListener("Menu:Remove");
        unregisterEventListener("Menu:Update");
        unregisterEventListener("Gecko:Ready");
        unregisterEventListener("Toast:Show");
        unregisterEventListener("DOMFullScreen:Start");
        unregisterEventListener("DOMFullScreen:Stop");
        unregisterEventListener("ToggleChrome:Hide");
        unregisterEventListener("ToggleChrome:Show");
        unregisterEventListener("ToggleChrome:Focus");
        unregisterEventListener("Permissions:Data");
        unregisterEventListener("Tab:ViewportMetadata");
        unregisterEventListener("Session:StatePurged");
        unregisterEventListener("Bookmark:Insert");
        unregisterEventListener("Accessibility:Event");
        unregisterEventListener("Accessibility:Ready");
        unregisterEventListener("Shortcut:Remove");
        unregisterEventListener("WebApps:Open");
        unregisterEventListener("WebApps:PreInstall");
        unregisterEventListener("WebApps:PostInstall");
        unregisterEventListener("WebApps:Install");
        unregisterEventListener("WebApps:Uninstall");
        unregisterEventListener("Share:Text");
        unregisterEventListener("Share:Image");
        unregisterEventListener("Wallpaper:Set");
        unregisterEventListener("Sanitize:ClearHistory");
        unregisterEventListener("Update:Check");
        unregisterEventListener("Update:Download");
        unregisterEventListener("Update:Install");
        unregisterEventListener("PrivateBrowsing:Data");

        deleteTempFiles();

        if (mLayerView != null)
            mLayerView.destroy();
        if (mDoorHangerPopup != null)
            mDoorHangerPopup.destroy();
        if (mFormAssistPopup != null)
            mFormAssistPopup.destroy();
        if (mPromptService != null)
            mPromptService.destroy();
        if (mTextSelection != null)
            mTextSelection.destroy();
        SiteIdentityPopup.clearInstance();

        Tabs.getInstance().detachFromActivity(this);

        if (SmsManager.getInstance() != null) {
            SmsManager.getInstance().stop();
            if (isFinishing())
                SmsManager.getInstance().shutdown();
        }

        super.onDestroy();

        Tabs.unregisterOnTabsChangedListener(this);
        if (mHealthRecorder != null) {
            mHealthRecorder.close(GeckoAppShell.getEventDispatcher());
            mHealthRecorder = null;
        }
    }

    protected void registerEventListener(String event) {
        GeckoAppShell.getEventDispatcher().registerEventListener(event, this);
    }

    protected void unregisterEventListener(String event) {
        GeckoAppShell.getEventDispatcher().unregisterEventListener(event, this);
    }

    // Get a temporary directory, may return null
    public static File getTempDirectory() {
        File dir = sAppContext.getExternalFilesDir("temp");
        return dir;
    }

    // Delete any files in our temporary directory
    public static void deleteTempFiles() {
        File dir = getTempDirectory();
        if (dir == null)
            return;
        File[] files = dir.listFiles();
        if (files == null)
            return;
        for (File file : files) {
            file.delete();
        }
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        if (mOrientation != newConfig.orientation) {
            mOrientation = newConfig.orientation;
            if (mFormAssistPopup != null)
                mFormAssistPopup.hide();
            SiteIdentityPopup.getInstance().dismiss();
            refreshChrome();
        }
    }

    @Override
    public Object onRetainCustomNonConfigurationInstance() {
        // Send a non-null value so that we can restart the application, 
        // when activity restarts due to configuration change.
        return Boolean.TRUE;
    } 

    public String getContentProcessName() {
        return AppConstants.MOZ_CHILD_PROCESS_NAME;
    }

    /*
     * Only one factory can be set on the inflater; however, we want to use two
     * factories (GeckoViewsFactory and the FragmentActivity factory).
     * Overriding onCreateView() here allows us to dispatch view creation to
     * both factories.
     */
    @Override
    public View onCreateView(String name, Context context, AttributeSet attrs) {
        View view = GeckoViewsFactory.getInstance().onCreateView(name, context, attrs);
        if (view == null) {
            view = super.onCreateView(name, context, attrs);
        }
        return view;
    }

    public void addEnvToIntent(Intent intent) {
        Map<String,String> envMap = System.getenv();
        Set<Map.Entry<String,String>> envSet = envMap.entrySet();
        Iterator<Map.Entry<String,String>> envIter = envSet.iterator();
        int c = 0;
        while (envIter.hasNext()) {
            Map.Entry<String,String> entry = envIter.next();
            intent.putExtra("env" + c, entry.getKey() + "="
                            + entry.getValue());
            c++;
        }
    }

    public void doRestart() {
        doRestart(RESTARTER_ACTION);
    }

    public void doRestart(String action) {
        Log.d(LOGTAG, "doRestart(\"" + action + "\")");
        try {
            Intent intent = new Intent(action);
            intent.setClassName(AppConstants.ANDROID_PACKAGE_NAME, RESTARTER_CLASS);
            /* TODO: addEnvToIntent(intent); */
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK |
                            Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
            Log.d(LOGTAG, "Restart intent: " + intent.toString());
            GeckoAppShell.killAnyZombies();
            startActivity(intent);
        } catch (Exception e) {
            Log.e(LOGTAG, "Error effecting restart.", e);
        }
        finish();
        // Give the restart process time to start before we die
        GeckoAppShell.waitForAnotherGeckoProc();
    }

    public void handleNotification(String action, String alertName, String alertCookie) {
        // If Gecko isn't running yet, we ignore the notification. Note that
        // even if Gecko is running but it was restarted since the notification
        // was created, the notification won't be handled (bug 849653).
        if (GeckoThread.checkLaunchState(GeckoThread.LaunchState.GeckoRunning)) {
            GeckoAppShell.handleNotification(action, alertName, alertCookie);
        }
    }

    private void checkMigrateProfile() {
        final File profileDir = getProfile().getDir();

        if (profileDir != null) {
            final GeckoApp app = GeckoApp.sAppContext;

            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    ProfileMigrator profileMigrator = new ProfileMigrator(app);

                    // Do a migration run on the first start after an upgrade.
                    if (!GeckoApp.sIsUsingCustomProfile &&
                        !profileMigrator.hasMigrationRun()) {
                        // Show the "Setting up Fennec" screen if this takes
                        // a while.

                        // Create a "final" holder for the setup screen so that we can
                        // create it in startCallback and still find a reference to it
                        // in stopCallback. (We must create it on the UI thread to fix
                        // bug 788216). Note that synchronization is not a problem here
                        // since it is only ever touched on the UI thread.
                        final SetupScreen[] setupScreenHolder = new SetupScreen[1];

                        final Runnable startCallback = new Runnable() {
                            @Override
                            public void run() {
                                ThreadUtils.postToUiThread(new Runnable() {
                                    @Override
                                    public void run() {
                                        setupScreenHolder[0] = new SetupScreen(app);
                                        setupScreenHolder[0].show();
                                    }
                                });
                            }
                        };

                        final Runnable stopCallback = new Runnable() {
                            @Override
                            public void run() {
                                ThreadUtils.postToUiThread(new Runnable() {
                                    @Override
                                    public void run() {
                                        SetupScreen screen = setupScreenHolder[0];
                                        // screen will never be null if this code runs, but
                                        // stranger things have happened...
                                        if (screen != null) {
                                            screen.dismiss();
                                        }
                                    }
                                });
                            }
                        };

                        profileMigrator.setLongOperationCallbacks(startCallback,
                                                                  stopCallback);
                        profileMigrator.launchPlaces(profileDir);
                        finishProfileMigration();
                    }
                }}
            );
        }
    }

    protected void finishProfileMigration() {
    }

    private void checkMigrateSync() {
        final File profileDir = getProfile().getDir();
        if (!GeckoApp.sIsUsingCustomProfile && profileDir != null) {
            final GeckoApp app = GeckoApp.sAppContext;
            ProfileMigrator profileMigrator = new ProfileMigrator(app);
            if (!profileMigrator.hasSyncMigrated()) {
                profileMigrator.launchSyncPrefs();
            }
        }
    }

    public PromptService getPromptService() {
        return mPromptService;
    }

    public void showReadingList() {
        Intent intent = new Intent(getBaseContext(), AwesomeBar.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION | Intent.FLAG_ACTIVITY_NO_HISTORY);
        intent.putExtra(AwesomeBar.TARGET_KEY, AwesomeBar.Target.CURRENT_TAB.toString());
        intent.putExtra(AwesomeBar.READING_LIST_KEY, true);

        int requestCode = GeckoAppShell.sActivityHelper.makeRequestCodeForAwesomebar();
        startActivityForResult(intent, requestCode);
    }

    @Override
    public void onBackPressed() {
        if (autoHideTabs()) {
            return;
        }

        if (mDoorHangerPopup != null && mDoorHangerPopup.isShowing()) {
            mDoorHangerPopup.dismiss();
            return;
        }

        if (mFullScreenPluginView != null) {
            GeckoAppShell.onFullScreenPluginHidden(mFullScreenPluginView);
            removeFullScreenPluginView(mFullScreenPluginView);
            return;
        }

        SiteIdentityPopup identityPopup = SiteIdentityPopup.getInstance();
        if (identityPopup.isShowing()) {
            identityPopup.dismiss();
            return;
        }

        if (mLayerView != null && mLayerView.isFullScreen()) {
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("FullScreen:Exit", null));
            return;
        }

        Tabs tabs = Tabs.getInstance();
        Tab tab = tabs.getSelectedTab();
        if (tab == null) {
            moveTaskToBack(true);
            return;
        }

        if (tab.doBack())
            return;

        if (tab.isExternal()) {
            moveTaskToBack(true);
            tabs.closeTab(tab);
            return;
        }

        int parentId = tab.getParentId();
        Tab parent = tabs.getTab(parentId);
        if (parent != null) {
            // The back button should always return to the parent (not a sibling).
            tabs.closeTab(tab, parent);
            return;
        }

        moveTaskToBack(true);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (!GeckoAppShell.sActivityHelper.handleActivityResult(requestCode, resultCode, data)) {
            super.onActivityResult(requestCode, resultCode, data);
        }
    }

    public LayerView getLayerView() {
        return mLayerView;
    }

    public AbsoluteLayout getPluginContainer() { return mPluginContainer; }

    // Accelerometer.
    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
    }

    @Override
    public void onSensorChanged(SensorEvent event) {
        GeckoAppShell.sendEventToGecko(GeckoEvent.createSensorEvent(event));
    }

    // Geolocation.
    @Override
    public void onLocationChanged(Location location) {
        // No logging here: user-identifying information.
        GeckoAppShell.sendEventToGecko(GeckoEvent.createLocationEvent(location));
    }

    @Override
    public void onProviderDisabled(String provider)
    {
    }

    @Override
    public void onProviderEnabled(String provider)
    {
    }

    @Override
    public void onStatusChanged(String provider, int status, Bundle extras)
    {
    }

    // Called when a Gecko Hal WakeLock is changed
    public void notifyWakeLockChanged(String topic, String state) {
        PowerManager.WakeLock wl = mWakeLocks.get(topic);
        if (state.equals("locked-foreground") && wl == null) {
            PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
            wl = pm.newWakeLock(PowerManager.SCREEN_BRIGHT_WAKE_LOCK, topic);
            wl.acquire();
            mWakeLocks.put(topic, wl);
        } else if (!state.equals("locked-foreground") && wl != null) {
            wl.release();
            mWakeLocks.remove(topic);
        }
    }

    public void notifyCheckUpdateResult(String result) {
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Update:CheckResult", result));
    }

    protected void connectGeckoLayerClient() {
        // See if we want to force 16-bit colour before doing anything
        PrefsHelper.getPref("gfx.android.rgb16.force", new PrefsHelper.PrefHandlerBase() {
            @Override public void prefValue(String pref, boolean force16bit) {
                if (force16bit) {
                    GeckoAppShell.setScreenDepthOverride(16);
                }
            }
        });

        mLayerView.getLayerClient().notifyGeckoReady();

        mLayerView.addTouchInterceptor(this);
    }

    public void setAccessibilityEnabled(boolean enabled) {
    }

    @Override
    public boolean onInterceptTouchEvent(View view, MotionEvent event) {
        return false;
    }

    @Override
    public boolean onTouch(View view, MotionEvent event) {
        if (event == null)
            return true;

        int action = event.getActionMasked();
        PointF point = new PointF(event.getX(), event.getY());
        if (action == MotionEvent.ACTION_DOWN) {
            mInitialTouchPoint = point;
        }

        if (mInitialTouchPoint != null && action == MotionEvent.ACTION_MOVE) {
            if (PointUtils.subtract(point, mInitialTouchPoint).length() <
                PanZoomController.PAN_THRESHOLD) {
                // Don't send the touchmove event if if the users finger hasn't moved far.
                // Necessary for Google Maps to work correctly. See bug 771099.
                return true;
            } else {
                mInitialTouchPoint = null;
            }
        }

        GeckoAppShell.sendEventToGecko(GeckoEvent.createMotionEvent(event, false));
        return true;
    }

    public static class MainLayout extends RelativeLayout {
        private TouchEventInterceptor mTouchEventInterceptor;
        private MotionEventInterceptor mMotionEventInterceptor;

        public MainLayout(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        public void setTouchEventInterceptor(TouchEventInterceptor interceptor) {
            mTouchEventInterceptor = interceptor;
        }

        public void setMotionEventInterceptor(MotionEventInterceptor interceptor) {
            mMotionEventInterceptor = interceptor;
        }

        @Override
        public boolean onInterceptTouchEvent(MotionEvent event) {
            if (mTouchEventInterceptor != null && mTouchEventInterceptor.onInterceptTouchEvent(this, event)) {
                return true;
            }
            return super.onInterceptTouchEvent(event);
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            if (mTouchEventInterceptor != null && mTouchEventInterceptor.onTouch(this, event)) {
                return true;
            }
            return super.onTouchEvent(event);
        }

        @Override
        public boolean onGenericMotionEvent(MotionEvent event) {
            if (mMotionEventInterceptor != null && mMotionEventInterceptor.onInterceptMotionEvent(this, event)) {
                return true;
            }
            return super.onGenericMotionEvent(event);
        }

        @Override
        public void setDrawingCacheEnabled(boolean enabled) {
            // Instead of setting drawing cache in the view itself, we simply
            // enable drawing caching on its children. This is mainly used in
            // animations (see PropertyAnimator)
            super.setChildrenDrawnWithCacheEnabled(enabled);
        }
    }

    private class FullScreenHolder extends FrameLayout {

        public FullScreenHolder(Context ctx) {
            super(ctx);
        }

        @Override
        public void addView(View view, int index) {
            /**
             * This normally gets called when Flash adds a separate SurfaceView
             * for the video. It is unhappy if we have the LayerView underneath
             * it for some reason so we need to hide that. Hiding the LayerView causes
             * its surface to be destroyed, which causes a pause composition
             * event to be sent to Gecko. We synchronously wait for that to be
             * processed. Simultaneously, however, Flash is waiting on a mutex so
             * the post() below is an attempt to avoid a deadlock.
             */
            super.addView(view, index);

            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    mLayerView.hide();
                }
            });
        }

        /**
         * The methods below are simply copied from what Android WebKit does.
         * It wasn't ever called in my testing, but might as well
         * keep it in case it is for some reason. The methods
         * all return true because we don't want any events
         * leaking out from the fullscreen view.
         */
        @Override
        public boolean onKeyDown(int keyCode, KeyEvent event) {
            if (event.isSystem()) {
                return super.onKeyDown(keyCode, event);
            }
            mFullScreenPluginView.onKeyDown(keyCode, event);
            return true;
        }

        @Override
        public boolean onKeyUp(int keyCode, KeyEvent event) {
            if (event.isSystem()) {
                return super.onKeyUp(keyCode, event);
            }
            mFullScreenPluginView.onKeyUp(keyCode, event);
            return true;
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            return true;
        }

        @Override
        public boolean onTrackballEvent(MotionEvent event) {
            mFullScreenPluginView.onTrackballEvent(event);
            return true;
        }
    }

    protected NotificationClient makeNotificationClient() {
        // Don't use a notification service; we may be killed in the background
        // during downloads.
        return new AppNotificationClient(getApplicationContext());
    }
}
