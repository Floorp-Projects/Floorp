/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.gfx.CairoImage;
import org.mozilla.gecko.gfx.BufferedCairoImage;
import org.mozilla.gecko.gfx.FloatSize;
import org.mozilla.gecko.gfx.GeckoLayerClient;
import org.mozilla.gecko.gfx.IntSize;
import org.mozilla.gecko.gfx.Layer;
import org.mozilla.gecko.gfx.LayerController;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.gfx.PluginLayer;
import org.mozilla.gecko.gfx.RectUtils;
import org.mozilla.gecko.gfx.SurfaceTextureLayer;
import org.mozilla.gecko.gfx.ViewportMetrics;
import org.mozilla.gecko.gfx.ImmutableViewportMetrics;

import java.io.*;
import java.util.*;
import java.util.regex.Pattern;
import java.util.regex.Matcher;
import java.util.zip.*;
import java.net.URL;
import java.nio.*;
import java.nio.channels.FileChannel;
import java.util.concurrent.*;
import java.lang.reflect.*;
import java.net.*;

import org.json.*;

import android.os.*;
import android.app.*;
import android.text.*;
import android.text.format.Time;
import android.view.*;
import android.view.inputmethod.*;
import android.view.ViewGroup.LayoutParams;
import android.content.*;
import android.content.res.*;
import android.graphics.*;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.BitmapDrawable;
import android.widget.*;
import android.hardware.*;
import android.location.*;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityEvent;

import android.util.*;
import android.net.*;
import android.database.*;
import android.database.sqlite.*;
import android.provider.*;
import android.content.pm.*;
import android.content.pm.PackageManager.*;
import dalvik.system.*;

abstract public class GeckoApp
                extends GeckoActivity 
                implements GeckoEventListener, SensorEventListener, LocationListener,
                           GeckoApplication.ApplicationLifecycleCallbacks {
    private static final String LOGTAG = "GeckoApp";

    public static enum StartupMode {
        NORMAL,
        NEW_VERSION,
        NEW_PROFILE
    }

    public static final String ACTION_ALERT_CLICK   = "org.mozilla.gecko.ACTION_ALERT_CLICK";
    public static final String ACTION_ALERT_CLEAR   = "org.mozilla.gecko.ACTION_ALERT_CLEAR";
    public static final String ACTION_WEBAPP        = "org.mozilla.gecko.WEBAPP";
    public static final String ACTION_DEBUG         = "org.mozilla.gecko.DEBUG";
    public static final String ACTION_BOOKMARK      = "org.mozilla.gecko.BOOKMARK";
    public static final String ACTION_LOAD          = "org.mozilla.gecko.LOAD";
    public static final String ACTION_UPDATE        = "org.mozilla.gecko.UPDATE";
    public static final String ACTION_INIT_PW       = "org.mozilla.gecko.INIT_PW";
    public static final String SAVED_STATE_TITLE    = "title";

    StartupMode mStartupMode = null;
    private LinearLayout mMainLayout;
    private RelativeLayout mGeckoLayout;
    public static SurfaceView cameraView;
    public static GeckoApp mAppContext;
    public static boolean mDOMFullScreen = false;
    public static Menu sMenu;
    private static GeckoThread sGeckoThread = null;
    public Handler mMainHandler;
    private GeckoProfile mProfile;
    public static boolean sIsGeckoReady = false;
    public static int mOrientation;

    private GeckoConnectivityReceiver mConnectivityReceiver;
    private GeckoBatteryManager mBatteryReceiver;

    public static BrowserToolbar mBrowserToolbar;
    public static DoorHangerPopup mDoorHangerPopup;
    public static FormAssistPopup mFormAssistPopup;
    public Favicons mFavicons;

    private static LayerController mLayerController;
    private static GeckoLayerClient mLayerClient;
    private AboutHomeContent mAboutHomeContent;
    private static AbsoluteLayout mPluginContainer;

    private int mRestoreMode = GeckoAppShell.RESTORE_NONE;
    private boolean mInitialized = false;

    static class ExtraMenuItem implements MenuItem.OnMenuItemClickListener {
        String label;
        String icon;
        int id;
        public boolean onMenuItemClick(MenuItem item) {
            Log.i(LOGTAG, "menu item clicked");
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Menu:Clicked", Integer.toString(id)));
            return true;
        }
    }

    static Vector<ExtraMenuItem> sExtraMenuItems = new Vector<ExtraMenuItem>();

    public enum LaunchState {Launching, WaitForDebugger,
                             Launched, GeckoRunning, GeckoExiting};
    private static LaunchState sLaunchState = LaunchState.Launching;

    private ActivityResultHandlerMap mActivityResultHandlerMap = new ActivityResultHandlerMap();
    private FilePickerResultHandlerSync mFilePickerResultHandlerSync = new FilePickerResultHandlerSync();
    private AwesomebarResultHandler mAwesomebarResultHandler = new AwesomebarResultHandler();
    private CameraImageResultHandler mCameraImageResultHandler = new CameraImageResultHandler();
    private CameraVideoResultHandler mCameraVideoResultHandler = new CameraVideoResultHandler();

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

    String[] getPluginDirectories() {

        // An awful hack to detect Tegra devices. Easiest way to do it without spinning up a EGL context
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.HONEYCOMB) {
            File tegraDriverPath = new File("/system/lib/hw/gralloc.tegra.so");
            if (tegraDriverPath.exists())
                return new String[0];
        }

        // we don't support Honeycomb
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB &&
            Build.VERSION.SDK_INT < 14 /*Build.VERSION_CODES.ICE_CREAM_SANDWICH*/ )
            return new String[0];

        Log.w(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - start of getPluginDirectories");

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
                    Log.w(LOGTAG, "Ignore bad plugin");
                    continue;
                }

                // Blacklist HTC's flash lite.
                // See bug #704516 - We're not quite sure what Flash Lite does,
                // but loading it causes Flash to give errors and fail to draw.
                if (serviceInfo.packageName.equals("com.htc.flashliteplugin")) {
                    Log.w(LOGTAG, "Skipping HTC's flash lite plugin");
                    continue;
                }

                Log.w(LOGTAG, "Loading plugin: " + serviceInfo.packageName);


                // retrieve information from the plugin's manifest
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
                    Log.w(LOGTAG, "Loading plugin: " + serviceInfo.packageName + ". Could not load package information.");
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
                    Log.w(LOGTAG, "Loading plugin: " + serviceInfo.packageName + ". Does not have required permission.");
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
                    Log.w(LOGTAG, "Loading plugin: " + serviceInfo.packageName + ". Does not have required permission (2).");
                    continue;
                }

                // check to ensure the plugin is properly signed
                Signature signatures[] = pkgInfo.signatures;
                if (signatures == null) {
                    Log.w(LOGTAG, "Loading plugin: " + serviceInfo.packageName + ". Not signed.");
                    continue;
                }

                // determine the type of plugin from the manifest
                if (serviceInfo.metaData == null) {
                    Log.e(LOGTAG, "The plugin '" + serviceInfo.name + "' has no type defined");
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

        String [] result = directories.toArray(new String[directories.size()]);
        Log.w(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - end of getPluginDirectories");
        return result;
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
    public boolean onCreateOptionsMenu(Menu menu)
    {
        sMenu = menu;
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.gecko_menu, menu);
        return true;
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu aMenu)
    {
        Iterator<ExtraMenuItem> i = sExtraMenuItems.iterator();
        while (i.hasNext()) {
            final ExtraMenuItem item = i.next();
            if (aMenu.findItem(item.id) == null) {
                final MenuItem mi = aMenu.add(Menu.NONE, item.id, Menu.NONE, item.label);
                if (item.icon != null) {
                    if (item.icon.startsWith("data")) {
                        byte[] raw = GeckoAppShell.decodeBase64(item.icon.substring(22), GeckoAppShell.BASE64_DEFAULT);
                        Bitmap bitmap = BitmapFactory.decodeByteArray(raw, 0, raw.length);
                        BitmapDrawable drawable = new BitmapDrawable(bitmap);
                        mi.setIcon(drawable);
                    }
                    else if (item.icon.startsWith("jar:") || item.icon.startsWith("file://")) {
                        GeckoAppShell.getHandler().post(new Runnable() {
                            public void run() {
                                try {
                                    URL url = new URL(item.icon);
                                    InputStream is = (InputStream) url.getContent();
                                    Drawable drawable = Drawable.createFromStream(is, "src");
                                    mi.setIcon(drawable);
                                } catch (Exception e) {
                                    Log.w(LOGTAG, "onPrepareOptionsMenu: Unable to set icon", e);
                                }
                            }
                        });
                    }
                }
                mi.setOnMenuItemClickListener(item);
            }
        }

        if (!sIsGeckoReady)
            aMenu.findItem(R.id.settings).setEnabled(false);

        Tab tab = Tabs.getInstance().getSelectedTab();
        MenuItem bookmark = aMenu.findItem(R.id.bookmark);
        MenuItem forward = aMenu.findItem(R.id.forward);
        MenuItem share = aMenu.findItem(R.id.share);
        MenuItem saveAsPDF = aMenu.findItem(R.id.save_as_pdf);
        MenuItem charEncoding = aMenu.findItem(R.id.char_encoding);

        if (tab == null || tab.getURL() == null) {
            bookmark.setEnabled(false);
            forward.setEnabled(false);
            share.setEnabled(false);
            saveAsPDF.setEnabled(false);
            return true;
        }
        
        bookmark.setEnabled(true);
        bookmark.setCheckable(true);
        
        if (tab.isBookmark()) {
            bookmark.setChecked(true);
            bookmark.setIcon(R.drawable.ic_menu_bookmark_remove);
        } else {
            bookmark.setChecked(false);
            bookmark.setIcon(R.drawable.ic_menu_bookmark_add);
        }

        forward.setEnabled(tab.canDoForward());

        // Disable share menuitem for about:, chrome:, file:, and resource: URIs
        String scheme = Uri.parse(tab.getURL()).getScheme();
        share.setEnabled(!(scheme.equals("about") || scheme.equals("chrome") ||
                           scheme.equals("file") || scheme.equals("resource")));

        // Disable save as PDF for about:home and xul pages
        saveAsPDF.setEnabled(!(tab.getURL().equals("about:home") ||
                               tab.getContentType().equals("application/vnd.mozilla.xul+xml")));

        charEncoding.setVisible(GeckoPreferences.getCharEncodingState());

        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        Tab tab = null;
        Intent intent = null;
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
                tab = Tabs.getInstance().getSelectedTab();
                if (tab != null) {
                    String url = tab.getURL();
                    if (url == null)
                        return false;

                    GeckoAppShell.openUriExternal(url, "text/plain", "", "",
                                                  Intent.ACTION_SEND, tab.getTitle());
                }
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
            case R.id.site_settings:
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Permissions:Get", null));
                return true;
            case R.id.addons:
                loadUrlInTab("about:addons");
                return true;
            case R.id.downloads:
                loadUrlInTab("about:downloads");
                return true;
            case R.id.char_encoding:
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("CharEncoding:Get", null));
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);

        if (outState == null)
            outState = new Bundle();

        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab != null)
            outState.putString(SAVED_STATE_TITLE, tab.getDisplayTitle());
    }

    void getAndProcessThumbnailForTab(final Tab tab) {
        boolean isSelectedTab = Tabs.getInstance().isSelectedTab(tab);
        final Bitmap bitmap = isSelectedTab ? mLayerClient.getBitmap() : null;
        
        if ("about:home".equals(tab.getURL())) {
            tab.updateThumbnail(null);
            return;
        }

        if (bitmap != null) {
            ByteArrayOutputStream bos = new ByteArrayOutputStream();
            bitmap.compress(Bitmap.CompressFormat.PNG, 0, bos);
            processThumbnail(tab, bitmap, bos.toByteArray());
        } else {
            if (tab.getState() == Tab.STATE_DELAYED) {
                byte[] thumbnail = BrowserDB.getThumbnailForUrl(getContentResolver(), tab.getURL());
                if (thumbnail != null)
                    processThumbnail(tab, null, thumbnail);
                return;
            }

            int dw = tab.getThumbnailWidth();
            int dh = tab.getThumbnailHeight();
            GeckoAppShell.sendEventToGecko(GeckoEvent.createScreenshotEvent(tab.getId(), 0, 0, 0, 0, 0, 0, dw, dh, GeckoAppShell.SCREENSHOT_THUMBNAIL));
        }
    }
    
    void processThumbnail(Tab thumbnailTab, Bitmap bitmap, byte[] compressed) {
        if (Tabs.getInstance().isSelectedTab(thumbnailTab)) {
            if (compressed == null) {
                ByteArrayOutputStream bos = new ByteArrayOutputStream();
                bitmap.compress(Bitmap.CompressFormat.PNG, 0, bos);
                compressed = bos.toByteArray();
            }
        }

        try {
            if (bitmap == null)
                bitmap = BitmapFactory.decodeByteArray(compressed, 0, compressed.length);
            thumbnailTab.updateThumbnail(bitmap);
        } catch (OutOfMemoryError ome) {
            Log.w(LOGTAG, "decoding byte array ran out of memory", ome);
        }
    }

    private void maybeCancelFaviconLoad(Tab tab) {
        long faviconLoadId = tab.getFaviconLoadId();

        if (faviconLoadId == Favicons.NOT_LOADING)
            return;

        // Cancel pending favicon load task
        mFavicons.cancelFaviconLoad(faviconLoadId);

        // Reset favicon load state
        tab.setFaviconLoadId(Favicons.NOT_LOADING);
    }

    private void loadFavicon(final Tab tab) {
        maybeCancelFaviconLoad(tab);

        long id = mFavicons.loadFavicon(tab.getURL(), tab.getFaviconURL(),
                        new Favicons.OnFaviconLoadedListener() {

            public void onFaviconLoaded(String pageUrl, Drawable favicon) {
                // Leave favicon UI untouched if we failed to load the image
                // for some reason.
                if (favicon == null)
                    return;

                Log.i(LOGTAG, "Favicon successfully loaded for URL = " + pageUrl);

                // The tab might be pointing to another URL by the time the
                // favicon is finally loaded, in which case we simply ignore it.
                if (!tab.getURL().equals(pageUrl))
                    return;

                Log.i(LOGTAG, "Favicon is for current URL = " + pageUrl);

                tab.updateFavicon(favicon);
                tab.setFaviconLoadId(Favicons.NOT_LOADING);

                if (Tabs.getInstance().isSelectedTab(tab))
                    mBrowserToolbar.setFavicon(tab.getFavicon());

                Tabs.getInstance().notifyListeners(tab, Tabs.TabEvents.FAVICON);
            }
        });

        tab.setFaviconLoadId(id);
    }

    void handleLocationChange(final int tabId, final String uri,
                              final String documentURI, final String contentType,
                              final boolean sameDocument) {
        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        if (Tabs.getInstance().isSelectedTab(tab)) {
            if (uri.equals("about:home"))
                showAboutHome();
            else 
                hideAboutHome();
        }
        
        tab.updateURL(uri);
        tab.setDocumentURI(documentURI);

        if (sameDocument) {
            mMainHandler.post(new Runnable() {
                public void run() {
                    if (Tabs.getInstance().isSelectedTab(tab)) {
                        mBrowserToolbar.setTitle(uri);
                    }
                }
            });
            return;
        }

        tab.setContentType(contentType);
        tab.updateFavicon(null);
        tab.updateFaviconURL(null);
        tab.updateIdentityData(null);
        tab.removeTransientDoorHangers();
        tab.setAllowZoom(true);
        tab.setDefaultZoom(0);
        tab.setMinZoom(0);
        tab.setMaxZoom(0);
        tab.setHasTouchListeners(false);
        tab.setCheckerboardColor(Color.WHITE);

        maybeCancelFaviconLoad(tab);

        mMainHandler.post(new Runnable() {
            public void run() {
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    mBrowserToolbar.setTitle(uri);
                    mBrowserToolbar.setFavicon(null);
                    mBrowserToolbar.setSecurityMode(tab.getSecurityMode());
                    mDoorHangerPopup.updatePopup();
                    mBrowserToolbar.setShadowVisibility(!(tab.getURL().startsWith("about:")));

                    if (tab != null)
                        hidePlugins(tab);
                }
            }
        });
    }

    void handleSecurityChange(final int tabId, final JSONObject identityData) {
        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        tab.updateIdentityData(identityData);
        
        mMainHandler.post(new Runnable() { 
            public void run() {
                if (Tabs.getInstance().isSelectedTab(tab))
                    mBrowserToolbar.setSecurityMode(tab.getSecurityMode());
            }
        });
    }

    void handleLoadError(final int tabId, final String uri, final String title) {
        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;
    
        // When a load error occurs, the URLBar can get corrupt so we reset it
        mMainHandler.post(new Runnable() {
            public void run() {
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    mBrowserToolbar.setTitle(tab.getDisplayTitle());
                    mBrowserToolbar.setFavicon(tab.getFavicon());
                    mBrowserToolbar.setSecurityMode(tab.getSecurityMode());
                    mBrowserToolbar.setProgressVisibility(tab.getState() == Tab.STATE_LOADING);
                }
            }
        });
    }

    void handlePageShow(final int tabId) {
        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;
    
        mMainHandler.post(new Runnable() {
            public void run() {
                loadFavicon(tab);
            }
        });
    }

    void updateAboutHomeTopSites() {
        if (mAboutHomeContent == null)
            return;

        GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
            public void run() {
                mAboutHomeContent.update(GeckoApp.mAppContext,
                        EnumSet.of(AboutHomeContent.UpdateFlags.TOP_SITES));
            }
        });
    }

    void handleClearHistory() {
        updateAboutHomeTopSites();
    }

    public StartupMode getStartupMode() {
        // This function might touch the disk and should not
        // be called from UI's main thread.

        synchronized(this) {
            if (mStartupMode != null)
                return mStartupMode;

            String packageName = getPackageName();
            SharedPreferences settings = getPreferences(Activity.MODE_PRIVATE);

            // This key should be profile-dependent. For now, we're simply hardcoding
            // the "default" profile here.
            String keyName = packageName + ".default.startup_version";
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

    void addTab() {
        showAwesomebar(AwesomeBar.Type.ADD);
    }

    void showTabs() {
        Intent intent = new Intent(mAppContext, TabsTray.class);
        startActivity(intent);
        overridePendingTransition(R.anim.grow_fade_in, 0);
    }

    public void handleMessage(String event, JSONObject message) {
        Log.i(LOGTAG, "Got message: " + event);
        try {
            if (event.equals("Menu:Add")) {
                ExtraMenuItem item = new ExtraMenuItem();
                item.label = message.getString("name");
                item.id = message.getInt("id");
                try { // icon is optional
                    item.icon = message.getString("icon");
                } catch (Exception ex) { }
                sExtraMenuItems.add(item);
            } else if (event.equals("Menu:Remove")) {
                // remove it from the menu and from our vector
                Iterator<ExtraMenuItem> i = sExtraMenuItems.iterator();
                int id = message.getInt("id");
                while (i.hasNext()) {
                    ExtraMenuItem item = i.next();
                    if (item.id == id) {
                        sExtraMenuItems.remove(item);
                        if (sMenu == null)
                            return;
                        MenuItem menu = sMenu.findItem(id);
                        if (menu != null)
                            sMenu.removeItem(id);
                    }
                }
            } else if (event.equals("Toast:Show")) {
                final String msg = message.getString("message");
                final String duration = message.getString("duration");
                handleShowToast(msg, duration);
            } else if (event.equals("DOMContentLoaded")) {
                final int tabId = message.getInt("tabID");
                final String backgroundColor = message.getString("bgColor");
                handleContentLoaded(tabId);
                Tab tab = Tabs.getInstance().getTab(tabId);
                if (backgroundColor != null) {
                    tab.setCheckerboardColor(backgroundColor);
                } else {
                    // Default to white if no color is given
                    tab.setCheckerboardColor(Color.WHITE);
                }

                // Sync up the LayerController and the tab if the tab's
                // currently displayed.
                if (getLayerController() != null && Tabs.getInstance().isSelectedTab(tab)) {
                    getLayerController().setCheckerboardColor(tab.getCheckerboardColor());
                }
            } else if (event.equals("DOMTitleChanged")) {
                final int tabId = message.getInt("tabID");
                final String title = message.getString("title");
                handleTitleChanged(tabId, title);
                Log.i(LOGTAG, "title - " + title);
            } else if (event.equals("DOMLinkAdded")) {
                final int tabId = message.getInt("tabID");
                final String rel = message.getString("rel");
                final String href = message.getString("href");
                Log.i(LOGTAG, "link rel - " + rel + ", href - " + href);
                handleLinkAdded(tabId, rel, href);
            } else if (event.equals("DOMWindowClose")) {
                final int tabId = message.getInt("tabID");
                handleWindowClose(tabId);
            } else if (event.equals("log")) {
                // generic log listener
                final String msg = message.getString("msg");
                Log.i(LOGTAG, "Log: " + msg);
            } else if (event.equals("Content:LocationChange")) {
                final int tabId = message.getInt("tabID");
                final String uri = message.getString("uri");
                final String documentURI = message.getString("documentURI");
                final String contentType = message.getString("contentType");
                final boolean sameDocument = message.getBoolean("sameDocument");
                Log.i(LOGTAG, "URI - " + uri);
                handleLocationChange(tabId, uri, documentURI, contentType, sameDocument);
            } else if (event.equals("Content:SecurityChange")) {
                final int tabId = message.getInt("tabID");
                final JSONObject identity = message.getJSONObject("identity");
                Log.i(LOGTAG, "Security Mode - " + identity.getString("mode"));
                handleSecurityChange(tabId, identity);
            } else if (event.equals("Content:StateChange")) {
                final int tabId = message.getInt("tabID");
                final String uri = message.getString("uri");
                final boolean success = message.getBoolean("success");
                int state = message.getInt("state");
                Log.i(LOGTAG, "State - " + state);
                if ((state & GeckoAppShell.WPL_STATE_IS_NETWORK) != 0) {
                    if ((state & GeckoAppShell.WPL_STATE_START) != 0) {
                        Log.i(LOGTAG, "Got a document start");
                        final boolean showProgress = message.getBoolean("showProgress");
                        handleDocumentStart(tabId, showProgress, uri);
                    } else if ((state & GeckoAppShell.WPL_STATE_STOP) != 0) {
                        Log.i(LOGTAG, "Got a document stop");
                        handleDocumentStop(tabId, success, uri);
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
            } else if (event.equals("Doorhanger:Add")) {
                handleDoorHanger(message);
            } else if (event.equals("Doorhanger:Remove")) {
                handleDoorHangerRemove(message);
            } else if (event.equals("Gecko:Ready")) {
                sIsGeckoReady = true;
                mMainHandler.post(new Runnable() {
                    public void run() {
                        if (sMenu != null)
                            sMenu.findItem(R.id.settings).setEnabled(true);
                    }
                });
                setLaunchState(GeckoApp.LaunchState.GeckoRunning);
                GeckoAppShell.sendPendingEventsToGecko();
                connectGeckoLayerClient();
            } else if (event.equals("ToggleChrome:Hide")) {
                mMainHandler.post(new Runnable() {
                    public void run() {
                        mBrowserToolbar.hide();
                    }
                });
            } else if (event.equals("ToggleChrome:Show")) {
                mMainHandler.post(new Runnable() {
                    public void run() {
                        mBrowserToolbar.show();
                    }
                });
            } else if (event.equals("ToggleChrome:Focus")) {
                mMainHandler.post(new Runnable() {
                    public void run() {
                        mBrowserToolbar.setVisibility(View.VISIBLE);
                        mBrowserToolbar.requestFocusFromTouch();
                    }
                });
            } else if (event.equals("DOMFullScreen:Start")) {
                mDOMFullScreen = true;
            } else if (event.equals("DOMFullScreen:Stop")) {
                mDOMFullScreen = false;
            } else if (event.equals("Permissions:Data")) {
                String host = message.getString("host");
                JSONArray permissions = message.getJSONArray("permissions");
                showSiteSettingsDialog(host, permissions);
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
                if (sMenu != null) {
                    mMainHandler.post(new Runnable() {
                        public void run() {
                            sMenu.findItem(R.id.char_encoding).setVisible(visible);
                        }
                    });
                }
            } else if (event.equals("Update:Restart")) {
                doRestart("org.mozilla.gecko.restart_update");
            } else if (event.equals("Tab:ViewportMetadata")) {
                int tabId = message.getInt("tabID");
                Tab tab = Tabs.getInstance().getTab(tabId);
                if (tab == null)
                    return;
                tab.setAllowZoom(message.getBoolean("allowZoom"));
                tab.setDefaultZoom((float) message.getDouble("defaultZoom"));
                tab.setMinZoom((float) message.getDouble("minZoom"));
                tab.setMaxZoom((float) message.getDouble("maxZoom"));
                // Sync up the LayerController and the tab if the tab's currently displayed.
                LayerController controller = getLayerController();
                if (controller != null && Tabs.getInstance().isSelectedTab(tab)) {
                    controller.setAllowZoom(tab.getAllowZoom());
                    controller.setDefaultZoom(tab.getDefaultZoom());
                    controller.setMinZoom(tab.getMinZoom());
                    controller.setMaxZoom(tab.getMaxZoom());
                }
            } else if (event.equals("Tab:HasTouchListener")) {
                int tabId = message.getInt("tabID");
                Tab tab = Tabs.getInstance().getTab(tabId);
                tab.setHasTouchListeners(true);
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    mMainHandler.post(new Runnable() {
                        public void run() {
                            mLayerController.getView().getTouchEventHandler().setWaitForTouchListeners(true);
                        }
                    });
                }
            } else if (event.equals("Session:StatePurged")) {
                if (mAboutHomeContent != null) {
                    mMainHandler.post(new Runnable() {
                        public void run() {
                            mAboutHomeContent.setLastTabsVisibility(false);
                        }
                    });
                }
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
                final AccessibilityEvent accEvent = AccessibilityEvent.obtain(message.getInt("eventType"));
                accEvent.setClassName(LayerView.class.getName());
                accEvent.setPackageName(mAppContext.getPackageName());

                final JSONArray text = message.getJSONArray("text");
                for (int i = 0; i < text.length(); i++)
                    accEvent.getText().add(text.getString(i));

                accEvent.setContentDescription(message.optString("description"));
                accEvent.setEnabled(message.optBoolean("enabled", true));
                accEvent.setChecked(message.optBoolean("checked"));
                accEvent.setPassword(message.optBoolean("password"));
                accEvent.setAddedCount(message.optInt("addedCount"));
                accEvent.setRemovedCount(message.optInt("removedCount"));
                accEvent.setFromIndex(message.optInt("fromIndex"));
                accEvent.setItemCount(message.optInt("itemCount"));
                accEvent.setCurrentItemIndex(message.optInt("currentItemIndex"));
                accEvent.setBeforeText(message.optString("beforeText"));

                mMainHandler.post(new Runnable() {
                    public void run() {
                        AccessibilityManager accessibilityManager =
                            (AccessibilityManager) mAppContext.getSystemService(Context.ACCESSIBILITY_SERVICE);
                        accessibilityManager.sendAccessibilityEvent(accEvent);
                    }
                });
            } else if (event.equals("Accessibility:Ready")) {
                mMainHandler.post(new Runnable() {
                    public void run() {
                        JSONObject ret = new JSONObject();
                        AccessibilityManager accessibilityManager =
                            (AccessibilityManager) mAppContext.getSystemService(Context.ACCESSIBILITY_SERVICE);
                        try {
                            ret.put("enabled", accessibilityManager.isEnabled());
                            // XXX: A placeholder for future explore by touch support.
                            ret.put("exploreByTouch", false);
                        } catch (Exception ex) {
                            Log.e(LOGTAG, "Error building JSON arguments for Accessibility:Ready:", ex);
                        }
                        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Accessibility:Settings",
                                                                                       ret.toString()));
                    }
                });
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception handling message \"" + event + "\":", e);
        }
    }

    public void showAboutHome() {
        Runnable r = new AboutHomeRunnable(true);
        mMainHandler.postAtFrontOfQueue(r);
    }

    public void hideAboutHome() {
        Runnable r = new AboutHomeRunnable(false);
        mMainHandler.postAtFrontOfQueue(r);
    }

    public class AboutHomeRunnable implements Runnable {
        boolean mShow;
        AboutHomeRunnable(boolean show) {
            mShow = show;
        }

        public void run() {
            mFormAssistPopup.hide();
            if (mShow) {
                if (mAboutHomeContent == null) {
                    mAboutHomeContent = (AboutHomeContent) findViewById(R.id.abouthome_content);
                    mAboutHomeContent.init();
                    mAboutHomeContent.update(GeckoApp.mAppContext, AboutHomeContent.UpdateFlags.ALL);
                    mAboutHomeContent.setUriLoadCallback(new AboutHomeContent.UriLoadCallback() {
                        public void callback(String url) {
                            mBrowserToolbar.setProgressVisibility(true);
                            loadUrl(url, AwesomeBar.Type.EDIT);
                        }
                    });
                } else {
                    mAboutHomeContent.update(GeckoApp.mAppContext,
                                             EnumSet.of(AboutHomeContent.UpdateFlags.TOP_SITES,
                                                        AboutHomeContent.UpdateFlags.REMOTE_TABS));
                }
            
                mAboutHomeContent.setVisibility(View.VISIBLE);
            } else {
                findViewById(R.id.abouthome_content).setVisibility(View.GONE);
            }
        } 
    }

    /**
     * @param aPermissions
     *        Array of JSON objects to represent site permissions.
     *        Example: { type: "offline-app", setting: "Store Offline Data: Allow" }
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
            // Eventually we should use a list adapter and custom checkable list items
            // to make a two-line UI to match the mock-ups
            CharSequence[] items = new CharSequence[aPermissions.length()];
            boolean[] states = new boolean[aPermissions.length()];
            for (int i = 0; i < aPermissions.length(); i++) {
                try {
                    items[i] = aPermissions.getJSONObject(i).
                               getString("setting");
                    // Make all the items checked by default
                    states[i] = true;
                } catch (JSONException e) {
                    Log.i(LOGTAG, "JSONException: " + e);
                }
            }
            builder.setMultiChoiceItems(items, states, new DialogInterface.OnMultiChoiceClickListener(){
                public void onClick(DialogInterface dialog, int item, boolean state) {
                    // Do nothing
                }
            });
            builder.setPositiveButton(R.string.site_settings_clear, new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int id) {
                    ListView listView = ((AlertDialog) dialog).getListView();
                    SparseBooleanArray checkedItemPositions = listView.getCheckedItemPositions();

                    // An array of the indices of the permissions we want to clear
                    JSONArray permissionsToClear = new JSONArray();
                    for (int i = 0; i < checkedItemPositions.size(); i++) {
                        boolean checked = checkedItemPositions.get(i);
                        if (checked)
                            permissionsToClear.put(i);
                    }
                    GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Permissions:Clear", permissionsToClear.toString()));
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
                builder.create().show();
            }
        });
    }

    void handleDoorHanger(JSONObject geckoObject) throws JSONException {
        final String message = geckoObject.getString("message");
        final String value = geckoObject.getString("value");
        final JSONArray buttons = geckoObject.getJSONArray("buttons");
        final int tabId = geckoObject.getInt("tabID");
        final JSONObject options = geckoObject.getJSONObject("options");

        Log.i(LOGTAG, "DoorHanger received for tab " + tabId + ", msg:" + message);

        mMainHandler.post(new Runnable() {
            public void run() {
                Tab tab = Tabs.getInstance().getTab(tabId);
                if (tab != null)
                    mDoorHangerPopup.addDoorHanger(message, value, buttons, tab, options);
            }
        });
    }

    void handleDoorHangerRemove(JSONObject geckoObject) throws JSONException {
        final String value = geckoObject.getString("value");
        final int tabId = geckoObject.getInt("tabID");

        Log.i(LOGTAG, "Doorhanger:Remove received for tab " + tabId);

        mMainHandler.post(new Runnable() {
            public void run() {
                Tab tab = Tabs.getInstance().getTab(tabId);
                if (tab == null)
                    return;
                tab.removeDoorHanger(value);
                mDoorHangerPopup.updatePopup();
            }
        });
    }

    void handleDocumentStart(int tabId, final boolean showProgress, String uri) {
        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        tab.setState("about:home".equals(uri) ? Tab.STATE_SUCCESS : Tab.STATE_LOADING);
        tab.updateIdentityData(null);
        if (Tabs.getInstance().isSelectedTab(tab))
            getLayerController().getView().getRenderer().resetCheckerboard();
        mMainHandler.post(new Runnable() {
            public void run() {
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    mBrowserToolbar.setSecurityMode(tab.getSecurityMode());
                    if (showProgress && tab.getState() == Tab.STATE_LOADING)
                        mBrowserToolbar.setProgressVisibility(true);
                }
                Tabs.getInstance().notifyListeners(tab, Tabs.TabEvents.START);
            }
        });
    }

    void handleDocumentStop(int tabId, boolean success, final String uri) {
        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        tab.setState(success ? Tab.STATE_SUCCESS : Tab.STATE_ERROR);

        mMainHandler.post(new Runnable() {
            public void run() {
                if (Tabs.getInstance().isSelectedTab(tab))
                    mBrowserToolbar.setProgressVisibility(false);
                Tabs.getInstance().notifyListeners(tab, Tabs.TabEvents.STOP);
            }
        });
        GeckoAppShell.getHandler().postDelayed(new Runnable() {
            public void run() {
                if (!uri.equals(tab.getURL()))
                    return;

                getAndProcessThumbnailForTab(tab);
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    GeckoAppShell.sendEventToGecko(GeckoEvent.createStartPaintListentingEvent(tab.getId()));
                    GeckoAppShell.screenshotWholePage(tab);
                }

            }
        }, 500);
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

        mMainHandler.post(new Runnable() {
            public void run() {
                Tabs.getInstance().notifyListeners(tab, Tabs.TabEvents.LOADED);
            }
        });
    }

    void handleTitleChanged(int tabId, String title) {
        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        tab.updateTitle(title);

        mMainHandler.post(new Runnable() {
            public void run() {
                if (Tabs.getInstance().isSelectedTab(tab))
                    mBrowserToolbar.setTitle(tab.getDisplayTitle());
                Tabs.getInstance().notifyListeners(tab, Tabs.TabEvents.TITLE);
            }
        });
    }

    void handleLinkAdded(final int tabId, String rel, final String href) {
        if (rel.indexOf("[icon]") != -1) {
            final Tab tab = Tabs.getInstance().getTab(tabId);
            if (tab != null) {
                tab.updateFaviconURL(href);

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
        }
    }

    void handleWindowClose(final int tabId) {
        Tabs tabs = Tabs.getInstance();
        Tab tab = tabs.getTab(tabId);
        tabs.closeTab(tab);
    }

    void addPluginView(final View view, final Rect rect) {
        mMainHandler.post(new Runnable() { 
            public void run() {
                Tabs tabs = Tabs.getInstance();
                Tab tab = tabs.getSelectedTab();

                PluginLayer layer = (PluginLayer) tab.getPluginLayer(view);
                if (layer == null) {
                    layer = new PluginLayer(view, rect, mLayerController.getView().getRenderer().getMaxTextureSize());
                    tab.addPluginLayer(view, layer);
                } else {
                    layer.reset(rect);
                    layer.setVisible(true);
                }

                mLayerController.getView().addLayer(layer);
            }
        });
    }

    void removePluginView(final View view) {
        mMainHandler.post(new Runnable() { 
            public void run() {
                Tabs tabs = Tabs.getInstance();
                Tab tab = tabs.getSelectedTab();

                PluginLayer layer = (PluginLayer) tab.removePluginLayer(view);
                if (layer != null) {
                    layer.destroy();
                }
            }
        });
    }

    public Surface createSurface() {
        Tabs tabs = Tabs.getInstance();
        Tab tab = tabs.getSelectedTab();
        if (tab == null)
            return null;

        SurfaceTextureLayer layer = SurfaceTextureLayer.create();
        if (layer == null)
            return null;

        Surface surface = layer.getSurface();
        tab.addPluginLayer(surface, layer);
        return surface;
    }

    public void destroySurface(Surface surface) {
        Tabs tabs = Tabs.getInstance();
        Tab tab = tabs.getSelectedTab();
        if (tab == null)
            return;

        Layer layer = tab.removePluginLayer(surface);
        hidePluginLayer(layer);
    }

    public void showSurface(Surface surface, int x, int y,
                            int w, int h, boolean inverted, boolean blend) {
        Tabs tabs = Tabs.getInstance();
        Tab tab = tabs.getSelectedTab();
        if (tab == null)
            return;

        LayerView layerView = mLayerController.getView();
        SurfaceTextureLayer layer = (SurfaceTextureLayer)tab.getPluginLayer(surface);
        if (layer == null)
            return;

        layer.update(new Rect(x, y, x + w, y + h), inverted, blend);
        layerView.addLayer(layer);

        // FIXME: shouldn't be necessary, layer will request
        // one when it gets first frame
        layerView.requestRender();
    }
    
    private void hidePluginLayer(Layer layer) {
        LayerView layerView = mLayerController.getView();
        layerView.removeLayer(layer);
        layerView.requestRender();
    }

    private void showPluginLayer(Layer layer) {
        LayerView layerView = mLayerController.getView();
        layerView.addLayer(layer);
        layerView.requestRender();
    }

    public void hideSurface(Surface surface) {
        Tabs tabs = Tabs.getInstance();
        Tab tab = tabs.getSelectedTab();
        if (tab == null)
            return;

        Layer layer = tab.getPluginLayer(surface);
        if (layer == null)
            return;

        hidePluginLayer(layer);
    }

    public void requestRender() {
        mLayerController.getView().requestRender();
    }

    public void hidePlugins() {
        Tabs tabs = Tabs.getInstance();
        Tab tab = tabs.getSelectedTab();

        if (tab == null)
            return;

        hidePlugins(tab);
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

    // The ActionBar needs to be refreshed on rotation as different orientation uses different resources
    public void refreshActionBar() {
        if (Build.VERSION.SDK_INT >= 11) {
            LinearLayout actionBar = (LinearLayout) getLayoutInflater().inflate(R.layout.browser_toolbar, null);
            mBrowserToolbar.from(actionBar);
            mBrowserToolbar.refresh();
            GeckoActionBar.setBackgroundDrawable(this, getResources().getDrawable(R.drawable.gecko_actionbar_bg));
            GeckoActionBar.setCustomView(this, actionBar);
        }
    }

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        GeckoAppShell.registerGlobalExceptionHandler();

        mAppContext = this;

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

        GeckoAppShell.loadMozGlue();
        mMainHandler = new Handler();
        Log.w(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - onCreate");

        LayoutInflater.from(this).setFactory(GeckoViewsFactory.getInstance());

        super.onCreate(savedInstanceState);

        mOrientation = getResources().getConfiguration().orientation;

        setContentView(R.layout.gecko_app);

        LinearLayout actionBar;
        if (Build.VERSION.SDK_INT >= 11) {
            actionBar = (LinearLayout) GeckoActionBar.getCustomView(this);
        } else {
            actionBar = (LinearLayout) findViewById(R.id.browser_toolbar);
        }

        mBrowserToolbar = new BrowserToolbar(mAppContext);
        mBrowserToolbar.from(actionBar);

        // setup gecko layout
        mGeckoLayout = (RelativeLayout) findViewById(R.id.gecko_layout);
        mMainLayout = (LinearLayout) findViewById(R.id.main_layout);

        if (savedInstanceState != null) {
            mBrowserToolbar.setTitle(savedInstanceState.getString(SAVED_STATE_TITLE));
            mRestoreMode = GeckoAppShell.RESTORE_OOM;
        }

        ((GeckoApplication) getApplication()).addApplicationLifecycleCallbacks(this);
    }

    private void initialize() {
        mInitialized = true;

        Intent intent = getIntent();
        String action = intent.getAction();
        String args = intent.getStringExtra("args");
        if (args != null && args.contains("-profile")) {
            Pattern p = Pattern.compile("(?:-profile\\s*)(\\w*)(\\s*)");
            Matcher m = p.matcher(args);
            if (m.find()) {
                mProfile = GeckoProfile.get(this, m.group(1));
                mBrowserToolbar.setTitle(null);
            }
        }

        if (ACTION_UPDATE.equals(action) || args != null && args.contains("-alert update-app")) {
            Log.i(LOGTAG,"onCreate: Update request");
            checkAndLaunchUpdate();
        }

        String passedUri = null;
        String uri = getURIFromIntent(intent);
        if (uri != null && uri.length() > 0) {
            passedUri = uri;
            mBrowserToolbar.setTitle(uri);
        }

        if (mRestoreMode == GeckoAppShell.RESTORE_NONE && getProfile().shouldRestoreSession())
            mRestoreMode = GeckoAppShell.RESTORE_CRASH;

        boolean isExternalURL = passedUri != null && !passedUri.equals("about:home");
        if (!isExternalURL) {
            // show about:home if we aren't restoring previous session
            if (mRestoreMode == GeckoAppShell.RESTORE_NONE) {
                mBrowserToolbar.updateTabCount(1);
                showAboutHome();
            }
        } else {
            mBrowserToolbar.updateTabCount(1);
        }

        mBrowserToolbar.setProgressVisibility(isExternalURL || (mRestoreMode != GeckoAppShell.RESTORE_NONE));

        // Start migrating as early as possible, can do this in
        // parallel with Gecko load.
        checkMigrateProfile();

        Uri data = intent.getData();
        if (data != null && "http".equals(data.getScheme()) &&
            isHostOnPrefetchWhitelist(data.getHost())) {
            Intent copy = new Intent(intent);
            copy.setAction(ACTION_LOAD);
            GeckoAppShell.getHandler().post(new RedirectorRunnable(copy));
            // We're going to handle this uri with the redirector, so setting
            // the action to MAIN and clearing the uri data prevents us from
            // loading it twice
            intent.setAction(Intent.ACTION_MAIN);
            intent.setData(null);
            passedUri = "about:empty";
        }

        sGeckoThread = new GeckoThread(intent, passedUri, mRestoreMode);
        if (!ACTION_DEBUG.equals(action) &&
            checkAndSetLaunchState(LaunchState.Launching, LaunchState.Launched)) {
            sGeckoThread.start();
        } else if (ACTION_DEBUG.equals(action) &&
            checkAndSetLaunchState(LaunchState.Launching, LaunchState.WaitForDebugger)) {
            mMainHandler.postDelayed(new Runnable() {
                public void run() {
                    Log.i(LOGTAG, "Launching from debug intent after 5s wait");
                    setLaunchState(LaunchState.Launching);
                    sGeckoThread.start();
                }
            }, 1000 * 5 /* 5 seconds */);
            Log.i(LOGTAG, "Intent : ACTION_DEBUG - waiting 5s before launching");
        }

        mFavicons = new Favicons(this);

        Tabs.getInstance().setContentResolver(getContentResolver()); 

        if (cameraView == null) {
            cameraView = new SurfaceView(this);
            cameraView.getHolder().setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
        }

        if (mLayerController == null) {
            /*
             * Create a layer client, but don't hook it up to the layer controller yet.
             */
            mLayerClient = new GeckoLayerClient(this);

            /*
             * Hook a placeholder layer client up to the layer controller so that the user can pan
             * and zoom a cached screenshot of the previous page. This call will return null if
             * there is no cached screenshot; in that case, we have no choice but to display a
             * checkerboard.
             *
             * TODO: Fall back to a built-in screenshot of the Fennec Start page for a nice first-
             * run experience, perhaps?
             */
            mLayerController = new LayerController(this);
            View v = mLayerController.getView();

            // Instead of flickering the checkerboard, show a white screen until Gecko paints
            v.setBackgroundColor(Color.WHITE);

            mGeckoLayout.addView(v, 0);
        }

        mPluginContainer = (AbsoluteLayout) findViewById(R.id.plugin_container);

        mDoorHangerPopup = new DoorHangerPopup(this);
        mFormAssistPopup = (FormAssistPopup) findViewById(R.id.form_assist_popup);

        Log.w(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - UI almost up");

        //register for events
        GeckoAppShell.registerGeckoEventListener("DOMContentLoaded", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("DOMTitleChanged", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("DOMLinkAdded", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("DOMWindowClose", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("log", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Content:LocationChange", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Content:SecurityChange", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Content:StateChange", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Content:LoadError", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Content:PageShow", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("onCameraCapture", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Doorhanger:Add", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Doorhanger:Remove", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Menu:Add", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Menu:Remove", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Gecko:Ready", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Toast:Show", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("DOMFullScreen:Start", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("DOMFullScreen:Stop", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("ToggleChrome:Hide", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("ToggleChrome:Show", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("ToggleChrome:Focus", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Permissions:Data", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("CharEncoding:Data", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("CharEncoding:State", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Update:Restart", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Tab:HasTouchListener", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Tab:ViewportMetadata", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Session:StatePurged", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Bookmark:Insert", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Accessibility:Event", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Accessibility:Ready", GeckoApp.mAppContext);

        if (SmsManager.getInstance() != null) {
          SmsManager.getInstance().start();
        }

        mBatteryReceiver = new GeckoBatteryManager();
        mBatteryReceiver.registerFor(mAppContext);

        mConnectivityReceiver = new GeckoConnectivityReceiver();
        mConnectivityReceiver.registerFor(mAppContext);

        GeckoNetworkManager.getInstance().init();
        GeckoNetworkManager.getInstance().start();

        GeckoScreenOrientationListener.getInstance().start();

        final GeckoApp self = this;

        GeckoAppShell.getHandler().postDelayed(new Runnable() {
            public void run() {
                Log.w(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - pre checkLaunchState");
                // Sync settings need Gecko to be loaded, so
                // no hurry in starting this.
                checkMigrateSync();

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
    }

    public GeckoProfile getProfile() {
        // fall back to default profile if we didn't load a specific one
        if (mProfile == null) {
            mProfile = GeckoProfile.get(this);
        }
        return mProfile;
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

    class RedirectorRunnable implements Runnable {
        Intent mIntent;
        RedirectorRunnable(Intent intent) {
            mIntent = intent;
        }
        public void run() {
            HttpURLConnection connection = null;
            try {
                // this class should only be initialized with an intent with non-null data
                URL url = new URL(mIntent.getData().toString());
                // data url should have an http scheme
                connection = (HttpURLConnection) url.openConnection();
                connection.setRequestProperty("User-Agent", getUAStringForHost(url.getHost()));
                connection.setInstanceFollowRedirects(false);
                connection.setRequestMethod("GET");
                connection.connect();
                int code = connection.getResponseCode();
                if (code >= 300 && code < 400) {
                    String location = connection.getHeaderField("Location");
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
                Log.i(LOGTAG, "exception trying to pre-fetch redirected url", ioe);
                mIntent.putExtra("prefetched", 1);
            } catch (Exception e) {
                Log.w(LOGTAG, "unexpected exception, passing url directly to Gecko but we should explicitly catch this", e);
                mIntent.putExtra("prefetched", 1);
            } finally {
                if (connection != null)
                    connection.disconnect();
            }
            mMainHandler.postAtFrontOfQueue(new Runnable() {
                public void run() {
                    onNewIntent(mIntent);
                }
            });
        }
    }

    private final String kPrefetchWhiteListArray[] = new String[] { 
        "t.co",
        "bit.ly",
        "moz.la",
        "aje.me",
        "facebook.com",
        "goo.gl",
        "tinyurl.com"
    };
    
    private final CopyOnWriteArrayList<String> kPrefetchWhiteList =
        new CopyOnWriteArrayList<String>(kPrefetchWhiteListArray);

    private boolean isHostOnPrefetchWhitelist(String host) {
        return kPrefetchWhiteList.contains(host);
    }

    @Override
    protected void onNewIntent(Intent intent) {
        Log.w(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - onNewIntent");

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
                (bundle == null || bundle.getInt("prefetched", 0) != 1) &&
                isHostOnPrefetchWhitelist(data.getHost())) {
                GeckoAppShell.getHandler().post(new RedirectorRunnable(intent));
                return;
            }
        }
        final String action = intent.getAction();
        if (ACTION_DEBUG.equals(action) &&
            checkAndSetLaunchState(LaunchState.Launching, LaunchState.WaitForDebugger)) {
            mMainHandler.postDelayed(new Runnable() {
                public void run() {
                    Log.i(LOGTAG, "Launching from debug intent after 5s wait");
                    setLaunchState(LaunchState.Launching);
                    sGeckoThread.start();
                }
            }, 1000 * 5 /* 5 seconds */);
            Log.i(LOGTAG, "Intent : ACTION_DEBUG - waiting 5s before launching");
            return;
        }
        if (checkLaunchState(LaunchState.WaitForDebugger) || intent == getIntent())
            return;

        if (Intent.ACTION_MAIN.equals(action)) {
            Log.i(LOGTAG, "Intent : ACTION_MAIN");
            GeckoAppShell.sendEventToGecko(GeckoEvent.createURILoadEvent(""));
        }
        else if (ACTION_LOAD.equals(action)) {
            String uri = intent.getDataString();
            loadUrl(uri, AwesomeBar.Type.EDIT);
            Log.i(LOGTAG,"onNewIntent: " + uri);
        }
        else if (Intent.ACTION_VIEW.equals(action)) {
            String uri = intent.getDataString();
            GeckoAppShell.sendEventToGecko(GeckoEvent.createURILoadEvent(uri));
            Log.i(LOGTAG,"onNewIntent: " + uri);
        }
        else if (ACTION_WEBAPP.equals(action)) {
            String uri = getURIFromIntent(intent);
            GeckoAppShell.sendEventToGecko(GeckoEvent.createWebappLoadEvent(uri));
            Log.i(LOGTAG,"Intent : WEBAPP - " + uri);
        }
        else if (ACTION_BOOKMARK.equals(action)) {
            String uri = getURIFromIntent(intent);
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBookmarkLoadEvent(uri));
            Log.i(LOGTAG,"Intent : BOOKMARK - " + uri);
        }
    }

    /*
     * Handles getting a uri from and intent in a way that is backwards
     * compatable with our previous implementations
     */
    private String getURIFromIntent(Intent intent) {
        String uri = intent.getDataString();
        if (uri != null)
            return uri;

        final String action = intent.getAction();
        if (ACTION_WEBAPP.equals(action) || ACTION_BOOKMARK.equals(action)) {
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
        Log.i(LOGTAG, "resume");

        // After an onPause, the activity is back in the foreground.
        // Undo whatever we did in onPause.
        super.onResume();

        SiteIdentityPopup.getInstance().dismiss();

        int newOrientation = getResources().getConfiguration().orientation;

        if (mOrientation != newOrientation) {
            mOrientation = newOrientation;
            refreshActionBar();
        }
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
        Log.i(LOGTAG, "stop");
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
    public void onRestart()
    {
        Log.i(LOGTAG, "restart");
        super.onRestart();
    }

    @Override
    public void onStart()
    {
        Log.w(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - onStart");

        Log.i(LOGTAG, "start");
        GeckoAppShell.sendEventToGecko(GeckoEvent.createStartEvent(isApplicationInBackground()));
        super.onStart();
    }

    @Override
    public void onDestroy()
    {
        Log.i(LOGTAG, "destroy");

        // Tell Gecko to shutting down; we'll end up calling System.exit()
        // in onXreExit.
        if (isFinishing())
            GeckoAppShell.sendEventToGecko(GeckoEvent.createShutdownEvent());
        
        GeckoAppShell.unregisterGeckoEventListener("DOMContentLoaded", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("DOMTitleChanged", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("DOMLinkAdded", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("DOMWindowClose", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("log", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Content:LocationChange", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Content:SecurityChange", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Content:StateChange", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Content:LoadError", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Content:PageShow", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("onCameraCapture", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Doorhanger:Add", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Menu:Add", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Menu:Remove", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Gecko:Ready", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Toast:Show", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("ToggleChrome:Hide", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("ToggleChrome:Show", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("ToggleChrome:Focus", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Permissions:Data", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("CharEncoding:Data", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("CharEncoding:State", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Tab:HasTouchListener", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Session:StatePurged", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Bookmark:Insert", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Accessibility:Event", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Accessibility:Ready", GeckoApp.mAppContext);

        if (mFavicons != null)
            mFavicons.close();

        if (SmsManager.getInstance() != null) {
            SmsManager.getInstance().stop();
            if (isFinishing())
                SmsManager.getInstance().shutdown();
        }

        super.onDestroy();

        if (mBatteryReceiver != null)
            mBatteryReceiver.unregisterFor(mAppContext);

        if (mAboutHomeContent != null)
            mAboutHomeContent.onDestroy();

        ((GeckoApplication) getApplication()).removeApplicationLifecycleCallbacks(this);
    }

    @Override
    public void onContentChanged() {
        super.onContentChanged();
        if (mAboutHomeContent != null)
            mAboutHomeContent.onActivityContentChanged(this);
    }


    @Override
    public void onConfigurationChanged(Configuration newConfig)
    {
        Log.i(LOGTAG, "configuration changed");

        super.onConfigurationChanged(newConfig);

        if (mOrientation != newConfig.orientation) {
            mOrientation = newConfig.orientation;
            if (mFormAssistPopup != null)
                mFormAssistPopup.hide();
            SiteIdentityPopup.getInstance().dismiss();
            refreshActionBar();
        }
    }

    @Override
    public void onLowMemory()
    {
        Log.e(LOGTAG, "low memory");
        if (checkLaunchState(LaunchState.GeckoRunning))
            GeckoAppShell.onLowMemory();
        super.onLowMemory();
        GeckoAppShell.geckoEventSync();
    }

    @Override
    public void onApplicationPause() {
        Log.i(LOGTAG, "application paused");
        GeckoAppShell.sendEventToGecko(GeckoEvent.createPauseEvent(true));

        if (mConnectivityReceiver != null)
            mConnectivityReceiver.unregisterFor(mAppContext);
        GeckoNetworkManager.getInstance().stop();
        GeckoScreenOrientationListener.getInstance().stop();
    }

    @Override
    public void onApplicationResume() {
        Log.i(LOGTAG, "application resumed");
        if (checkLaunchState(LaunchState.GeckoRunning))
            GeckoAppShell.sendEventToGecko(GeckoEvent.createResumeEvent(true));

        if (mConnectivityReceiver != null)
            mConnectivityReceiver.registerFor(mAppContext);
        GeckoNetworkManager.getInstance().start();
        GeckoScreenOrientationListener.getInstance().start();
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
        Log.i(LOGTAG, "doRestart(\"" + action + "\")");
        try {
            Intent intent = new Intent(action);
            intent.setClassName(getPackageName(),
                                getPackageName() + ".Restarter");
            /* TODO: addEnvToIntent(intent); */
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK |
                            Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
            Log.i(LOGTAG, intent.toString());
            GeckoAppShell.killAnyZombies();
            startActivity(intent);
        } catch (Exception e) {
            Log.i(LOGTAG, "error doing restart", e);
        }
        finish();
        // Give the restart process time to start before we die
        GeckoAppShell.waitForAnotherGeckoProc();
    }

    public void handleNotification(String action, String alertName, String alertCookie) {
        GeckoAppShell.handleNotification(action, alertName, alertCookie);
    }

    private void checkAndLaunchUpdate() {
        Log.i(LOGTAG, "Checking for an update");

        int statusCode = 8; // UNEXPECTED_ERROR
        File baseUpdateDir = null;
        if (Build.VERSION.SDK_INT >= 8)
            baseUpdateDir = getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS);
        else
            baseUpdateDir = new File(Environment.getExternalStorageDirectory().getPath(), "download");

        File updateDir = new File(new File(baseUpdateDir, "updates"),"0");

        File updateFile = new File(updateDir, "update.apk");
        File statusFile = new File(updateDir, "update.status");

        if (!statusFile.exists() || !readUpdateStatus(statusFile).equals("pending"))
            return;

        if (!updateFile.exists())
            return;

        Log.i(LOGTAG, "Update is available!");

        // Launch APK
        File updateFileToRun = new File(updateDir, getPackageName() + "-update.apk");
        try {
            if (updateFile.renameTo(updateFileToRun)) {
                String amCmd = "/system/bin/am start -a android.intent.action.VIEW " +
                               "-n com.android.packageinstaller/.PackageInstallerActivity -d file://" +
                               updateFileToRun.getPath();
                Log.i(LOGTAG, amCmd);
                Runtime.getRuntime().exec(amCmd);
                statusCode = 0; // OK
            } else {
                Log.i(LOGTAG, "Cannot rename the update file!");
                statusCode = 7; // WRITE_ERROR
            }
        } catch (Exception e) {
            Log.i(LOGTAG, "error launching installer to update", e);
        }

        // Update the status file
        String status = statusCode == 0 ? "succeeded\n" : "failed: "+ statusCode + "\n";

        OutputStream outStream;
        try {
            byte[] buf = status.getBytes("UTF-8");
            outStream = new FileOutputStream(statusFile);
            outStream.write(buf, 0, buf.length);
            outStream.close();
        } catch (Exception e) {
            Log.i(LOGTAG, "error writing status file", e);
        }

        if (statusCode == 0)
            System.exit(0);
    }

    private String readUpdateStatus(File statusFile) {
        String status = "";
        try {
            BufferedReader reader = new BufferedReader(new FileReader(statusFile));
            status = reader.readLine();
            reader.close();
        } catch (Exception e) {
            Log.i(LOGTAG, "error reading update status", e);
        }
        return status;
    }

    private void checkMigrateProfile() {
        final File profileDir = getProfile().getDir();
        final long currentTime = SystemClock.uptimeMillis();

        if (profileDir != null) {
            final GeckoApp app = GeckoApp.mAppContext;

            GeckoAppShell.getHandler().post(new Runnable() {
                public void run() {
                    Log.i(LOGTAG, "Checking profile migration in: " + profileDir.getAbsolutePath());

                    ProfileMigrator profileMigrator = new ProfileMigrator(app);

                    // Do a migration run on the first start after an upgrade.
                    if (!profileMigrator.hasMigrationRun()) {
                        // Show the "Setting up Fennec" screen if this takes
                        // a while.
                        final SetupScreen setupScreen = new SetupScreen(app);

                        final Runnable startCallback = new Runnable() {
                            public void run() {
                                GeckoApp.mAppContext.runOnUiThread(new Runnable() {
                                    public void run() {
                                       setupScreen.show();
                                    }
                                });
                            }
                        };

                        final Runnable stopCallback = new Runnable() {
                            public void run() {
                                GeckoApp.mAppContext.runOnUiThread(new Runnable() {
                                    public void run() {
                                        setupScreen.dismiss();
                                    }
                                });
                            }
                        };

                        profileMigrator.setLongOperationCallbacks(startCallback,
                                                                  stopCallback);
                        profileMigrator.launchPlaces(profileDir);

                        long timeDiff = SystemClock.uptimeMillis() - currentTime;
                        Log.i(LOGTAG, "Profile migration took " + timeDiff + " ms");

                        // Update about:home with the new information.
                        updateAboutHomeTopSites();
                    }
                }}
            );
        }
    }

    private void checkMigrateSync() {
        final File profileDir = getProfile().getDir();
        if (profileDir != null) {
            final GeckoApp app = GeckoApp.mAppContext;
            ProfileMigrator profileMigrator = new ProfileMigrator(app);
            if (!profileMigrator.hasSyncMigrated()) {
                Log.i(LOGTAG, "Checking Sync settings in: " + profileDir.getAbsolutePath());
                profileMigrator.launchSyncPrefs();
            }
        }
    }

    /**
     * The FilePickerPromptRunnable has to be called to show an intent-like
     * context menu UI using the PromptService.
     */
    private class FilePickerPromptRunnable implements Runnable {
        public FilePickerPromptRunnable(String aTitle, PromptService.PromptListItem[] aItems) {
            super();
            mTitle = aTitle;
            mItems = aItems;
        }

        public void run() {
            GeckoAppShell.getPromptService().Show(mTitle, "", null, mItems, false);
        }

        private String mTitle;
        private PromptService.PromptListItem[] mItems;
    }

    private int addIntentActivitiesToList(Intent intent, ArrayList<PromptService.PromptListItem> items, ArrayList<Intent> aIntents) {
        PackageManager pm = mAppContext.getPackageManager();
        final List<ResolveInfo> lri =
            pm.queryIntentActivityOptions(GeckoApp.mAppContext.getComponentName(), null, intent, 0);

        if (lri == null) {
            return 0;
        }

        for (int i=0; i<lri.size(); i++) {
            final ResolveInfo ri = lri.get(i);
            Intent rintent = new Intent(intent);
            rintent.setComponent(new ComponentName(
                    ri.activityInfo.applicationInfo.packageName,
                    ri.activityInfo.name));

            PromptService.PromptListItem item = new PromptService.PromptListItem(ri.loadLabel(pm).toString());
            item.icon = ri.loadIcon(pm);
            items.add(item);
            aIntents.add(rintent);
        }

        return lri.size();
    }

    private int AddFilePickingActivities(ArrayList<PromptService.PromptListItem> aItems, String aType, ArrayList<Intent> aIntents) {
        Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        intent.setType(aType);
        intent.addCategory(Intent.CATEGORY_OPENABLE);

        return addIntentActivitiesToList(intent, aItems, aIntents);
    }

    static private String generateImageName() {
        Time now = new Time();
        now.setToNow();
        return now.format("%Y-%m-%d %H.%M.%S") + ".jpg";
    }

    private PromptService.PromptListItem[] getItemsAndIntentsForFilePicker(String aMimeType, ArrayList<Intent> aIntents) {
        ArrayList<PromptService.PromptListItem> items = new ArrayList<PromptService.PromptListItem>();

        if (aMimeType.equals("audio/*")) {
            if (AddFilePickingActivities(items, "audio/*", aIntents) <= 0) {
                AddFilePickingActivities(items, "*/*", aIntents);
            }
        } else if (aMimeType.equals("image/*")) {
            mImageFilePath = generateImageName();
            Intent intent = new Intent(android.provider.MediaStore.ACTION_IMAGE_CAPTURE);
            intent.putExtra(MediaStore.EXTRA_OUTPUT,
                            Uri.fromFile(new File(Environment.getExternalStorageDirectory(),
                                                  mImageFilePath)));
            addIntentActivitiesToList(intent, items, aIntents);

            if (AddFilePickingActivities(items, "image/*", aIntents) <= 0) {
              AddFilePickingActivities(items, "*/*", aIntents);
            }
        } else if (aMimeType.equals("video/*")) {
            Intent intent = new Intent(android.provider.MediaStore.ACTION_VIDEO_CAPTURE);
            addIntentActivitiesToList(intent, items, aIntents);

            if (AddFilePickingActivities(items, "video/*", aIntents) <= 0) {
              AddFilePickingActivities(items, "*/*", aIntents);
            }
        } else {
            mImageFilePath = generateImageName();
            Intent intent = new Intent(android.provider.MediaStore.ACTION_IMAGE_CAPTURE);
            intent.putExtra(MediaStore.EXTRA_OUTPUT,
                            Uri.fromFile(new File(Environment.getExternalStorageDirectory(),
                                                  mImageFilePath)));
            addIntentActivitiesToList(intent, items, aIntents);

            intent = new Intent(android.provider.MediaStore.ACTION_VIDEO_CAPTURE);
            addIntentActivitiesToList(intent, items, aIntents);

            AddFilePickingActivities(items, "*/*", aIntents);
        }

        return items.toArray(new PromptService.PromptListItem[] {});
    }

    private String getFilePickerTitle(String aMimeType) {
        if (aMimeType.equals("audio/*")) {
            return getString(R.string.filepicker_audio_title);
        } else if (aMimeType.equals("image/*")) {
            return getString(R.string.filepicker_image_title);
        } else if (aMimeType.equals("video/*")) {
            return getString(R.string.filepicker_video_title);
        } else {
            return getString(R.string.filepicker_title);
        }
    }

    private Intent getFilePickerIntent(String aMimeType) {
        ArrayList<Intent> intents = new ArrayList<Intent>();
        PromptService.PromptListItem[] items = getItemsAndIntentsForFilePicker(aMimeType, intents);

        if (intents.size() == 0) {
            Log.i(LOGTAG, "no activities for the file picker!");
            return null;
        }

        if (intents.size() == 1)
            return intents.get(0);

        GeckoAppShell.getHandler().post(new FilePickerPromptRunnable(getFilePickerTitle(aMimeType), items));

        String promptServiceResult = "";
        try {
            promptServiceResult = PromptService.waitForReturn();
        } catch (InterruptedException e) {
            Log.e(LOGTAG, "showing prompt failed: ",  e);
            return null;
        }

        int itemId = -1;
        try {
            itemId = new JSONObject(promptServiceResult).getInt("button");

            if (itemId == -1) {
                return null;
            }
        } catch (org.json.JSONException e) {
            Log.e(LOGTAG, "result from promptservice was invalid: ", e);
            return null;
        }

        return intents.get(itemId);
    }

    private String mImageFilePath = "";
    private SynchronousQueue<String> mFilePickerResult = new SynchronousQueue<String>();

    public boolean showFilePicker(String aMimeType, ActivityResultHandler handler) {
        Intent intent = getFilePickerIntent(aMimeType);

        if (intent == null) {
            return false;
        }
        startActivityForResult(intent, mActivityResultHandlerMap.put(handler));
        return true;
    }

    public String showFilePicker(String aMimeType) {
        Intent intent = getFilePickerIntent(aMimeType);

        if (intent == null) {
            return "";
        }

        if (intent.getAction().equals(android.provider.MediaStore.ACTION_IMAGE_CAPTURE)) {
            startActivityForResult(intent, mActivityResultHandlerMap.put(mCameraImageResultHandler));
        } else if (intent.getAction().equals(android.provider.MediaStore.ACTION_VIDEO_CAPTURE)) {
            startActivityForResult(intent, mActivityResultHandlerMap.put(mCameraVideoResultHandler));
        } else if (intent.getAction().equals(Intent.ACTION_GET_CONTENT)) {
            startActivityForResult(intent, mActivityResultHandlerMap.put(mFilePickerResultHandlerSync));
        } else {
            Log.e(LOGTAG, "We should not get an intent with another action!");
            return "";
        }

        String filePickerResult = "";

        try {
            while (null == (filePickerResult = mFilePickerResult.poll(1, TimeUnit.MILLISECONDS))) {
                GeckoAppShell.processNextNativeEvent();
            }
        } catch (InterruptedException e) {
            Log.e(LOGTAG, "showing file picker failed: ",  e);
        }

        return filePickerResult;
    }

    @Override
    public boolean onSearchRequested() {
        return showAwesomebar(AwesomeBar.Type.EDIT);
    }

    public boolean showAwesomebar(AwesomeBar.Type aType) {
        Intent intent = new Intent(getBaseContext(), AwesomeBar.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION | Intent.FLAG_ACTIVITY_NO_HISTORY);
        intent.putExtra(AwesomeBar.TYPE_KEY, aType.name());

        if (aType != AwesomeBar.Type.ADD) {
            // if we're not adding a new tab, show the old url
            Tab tab = Tabs.getInstance().getSelectedTab();
            if (tab != null) {
                String url = tab.getURL();
                if (url != null) {
                    intent.putExtra(AwesomeBar.CURRENT_URL_KEY, url);
                }
            }
        }
        startActivityForResult(intent, mActivityResultHandlerMap.put(mAwesomebarResultHandler));
        return true;
    }

    @Override
    public void onBackPressed() {
        if (mDoorHangerPopup.isShowing()) {
            mDoorHangerPopup.dismiss();
            return;
        }

        SiteIdentityPopup identityPopup = SiteIdentityPopup.getInstance();
        if (identityPopup.isShowing()) {
            identityPopup.dismiss();
            return;
        }

        if (mDOMFullScreen) {
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

    static int kCaptureIndex = 0;

    public interface ActivityResultHandler {
        public void onActivityResult(int resultCode, Intent data);
    }

    class ActivityResultHandlerMap {
        private Map<Integer, ActivityResultHandler> mMap = new HashMap<Integer, ActivityResultHandler>();
        private int mCounter = 0;

        synchronized int put(ActivityResultHandler handler) {
            mMap.put(mCounter, handler);
            return mCounter++;
        }

        synchronized ActivityResultHandler getAndRemove(int i) {
            return mMap.remove(i);
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode,
                                    Intent data) {
        ActivityResultHandler handler = mActivityResultHandlerMap.getAndRemove(requestCode);
        if (handler != null)
            handler.onActivityResult(resultCode, data);
        else
            super.onActivityResult(requestCode, resultCode, data);
    }

    static abstract class FilePickerResultHandler implements ActivityResultHandler {
        String handleActivityResult(int resultCode, Intent data) {
            if (data == null && resultCode != RESULT_OK)
                return "";
            Uri uri = data.getData();
            if ("file".equals(uri.getScheme())) {
                String path = uri.getPath();
                return path == null ? "" : path;
            }
            try {
                ContentResolver cr = GeckoApp.mAppContext.getContentResolver();
                Cursor cursor = cr.query(uri, new String[] { OpenableColumns.DISPLAY_NAME },
                                         null, null, null);
                String name = null;
                if (cursor != null) {
                    try {
                        if (cursor.moveToNext()) {
                            name = cursor.getString(0);
                        }
                    } finally {
                        cursor.close();
                    }
                }
                String fileName = "tmp_";
                String fileExt = null;
                int period;
                if (name == null || (period = name.lastIndexOf('.')) == -1) {
                    String mimeType = cr.getType(uri);
                    fileExt = "." + GeckoAppShell.getExtensionFromMimeType(mimeType);
                } else {
                    fileExt = name.substring(period);
                    fileName = name.substring(0, period);
                }
                File file = File.createTempFile(fileName, fileExt, GeckoAppShell.getGREDir(GeckoApp.mAppContext));
                FileOutputStream fos = new FileOutputStream(file);
                InputStream is = cr.openInputStream(uri);
                byte[] buf = new byte[4096];
                int len = is.read(buf);
                while (len != -1) {
                    fos.write(buf, 0, len);
                    len = is.read(buf);
                }
                fos.close();
                String path = file.getAbsolutePath();
                return path == null ? "" : path;
            } catch (Exception e) {
                Log.e(LOGTAG, "showing file picker", e);
            }
            return "";
        }
    }

    class FilePickerResultHandlerSync extends FilePickerResultHandler {
        public void onActivityResult(int resultCode, Intent data) {
            try {
                mFilePickerResult.put(handleActivityResult(resultCode, data));
            } catch (InterruptedException e) {
                Log.i(LOGTAG, "error returning file picker result", e);
            }

        }
    }

    class AwesomebarResultHandler implements ActivityResultHandler {
        public void onActivityResult(int resultCode, Intent data) {
            if (data != null) {
                String url = data.getStringExtra(AwesomeBar.URL_KEY);
                AwesomeBar.Type type = AwesomeBar.Type.valueOf(data.getStringExtra(AwesomeBar.TYPE_KEY));
                String searchEngine = data.getStringExtra(AwesomeBar.SEARCH_KEY);
                boolean userEntered = data.getBooleanExtra(AwesomeBar.USER_ENTERED_KEY, false);
                if (url != null && url.length() > 0)
                    loadRequest(url, type, searchEngine, userEntered);
            }
        }
    }

    class CameraImageResultHandler implements ActivityResultHandler {
        public void onActivityResult(int resultCode, Intent data) {            
            try {
                if (resultCode != Activity.RESULT_OK) {
                    mFilePickerResult.put("");
                    return;
                }

                File file = new File(Environment.getExternalStorageDirectory(), mImageFilePath);
                mImageFilePath = "";
                mFilePickerResult.put(file.getAbsolutePath());
            } catch (InterruptedException e) {
                Log.i(LOGTAG, "error returning file picker result", e);
            }
        }
    }

    class CameraVideoResultHandler implements ActivityResultHandler {
        public void onActivityResult(int resultCode, Intent data) {
            try {
                if (data == null || resultCode != Activity.RESULT_OK) {
                    mFilePickerResult.put("");
                    return;
                }

                Cursor cursor = managedQuery(data.getData(),
                                             new String[] { MediaStore.Video.Media.DATA },
                                             null,
                                             null,
                                             null);
                cursor.moveToFirst();
                mFilePickerResult.put(cursor.getString(cursor.getColumnIndexOrThrow(MediaStore.Video.Media.DATA)));
            } catch (InterruptedException e) {
                Log.i(LOGTAG, "error returning file picker result", e);
            }

        }
    }

    // If searchEngine is provided, url will be used as the search query.
    // Otherwise, the url is loaded.
    private void loadRequest(String url, AwesomeBar.Type type, String searchEngine, boolean userEntered) {
        mBrowserToolbar.setTitle(url);
        Log.d(LOGTAG, type.name());
        JSONObject args = new JSONObject();
        try {
            args.put("url", url);
            args.put("engine", searchEngine);
            args.put("userEntered", userEntered);
        } catch (Exception e) {
            Log.e(LOGTAG, "error building JSON arguments");
        }
        if (type == AwesomeBar.Type.ADD) {
            Log.d(LOGTAG, "Sending message to Gecko: " + SystemClock.uptimeMillis() + " - Tab:Add");
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Tab:Add", args.toString()));
        } else {
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Tab:Load", args.toString()));
        }
    }

    public void loadUrl(String url, AwesomeBar.Type type) {
        loadRequest(url, type, null, false);
    }

    /**
     * Open the url as a new tab, and mark the selected tab as its "parent".
     * If the url is already open in a tab, the existing tab is selected.
     * Use this for tabs opened by the browser chrome, so users can press the
     * "Back" button to return to the previous tab.
     */
    public void loadUrlInTab(String url) {
        ArrayList<Tab> tabs = Tabs.getInstance().getTabsInOrder();
        if (tabs != null) {
            Iterator<Tab> tabsIter = tabs.iterator();
            while (tabsIter.hasNext()) {
                Tab tab = tabsIter.next();
                if (url.equals(tab.getURL())) {
                    Tabs.getInstance().selectTab(tab.getId());
                    return;
                }
            }
        }

        JSONObject args = new JSONObject();
        try {
            args.put("url", url);
            args.put("parentId", Tabs.getInstance().getSelectedTab().getId());
        } catch (Exception e) {
            Log.e(LOGTAG, "error building JSON arguments");
        }
        Log.i(LOGTAG, "Sending message to Gecko: " + SystemClock.uptimeMillis() + " - Tab:Add");
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Tab:Add", args.toString()));
    }

    /* This method is referenced by Robocop via reflection. */
    public GeckoLayerClient getLayerClient() { return mLayerClient; }
    public LayerController getLayerController() { return mLayerController; }

    public AbsoluteLayout getPluginContainer() { return mPluginContainer; }

    // accelerometer
    public void onAccuracyChanged(Sensor sensor, int accuracy) {}

    public void onSensorChanged(SensorEvent event)
    {
        GeckoAppShell.sendEventToGecko(GeckoEvent.createSensorEvent(event));
    }

    // geolocation
    public void onLocationChanged(Location location)
    {
        Log.w(LOGTAG, "onLocationChanged "+location);
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


    private void connectGeckoLayerClient() {
        LayerController layerController = getLayerController();
        layerController.setLayerClient(mLayerClient);

        layerController.getView().getTouchEventHandler().setOnTouchListener(new View.OnTouchListener() {
            public boolean onTouch(View view, MotionEvent event) {
                if (event == null)
                    return true;
                GeckoAppShell.sendEventToGecko(GeckoEvent.createMotionEvent(event));
                return true;
            }
        });
    }

    public boolean linkerExtract() {
        return false;
    }
}
