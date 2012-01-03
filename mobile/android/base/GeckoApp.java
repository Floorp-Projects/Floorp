/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Matt Brubeck <mbrubeck@mozilla.com>
 *   Vivien Nicolas <vnicolas@mozilla.com>
 *   Sriram Ramasubramanian <sriram@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.FloatSize;
import org.mozilla.gecko.gfx.GeckoSoftwareLayerClient;
import org.mozilla.gecko.gfx.IntSize;
import org.mozilla.gecko.gfx.LayerController;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.gfx.PlaceholderLayerClient;
import org.mozilla.gecko.gfx.RectUtils;
import org.mozilla.gecko.gfx.ViewportMetrics;
import org.mozilla.gecko.Tab.HistoryEntry;

import java.io.*;
import java.util.*;
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

import android.util.*;
import android.net.*;
import android.database.*;
import android.database.sqlite.*;
import android.provider.*;
import android.content.pm.*;
import android.content.pm.PackageManager.*;
import dalvik.system.*;

abstract public class GeckoApp
    extends Activity implements GeckoEventListener, SensorEventListener, LocationListener
{
    private static final String LOGTAG = "GeckoApp";

    public static final String ACTION_ALERT_CLICK   = "org.mozilla.gecko.ACTION_ALERT_CLICK";
    public static final String ACTION_ALERT_CLEAR   = "org.mozilla.gecko.ACTION_ALERT_CLEAR";
    public static final String ACTION_WEBAPP        = "org.mozilla.gecko.WEBAPP";
    public static final String ACTION_DEBUG         = "org.mozilla.gecko.DEBUG";
    public static final String ACTION_BOOKMARK      = "org.mozilla.gecko.BOOKMARK";
    public static final String SAVED_STATE_URI      = "uri";
    public static final String SAVED_STATE_TITLE    = "title";
    public static final String SAVED_STATE_VIEWPORT = "viewport";
    public static final String SAVED_STATE_SCREEN   = "screen";

    private LinearLayout mMainLayout;
    private RelativeLayout mGeckoLayout;
    public static SurfaceView cameraView;
    public static GeckoApp mAppContext;
    public static boolean mFullScreen = false;
    public static File sGREDir = null;
    public static Menu sMenu;
    public Handler mMainHandler;
    private File mProfileDir;
    private static boolean sIsGeckoReady = false;
    private static int mOrientation;

    private IntentFilter mConnectivityFilter;
    private IntentFilter mBatteryFilter;

    private BroadcastReceiver mConnectivityReceiver;
    private BroadcastReceiver mSmsReceiver;
    private BroadcastReceiver mBatteryReceiver;

    public static BrowserToolbar mBrowserToolbar;
    public static DoorHangerPopup mDoorHangerPopup;
    public static AutoCompletePopup mAutoCompletePopup;
    public Favicons mFavicons;

    private Geocoder mGeocoder;
    private Address  mLastGeoAddress;
    private static LayerController mLayerController;
    private static PlaceholderLayerClient mPlaceholderLayerClient;
    private static GeckoSoftwareLayerClient mSoftwareLayerClient;
    private AboutHomeContent mAboutHomeContent;
    private static AbsoluteLayout mPluginContainer;
    boolean mUserDefinedProfile = false;

    public String mLastUri;
    public String mLastTitle;
    public String mLastViewport;
    public byte[] mLastScreen;
    public int mOwnActivityDepth = 0;

    private Vector<View> mPluginViews = new Vector<View>();

    public interface OnTabsChangedListener {
        public void onTabsChanged(Tab tab);
    }
    
    private static ArrayList<OnTabsChangedListener> mTabsChangedListeners;

    static class ExtraMenuItem implements MenuItem.OnMenuItemClickListener {
        String label;
        String icon;
        int id;
        public boolean onMenuItemClick(MenuItem item) {
            Log.i(LOGTAG, "menu item clicked");
            GeckoAppShell.sendEventToGecko(new GeckoEvent("Menu:Clicked", Integer.toString(id)));
            return true;
        }
    }

    static Vector<ExtraMenuItem> sExtraMenuItems = new Vector<ExtraMenuItem>();

    public enum LaunchState {Launching, WaitForDebugger,
                             Launched, GeckoRunning, GeckoExiting};
    private static LaunchState sLaunchState = LaunchState.Launching;
    private static boolean sTryCatchAttached = false;

    private static final int FILE_PICKER_REQUEST = 1;
    private static final int AWESOMEBAR_REQUEST = 2;
    private static final int CAMERA_CAPTURE_REQUEST = 3;

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

    void showErrorDialog(String message)
    {
        new AlertDialog.Builder(this)
            .setMessage(message)
            .setCancelable(false)
            .setPositiveButton(R.string.exit_label,
                               new DialogInterface.OnClickListener() {
                                   public void onClick(DialogInterface dialog,
                                                       int id)
                                   {
                                       GeckoApp.this.finish();
                                       System.exit(0);
                                   }
                               }).show();
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
        // we don't support Honeycomb and later
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB)
            return new String[0];

        Log.w(LOGTAG, "zerdatime " + new Date().getTime() + " - start of getPluginDirectories");

        ArrayList<String> directories = new ArrayList<String>();
        PackageManager pm = this.mAppContext.getPackageManager();
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
        Log.w(LOGTAG, "zerdatime " + new Date().getTime() + " - end of getPluginDirectories");
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
        Context pluginContext = this.mAppContext.createPackageContext(packageName,
                Context.CONTEXT_INCLUDE_CODE |
                Context.CONTEXT_IGNORE_SECURITY);
        ClassLoader pluginCL = pluginContext.getClassLoader();
        return pluginCL.loadClass(className);
    }

    // Returns true when the intent is going to be handled by gecko launch
    boolean launch(Intent intent)
    {
        Log.w(LOGTAG, "zerdatime " + new Date().getTime() + " - launch");
        
        if (!checkAndSetLaunchState(LaunchState.Launching, LaunchState.Launched))
            return false;

        if (intent == null)
            intent = getIntent();

        String args = intent.getStringExtra("args");
        if (args != null && args.contains("-profile")) {
            // XXX: TO-DO set mProfileDir to the path passed in
            mUserDefinedProfile = true;
        }

        prefetchDNS(intent.getData());
        new GeckoThread(intent, mLastUri, mLastTitle).start();

        return true;
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu)
    {
        sMenu = menu;
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.layout.gecko_menu, menu);
        return true;
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu aMenu)
    {
        Iterator<ExtraMenuItem> i = sExtraMenuItems.iterator();
        while (i.hasNext()) {
            final ExtraMenuItem item = i.next();
            if (aMenu.findItem(item.id) == null) {
                final MenuItem mi = aMenu.add(aMenu.NONE, item.id, aMenu.NONE, item.label);
                if (item.icon != null) {
                    if (item.icon.startsWith("data")) {
                        byte[] raw = Base64.decode(item.icon.substring(22), Base64.DEFAULT);
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
                                } catch(Exception e) {
                                    Log.e(LOGTAG, "onPrepareOptionsMenu: Unable to set icon", e);
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
        MenuItem agentMode = aMenu.findItem(R.id.agent_mode);
        MenuItem saveAsPDF = aMenu.findItem(R.id.save_as_pdf);

        if (tab == null) {
            bookmark.setEnabled(false);
            forward.setEnabled(false);
            share.setEnabled(false);
            saveAsPDF.setEnabled(false);
            agentMode.setEnabled(false);
            return true;
        }
        
        bookmark.setEnabled(true);
        bookmark.setCheckable(true);
        
        if (tab.isBookmark()) {
            bookmark.setChecked(true);
            bookmark.setIcon(R.drawable.ic_menu_bookmark_remove);
            bookmark.setTitle(R.string.bookmark_remove);
        } else {
            bookmark.setChecked(false);
            bookmark.setIcon(R.drawable.ic_menu_bookmark_add);
            bookmark.setTitle(R.string.bookmark_add);
        }

        forward.setEnabled(tab.canDoForward());

        // Disable share and agentMode menuitems for about:, chrome: and file: URIs
        String scheme = Uri.parse(tab.getURL()).getScheme();
        boolean enabled = !(scheme.equals("about") || scheme.equals("chrome") ||
                            scheme.equals("file"));
        share.setEnabled(enabled);
        agentMode.setEnabled(enabled);

        // Disable save as PDF for about:home and xul pages
        saveAsPDF.setEnabled(!(tab.getURL().equals("about:home") ||
                               tab.getContentType().equals("application/vnd.mozilla.xul+xml")));

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
                            new GeckoEvent("Browser:Quit", null));
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
                        item.setTitle(R.string.bookmark_add);
                    } else {
                        tab.addBookmark();
                        Toast.makeText(this, R.string.bookmark_added, Toast.LENGTH_SHORT).show();
                        item.setIcon(R.drawable.ic_menu_bookmark_remove);
                        item.setTitle(R.string.bookmark_remove);
                    }
                }
                return true;
            case R.id.share:
                tab = Tabs.getInstance().getSelectedTab();
                if (tab != null) {
                  GeckoAppShell.openUriExternal(tab.getURL(), "text/plain", "", "",
                                                Intent.ACTION_SEND, tab.getTitle());
                }
                return true;
            case R.id.reload:
                doReload();
                return true;
            case R.id.forward:
                doForward();
                return true;
            case R.id.save_as_pdf:
                GeckoAppShell.sendEventToGecko(new GeckoEvent("SaveAs:PDF", null));
                return true;
            case R.id.settings:
                intent = new Intent(this, GeckoPreferences.class);
                startActivity(intent);
                return true;
            case R.id.site_settings:
                GeckoAppShell.sendEventToGecko(new GeckoEvent("Permissions:Get", null));
                return true;
            case R.id.addons:
                loadUrlInNewTab("about:addons");
                return true;
            case R.id.agent_mode:
                Tab selectedTab = Tabs.getInstance().getSelectedTab();
                if (selectedTab == null)
                    return true;
                JSONObject args = new JSONObject();
                try {
                    args.put("agent", selectedTab.getAgentMode() == Tab.AgentMode.MOBILE ? "desktop" : "mobile");
                    args.put("tabId", selectedTab.getId());
                } catch (JSONException e) {
                    Log.e(LOGTAG, "error building json arguments");
                }
                GeckoAppShell.sendEventToGecko(new GeckoEvent("AgentMode:Change", args.toString()));
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    public String getLastViewport() {
        return mLastViewport;
    }

    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        if (mOwnActivityDepth > 0)
            return; // we're showing one of our own activities and likely won't get paged out

        if (outState == null)
            outState = new Bundle();

        new SessionSnapshotRunnable(null).run();

        outState.putString(SAVED_STATE_URI, mLastUri);
        outState.putString(SAVED_STATE_TITLE, mLastTitle);
        outState.putString(SAVED_STATE_VIEWPORT, mLastViewport);
        outState.putByteArray(SAVED_STATE_SCREEN, mLastScreen);
    }

    public class SessionSnapshotRunnable implements Runnable {
        Tab mThumbnailTab;
        SessionSnapshotRunnable(Tab thumbnailTab) {
            mThumbnailTab = thumbnailTab;
        }

        public void run() {
            synchronized (mSoftwareLayerClient) {
                Tab tab = Tabs.getInstance().getSelectedTab();
                if (tab == null)
                    return;

                HistoryEntry lastHistoryEntry = tab.getLastHistoryEntry();
                if (lastHistoryEntry == null)
                    return;

                if (getLayerController().getLayerClient() != mSoftwareLayerClient)
                    return;

                if (lastHistoryEntry.mUri.equals(mLastUri))
                    return;
   
                mLastViewport = mSoftwareLayerClient.getGeckoViewportMetrics().toJSON();
                mLastUri = lastHistoryEntry.mUri;
                mLastTitle = lastHistoryEntry.mTitle;
                Bitmap bitmap = mSoftwareLayerClient.getBitmap();
                if (bitmap != null) {
                    ByteArrayOutputStream bos = new ByteArrayOutputStream();
                    bitmap.compress(Bitmap.CompressFormat.PNG, 0, bos);
                    mLastScreen = bos.toByteArray();

                    // Make a thumbnail for the given tab, if it's still selected
                    // NOTE: bitmap is recycled in updateThumbnail
                    if (tab == mThumbnailTab) {
                        if (mThumbnailTab.getURL().equals("about:home"))
                            mThumbnailTab.updateThumbnail(null);
                        else
                            mThumbnailTab.updateThumbnail(bitmap);
                    }
                } else {
                    mLastScreen = null;
                }
            }
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

                onTabsChanged(tab);
            }
        });

        tab.setFaviconLoadId(id);
    }

    void handleLocationChange(final int tabId, final String uri,
                              final String documentURI, final String contentType) {
        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        if (Tabs.getInstance().isSelectedTab(tab)) {
            if (uri.equals("about:home"))
                showAboutHome();
            else 
                hideAboutHome();
        }
        
        String oldBaseURI = tab.getURL();
        tab.updateURL(uri);
        tab.setDocumentURI(documentURI);
        tab.setContentType(contentType);

        String baseURI = uri;
        if (baseURI.indexOf('#') != -1)
            baseURI = uri.substring(0, uri.indexOf('#'));

        if (oldBaseURI != null && oldBaseURI.indexOf('#') != -1)
            oldBaseURI = oldBaseURI.substring(0, oldBaseURI.indexOf('#'));
        
        if (baseURI.equals(oldBaseURI)) {
            mMainHandler.post(new Runnable() {
                public void run() {
                    if (Tabs.getInstance().isSelectedTab(tab)) {
                        mBrowserToolbar.setTitle(uri);
                    }
                }
            });
            return;
        }

        tab.updateFavicon(null);
        tab.updateFaviconURL(null);
        tab.updateSecurityMode("unknown");
        tab.removeTransientDoorHangers();

        maybeCancelFaviconLoad(tab);

        mMainHandler.post(new Runnable() {
            public void run() {
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    mBrowserToolbar.setTitle(uri);
                    mBrowserToolbar.setFavicon(null);
                    mBrowserToolbar.setSecurityMode("unknown");
                    mDoorHangerPopup.updatePopup();
                    mBrowserToolbar.setShadowVisibility(!(tab.getURL().startsWith("about:")));
                }
            }
        });
    }

    void handleSecurityChange(final int tabId, final String mode) {
        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        tab.updateSecurityMode(mode);
        
        mMainHandler.post(new Runnable() { 
            public void run() {
                if (Tabs.getInstance().isSelectedTab(tab))
                    mBrowserToolbar.setSecurityMode(mode);
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
                    mBrowserToolbar.setProgressVisibility(tab.isLoading());
                }
            }
        });
    }

    public File getProfileDir() {
        // XXX: TO-DO read profiles.ini to get the default profile
        return getProfileDir("default");
    }

    public File getProfileDir(final String profileName) {
        if (mProfileDir == null && !mUserDefinedProfile) {
            File mozDir = new File(GeckoAppShell.sHomeDir, "mozilla");
            if (!mozDir.exists()) {
                // Profile directory may have been deleted or not created yet.
                return null;
            }
            File[] profiles = mozDir.listFiles(new FileFilter() {
                public boolean accept(File pathname) {
                    return pathname.getName().endsWith("." + profileName);
                }
            });
            if (profiles != null && profiles.length == 1)
                mProfileDir = profiles[0];
        }
        return mProfileDir;
    }

    void addTab() {
        showAwesomebar(AwesomeBar.Type.ADD);
    }

    void showTabs() {
        Intent intent = new Intent(mAppContext, TabsTray.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NO_HISTORY);
        startActivity(intent);
        overridePendingTransition(R.anim.grow_fade_in, 0);
    }

    public static void registerOnTabsChangedListener(OnTabsChangedListener listener) {
        if (mTabsChangedListeners == null)
            mTabsChangedListeners = new ArrayList<OnTabsChangedListener>();
        
        mTabsChangedListeners.add(listener);
    }

    public static void unregisterOnTabsChangedListener(OnTabsChangedListener listener) {
        if (mTabsChangedListeners == null)
            return;
        
        mTabsChangedListeners.remove(listener);
    }

    public void onTabsChanged(Tab tab) {
        if (mTabsChangedListeners == null)
            return;

        Iterator items = mTabsChangedListeners.iterator();
        while (items.hasNext()) {
            ((OnTabsChangedListener) items.next()).onTabsChanged(tab);
        }
    }

    public void handleMessage(String event, JSONObject message) {
        Log.i(LOGTAG, "Got message: " + event);
        try {
            if (event.equals("Menu:Add")) {
                String name = message.getString("name");
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
                        MenuItem menu = sMenu.findItem(id);
                        if (menu != null)
                            sMenu.removeItem(id);
                        return;
                    }
                }
            } else if (event.equals("Toast:Show")) {
                final String msg = message.getString("message");
                final String duration = message.getString("duration");
                handleShowToast(msg, duration);
            } else if (event.equals("DOMContentLoaded")) {
                final int tabId = message.getInt("tabID");
                final String uri = message.getString("uri");
                final String title = message.getString("title");
                final CharSequence titleText = title;
                handleContentLoaded(tabId, uri, title);
                Log.i(LOGTAG, "URI - " + uri + ", title - " + title);
            } else if (event.equals("DOMTitleChanged")) {
                final int tabId = message.getInt("tabID");
                final String title = message.getString("title");
                final CharSequence titleText = title;
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
                Log.i(LOGTAG, "URI - " + uri);
                handleLocationChange(tabId, uri, documentURI, contentType);
            } else if (event.equals("Content:SecurityChange")) {
                final int tabId = message.getInt("tabID");
                final String mode = message.getString("mode");
                Log.i(LOGTAG, "Security Mode - " + mode);
                handleSecurityChange(tabId, mode);
            } else if (event.equals("Content:StateChange")) {
                final int tabId = message.getInt("tabID");
                int state = message.getInt("state");
                Log.i(LOGTAG, "State - " + state);
                if ((state & GeckoAppShell.WPL_STATE_IS_NETWORK) != 0) {
                    if ((state & GeckoAppShell.WPL_STATE_START) != 0) {
                        Log.i(LOGTAG, "Got a document start");
                        handleDocumentStart(tabId);
                    } else if ((state & GeckoAppShell.WPL_STATE_STOP) != 0) {
                        Log.i(LOGTAG, "Got a document stop");
                        handleDocumentStop(tabId);
                    }
                }
            } else if (event.equals("Content:LoadError")) {
                final int tabId = message.getInt("tabID");
                final String uri = message.getString("uri");
                final String title = message.getString("title");
                handleLoadError(tabId, uri, title);
            } else if (event.equals("onCameraCapture")) {
                //GeckoApp.mAppContext.doCameraCapture(message.getString("path"));
                doCameraCapture();
            } else if (event.equals("Tab:Added")) {
                Log.i(LOGTAG, "Created a new tab");
                Tab tab = handleAddTab(message);
                Boolean selected = message.getBoolean("selected");
                if (selected)
                    handleSelectTab(tab.getId());
            } else if (event.equals("Tab:Closed")) {
                Log.i(LOGTAG, "Destroyed a tab");
                int tabId = message.getInt("tabID");
                handleCloseTab(tabId);
            } else if (event.equals("Tab:Selected")) {
                int tabId = message.getInt("tabID");
                Log.i(LOGTAG, "Switched to tab: " + tabId);
                handleSelectTab(tabId);
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
                        mBrowserToolbar.setVisibility(View.GONE);
                    }
                });
            } else if (event.equals("ToggleChrome:Show")) {
                mMainHandler.post(new Runnable() {
                    public void run() {
                        mBrowserToolbar.setVisibility(View.VISIBLE);
                    }
                });
            } else if (event.equals("FormAssist:AutoComplete")) {
                final JSONArray suggestions = message.getJSONArray("suggestions");
                if (suggestions.length() == 0) {
                    mMainHandler.post(new Runnable() {
                        public void run() {
                            mAutoCompletePopup.hide();
                        }
                    });
                } else {
                    final JSONArray rect = message.getJSONArray("rect");
                    final double zoom = message.getDouble("zoom");
                    mMainHandler.post(new Runnable() {
                        public void run() {
                            mAutoCompletePopup.show(suggestions, rect, zoom);
                        }
                    });
                }
            } else if (event.equals("AgentMode:Changed")) {
                Tab.AgentMode agentMode = message.getString("agentMode").equals("mobile") ? Tab.AgentMode.MOBILE : Tab.AgentMode.DESKTOP;
                int tabId = message.getInt("tabId");
                Tab tab = Tabs.getInstance().getTab(tabId);
                if (tab == null)
                    return;

                tab.setAgentMode(agentMode);
                if (tab == Tabs.getInstance().getSelectedTab())
                    updateAgentModeMenuItem(tab, agentMode);
            } else if (event.equals("Permissions:Data")) {
                String host = message.getString("host");
                JSONArray permissions = message.getJSONArray("permissions");
                showSiteSettingsDialog(host, permissions);
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
            mAutoCompletePopup.hide();
            if (mAboutHomeContent == null && mShow) {
                mAboutHomeContent = new AboutHomeContent(GeckoApp.mAppContext, null);
                mAboutHomeContent.init(GeckoApp.mAppContext);
                mAboutHomeContent.setUriLoadCallback(new AboutHomeContent.UriLoadCallback() {
                    public void callback(String url) {
                        mBrowserToolbar.setProgressVisibility(true);
                        loadUrl(url, AwesomeBar.Type.EDIT);
                    }
                });
                RelativeLayout.LayoutParams lp = 
                    new RelativeLayout.LayoutParams(LayoutParams.FILL_PARENT, 
                                                    LayoutParams.FILL_PARENT);
                mGeckoLayout.addView(mAboutHomeContent, lp);
            }
            if (mAboutHomeContent != null)
                mAboutHomeContent.setVisibility(mShow ? View.VISIBLE : View.GONE);
        }
    }

    void updateAgentModeMenuItem(final Tab tab, final Tab.AgentMode agentMode) {
        if (sMenu == null)
            return;

        mMainHandler.post(new Runnable() {
            public void run() {
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    int strId = agentMode == Tab.AgentMode.MOBILE ? R.string.agent_request_desktop : R.string.agent_request_mobile;
                    sMenu.findItem(R.id.agent_mode).setTitle(getString(strId));
                }
            }
        });
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
                    GeckoAppShell.sendEventToGecko(new GeckoEvent("Permissions:Clear", permissionsToClear.toString()));
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
                mAppContext.mDoorHangerPopup.addDoorHanger(message, value, buttons,
                                                           tab, options);
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

    Tab handleAddTab(JSONObject params) throws JSONException {
        Log.i(LOGTAG, params.toString());
        final Tab tab = Tabs.getInstance().addTab(params);

        mMainHandler.post(new Runnable() {
            public void run() {
                mBrowserToolbar.updateTabs(Tabs.getInstance().getCount());
            }
        });

        return tab;
    }

    void handleCloseTab(final int tabId) {
        final Tab tab = Tabs.getInstance().getTab(tabId);
        Tabs.getInstance().removeTab(tabId);
        tab.removeAllDoorHangers();

        mMainHandler.post(new Runnable() { 
            public void run() {
                onTabsChanged(tab);
                mBrowserToolbar.updateTabs(Tabs.getInstance().getCount());
                mDoorHangerPopup.updatePopup();
            }
        });
    }

    void handleSelectTab(int tabId) {
        final Tab tab = Tabs.getInstance().selectTab(tabId);
        if (tab == null)
            return;

        if (tab.getURL().equals("about:home"))
            showAboutHome();
        else
            hideAboutHome();

        updateAgentModeMenuItem(tab, tab.getAgentMode());

        mMainHandler.post(new Runnable() { 
            public void run() {
                mAutoCompletePopup.hide();
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    mBrowserToolbar.setTitle(tab.getDisplayTitle());
                    mBrowserToolbar.setFavicon(tab.getFavicon());
                    mBrowserToolbar.setSecurityMode(tab.getSecurityMode());
                    mBrowserToolbar.setProgressVisibility(tab.isLoading());
                    mDoorHangerPopup.updatePopup();
                    mBrowserToolbar.setShadowVisibility(!(tab.getURL().startsWith("about:")));
                }
            }
        });
    }

    void handleDocumentStart(int tabId) {
        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        tab.setLoading(true);
        tab.updateSecurityMode("unknown");

        mMainHandler.post(new Runnable() {
            public void run() {
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    mBrowserToolbar.setSecurityMode(tab.getSecurityMode());
                    mBrowserToolbar.setProgressVisibility(true);
                }
                onTabsChanged(tab);
            }
        });
    }

    void handleDocumentStop(int tabId) {
        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        tab.setLoading(false);

        mMainHandler.post(new Runnable() {
            public void run() {
                if (Tabs.getInstance().isSelectedTab(tab))
                    mBrowserToolbar.setProgressVisibility(false);
                onTabsChanged(tab);
            }
        });

        Runnable r = new SessionSnapshotRunnable(tab);
        GeckoAppShell.getHandler().postDelayed(r, 500);
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

    void handleContentLoaded(int tabId, String uri, String title) {
        final Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        tab.updateTitle(title);

        // Make the UI changes
        mMainHandler.post(new Runnable() {
            public void run() {
                loadFavicon(tab);

                if (Tabs.getInstance().isSelectedTab(tab))
                    mBrowserToolbar.setTitle(tab.getDisplayTitle());

                onTabsChanged(tab);
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
                onTabsChanged(tab);
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
                if (!tab.isLoading()) {
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

    void addPluginView(final View view,
                       final double x, final double y,
                       final double w, final double h) {
        mMainHandler.post(new Runnable() { 
            public void run() {
                PluginLayoutParams lp;

                ViewportMetrics geckoViewport = mSoftwareLayerClient.getGeckoViewportMetrics();

                if (mPluginContainer.indexOfChild(view) == -1) {
                    lp = PluginLayoutParams.create((int)x, (int)y, (int)w, (int)h, geckoViewport.getZoomFactor());

                    view.setWillNotDraw(false);
                    if (view instanceof SurfaceView) {
                        SurfaceView sview = (SurfaceView)view;

                        sview.setZOrderOnTop(false);
                        sview.setZOrderMediaOverlay(true);
                    }

                    mPluginContainer.addView(view, lp);
                    mPluginViews.add(view);
                } else {
                    lp = (PluginLayoutParams)view.getLayoutParams();
                    lp.reset((int)x, (int)y, (int)w, (int)h, geckoViewport.getZoomFactor());
                    try {
                        mPluginContainer.updateViewLayout(view, lp);
                    } catch (IllegalArgumentException e) {
                        Log.i(LOGTAG, "e:" + e);
                        // it can be the case where we
                        // get an update before the view
                        // is actually attached.
                    }
                }
            }
        });
    }

    void removePluginView(final View view) {
        mMainHandler.post(new Runnable() { 
            public void run() {
                try {
                    mPluginContainer.removeView(view);
                    mPluginViews.remove(view);
                } catch (Exception e) {}
            }
        });
    }

    public void hidePluginViews() {
        for (View view : mPluginViews) {
            view.setVisibility(View.GONE);
        }
    }

    public void showPluginViews() {
        repositionPluginViews(true);
    }

    public void repositionPluginViews(boolean setVisible) {
        ViewportMetrics hostViewport = mSoftwareLayerClient.getGeckoViewportMetrics();
        ViewportMetrics targetViewport = mLayerController.getViewportMetrics();

        if (hostViewport == null || targetViewport == null)
            return;

        for (View view : mPluginViews) {
            PluginLayoutParams lp = (PluginLayoutParams)view.getLayoutParams();
            lp.reposition(hostViewport, targetViewport);

            if (setVisible) {
                view.setVisibility(View.VISIBLE);
            }

            mPluginContainer.updateViewLayout(view, lp);
        }
    }

    public void setFullScreen(final boolean fullscreen) {
        mFullScreen = fullscreen;
        mMainHandler.post(new Runnable() { 
            public void run() {
                // Hide/show the system notification bar
                getWindow().setFlags(fullscreen ?
                                     WindowManager.LayoutParams.FLAG_FULLSCREEN : 0,
                                     WindowManager.LayoutParams.FLAG_FULLSCREEN);
            }
        });
    }

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        // StrictMode is set by defaults resource flag |enableStrictMode|.
        if (getResources().getBoolean(R.bool.enableStrictMode)) {
            enableStrictMode();
        }

        System.loadLibrary("mozutils");
        mMainHandler = new Handler();
        Log.w(LOGTAG, "zerdatime " + new Date().getTime() + " - onCreate");
        if (savedInstanceState != null) {
            mLastUri = savedInstanceState.getString(SAVED_STATE_URI);
            mLastTitle = savedInstanceState.getString(SAVED_STATE_TITLE);
            mLastViewport = savedInstanceState.getString(SAVED_STATE_VIEWPORT);
            mLastScreen = savedInstanceState.getByteArray(SAVED_STATE_SCREEN);
        }
        String uri = getIntent().getDataString();
        String title = uri;
        if (uri != null && uri.length() > 0) {
            mLastUri = uri;
            mLastTitle = title;
        }

        if (mLastUri == null || mLastUri.equals("") ||
            mLastUri.equals("about:home")) {
            showAboutHome();
        }

        super.onCreate(savedInstanceState);

        setContentView(R.layout.gecko_app);
        mAppContext = this;

        if (Build.VERSION.SDK_INT >= 11) {
            mBrowserToolbar = (BrowserToolbar) getLayoutInflater().inflate(R.layout.gecko_app_actionbar, null);

            GeckoActionBar.setBackgroundDrawable(this, getResources().getDrawable(R.drawable.gecko_actionbar_bg));
            GeckoActionBar.setDisplayOptions(this, ActionBar.DISPLAY_SHOW_CUSTOM, ActionBar.DISPLAY_SHOW_CUSTOM |
                                                                                  ActionBar.DISPLAY_SHOW_HOME |
                                                                                  ActionBar.DISPLAY_SHOW_TITLE |
                                                                                  ActionBar.DISPLAY_USE_LOGO);
            GeckoActionBar.setCustomView(this, mBrowserToolbar);
        } else {
            mBrowserToolbar = (BrowserToolbar) findViewById(R.id.browser_toolbar);
        }

        mFavicons = new Favicons(this);

        // setup gecko layout
        mGeckoLayout = (RelativeLayout) findViewById(R.id.gecko_layout);
        mMainLayout = (LinearLayout) findViewById(R.id.main_layout);

        mDoorHangerPopup = new DoorHangerPopup(this);
        mAutoCompletePopup = (AutoCompletePopup) findViewById(R.id.autocomplete_popup);

        Tabs tabs = Tabs.getInstance();
        Tab tab = tabs.getSelectedTab();
        if (tab != null) {
            mBrowserToolbar.setTitle(tab.getDisplayTitle());
            mBrowserToolbar.setFavicon(tab.getFavicon());
            mBrowserToolbar.setProgressVisibility(tab.isLoading());
            mBrowserToolbar.updateTabs(Tabs.getInstance().getCount()); 
        }

        tabs.setContentResolver(getContentResolver()); 

        if (cameraView == null) {
            cameraView = new SurfaceView(this);
            cameraView.getHolder().setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
        }

        if (mLayerController == null) {
            /*
             * Create a layer client so that Gecko will have a buffer to draw into, but don't hook
             * it up to the layer controller yet.
             */
            mSoftwareLayerClient = new GeckoSoftwareLayerClient(this);

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
            mPlaceholderLayerClient = mUserDefinedProfile ?  null :
                PlaceholderLayerClient.createInstance(this);
            if (mPlaceholderLayerClient != null) {
                mLayerController.setLayerClient(mPlaceholderLayerClient);
            }

            mGeckoLayout.addView(mLayerController.getView(), 0);
        }

        mPluginContainer = (AbsoluteLayout) findViewById(R.id.plugin_container);

        Log.w(LOGTAG, "zerdatime " + new Date().getTime() + " - UI almost up");

        if (sGREDir == null)
            sGREDir = new File(this.getApplicationInfo().dataDir);

        if (!sTryCatchAttached) {
            sTryCatchAttached = true;
            mMainHandler.post(new Runnable() {
                public void run() {
                    try {
                        Looper.loop();
                    } catch (Exception e) {
                        Log.e(LOGTAG, "top level exception", e);
                        StringWriter sw = new StringWriter();
                        PrintWriter pw = new PrintWriter(sw);
                        e.printStackTrace(pw);
                        pw.flush();
                        GeckoAppShell.reportJavaCrash(sw.toString());
                    }
                    // resetting this is kinda pointless, but oh well
                    sTryCatchAttached = false;
                }
            });
        }

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
        GeckoAppShell.registerGeckoEventListener("onCameraCapture", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Tab:Added", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Tab:Closed", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Tab:Selected", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Doorhanger:Add", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Doorhanger:Remove", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Menu:Add", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Menu:Remove", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Gecko:Ready", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Toast:Show", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("ToggleChrome:Hide", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("ToggleChrome:Show", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("AgentMode:Changed", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("FormAssist:AutoComplete", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Permissions:Data", GeckoApp.mAppContext);

        mConnectivityFilter = new IntentFilter();
        mConnectivityFilter.addAction(ConnectivityManager.CONNECTIVITY_ACTION);
        mConnectivityReceiver = new GeckoConnectivityReceiver();

        IntentFilter batteryFilter = new IntentFilter();
        batteryFilter.addAction(Intent.ACTION_BATTERY_CHANGED);
        mBatteryReceiver = new GeckoBatteryManager();
        registerReceiver(mBatteryReceiver, batteryFilter);

        IntentFilter smsFilter = new IntentFilter();
        smsFilter.addAction("android.provider.Telephony.SMS_RECEIVED");
        mSmsReceiver = new GeckoSmsManager();
        registerReceiver(mSmsReceiver, smsFilter);

        final GeckoApp self = this;
 
        mMainHandler.postDelayed(new Runnable() {
            public void run() {
                
                Log.w(LOGTAG, "zerdatime " + new Date().getTime() + " - pre checkLaunchState");

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

                // it would be good only to do this if MOZ_UPDATER was defined 
                long startTime = new Date().getTime();
                checkAndLaunchUpdate();
                Log.w(LOGTAG, "checking for an update took " + (new Date().getTime() - startTime) + "ms");
                checkMigrateProfile();
            }
        }, 50);

        mOrientation = getResources().getConfiguration().orientation;
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

    @Override
    protected void onNewIntent(Intent intent) {
        Log.w(LOGTAG, "zerdatime " + new Date().getTime() + " - onNewIntent");

        if (checkLaunchState(LaunchState.GeckoExiting)) {
            // We're exiting and shouldn't try to do anything else just incase
            // we're hung for some reason we'll force the process to exit
            System.exit(0);
            return;
        }
        final String action = intent.getAction();
        if (ACTION_DEBUG.equals(action) &&
            checkAndSetLaunchState(LaunchState.Launching, LaunchState.WaitForDebugger)) {

            mMainHandler.postDelayed(new Runnable() {
                public void run() {
                    Log.i(LOGTAG, "Launching from debug intent after 5s wait");
                    setLaunchState(LaunchState.Launching);
                    launch(getIntent());
                }
            }, 1000 * 5 /* 5 seconds */);
            Log.i(LOGTAG, "Intent : ACTION_DEBUG - waiting 5s before launching");
            return;
        }
        if (checkLaunchState(LaunchState.WaitForDebugger) || launch(intent))
            return;

        if (Intent.ACTION_MAIN.equals(action)) {
            Log.i(LOGTAG, "Intent : ACTION_MAIN");
            GeckoAppShell.sendEventToGecko(new GeckoEvent(""));
        }
        else if (Intent.ACTION_VIEW.equals(action)) {
            prefetchDNS(intent.getData());
            String uri = intent.getDataString();
            GeckoAppShell.sendEventToGecko(new GeckoEvent(uri));
            Log.i(LOGTAG,"onNewIntent: " + uri);
        }
        else if (ACTION_WEBAPP.equals(action)) {
            String uri = intent.getStringExtra("args");
            GeckoAppShell.sendEventToGecko(new GeckoEvent(uri));
            Log.i(LOGTAG,"Intent : WEBAPP - " + uri);
        }
        else if (ACTION_BOOKMARK.equals(action)) {
            String args = intent.getStringExtra("args");
            GeckoAppShell.sendEventToGecko(new GeckoEvent(args));
            Log.i(LOGTAG,"Intent : BOOKMARK - " + args);
        }
    }

    @Override
    public void onPause()
    {
        Log.i(LOGTAG, "pause");

        Runnable r = new SessionSnapshotRunnable(null);
        GeckoAppShell.getHandler().post(r);

        GeckoAppShell.sendEventToGecko(new GeckoEvent(GeckoEvent.ACTIVITY_PAUSING));
        // The user is navigating away from this activity, but nothing
        // has come to the foreground yet; for Gecko, we may want to
        // stop repainting, for example.

        // Whatever we do here should be fast, because we're blocking
        // the next activity from showing up until we finish.

        // onPause will be followed by either onResume or onStop.
        super.onPause();

        unregisterReceiver(mConnectivityReceiver);
    }

    @Override
    public void onResume()
    {
        Log.i(LOGTAG, "resume");
        if (checkLaunchState(LaunchState.GeckoRunning))
            GeckoAppShell.onResume();
        // After an onPause, the activity is back in the foreground.
        // Undo whatever we did in onPause.
        super.onResume();

        // Just in case. Normally we start in onNewIntent
        if (checkLaunchState(LaunchState.Launching))
            onNewIntent(getIntent());

        registerReceiver(mConnectivityReceiver, mConnectivityFilter);
        if (mOwnActivityDepth > 0)
            mOwnActivityDepth--;
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

        GeckoAppShell.sendEventToGecko(new GeckoEvent(GeckoEvent.ACTIVITY_STOPPING));
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
        Log.w(LOGTAG, "zerdatime " + new Date().getTime() + " - onStart");

        Log.i(LOGTAG, "start");
        GeckoAppShell.sendEventToGecko(new GeckoEvent(GeckoEvent.ACTIVITY_START));
        super.onStart();
    }

    @Override
    public void onDestroy()
    {
        Log.i(LOGTAG, "destroy");

        // Tell Gecko to shutting down; we'll end up calling System.exit()
        // in onXreExit.
        if (isFinishing())
            GeckoAppShell.sendEventToGecko(new GeckoEvent(GeckoEvent.ACTIVITY_SHUTDOWN));
        
        GeckoAppShell.unregisterGeckoEventListener("DOMContentLoaded", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("DOMTitleChanged", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("DOMLinkAdded", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("DOMWindowClose", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("log", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Content:LocationChange", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Content:SecurityChange", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Content:StateChange", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Content:LoadError", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("onCameraCapture", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Tab:Added", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Tab:Closed", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Tab:Selected", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Doorhanger:Add", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Menu:Add", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Menu:Remove", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Gecko:Ready", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Toast:Show", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("ToggleChrome:Hide", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("ToggleChrome:Show", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("AgentMode:Changed", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("FormAssist:AutoComplete", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Permissions:Data", GeckoApp.mAppContext);

        mFavicons.close();

        super.onDestroy();

        unregisterReceiver(mSmsReceiver);
        unregisterReceiver(mBatteryReceiver);
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
            mAutoCompletePopup.hide();

            if (Build.VERSION.SDK_INT >= 11) {
                mBrowserToolbar = (BrowserToolbar) getLayoutInflater().inflate(R.layout.gecko_app_actionbar, null);
                GeckoActionBar.setCustomView(mAppContext, mBrowserToolbar);
            }
        }
    }

    @Override
    public void onLowMemory()
    {
        Log.e(LOGTAG, "low memory");
        if (checkLaunchState(LaunchState.GeckoRunning))
            GeckoAppShell.onLowMemory();
        super.onLowMemory();
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
        try {
            String action = "org.mozilla.gecko.restart";
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
        File profileDir = getProfileDir();
        if (profileDir != null) {
            Log.i(LOGTAG, "checking profile migration in: " + profileDir.getAbsolutePath());
            ProfileMigrator profileMigrator =
                new ProfileMigrator(GeckoApp.mAppContext.getContentResolver(),
                                    profileDir);
            profileMigrator.launchBackground();
        }
    }

    private SynchronousQueue<String> mFilePickerResult = new SynchronousQueue<String>();
    public String showFilePicker(String aMimeType) {
        Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType(aMimeType);
        GeckoApp.this.
            startActivityForResult(
                Intent.createChooser(intent, getString(R.string.choose_file)),
                FILE_PICKER_REQUEST);
        String filePickerResult = "";

        try {
            while (null == (filePickerResult = mFilePickerResult.poll(1, TimeUnit.MILLISECONDS))) {
                Log.i(LOGTAG, "processing events from showFilePicker ");
                GeckoAppShell.processNextNativeEvent();
            }
        } catch (InterruptedException e) {
            Log.i(LOGTAG, "showing file picker ",  e);
        }

        return filePickerResult;
    }

    @Override
    public boolean onSearchRequested() {
        return showAwesomebar(AwesomeBar.Type.ADD);
    }
 
    public boolean onEditRequested() {
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
                Tab.HistoryEntry he = tab.getLastHistoryEntry();
                if (he != null) {
                    intent.putExtra(AwesomeBar.CURRENT_URL_KEY, he.mUri);
                }
            }
        }
        mOwnActivityDepth++;
        startActivityForResult(intent, AWESOMEBAR_REQUEST);
        return true;
    }

    public boolean doReload() {
        Log.i(LOGTAG, "Reload requested");
        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab == null)
            return false;

        return tab.doReload();
    }

    public boolean doForward() {
        Log.i(LOGTAG, "Forward requested");
        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab == null)
            return false;

        return tab.doForward();
    }

    public boolean doStop() {
        Log.i(LOGTAG, "Stop requested");
        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab == null)
            return false;

        return tab.doStop();
    }

    @Override
    public void onBackPressed() {
        if (mDoorHangerPopup.isShowing()) {
            mDoorHangerPopup.dismiss();
            return;
        }

        if (mFullScreen) {
            GeckoAppShell.sendEventToGecko(new GeckoEvent("FullScreen:Exit", null));
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

    @Override
    protected void onActivityResult(int requestCode, int resultCode,
                                    Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        switch (requestCode) {
        case FILE_PICKER_REQUEST:
            String filePickerResult = "";
            if (data != null && resultCode == RESULT_OK) {
                try {
                    ContentResolver cr = getContentResolver();
                    Uri uri = data.getData();
                    Cursor cursor = GeckoApp.mAppContext.getContentResolver().query(
                        uri, 
                        new String[] { OpenableColumns.DISPLAY_NAME },
                        null, 
                        null, 
                        null);
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
                    File file = File.createTempFile(fileName, fileExt, sGREDir);

                    FileOutputStream fos = new FileOutputStream(file);
                    InputStream is = cr.openInputStream(uri);
                    byte[] buf = new byte[4096];
                    int len = is.read(buf);
                    while (len != -1) {
                        fos.write(buf, 0, len);
                        len = is.read(buf);
                    }
                    fos.close();
                    filePickerResult =  file.getAbsolutePath();
                }catch (Exception e) {
                    Log.e(LOGTAG, "showing file picker", e);
                }
            }
            try {
                mFilePickerResult.put(filePickerResult);
            } catch (InterruptedException e) {
                Log.i(LOGTAG, "error returning file picker result", e);
            }
            break;
        case AWESOMEBAR_REQUEST:
            if (data != null) {
                String url = data.getStringExtra(AwesomeBar.URL_KEY);
                AwesomeBar.Type type = AwesomeBar.Type.valueOf(data.getStringExtra(AwesomeBar.TYPE_KEY));
                String searchEngine = data.getStringExtra(AwesomeBar.SEARCH_KEY);
                if (url != null && url.length() > 0)
                    loadRequest(url, type, searchEngine);
            }
            break;
        case CAMERA_CAPTURE_REQUEST:
            Log.i(LOGTAG, "Returning from CAMERA_CAPTURE_REQUEST: " + resultCode);
            File file = new File(Environment.getExternalStorageDirectory(), "cameraCapture-" + Integer.toString(kCaptureIndex) + ".jpg");
            kCaptureIndex++;
            GeckoEvent e = new GeckoEvent("cameraCaptureDone", resultCode == Activity.RESULT_OK ?
                                          "{\"ok\": true,  \"path\": \"" + file.getPath() + "\" }" :
                                          "{\"ok\": false, \"path\": \"" + file.getPath() + "\" }");
            GeckoAppShell.sendEventToGecko(e);
            break;
       }
    }

    public void doCameraCapture() {
        File file = new File(Environment.getExternalStorageDirectory(), "cameraCapture-" + Integer.toString(kCaptureIndex) + ".jpg");

        Intent intent = new Intent(android.provider.MediaStore.ACTION_IMAGE_CAPTURE);
        intent.putExtra(MediaStore.EXTRA_OUTPUT, Uri.fromFile(file));

        startActivityForResult(intent, CAMERA_CAPTURE_REQUEST);
    }

    // If searchEngine is provided, url will be used as the search query.
    // Otherwise, the url is loaded.
    private void loadRequest(String url, AwesomeBar.Type type, String searchEngine) {
        mBrowserToolbar.setTitle(url);
        Log.d(LOGTAG, type.name());
        JSONObject args = new JSONObject();
        try {
            args.put("url", url);
            args.put("engine", searchEngine);
        } catch (Exception e) {
            Log.e(LOGTAG, "error building JSON arguments");
        }
        if (type == AwesomeBar.Type.ADD) {
            GeckoAppShell.sendEventToGecko(new GeckoEvent("Tab:Add", args.toString()));
        } else {
            GeckoAppShell.sendEventToGecko(new GeckoEvent("Tab:Load", args.toString()));
        }
    }

    public void loadUrl(String url, AwesomeBar.Type type) {
        loadRequest(url, type, null);
    }

    /**
     * Open the link as a new tab, and mark the selected tab as its "parent".
     * Use this for tabs opened by the browser chrome, so users can press the
     * "Back" button to return to the previous tab.
     */
    public void loadUrlInNewTab(String url) {
        JSONObject args = new JSONObject();
        try {
            args.put("url", url);
            args.put("parentId", Tabs.getInstance().getSelectedTabId());
        } catch (Exception e) {
            Log.e(LOGTAG, "error building JSON arguments");
        }
        GeckoAppShell.sendEventToGecko(new GeckoEvent("Tab:Add", args.toString()));
    }

    public GeckoSoftwareLayerClient getSoftwareLayerClient() { return mSoftwareLayerClient; }
    public LayerController getLayerController() { return mLayerController; }

    // accelerometer
    public void onAccuracyChanged(Sensor sensor, int accuracy)
    {
    }

    public void onSensorChanged(SensorEvent event)
    {
        Log.w(LOGTAG, "onSensorChanged "+event);
        GeckoAppShell.sendEventToGecko(new GeckoEvent(event));
    }

    private class GeocoderRunnable implements Runnable {
        Location mLocation;
        GeocoderRunnable (Location location) {
            mLocation = location;
        }
        public void run() {
            try {
                List<Address> addresses = mGeocoder.getFromLocation(mLocation.getLatitude(),
                                                                    mLocation.getLongitude(), 1);
                // grab the first address.  in the future,
                // may want to expose multiple, or filter
                // for best.
                mLastGeoAddress = addresses.get(0);
                GeckoAppShell.sendEventToGecko(new GeckoEvent(mLocation, mLastGeoAddress));
            } catch (Exception e) {
                Log.w(LOGTAG, "GeocoderTask "+e);
            }
        }
    }

    // geolocation
    public void onLocationChanged(Location location)
    {
        Log.w(LOGTAG, "onLocationChanged "+location);
        if (mGeocoder == null)
            mGeocoder = new Geocoder(mLayerController.getView().getContext(), Locale.getDefault());

        if (mLastGeoAddress == null) {
            GeckoAppShell.getHandler().post(new GeocoderRunnable(location));
        }
        else {
            float[] results = new float[1];
            Location.distanceBetween(location.getLatitude(),
                                     location.getLongitude(),
                                     mLastGeoAddress.getLatitude(),
                                     mLastGeoAddress.getLongitude(),
                                     results);
            // pfm value.  don't want to slam the
            // geocoder with very similar values, so
            // only call after about 100m
            if (results[0] > 100)
                GeckoAppShell.getHandler().post(new GeocoderRunnable(location));
        }

        GeckoAppShell.sendEventToGecko(new GeckoEvent(location, mLastGeoAddress));
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
        if (mPlaceholderLayerClient != null)
            mPlaceholderLayerClient.destroy();

        LayerController layerController = getLayerController();
        layerController.setLayerClient(mSoftwareLayerClient);
    }

    private void prefetchDNS(final Uri u) {
        // resolving the host here starts up the radio
        // and may prime the dns cache.  See
        // http://www.stevesouders.com/blog/2011/09/21/making-a-mobile-connection/
        // for more information.
        new Thread(new Runnable() {
                public void run() {
                    try {
                        Log.i(LOGTAG,"resolving: " + u.getHost());
                        InetAddress.getByName(u.getHost());
                    } catch (Exception e) {
                        // we really don't care.
                    }
                }
            }, "DNSPrefetcher Thread").start();
    }
}

class PluginLayoutParams extends AbsoluteLayout.LayoutParams
{
    private static final int MAX_DIMENSION = 2048;
    private static final String LOGTAG = "GeckoApp.PluginLayoutParams";

    private int mOriginalX;
    private int mOriginalY;
    private int mOriginalWidth;
    private int mOriginalHeight;
    private float mOriginalResolution;

    private float mLastResolution;

    /*
     * This awkward pattern is necessary due to Java's restrictions on when one can call superclass
     * constructors.
     */
    private PluginLayoutParams(int aX, int aY, int aWidth, int aHeight, float aResolution) {
        super(aWidth, aHeight, aX, aY);

        Log.i(LOGTAG, "Creating plugin at " + aX + ", " + aY + ", " + aWidth + "x" + aHeight + ", (" + (aResolution * 100) + "%)");

        mOriginalX = aX;
        mOriginalY = aY;
        mOriginalWidth = aWidth;
        mOriginalHeight = aHeight;
        mLastResolution = mOriginalResolution = aResolution;

        clampToMaxSize();
    }

    public static PluginLayoutParams create(int aX, int aY, int aWidth, int aHeight, float aResolution) {
        aX = (int)Math.round(aX * aResolution);
        aY = (int)Math.round(aY * aResolution);
        aWidth = (int)Math.round(aWidth * aResolution);
        aHeight = (int)Math.round(aHeight * aResolution);

        return new PluginLayoutParams(aX, aY, aWidth, aHeight, aResolution);
    }

    private void clampToMaxSize() {
        if (width > MAX_DIMENSION || height > MAX_DIMENSION) {
            if (width > height) {
                height = (int)(((float)height/(float)width) * MAX_DIMENSION);
                width = MAX_DIMENSION;
            } else {
                width = (int)(((float)width/(float)height) * MAX_DIMENSION);
                height = MAX_DIMENSION;
            }
        }
    }

    public void reset(int aX, int aY, int aWidth, int aHeight, float aResolution) {
        x = mOriginalX = (int)Math.round(aX * aResolution);
        y = mOriginalY = (int)Math.round(aY * aResolution);
        width = mOriginalWidth = (int)Math.round(aWidth * aResolution);
        height = mOriginalHeight = (int)Math.round(aHeight * aResolution);
        mLastResolution = mOriginalResolution = aResolution;

        clampToMaxSize();

        Log.i(LOGTAG, "Resetting plugin to " + x + ", " + y + ", " + width + "x" + height + ", (" + (aResolution * 100) + "%)");
    }

    private void reposition(Point aOffset, float aResolution) {
        Log.i(LOGTAG, "Repositioning plugin by " + aOffset.x + ", " + aOffset.y + ", (" + (aResolution * 100) + "%)");
        x = mOriginalX + aOffset.x;
        y = mOriginalY + aOffset.y;

        if (!FloatUtils.fuzzyEquals(mLastResolution, aResolution)) {
            float scaleFactor = aResolution / mOriginalResolution;
            width = (int)Math.round(scaleFactor * mOriginalWidth);
            height = (int)Math.round(scaleFactor * mOriginalHeight);
            mLastResolution = aResolution;

            clampToMaxSize();
        }
    }

    public void reposition(ViewportMetrics hostViewport, ViewportMetrics targetViewport) {
        PointF targetOrigin = targetViewport.getOrigin();
        PointF hostOrigin = hostViewport.getOrigin();

        Point offset = new Point((int)Math.round(hostOrigin.x - targetOrigin.x),
                                 (int)Math.round(hostOrigin.y - targetOrigin.y));

        reposition(offset, hostViewport.getZoomFactor());
    }
}
