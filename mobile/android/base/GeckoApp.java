/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.background.announcements.AnnouncementsBroadcastService;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.gfx.Layer;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.gfx.PluginLayer;
import org.mozilla.gecko.gfx.PointUtils;
import org.mozilla.gecko.ui.PanZoomController;
import org.mozilla.gecko.util.GeckoAsyncTask;
import org.mozilla.gecko.util.GeckoBackgroundThread;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.GeckoEventResponder;
import org.mozilla.gecko.updater.UpdateServiceHelper;
import org.mozilla.gecko.updater.UpdateService;

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
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.content.pm.Signature;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
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
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.AbsoluteLayout;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
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
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

abstract public class GeckoApp
                extends GeckoActivity 
                implements GeckoEventListener, SensorEventListener, LocationListener,
                           Tabs.OnTabsChangedListener, GeckoEventResponder,
                           GeckoMenu.Callback, GeckoMenu.MenuPresenter
{
    private static final String LOGTAG = "GeckoApp";

    public static enum StartupMode {
        NORMAL,
        NEW_VERSION,
        NEW_PROFILE
    }

    private static enum StartupAction {
        NORMAL,     /* normal application start */
        URL,        /* launched with a passed URL */
        PREFETCH,   /* launched with a passed URL that we prefetch */
        REDIRECTOR  /* launched with a passed URL in our redirect list */
    }

    public static final String ACTION_ALERT_CLICK   = "org.mozilla.gecko.ACTION_ALERT_CLICK";
    public static final String ACTION_ALERT_CLEAR   = "org.mozilla.gecko.ACTION_ALERT_CLEAR";
    public static final String ACTION_ALERT_CALLBACK = "org.mozilla.gecko.ACTION_ALERT_CALLBACK";
    public static final String ACTION_WEBAPP_PREFIX = "org.mozilla.gecko.WEBAPP";
    public static final String ACTION_DEBUG         = "org.mozilla.gecko.DEBUG";
    public static final String ACTION_BOOKMARK      = "org.mozilla.gecko.BOOKMARK";
    public static final String ACTION_LOAD          = "org.mozilla.gecko.LOAD";
    public static final String ACTION_INIT_PW       = "org.mozilla.gecko.INIT_PW";
    public static final String ACTION_WIDGET        = "org.mozilla.gecko.WIDGET";
    public static final String SAVED_STATE_IN_BACKGROUND = "inBackground";
    public static final String SAVED_STATE_PRIVATE_SESSION = "privateSession";

    public static final String PREFS_NAME          = "GeckoApp";
    public static final String PREFS_OOM_EXCEPTION = "OOMException";
    public static final String PREFS_WAS_STOPPED   = "wasStopped";
    public static final String PREFS_CRASHED       = "crashed";

    static public final int RESTORE_NONE = 0;
    static public final int RESTORE_OOM = 1;
    static public final int RESTORE_CRASH = 2;

    StartupMode mStartupMode = null;
    protected LinearLayout mMainLayout;
    protected RelativeLayout mGeckoLayout;
    public View getView() { return mGeckoLayout; }
    public SurfaceView cameraView;
    public static GeckoApp mAppContext;
    protected MenuPanel mMenuPanel;
    protected Menu mMenu;
    private static GeckoThread sGeckoThread;
    public Handler mMainHandler;
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

    public enum LaunchState {Launching, WaitForDebugger,
                             Launched, GeckoRunning, GeckoExiting};
    private static LaunchState sLaunchState = LaunchState.Launching;

    abstract public int getLayout();
    abstract public boolean hasTabsSideBar();
    abstract protected String getDefaultProfileName();

    public static boolean checkLaunchState(LaunchState checkState) {
        synchronized(sLaunchState) {
            return sLaunchState == checkState;
        }
    }

    static void setLaunchState(LaunchState setState) {
        synchronized(sLaunchState) {
            sLaunchState = setState;
        }
    }

    // if mLaunchState is equal to checkState this sets mLaunchState to setState
    // and return true. Otherwise we return false.
    static boolean checkAndSetLaunchState(LaunchState checkState, LaunchState setState) {
        synchronized(sLaunchState) {
            if (sLaunchState != checkState)
                return false;
            sLaunchState = setState;
            return true;
        }
    }

    void toggleChrome(final boolean aShow) { }

    void focusChrome() { }

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
        }
    }

    public void refreshChrome() { }

    public static final String PLUGIN_ACTION = "android.webkit.PLUGIN";

    /**
     * A plugin that wish to be loaded in the WebView must provide this permission
     * in their AndroidManifest.xml.
     */
    public static final String PLUGIN_PERMISSION = "android.webkit.permission.PLUGIN";

    private static final String PLUGIN_SYSTEM_LIB = "/system/lib/plugins/";

    private static final String PLUGIN_TYPE = "type";
    private static final String TYPE_NATIVE = "native";
    public ArrayList<PackageInfo> mPackageInfoCache = new ArrayList<PackageInfo>();

    // Returns null if plugins are blocked on the device.
    String[] getPluginDirectories() {

        // An awful hack to detect Tegra devices. Easiest way to do it without spinning up a EGL context.
        boolean isTegra = (new File("/system/lib/hw/gralloc.tegra.so")).exists();
        if (isTegra) {
            // disable Flash on Tegra ICS with CM9 and other custom firmware (bug 736421)
            File vfile = new File("/proc/version");
            FileReader vreader = null;
            try {
                if (vfile.canRead()) {
                    vreader = new FileReader(vfile);
                    String version = new BufferedReader(vreader).readLine();
                    if (version.indexOf("CM9") != -1 ||
                        version.indexOf("cyanogen") != -1 ||
                        version.indexOf("Nova") != -1)
                    {
                        Log.w(LOGTAG, "Blocking plugins because of Tegra 2 + unofficial ICS bug (bug 736421)");
                        return null;
                    }
                }
            } catch (IOException ex) {
                // nothing
            } finally {
                try {
                    if (vreader != null) {
                        vreader.close();
                    }
                } catch (IOException ex) {
                    // nothing
                }
            }
        }

        ArrayList<String> directories = new ArrayList<String>();
        PackageManager pm = mAppContext.getPackageManager();
        List<ResolveInfo> plugins = pm.queryIntentServices(new Intent(PLUGIN_ACTION),
                PackageManager.GET_SERVICES | PackageManager.GET_META_DATA);

        synchronized(mPackageInfoCache) {

            // clear the list of existing packageInfo objects
            mPackageInfoCache.clear();


            for (ResolveInfo info : plugins) {

                // retrieve the plugin's service information
                ServiceInfo serviceInfo = info.serviceInfo;
                if (serviceInfo == null) {
                    Log.w(LOGTAG, "Ignoring bad plugin.");
                    continue;
                }

                // Blacklist HTC's flash lite.
                // See bug #704516 - We're not quite sure what Flash Lite does,
                // but loading it causes Flash to give errors and fail to draw.
                if (serviceInfo.packageName.equals("com.htc.flashliteplugin")) {
                    Log.w(LOGTAG, "Skipping HTC's flash lite plugin");
                    continue;
                }


                // Retrieve information from the plugin's manifest.
                PackageInfo pkgInfo;
                try {
                    pkgInfo = pm.getPackageInfo(serviceInfo.packageName,
                                    PackageManager.GET_PERMISSIONS
                                    | PackageManager.GET_SIGNATURES);
                } catch (Exception e) {
                    Log.w(LOGTAG, "Can't find plugin: " + serviceInfo.packageName);
                    continue;
                }

                if (pkgInfo == null) {
                    Log.w(LOGTAG, "Not loading plugin: " + serviceInfo.packageName + ". Could not load package information.");
                    continue;
                }

                /*
                 * find the location of the plugin's shared library. The default
                 * is to assume the app is either a user installed app or an
                 * updated system app. In both of these cases the library is
                 * stored in the app's data directory.
                 */
                String directory = pkgInfo.applicationInfo.dataDir + "/lib";
                final int appFlags = pkgInfo.applicationInfo.flags;
                final int updatedSystemFlags = ApplicationInfo.FLAG_SYSTEM |
                                               ApplicationInfo.FLAG_UPDATED_SYSTEM_APP;

                // preloaded system app with no user updates
                if ((appFlags & updatedSystemFlags) == ApplicationInfo.FLAG_SYSTEM) {
                    directory = PLUGIN_SYSTEM_LIB + pkgInfo.packageName;
                }

                // check if the plugin has the required permissions
                String permissions[] = pkgInfo.requestedPermissions;
                if (permissions == null) {
                    Log.w(LOGTAG, "Not loading plugin: " + serviceInfo.packageName + ". Does not have required permission.");
                    continue;
                }
                boolean permissionOk = false;
                for (String permit : permissions) {
                    if (PLUGIN_PERMISSION.equals(permit)) {
                        permissionOk = true;
                        break;
                    }
                }
                if (!permissionOk) {
                    Log.w(LOGTAG, "Not loading plugin: " + serviceInfo.packageName + ". Does not have required permission (2).");
                    continue;
                }

                // check to ensure the plugin is properly signed
                Signature signatures[] = pkgInfo.signatures;
                if (signatures == null) {
                    Log.w(LOGTAG, "Not loading plugin: " + serviceInfo.packageName + ". Not signed.");
                    continue;
                }

                // determine the type of plugin from the manifest
                if (serviceInfo.metaData == null) {
                    Log.e(LOGTAG, "The plugin '" + serviceInfo.name + "' has no defined type.");
                    continue;
                }

                String pluginType = serviceInfo.metaData.getString(PLUGIN_TYPE);
                if (!TYPE_NATIVE.equals(pluginType)) {
                    Log.e(LOGTAG, "Unrecognized plugin type: " + pluginType);
                    continue;
                }

                try {
                    Class<?> cls = getPluginClass(serviceInfo.packageName, serviceInfo.name);

                    //TODO implement any requirements of the plugin class here!
                    boolean classFound = true;

                    if (!classFound) {
                        Log.e(LOGTAG, "The plugin's class' " + serviceInfo.name + "' does not extend the appropriate class.");
                        continue;
                    }

                } catch (NameNotFoundException e) {
                    Log.e(LOGTAG, "Can't find plugin: " + serviceInfo.packageName);
                    continue;
                } catch (ClassNotFoundException e) {
                    Log.e(LOGTAG, "Can't find plugin's class: " + serviceInfo.name);
                    continue;
                }

                // if all checks have passed then make the plugin available
                mPackageInfoCache.add(pkgInfo);
                directories.add(directory);
            }
        }

        return directories.toArray(new String[directories.size()]);
    }

    String getPluginPackage(String pluginLib) {

        if (pluginLib == null || pluginLib.length() == 0) {
            return null;
        }

        synchronized(mPackageInfoCache) {
            for (PackageInfo pkgInfo : mPackageInfoCache) {
                if (pluginLib.contains(pkgInfo.packageName)) {
                    return pkgInfo.packageName;
                }
            }
        }

        return null;
    }

    Class<?> getPluginClass(String packageName, String className)
            throws NameNotFoundException, ClassNotFoundException {
        Context pluginContext = mAppContext.createPackageContext(packageName,
                Context.CONTEXT_INCLUDE_CODE |
                Context.CONTEXT_IGNORE_SECURITY);
        ClassLoader pluginCL = pluginContext.getClassLoader();
        return pluginCL.loadClass(className);
    }

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
            return new GeckoMenuInflater(mAppContext);
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
        if (!hasPermanentMenuKey())
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
                mMenuPanel = new MenuPanel(mAppContext, null);
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

            GeckoMenu gMenu = new GeckoMenu(mAppContext, null);
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
        if (mLayerView.isFullScreen()) {
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
                synchronized(sLaunchState) {
                    if (sLaunchState == LaunchState.GeckoRunning)
                        GeckoAppShell.notifyGeckoOfEvent(
                            GeckoEvent.createBroadcastEvent("Browser:Quit", null));
                    else
                        System.exit(0);
                    sLaunchState = LaunchState.GeckoExiting;
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

    protected void shareCurrentUrl() {
        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab == null)
          return;

        String url = tab.getURL();
        if (url == null)
            return;

        if (ReaderModeUtils.isAboutReader(url))
            url = ReaderModeUtils.getUrlFromAboutReader(url);

        GeckoAppShell.openUriExternal(url, "text/plain", "", "",
                                      Intent.ACTION_SEND, tab.getDisplayTitle());
    }

    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);

        if (outState == null)
            outState = new Bundle();

        boolean inBackground =
            ((GeckoApplication)getApplication()).isApplicationInBackground();

        outState.putBoolean(SAVED_STATE_IN_BACKGROUND, inBackground);
        outState.putString(SAVED_STATE_PRIVATE_SESSION, mPrivateBrowsingSession);
    }

    void handleSecurityChange(final int tabId, final JSONObject identityData) {
        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        tab.updateIdentityData(identityData);
    }

    void handleReaderEnabled(final int tabId) {
        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        tab.setReaderEnabled(true);
    }

    void handleFaviconRequest(final String url) {
        (new GeckoAsyncTask<Void, Void, String>(mAppContext, GeckoAppShell.getHandler()) {
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

    void handleLoadError(final int tabId, final String uri, final String title) {
        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        // When a load error occurs, the URLBar can get corrupt so we reset it
        Tabs.getInstance().notifyListeners(tab, Tabs.TabEvents.LOAD_ERROR);
    }

    void handlePageShow(final int tabId) { }

    void handleClearHistory() {
        BrowserDB.clearHistory(getContentResolver());
    }

    /**
     * This function might perform synchronous IO. Don't call it
     * from the main thread.
     */
    public StartupMode getStartupMode() {

        synchronized(this) {
            if (mStartupMode != null)
                return mStartupMode;

            String packageName = getPackageName();
            SharedPreferences settings = getPreferences(Activity.MODE_PRIVATE);

            // This key should be profile-dependent. For now, we're simply hardcoding
            // the "default" profile here.
            String profileName = getDefaultProfileName();
            if (profileName == null)
                profileName = "default";
            String keyName = packageName + "." + profileName + ".startup_version";
            String appVersion = null;

            try {
                PackageInfo pkgInfo = getPackageManager().getPackageInfo(packageName, 0);
                appVersion = pkgInfo.versionName;
            } catch(NameNotFoundException nnfe) {
                // If, for some reason, we can't fetch the app version
                // we fallback to NORMAL startup mode.
                mStartupMode = StartupMode.NORMAL;
                return mStartupMode;
            }

            String startupVersion = settings.getString(keyName, null);
            if (startupVersion == null) {
                mStartupMode = StartupMode.NEW_PROFILE;
            } else {
                if (startupVersion.equals(appVersion))
                    mStartupMode = StartupMode.NORMAL;
                else
                    mStartupMode = StartupMode.NEW_VERSION;
            }

            if (mStartupMode != StartupMode.NORMAL)
                settings.edit().putString(keyName, appVersion).commit();

            Log.i(LOGTAG, "Startup mode: " + mStartupMode);

            return mStartupMode;
        }
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

    public boolean hasPermanentMenuKey() {
        boolean hasMenu = true;

        if (Build.VERSION.SDK_INT >= 11)
            hasMenu = false;

        if (Build.VERSION.SDK_INT >= 14)
            hasMenu = ViewConfiguration.get(GeckoApp.mAppContext).hasPermanentMenuKey();

        return hasMenu;
    }

    public void handleMessage(String event, JSONObject message) {
        try {
            if (event.equals("Toast:Show")) {
                final String msg = message.getString("message");
                final String duration = message.getString("duration");
                handleShowToast(msg, duration);
            } else if (event.equals("DOMContentLoaded")) {
                final int tabId = message.getInt("tabID");
                final String backgroundColor = message.getString("bgColor");
                handleContentLoaded(tabId);
                Tab tab = Tabs.getInstance().getTab(tabId);
                if (backgroundColor != null) {
                    tab.setBackgroundColor(backgroundColor);
                } else {
                    // Default to white if no color is given
                    tab.setBackgroundColor(Color.WHITE);
                }

                // Sync up the layer view and the tab if the tab is
                // currently displayed.
                LayerView layerView = mLayerView;
                if (layerView != null && Tabs.getInstance().isSelectedTab(tab)) {
                    layerView.setBackgroundColor(tab.getBackgroundColor());
                }
            } else if (event.equals("DOMTitleChanged")) {
                final int tabId = message.getInt("tabID");
                final String title = message.getString("title");
                handleTitleChanged(tabId, title);
            } else if (event.equals("DOMLinkAdded")) {
                final int tabId = message.getInt("tabID");
                final String rel = message.getString("rel");
                final String href = message.getString("href");
                final int size = message.getInt("size");
                handleLinkAdded(tabId, rel, href, size);
            } else if (event.equals("DOMWindowClose")) {
                final int tabId = message.getInt("tabID");
                handleWindowClose(tabId);
            } else if (event.equals("log")) {
                // generic log listener
                final String msg = message.getString("msg");
                Log.d(LOGTAG, "Log: " + msg);
            } else if (event.equals("Content:SecurityChange")) {
                final int tabId = message.getInt("tabID");
                final JSONObject identity = message.getJSONObject("identity");
                Log.i(LOGTAG, "Security Mode - " + identity.getString("mode"));
                handleSecurityChange(tabId, identity);
            } else if (event.equals("Content:ReaderEnabled")) {
                final int tabId = message.getInt("tabID");
                handleReaderEnabled(tabId);
            } else if (event.equals("Reader:FaviconRequest")) {
                final String url = message.getString("url");
                handleFaviconRequest(url);
            } else if (event.equals("Reader:GoToReadingList")) {
                showReadingList();
            } else if (event.equals("Content:StateChange")) {
                final int tabId = message.getInt("tabID");
                final String uri = message.getString("uri");
                final boolean success = message.getBoolean("success");
                int state = message.getInt("state");
                Log.d(LOGTAG, "State - " + state);
                if ((state & GeckoAppShell.WPL_STATE_IS_NETWORK) != 0) {
                    if ((state & GeckoAppShell.WPL_STATE_START) != 0) {
                        Log.d(LOGTAG, "Got a document start event.");
                        final boolean showProgress = message.getBoolean("showProgress");
                        handleDocumentStart(tabId, showProgress, uri);
                    } else if ((state & GeckoAppShell.WPL_STATE_STOP) != 0) {
                        Log.d(LOGTAG, "Got a document stop event.");
                        handleDocumentStop(tabId, success);
                    }
                }
            } else if (event.equals("Content:LoadError")) {
                final int tabId = message.getInt("tabID");
                final String uri = message.getString("uri");
                final String title = message.getString("title");
                handleLoadError(tabId, uri, title);
            } else if (event.equals("Content:PageShow")) {
                final int tabId = message.getInt("tabID");
                handlePageShow(tabId);
            } else if (event.equals("Gecko:Ready")) {
                mGeckoReadyStartupTimer.stop();
                setLaunchState(GeckoApp.LaunchState.GeckoRunning);
                GeckoAppShell.sendPendingEventsToGecko();
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
                // Sync up the layer view and the tab if the tab is currently displayed.
                LayerView layerView = mLayerView;
                if (layerView != null && Tabs.getInstance().isSelectedTab(tab)) {
                    layerView.setZoomConstraints(tab.getZoomConstraints());
                }
            } else if (event.equals("Tab:HasTouchListener")) {
                int tabId = message.getInt("tabID");
                final Tab tab = Tabs.getInstance().getTab(tabId);
                tab.setHasTouchListeners(true);
                mMainHandler.post(new Runnable() {
                    public void run() {
                        if (Tabs.getInstance().isSelectedTab(tab))
                            mLayerView.getTouchEventHandler().setWaitForTouchListeners(true);
                    }
                });
            } else if (event.equals("Session:StatePurged")) {
                onStatePurged();
            } else if (event.equals("Bookmark:Insert")) {
                final String url = message.getString("url");
                final String title = message.getString("title");
                mMainHandler.post(new Runnable() {
                    public void run() {
                        Toast.makeText(GeckoApp.mAppContext, R.string.bookmark_added, Toast.LENGTH_SHORT).show();
                        GeckoAppShell.getHandler().post(new Runnable() {
                            public void run() {
                                BrowserDB.addBookmark(GeckoApp.mAppContext.getContentResolver(), title, url);
                            }
                        });
                    }
                });
            } else if (event.equals("Accessibility:Event")) {
                GeckoAccessibility.sendAccessibilityEvent(message);
            } else if (event.equals("Accessibility:Ready")) {
                GeckoAccessibility.updateAccessibilitySettings();
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

                // installWebapp will return a File object pointing to the profile directory of the webapp
                mCurrentResponse = GeckoAppShell.installWebApp(name, manifestURL, origin, iconURL).toString();
            } else if (event.equals("WebApps:Uninstall")) {
                String origin = message.getString("origin");
                GeckoAppShell.uninstallWebApp(origin);
            } else if (event.equals("DesktopMode:Changed")) {
                int tabId = message.getInt("tabId");
                boolean desktopMode = message.getBoolean("desktopMode");
                final Tab tab = Tabs.getInstance().getTab(tabId);
                if (tab == null)
                    return;

                tab.setDesktopMode(desktopMode);
                mMainHandler.post(new Runnable() {
                    public void run() {
                        if (tab == Tabs.getInstance().getSelectedTab())
                            invalidateOptionsMenu();
                    }
                });
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

    public String getResponse() {
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
                    public void onClick(DialogInterface dialog, int id) { }
                });

            builder.setPositiveButton(R.string.site_settings_clear, new DialogInterface.OnClickListener() {
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
            public void onClick(DialogInterface dialog, int id) {
                dialog.cancel();
            }
        });

        mMainHandler.post(new Runnable() {
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

    void handleDocumentStart(int tabId, final boolean showProgress, String uri) {
        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        tab.setState(shouldShowProgress(uri) ? Tab.STATE_SUCCESS : Tab.STATE_LOADING);
        tab.updateIdentityData(null);
        tab.setReaderEnabled(false);
        Tabs.getInstance().notifyListeners(tab, Tabs.TabEvents.START, showProgress);
    }

    void handleDocumentStop(int tabId, boolean success) {
        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        tab.setState(success ? Tab.STATE_SUCCESS : Tab.STATE_ERROR);

        Tabs.getInstance().notifyListeners(tab, Tabs.TabEvents.STOP);

        final String oldURL = tab.getURL();
        GeckoAppShell.getHandler().postDelayed(new Runnable() {
            public void run() {
                // tab.getURL() may return null
                if (!TextUtils.equals(oldURL, tab.getURL()))
                    return;

                ThumbnailHelper.getInstance().getAndProcessThumbnailFor(tab);
            }
        }, 500);
    }

    public void showToast(final int resId, final int duration) {
        mMainHandler.post(new Runnable() {
            public void run() {
                Toast.makeText(mAppContext, resId, duration).show();
            }
        });
    }

    void handleShowToast(final String message, final String duration) {
        mMainHandler.post(new Runnable() {
            public void run() {
                Toast toast;
                if (duration.equals("long"))
                    toast = Toast.makeText(mAppContext, message, Toast.LENGTH_LONG);
                else
                    toast = Toast.makeText(mAppContext, message, Toast.LENGTH_SHORT);
                toast.show();
            }
        });
    }

    void handleContentLoaded(int tabId) {
        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        Tabs.getInstance().notifyListeners(tab, Tabs.TabEvents.LOADED);
    }

    void handleTitleChanged(int tabId, String title) {
        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        tab.updateTitle(title);
    }

    void handleLinkAdded(final int tabId, String rel, final String href, int size) {
        if (rel.indexOf("[icon]") == -1)
            return; 

        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        tab.updateFaviconURL(href, size);
    }

    void handleWindowClose(final int tabId) {
        Tabs tabs = Tabs.getInstance();
        Tab tab = tabs.getTab(tabId);
        tabs.closeTab(tab);
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

    void addPluginView(final View view, final Rect rect, final boolean isFullScreen) {
        mMainHandler.post(new Runnable() { 
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
        mMainHandler.post(new Runnable() {
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

    void removePluginView(final View view, final boolean isFullScreen) {
        mMainHandler.post(new Runnable() { 
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
        final String progText = mAppContext.getString(R.string.wallpaper_progress);
        final String successText = mAppContext.getString(R.string.wallpaper_success);
        final String failureText = mAppContext.getString(R.string.wallpaper_fail);
        final String fileName = aSrc.substring(aSrc.lastIndexOf("/") + 1);
        final PendingIntent emptyIntent = PendingIntent.getActivity(mAppContext, 0, new Intent(), 0);
        final AlertNotification notification = new AlertNotification(mAppContext, fileName.hashCode(), 
                                R.drawable.alert_download, fileName, progText, System.currentTimeMillis() );
        notification.setLatestEventInfo(mAppContext, fileName, progText, emptyIntent );
        notification.flags |= Notification.FLAG_ONGOING_EVENT;
        notification.show();
        new GeckoAsyncTask<Void, Void, Boolean>(mAppContext, GeckoAppShell.getHandler()){

            @Override
            protected Boolean doInBackground(Void... params) {
                WallpaperManager mgr = WallpaperManager.getInstance(mAppContext);
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
                        BitmapFactory.decodeByteArray(buf, 0, buf.length, options);
                        options.inSampleSize = getBitmapSampleSize(options, idealWidth, idealHeight);
                        options.inJustDecodeBounds = false;
                        image = BitmapFactory.decodeByteArray(buf, 0, buf.length, options);
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
                        BitmapFactory.decodeByteArray(imgBuffer, 0, imgBuffer.length, options);
                        options.inSampleSize = getBitmapSampleSize(options, idealWidth, idealHeight);
                        options.inJustDecodeBounds = false;
                        image = BitmapFactory.decodeByteArray(imgBuffer, 0, imgBuffer.length, options);
                    }
                    if(image != null) {
                        mgr.setBitmap(image);
                        return true;
                    } else {
                        return false;
                    }
                } catch(OutOfMemoryError ome) {
                    Log.e(LOGTAG, "Out of Memmory when coverting to byte array", ome);
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
                    notification.setLatestEventInfo(mAppContext, fileName, failureText, emptyIntent);
                } else {
                    notification.tickerText = successText;
                    notification.setLatestEventInfo(mAppContext, fileName, successText, emptyIntent);
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
        mMainHandler.post(new Runnable() { 
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

    public boolean isTablet() {
        int screenLayout = getResources().getConfiguration().screenLayout;
        return (Build.VERSION.SDK_INT >= 11 &&
                (((screenLayout & Configuration.SCREENLAYOUT_SIZE_MASK) == Configuration.SCREENLAYOUT_SIZE_LARGE) || 
                 ((screenLayout & Configuration.SCREENLAYOUT_SIZE_MASK) == Configuration.SCREENLAYOUT_SIZE_XLARGE)));
    }

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        GeckoAppShell.registerGlobalExceptionHandler();

        // The clock starts...now. Better hurry!
        mJavaUiStartupTimer = new Telemetry.Timer("FENNEC_STARTUP_TIME_JAVAUI");
        mGeckoReadyStartupTimer = new Telemetry.Timer("FENNEC_STARTUP_TIME_GECKOREADY");

        ((GeckoApplication)getApplication()).initialize();

        mAppContext = this;
        Tabs.getInstance().attachToActivity(this);
        Favicons.getInstance().attachToContext(this);

        // Check to see if the activity is restarted after configuration change.
        if (getLastNonConfigurationInstance() != null) {
            // Restart the application as a safe way to handle the configuration change.
            doRestart();
            System.exit(0);
            return;
        }

        // StrictMode is set by defaults resource flag |enableStrictMode|.
        if (getResources().getBoolean(R.bool.enableStrictMode)) {
            enableStrictMode();
        }

        GeckoAppShell.loadMozGlue(this);
        if (sGeckoThread != null) {
            // this happens when the GeckoApp activity is destroyed by android
            // without killing the entire application (see bug 769269)
            mIsRestoringActivity = true;
            Telemetry.HistogramAdd("FENNEC_RESTORING_ACTIVITY", 1);
        }

        mMainHandler = new Handler();

        LayoutInflater.from(this).setFactory(GeckoViewsFactory.getInstance());

        super.onCreate(savedInstanceState);

        mOrientation = getResources().getConfiguration().orientation;

        setContentView(getLayout());

        // setup gecko layout
        mGeckoLayout = (RelativeLayout) findViewById(R.id.gecko_layout);
        mMainLayout = (LinearLayout) findViewById(R.id.main_layout);

        // setup tabs panel
        mTabsPanel = (TabsPanel) findViewById(R.id.tabs_panel);

        // check if the last run was exited due to a normal kill while
        // we were in the background, or a more harsh kill while we were
        // active
        if (savedInstanceState != null) {
            mRestoreMode = RESTORE_OOM;

            boolean wasInBackground =
                savedInstanceState.getBoolean(SAVED_STATE_IN_BACKGROUND, false);

            // Don't log OOM-kills if only one activity was destroyed. (For example
            // from "Don't keep activities" on ICS)
            if (!wasInBackground && !mIsRestoringActivity) {
                Telemetry.HistogramAdd("FENNEC_WAS_KILLED", 1);
            }

            mPrivateBrowsingSession = savedInstanceState.getString(SAVED_STATE_PRIVATE_SESSION);
        }

        GeckoBackgroundThread.getHandler().post(new Runnable() {
            public void run() {
                SharedPreferences prefs =
                    GeckoApp.mAppContext.getSharedPreferences(PREFS_NAME, 0);

                boolean wasOOM = prefs.getBoolean(PREFS_OOM_EXCEPTION, false);
                boolean wasStopped = prefs.getBoolean(PREFS_WAS_STOPPED, true);
                if (wasOOM || !wasStopped) {
                    Telemetry.HistogramAdd("FENNEC_WAS_KILLED", 1);
                }
                SharedPreferences.Editor editor = prefs.edit();
                editor.putBoolean(GeckoApp.PREFS_OOM_EXCEPTION, false);

                // put a flag to check if we got a normal onSaveInstaceState
                // on exit, or if we were suddenly killed (crash or native OOM)
                editor.putBoolean(GeckoApp.PREFS_WAS_STOPPED, false);
                editor.commit();
            }
        });
    }

    protected void initializeChrome(String uri, boolean isExternalURL) {
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
        }
    }

    private void initialize() {
        mInitialized = true;

        invalidateOptionsMenu();

        Intent intent = getIntent();
        String action = intent.getAction();
        String args = intent.getStringExtra("args");

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

        String passedUri = null;
        String uri = getURIFromIntent(intent);
        if (uri != null && uri.length() > 0) {
            passedUri = uri;
        }

        if (mRestoreMode == RESTORE_NONE && shouldRestoreSession()) {
            mRestoreMode = RESTORE_CRASH;
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
            Intent copy = new Intent(intent);
            copy.setAction(ACTION_LOAD);
            if (isHostOnRedirectWhitelist(data.getHost())) {
                startupAction = StartupAction.REDIRECTOR;
                GeckoAppShell.getHandler().post(new RedirectorRunnable(copy));
                // We're going to handle this uri with the redirector, so setting
                // the action to MAIN and clearing the uri data prevents us from
                // loading it twice
                intent.setAction(Intent.ACTION_MAIN);
                intent.setData(null);
                passedUri = null;
            } else {
                startupAction = StartupAction.PREFETCH;
                GeckoAppShell.getHandler().post(new PrefetchRunnable(copy));
            }
        }

        Tabs.registerOnTabsChangedListener(this);

        // If we are doing a restore, read the session data and send it to Gecko
        String restoreMessage = null;
        if (mRestoreMode != RESTORE_NONE && !mIsRestoringActivity) {
            try {
                String sessionString = getProfile().readSessionFile(false);
                if (sessionString == null) {
                    throw new IOException("could not read from session file");
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

                            Tab tab = Tabs.getInstance().loadUrl(sessionTab.getSelectedUrl(), flags);
                            tab.updateTitle(sessionTab.getSelectedTitle());

                            try {
                                tabObject.put("tabId", tab.getId());
                            } catch (JSONException e) {
                                Log.e(LOGTAG, "JSON error", e);
                            }
                            tabs.put(tabObject);
                        }
                    };

                    parser.parse(sessionString);
                    if (mPrivateBrowsingSession != null) {
                        parser.parse(mPrivateBrowsingSession);
                    }

                    if (tabs.length() > 0) {
                        sessionString = new JSONObject().put("windows", new JSONArray().put(new JSONObject().put("tabs", tabs))).toString();
                    } else {
                        throw new Exception("No tabs could be read from session file");
                    }
                }

                JSONObject restoreData = new JSONObject();
                restoreData.put("restoringOOM", mRestoreMode == RESTORE_OOM);
                restoreData.put("sessionString", sessionString);
                restoreMessage = restoreData.toString();
            } catch (Exception e) {
                // If restore failed, do a normal startup
                Log.e(LOGTAG, "An error occurred during restore", e);
                mRestoreMode = RESTORE_NONE;
            }
        }

        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Session:Restore", restoreMessage));

        if (mRestoreMode == RESTORE_OOM) {
            // If we successfully did an OOM restore, we now have tab stubs
            // from the last session. Any future tabs should be animated.
            Tabs.getInstance().notifyListeners(null, Tabs.TabEvents.RESTORED);
        } else {
            // Move the session file if it exists
            getProfile().moveSessionFile();
        }

        initializeChrome(passedUri, isExternalURL);

        // Show telemetry door hanger if we aren't restoring a session
        if (mRestoreMode == RESTORE_NONE) {
            Tabs.getInstance().notifyListeners(null, Tabs.TabEvents.RESTORED);
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Telemetry:Prompt", null));
        }

        Telemetry.HistogramAdd("FENNEC_STARTUP_GECKOAPP_ACTION", startupAction.ordinal());

        if (!mIsRestoringActivity) {
            sGeckoThread = new GeckoThread(intent, passedUri);
        }
        if (!ACTION_DEBUG.equals(action) &&
            checkAndSetLaunchState(LaunchState.Launching, LaunchState.Launched)) {
            sGeckoThread.start();
        } else if (ACTION_DEBUG.equals(action) &&
            checkAndSetLaunchState(LaunchState.Launching, LaunchState.WaitForDebugger)) {
            mMainHandler.postDelayed(new Runnable() {
                public void run() {
                    setLaunchState(LaunchState.Launching);
                    sGeckoThread.start();
                }
            }, 1000 * 5 /* 5 seconds */);
        }

        //register for events
        registerEventListener("DOMContentLoaded");
        registerEventListener("DOMTitleChanged");
        registerEventListener("DOMLinkAdded");
        registerEventListener("DOMWindowClose");
        registerEventListener("log");
        registerEventListener("Content:SecurityChange");
        registerEventListener("Content:ReaderEnabled");
        registerEventListener("Content:StateChange");
        registerEventListener("Content:LoadError");
        registerEventListener("Content:PageShow");
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
        registerEventListener("Tab:HasTouchListener");
        registerEventListener("Tab:ViewportMetadata");
        registerEventListener("Session:StatePurged");
        registerEventListener("Bookmark:Insert");
        registerEventListener("Accessibility:Event");
        registerEventListener("Accessibility:Ready");
        registerEventListener("Shortcut:Remove");
        registerEventListener("WebApps:Open");
        registerEventListener("WebApps:Install");
        registerEventListener("WebApps:Uninstall");
        registerEventListener("DesktopMode:Changed");
        registerEventListener("Share:Text");
        registerEventListener("Share:Image");
        registerEventListener("Wallpaper:Set");
        registerEventListener("Sanitize:ClearHistory");
        registerEventListener("Update:Check");
        registerEventListener("PrivateBrowsing:Data");

        if (SmsManager.getInstance() != null) {
          SmsManager.getInstance().start();
        }

        mPromptService = new PromptService();

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

        final GeckoApp self = this;

        // End of the startup of our Java App
        mJavaUiStartupTimer.stop();

        GeckoAppShell.getHandler().postDelayed(new Runnable() {
            public void run() {
                // Sync settings need Gecko to be loaded, so
                // no hurry in starting this.
                checkMigrateSync();

                // Record our launch time for the announcements service
                // to use in assessing inactivity.
                final Context context = GeckoApp.mAppContext;
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

                if (!checkLaunchState(LaunchState.Launched)) {
                    return;
                }
            }
        }, 50);

        if (mIsRestoringActivity) {
            setLaunchState(GeckoApp.LaunchState.GeckoRunning);
            Tab selectedTab = Tabs.getInstance().getSelectedTab();
            if (selectedTab != null)
                Tabs.getInstance().notifyListeners(selectedTab, Tabs.TabEvents.SELECTED);
            connectGeckoLayerClient();
            GeckoAppShell.setLayerClient(mLayerView.getLayerClient());
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Viewport:Flush", null));
        }

        // Homescreen Widget requests awesome bar
        if (ACTION_WIDGET.equals(action)) {
            if (mRestoreMode != RESTORE_NONE && !mIsRestoringActivity) {
                addTab();
            } else {
                onSearchRequested();
            }
        }

    }

    public GeckoProfile getProfile() {
        // fall back to default profile if we didn't load a specific one
        if (mProfile == null) {
            mProfile = GeckoProfile.get(this);
        }
        return mProfile;
    }

    protected boolean shouldRestoreSession() {
        SharedPreferences prefs = GeckoApp.mAppContext.getSharedPreferences(PREFS_NAME, 0);

        // We record crashes in the crash reporter. If sessionstore.js
        // exists, but we didn't flag a crash in the crash reporter, we
        // were probably just force killed by the user, so we shouldn't do
        // a restore.
        if (prefs.getBoolean(PREFS_CRASHED, false)) {
            prefs.edit().putBoolean(GeckoApp.PREFS_CRASHED, false).commit();
            if (getProfile().shouldRestoreSession()) {
                return true;
            }
        }
        return false;
    }

    /**
     * Enable Android StrictMode checks (for supported OS versions).
     * http://developer.android.com/reference/android/os/StrictMode.html
     */
    private void enableStrictMode()
    {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.GINGERBREAD) {
            return;
        }

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

    abstract public String getDefaultUAString();
    abstract public String getUAStringForHost(String host);

    class PrefetchRunnable implements Runnable {
        Intent mIntent;
        protected HttpURLConnection mConnection = null;
        PrefetchRunnable(Intent intent) {
            mIntent = intent;
        }

        private void afterLoad() { }

        public void run() {
            try {
                // this class should only be initialized with an intent with non-null data
                URL url = new URL(mIntent.getData().toString());
                // data url should have an http scheme
                mConnection = (HttpURLConnection) url.openConnection();
                mConnection.setRequestProperty("User-Agent", getUAStringForHost(url.getHost()));
                mConnection.setInstanceFollowRedirects(false);
                mConnection.setRequestMethod("GET");
                mConnection.connect();
                afterLoad();
            } catch (Exception e) {
                Log.w(LOGTAG, "unexpected exception, passing url directly to Gecko but we should explicitly catch this", e);
                mIntent.putExtra("prefetched", 1);
            } finally {
                if (mConnection != null)
                    mConnection.disconnect();
            }
        }
    }

    class RedirectorRunnable extends PrefetchRunnable {
        RedirectorRunnable(Intent intent) {
            super(intent);
        }
        private void afterLoad() {
            try {
                int code = mConnection.getResponseCode();
                if (code >= 300 && code < 400) {
                    String location = mConnection.getHeaderField("Location");
                    Uri data;
                    if (location != null &&
                        (data = Uri.parse(location)) != null &&
                        !"about".equals(data.getScheme()) && 
                        !"chrome".equals(data.getScheme())) {
                        mIntent.setData(data);
                    } else {
                        mIntent.putExtra("prefetched", 1);
                    }
                } else {
                    mIntent.putExtra("prefetched", 1);
                }
            } catch (IOException ioe) {
                Log.w(LOGTAG, "Exception trying to pre-fetch redirected URL.", ioe);
                mIntent.putExtra("prefetched", 1);
            }
        }
        public void run() {
            super.run();

            mMainHandler.postAtFrontOfQueue(new Runnable() {
                public void run() {
                    onNewIntent(mIntent);
                }
            });
        }
    }

    private final String kRedirectWhiteListArray[] = new String[] { 
        "t.co",
        "bit.ly",
        "moz.la",
        "aje.me",
        "facebook.com",
        "goo.gl",
        "tinyurl.com"
    };
    
    private final CopyOnWriteArrayList<String> kRedirectWhiteList =
        new CopyOnWriteArrayList<String>(kRedirectWhiteListArray);

    private boolean isHostOnRedirectWhitelist(String host) {
        return kRedirectWhiteList.contains(host);
    }

    @Override
    protected void onNewIntent(Intent intent) {
        if (checkLaunchState(LaunchState.GeckoExiting)) {
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

        if (checkLaunchState(LaunchState.Launched)) {
            Uri data = intent.getData();
            Bundle bundle = intent.getExtras();
            // if the intent has data (i.e. a URI to be opened) and the scheme
            // is either http, we'll prefetch it, which means warming
            // up the radio and DNS cache by connecting and parsing the redirect
            // if the return code is between 300 and 400
            if (data != null && 
                "http".equals(data.getScheme()) &&
                (bundle == null || bundle.getInt("prefetched", 0) != 1)) {
                if (isHostOnRedirectWhitelist(data.getHost())) {
                    GeckoAppShell.getHandler().post(new RedirectorRunnable(intent));
                    return;
                } else {
                    GeckoAppShell.getHandler().post(new PrefetchRunnable(intent));
                }
            }
        }
        final String action = intent.getAction();

        if (Intent.ACTION_MAIN.equals(action)) {
            GeckoAppShell.sendEventToGecko(GeckoEvent.createURILoadEvent(""));
        }
        else if (ACTION_LOAD.equals(action)) {
            String uri = intent.getDataString();
            Tabs.getInstance().loadUrl(uri);
        }
        else if (Intent.ACTION_VIEW.equals(action)) {
            String uri = intent.getDataString();
            GeckoAppShell.sendEventToGecko(GeckoEvent.createURILoadEvent(uri));
        }
        else if (action != null && action.startsWith(ACTION_WEBAPP_PREFIX)) {
            String uri = getURIFromIntent(intent);
            GeckoAppShell.sendEventToGecko(GeckoEvent.createWebappLoadEvent(uri));
        }
        else if (ACTION_BOOKMARK.equals(action)) {
            String uri = getURIFromIntent(intent);
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBookmarkLoadEvent(uri));
        }
        else if (Intent.ACTION_SEARCH.equals(action)) {
            String uri = getURIFromIntent(intent);
            GeckoAppShell.sendEventToGecko(GeckoEvent.createURILoadEvent(uri));
        }
        else if (ACTION_ALERT_CALLBACK.equals(action)) {
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
        else if (ACTION_WIDGET.equals(action)) {
            addTab();
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
        GeckoAccessibility.updateAccessibilitySettings();

        GeckoBackgroundThread.getHandler().post(new Runnable() {
            public void run() {
                SharedPreferences prefs =
                    GeckoApp.mAppContext.getSharedPreferences(GeckoApp.PREFS_NAME, 0);
                SharedPreferences.Editor editor = prefs.edit();
                editor.putBoolean(GeckoApp.PREFS_WAS_STOPPED, false);
                editor.commit();
            }
         });
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);

        if (!mInitialized && hasFocus)
            initialize();
    }

    @Override
    public void onStop()
    {
        // We're about to be stopped, potentially in preparation for
        // being destroyed.  We're killable after this point -- as I
        // understand it, in extreme cases the process can be terminated
        // without going through onDestroy.
        //
        // We might also get an onRestart after this; not sure what
        // that would mean for Gecko if we were to kill it here.
        // Instead, what we should do here is save prefs, session,
        // etc., and generally mark the profile as 'clean', and then
        // dirty it again if we get an onResume.

        GeckoAppShell.sendEventToGecko(GeckoEvent.createStoppingEvent(isApplicationInBackground()));
        super.onStop();
    }

    @Override
    public void onPause()
    {
        // In some way it's sad that Android will trigger StrictMode warnings
        // here as the whole point is to save to disk while the activity is not
        // interacting with the user.
        GeckoBackgroundThread.getHandler().post(new Runnable() {
            public void run() {
                SharedPreferences prefs =
                    GeckoApp.mAppContext.getSharedPreferences(GeckoApp.PREFS_NAME, 0);
                SharedPreferences.Editor editor = prefs.edit();
                editor.putBoolean(GeckoApp.PREFS_WAS_STOPPED, true);
                editor.commit();

                BrowserDB.expireHistory(getContentResolver(),
                                        BrowserContract.ExpirePriority.NORMAL);
            }
        });

        GeckoScreenOrientationListener.getInstance().stop();

        super.onPause();
    }

    @Override
    public void onRestart()
    {
        GeckoBackgroundThread.getHandler().post(new Runnable() {
            public void run() {
                SharedPreferences prefs =
                    GeckoApp.mAppContext.getSharedPreferences(GeckoApp.PREFS_NAME, 0);
                SharedPreferences.Editor editor = prefs.edit();
                editor.putBoolean(GeckoApp.PREFS_WAS_STOPPED, false);
                editor.commit();
            }
        });

        super.onRestart();
    }

    @Override
    public void onStart()
    {
        GeckoAppShell.sendEventToGecko(GeckoEvent.createStartEvent(isApplicationInBackground()));
        super.onStart();
    }

    @Override
    public void onDestroy()
    {
        // Tell Gecko to shutting down; we'll end up calling System.exit()
        // in onXreExit.
        if (isFinishing())
            GeckoAppShell.sendEventToGecko(GeckoEvent.createShutdownEvent());

        unregisterEventListener("DOMContentLoaded");
        unregisterEventListener("DOMTitleChanged");
        unregisterEventListener("DOMLinkAdded");
        unregisterEventListener("DOMWindowClose");
        unregisterEventListener("log");
        unregisterEventListener("Content:SecurityChange");
        unregisterEventListener("Content:ReaderEnabled");
        unregisterEventListener("Content:StateChange");
        unregisterEventListener("Content:LoadError");
        unregisterEventListener("Content:PageShow");
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
        unregisterEventListener("Tab:HasTouchListener");
        unregisterEventListener("Tab:ViewportMetadata");
        unregisterEventListener("Session:StatePurged");
        unregisterEventListener("Bookmark:Insert");
        unregisterEventListener("Accessibility:Event");
        unregisterEventListener("Accessibility:Ready");
        unregisterEventListener("Shortcut:Remove");
        unregisterEventListener("WebApps:Open");
        unregisterEventListener("WebApps:Install");
        unregisterEventListener("WebApps:Uninstall");
        unregisterEventListener("DesktopMode:Changed");
        unregisterEventListener("Share:Text");
        unregisterEventListener("Share:Image");
        unregisterEventListener("Wallpaper:Set");
        unregisterEventListener("Sanitize:ClearHistory");
        unregisterEventListener("Update:Check");
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

        Tabs.getInstance().detachFromActivity(this);

        if (SmsManager.getInstance() != null) {
            SmsManager.getInstance().stop();
            if (isFinishing())
                SmsManager.getInstance().shutdown();
        }

        super.onDestroy();

        Tabs.unregisterOnTabsChangedListener(this);
    }

    protected void registerEventListener(String event) {
        GeckoAppShell.getEventDispatcher().registerEventListener(event, this);
    }

    protected void unregisterEventListener(String event) {
        GeckoAppShell.getEventDispatcher().unregisterEventListener(event, this);
    }

    // Get a temporary directory, may return null
    public static File getTempDirectory() {
        File dir = mAppContext.getExternalFilesDir("temp");
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
    public void onContentChanged() {
        super.onContentChanged();
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
    public Object onRetainNonConfigurationInstance() {
        // Send a non-null value so that we can restart the application, 
        // when activity restarts due to configuration change.
        return new Boolean(true);
    } 

    abstract public String getPackageName();
    abstract public String getContentProcessName();

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
        doRestart("org.mozilla.gecko.restart");
    }

    public void doRestart(String action) {
        Log.d(LOGTAG, "doRestart(\"" + action + "\")");
        try {
            Intent intent = new Intent(action);
            intent.setClassName(getPackageName(),
                                getPackageName() + ".Restarter");
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
        GeckoAppShell.handleNotification(action, alertName, alertCookie);
    }

    private void checkMigrateProfile() {
        final File profileDir = getProfile().getDir();

        if (profileDir != null) {
            final GeckoApp app = GeckoApp.mAppContext;

            GeckoAppShell.getHandler().post(new Runnable() {
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
                            public void run() {
                                GeckoApp.mAppContext.runOnUiThread(new Runnable() {
                                    public void run() {
                                        setupScreenHolder[0] = new SetupScreen(app);
                                        setupScreenHolder[0].show();
                                    }
                                });
                            }
                        };

                        final Runnable stopCallback = new Runnable() {
                            public void run() {
                                GeckoApp.mAppContext.runOnUiThread(new Runnable() {
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
            final GeckoApp app = GeckoApp.mAppContext;
            ProfileMigrator profileMigrator = new ProfileMigrator(app);
            if (!profileMigrator.hasSyncMigrated()) {
                profileMigrator.launchSyncPrefs();
            }
        }
    }

    PromptService getPromptService() {
        return mPromptService;
    }

    @Override
    public boolean onSearchRequested() {
        return showAwesomebar(AwesomeBar.Target.CURRENT_TAB);
    }

    public boolean showAwesomebar(AwesomeBar.Target aTarget) {
        return showAwesomebar(aTarget, null);
    }

    public boolean showAwesomebar(AwesomeBar.Target aTarget, String aUrl) {
        Intent intent = new Intent(getBaseContext(), AwesomeBar.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NO_HISTORY);
        intent.putExtra(AwesomeBar.TARGET_KEY, aTarget.name());

        // if we were passed in a url, show it
        if (aUrl != null && !TextUtils.isEmpty(aUrl)) {
            intent.putExtra(AwesomeBar.CURRENT_URL_KEY, aUrl);
        } else if (aTarget == AwesomeBar.Target.CURRENT_TAB) {
            // otherwise, if we're editing the current tab, show its url
            Tab tab = Tabs.getInstance().getSelectedTab();
            if (tab != null) {

                aUrl = tab.getURL();
                if (aUrl != null) {
                    intent.putExtra(AwesomeBar.CURRENT_URL_KEY, aUrl);
                }

            }
        }

        int requestCode = GeckoAppShell.sActivityHelper.makeRequestCodeForAwesomebar();
        startActivityForResult(intent, requestCode);
        overridePendingTransition (R.anim.awesomebar_fade_in, R.anim.awesomebar_hold_still);
        return true;
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

        if (mLayerView.isFullScreen()) {
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
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
    }

    public void onSensorChanged(SensorEvent event) {
        GeckoAppShell.sendEventToGecko(GeckoEvent.createSensorEvent(event));
    }

    // Geolocation.
    public void onLocationChanged(Location location) {
        // No logging here: user-identifying information.
        GeckoAppShell.sendEventToGecko(GeckoEvent.createLocationEvent(location));
    }

    public void onProviderDisabled(String provider)
    {
    }

    public void onProviderEnabled(String provider)
    {
    }

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

    public void notifyCheckUpdateResult(boolean result) {
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Update:CheckResult", result ? "true" : "false"));
    }

    protected void connectGeckoLayerClient() {
        mLayerView.getLayerClient().notifyGeckoReady();

        mLayerView.getTouchEventHandler().setOnTouchListener(new OnInterceptTouchListener() {
            private PointF initialPoint = null;

            @Override
            public boolean onInterceptTouchEvent(View view, MotionEvent event) {
                return false;
            }

            @Override
            public boolean onTouch(View view, MotionEvent event) {
                if (event == null)
                    return true;

                int action = event.getAction();
                PointF point = new PointF(event.getX(), event.getY());
                if ((action & MotionEvent.ACTION_MASK) == MotionEvent.ACTION_DOWN) {
                    initialPoint = point;
                }

                if (initialPoint != null && (action & MotionEvent.ACTION_MASK) == MotionEvent.ACTION_MOVE) {
                    if (PointUtils.subtract(point, initialPoint).length() < PanZoomController.PAN_THRESHOLD) {
                        // Don't send the touchmove event if if the users finger hasn't move far
                        // Necessary for Google Maps to work correctlly. See bug 771099.
                        return true;
                    } else {
                        initialPoint = null;
                    }
                }

                GeckoAppShell.sendEventToGecko(GeckoEvent.createMotionEvent(event));
                return true;
            }
        });
    }

    public static class MainLayout extends LinearLayout {
        private OnInterceptTouchListener mOnInterceptTouchListener;

        public MainLayout(Context context, AttributeSet attrs) {
            super(context, attrs);
            mOnInterceptTouchListener = null;
        }

        public void setOnInterceptTouchListener(OnInterceptTouchListener listener) {
            mOnInterceptTouchListener = listener;
        }

        @Override
        public boolean onInterceptTouchEvent(MotionEvent event) {
            if (mOnInterceptTouchListener != null && mOnInterceptTouchListener.onInterceptTouchEvent(this, event))
                return true;
            return super.onInterceptTouchEvent(event);
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            if (mOnInterceptTouchListener != null && mOnInterceptTouchListener.onTouch(this, event))
                return true;
            return super.onTouchEvent(event);
        }

        @Override
        public void setDrawingCacheEnabled(boolean enabled) {
            // Instead of setting drawing cache in the view itself, we simply
            // enable drawing caching on its children. This is mainly used in
            // animations (see PropertyAnimator)
            super.setChildrenDrawnWithCacheEnabled(enabled);
        }
    }

    public boolean linkerExtract() {
        return false;
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

            mMainHandler.post(new Runnable() { 
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

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        switch(item.getItemId()) {
            case R.id.pasteandgo: {
                String text = GeckoAppShell.getClipboardText();
                if (text != null && !TextUtils.isEmpty(text)) {
                    Tabs.getInstance().loadUrl(text);
                }
                return true;
            }
            case R.id.site_settings: {
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Permissions:Get", null));
                return true;
            }
            case R.id.paste: {
                String text = GeckoAppShell.getClipboardText();
                if (text != null && !TextUtils.isEmpty(text)) {
                    showAwesomebar(AwesomeBar.Target.CURRENT_TAB, text);
                }
                return true;
            }
            case R.id.share: {
                shareCurrentUrl();
                return true;
            }
            case R.id.copyurl: {
                Tab tab = Tabs.getInstance().getSelectedTab();
                if (tab != null) {
                    String url = tab.getURL();
                    if (url != null) {
                        GeckoAppShell.setClipboardText(url);
                    }
                }
                return true;
            }
            case R.id.add_to_launcher: {
                Tab tab = Tabs.getInstance().getSelectedTab();
                if (tab != null) {
                    String url = tab.getURL();
                    String title = tab.getDisplayTitle();
                    Bitmap favicon = tab.getFavicon();
                    if (url != null && title != null) {
                        GeckoAppShell.createShortcut(title, url, url, favicon == null ? null : favicon, "");
                    }
                }
                return true;
            }
        }
        return false;
    }

    public static boolean shouldShowProgress(String url) {
        return "about:home".equals(url) || ReaderModeUtils.isAboutReader(url);
    }

    public static void assertOnUiThread() {
        Thread uiThread = mAppContext.getMainLooper().getThread();
        assertOnThread(uiThread);
    }

    public static void assertOnGeckoThread() {
        assertOnThread(sGeckoThread);
    }

    private static void assertOnThread(Thread expectedThread) {
        Thread currentThread = Thread.currentThread();
        long currentThreadId = currentThread.getId();
        long expectedThreadId = expectedThread.getId();

        if (currentThreadId != expectedThreadId) {
            throw new IllegalThreadStateException("Expected thread " + expectedThreadId + " (\""
                                                  + expectedThread.getName()
                                                  + "\"), but running on thread " + currentThreadId
                                                  + " (\"" + currentThread.getName() + ")");
        }
    }
}
