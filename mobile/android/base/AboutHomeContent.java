/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.InputStream;
import java.io.IOException;
import java.net.URL;
import java.net.MalformedURLException;
import java.util.ArrayList;
import java.util.EnumSet;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.sync.setup.activities.SetupSyncActivity;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.OnAccountsUpdateListener;
import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;
import android.text.SpannableString;
import android.text.style.StyleSpan;
import android.text.style.UnderlineSpan;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.FrameLayout;
import android.widget.GridView;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.RelativeLayout;
import android.widget.ScrollView;
import android.widget.SimpleCursorAdapter;
import android.widget.TextView;
import android.text.TextUtils;

public class AboutHomeContent extends ScrollView
       implements TabsAccessor.OnQueryTabsCompleteListener {
    private static final String LOGTAG = "GeckoAboutHome";

    private static final int NUMBER_OF_TOP_SITES_PORTRAIT = 4;
    private static final int NUMBER_OF_TOP_SITES_LANDSCAPE = 3;

    private static final int NUMBER_OF_COLS_PORTRAIT = 2;
    private static final int NUMBER_OF_COLS_LANDSCAPE = 3;

    private static final int NUMBER_OF_REMOTE_TABS = 5;

    static enum UpdateFlags {
        TOP_SITES,
        PREVIOUS_TABS,
        RECOMMENDED_ADDONS,
        REMOTE_TABS;

        public static final EnumSet<UpdateFlags> ALL = EnumSet.allOf(UpdateFlags.class);
    }

    private Cursor mCursor;
    UriLoadCallback mUriLoadCallback = null;
    private LayoutInflater mInflater;

    private AccountManager mAccountManager;
    private OnAccountsUpdateListener mAccountListener = null;

    protected SimpleCursorAdapter mTopSitesAdapter;
    protected GridView mTopSitesGrid;

    protected AboutHomeSection mAddons;
    protected AboutHomeSection mLastTabs;
    protected AboutHomeSection mRemoteTabs;

    private View.OnClickListener mRemoteTabClickListener;

    public interface UriLoadCallback {
        public void callback(String uriSpec);
    }

    public AboutHomeContent(Context context) {
        super(context);
    }

    public AboutHomeContent(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public void init() {
        Context context = getContext();
        mInflater = (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mInflater.inflate(R.layout.abouthome_content, this);

        mAccountManager = AccountManager.get(context);

        // The listener will run on the background thread (see 2nd argument)
        mAccountManager.addOnAccountsUpdatedListener(mAccountListener = new OnAccountsUpdateListener() {
            public void onAccountsUpdated(Account[] accounts) {
                final GeckoApp.StartupMode startupMode = GeckoApp.mAppContext.getStartupMode();
                final boolean syncIsSetup = isSyncSetup();

                GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
                    public void run() {
                        // The listener might run before the UI is initially updated.
                        // In this case, we should simply wait for the initial setup
                        // to happen.
                        if (mTopSitesAdapter != null)
                            updateLayout(startupMode, syncIsSetup);
                    }
                });
            }
        }, GeckoAppShell.getHandler(), false);

        mTopSitesGrid = (GridView)findViewById(R.id.top_sites_grid);
        mTopSitesGrid.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            public void onItemClick(AdapterView<?> parent, View v, int position, long id) {
                Cursor c = (Cursor) parent.getItemAtPosition(position);

                String spec = c.getString(c.getColumnIndex(URLColumns.URL));
                Log.i(LOGTAG, "clicked: " + spec);

                if (mUriLoadCallback != null)
                    mUriLoadCallback.callback(spec);
            }
        });

        mAddons = (AboutHomeSection) findViewById(R.id.recommended_addons);
        mLastTabs = (AboutHomeSection) findViewById(R.id.last_tabs);
        mRemoteTabs = (AboutHomeSection) findViewById(R.id.remote_tabs);

        TextView allTopSitesText = (TextView) findViewById(R.id.all_top_sites_text);
        allTopSitesText.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                GeckoApp.mAppContext.showAwesomebar(AwesomeBar.Target.CURRENT_TAB);
            }
        });

        mAddons.setOnMoreTextClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                if (mUriLoadCallback != null)
                    mUriLoadCallback.callback("https://addons.mozilla.org/android");
            }
        });

        mRemoteTabs.setOnMoreTextClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                GeckoApp.mAppContext.showRemoteTabs();
            }
        });

        TextView syncTextView = (TextView) findViewById(R.id.sync_text);
        String syncText = syncTextView.getText().toString() + " \u00BB";
        String boldName = getContext().getResources().getString(R.string.abouthome_sync_bold_name);
        int styleIndex = syncText.indexOf(boldName);

        // Highlight any occurrence of "Firefox Sync" in the string
        // with a bold style.
        if (styleIndex >= 0) {
            SpannableString spannableText = new SpannableString(syncText);
            spannableText.setSpan(new StyleSpan(android.graphics.Typeface.BOLD), styleIndex, styleIndex + 12, 0);
            syncTextView.setText(spannableText, TextView.BufferType.SPANNABLE);
        }

        RelativeLayout syncBox = (RelativeLayout) findViewById(R.id.sync_box);
        syncBox.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                Context context = v.getContext();
                Intent intent = new Intent(context, SetupSyncActivity.class);
                context.startActivity(intent);
            }
        });

        mRemoteTabClickListener = new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                String url = ((String) v.getTag());
                JSONObject args = new JSONObject();
                try {
                    args.put("url", url);
                    args.put("engine", null);
                    args.put("userEntered", false);
                } catch (Exception e) {
                    Log.e(LOGTAG, "error building JSON arguments");
                }
    
                Log.d(LOGTAG, "Sending message to Gecko: " + SystemClock.uptimeMillis() + " - Tab:Add");
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Tab:Add", args.toString()));
            }
        };
    }

    public void onDestroy() {
        if (mAccountListener != null) {
            mAccountManager.removeOnAccountsUpdatedListener(mAccountListener);
            mAccountListener = null;
        }

        if (mCursor != null && !mCursor.isClosed())
            mCursor.close();
    }

    void setLastTabsVisibility(boolean visible) {
        if (visible)
            mLastTabs.show();
        else
            mLastTabs.hide();
    }

    private void setTopSitesVisibility(boolean visible, boolean hasTopSites) {
        int visibility = visible ? View.VISIBLE : View.GONE;
        int visibilityWithTopSites = visible && hasTopSites ? View.VISIBLE : View.GONE;
        int visibilityWithoutTopSites = visible && !hasTopSites ? View.VISIBLE : View.GONE;

        findViewById(R.id.top_sites_grid).setVisibility(visibilityWithTopSites);
        findViewById(R.id.top_sites_title).setVisibility(visibility);
        findViewById(R.id.all_top_sites_text).setVisibility(visibilityWithTopSites);
        findViewById(R.id.no_top_sites_text).setVisibility(visibilityWithoutTopSites);
    }

    private void setSyncVisibility(boolean visible) {
        int visibility = visible ? View.VISIBLE : View.GONE;
        findViewById(R.id.sync_box_container).setVisibility(visibility);
    }

    private void updateSyncLayout(boolean isFirstRun, boolean hasTopSites) {
        RelativeLayout syncContainer = (RelativeLayout) findViewById(R.id.sync_box_container);
        RelativeLayout.LayoutParams params = (RelativeLayout.LayoutParams) syncContainer.getLayoutParams();

        int below = R.id.all_top_sites_text;
        if (isFirstRun && !hasTopSites)
            below = R.id.top_sites_top;
        else if (!hasTopSites)
            below = R.id.no_top_sites_text;

        int background = R.drawable.abouthome_bg_repeat;
        if (isFirstRun && !hasTopSites)
            background = 0;

        params.addRule(RelativeLayout.BELOW, below);
        syncContainer.setLayoutParams(params);

        syncContainer.setBackgroundResource(background);
    }

    private boolean isSyncSetup() {
        Account[] accounts = mAccountManager.getAccountsByType("org.mozilla.firefox_sync");
        return accounts.length > 0;
    }

    private void updateLayout(GeckoApp.StartupMode startupMode, boolean syncIsSetup) {
        // The idea here is that we only show the sync invitation
        // on the very first run. Show sync banner below the top
        // sites section in all other cases.

        boolean hasTopSites = mTopSitesAdapter.getCount() > 0;
        boolean isFirstRun = (startupMode == GeckoApp.StartupMode.NEW_PROFILE);

        setTopSitesVisibility(!isFirstRun || hasTopSites, hasTopSites);
        setSyncVisibility(!syncIsSetup);
        updateSyncLayout(isFirstRun, hasTopSites);
    }

    private int getNumberOfTopSites() {
        Configuration config = getContext().getResources().getConfiguration();
        if (config.orientation == Configuration.ORIENTATION_LANDSCAPE)
            return NUMBER_OF_TOP_SITES_LANDSCAPE;
        else
            return NUMBER_OF_TOP_SITES_PORTRAIT;
    }

    private void loadTopSites(final Activity activity) {
        // Ensure we initialize GeckoApp's startup mode in
        // background thread before we use it when updating
        // the top sites section layout in main thread.
        final GeckoApp.StartupMode startupMode = GeckoApp.mAppContext.getStartupMode();

        // The isSyncSetup method should not be called on
        // UI thread as it touches disk to access a sqlite DB.
        final boolean syncIsSetup = isSyncSetup();

        final ContentResolver resolver = GeckoApp.mAppContext.getContentResolver();
        final Cursor oldCursor = mCursor;
        // Swap in the new cursor.
        mCursor = BrowserDB.getTopSites(resolver, NUMBER_OF_TOP_SITES_PORTRAIT);;

        GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
            public void run() {
                if (mTopSitesAdapter == null) {
                    mTopSitesAdapter = new TopSitesCursorAdapter(activity,
                                                                 R.layout.abouthome_topsite_item,
                                                                 mCursor,
                                                                 new String[] { URLColumns.TITLE,
                                                                                URLColumns.THUMBNAIL },
                                                                 new int[] { R.id.title, R.id.thumbnail });

                    mTopSitesAdapter.setViewBinder(new TopSitesViewBinder());
                    mTopSitesGrid.setAdapter(mTopSitesAdapter);
                } else {
                    mTopSitesAdapter.changeCursor(mCursor);
                }

                updateLayout(startupMode, syncIsSetup);

                // Free the old Cursor in the right thread now.
                if (oldCursor != null && !oldCursor.isClosed())
                    oldCursor.close();
            }
        });
    }

    void update(final Activity activity, final EnumSet<UpdateFlags> flags) {
        GeckoAppShell.getHandler().post(new Runnable() {
            public void run() {
                if (flags.contains(UpdateFlags.TOP_SITES))
                    loadTopSites(activity);

                if (flags.contains(UpdateFlags.PREVIOUS_TABS))
                    readLastTabs(activity);

                if (flags.contains(UpdateFlags.RECOMMENDED_ADDONS))
                    readRecommendedAddons(activity);

                if (flags.contains(UpdateFlags.REMOTE_TABS))
                    loadRemoteTabs(activity);
            }
        });
    }

    public void setUriLoadCallback(UriLoadCallback uriLoadCallback) {
        mUriLoadCallback = uriLoadCallback;
    }

    public void onActivityContentChanged(Activity activity) {
        update(activity, EnumSet.of(UpdateFlags.TOP_SITES));
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        if (mTopSitesAdapter != null)
            mTopSitesAdapter.notifyDataSetChanged();

        super.onConfigurationChanged(newConfig);
    }

    private String readFromZipFile(Activity activity, String filename) {
        ZipFile zip = null;
        String str = null;
        try {
            InputStream fileStream = null;
            File applicationPackage = new File(activity.getApplication().getPackageResourcePath());
            zip = new ZipFile(applicationPackage);
            if (zip == null)
                return null;
            ZipEntry fileEntry = zip.getEntry(filename);
            if (fileEntry == null)
                return null;
            fileStream = zip.getInputStream(fileEntry);
            str = readStringFromStream(fileStream);
        } catch (IOException ioe) {
            Log.e(LOGTAG, "error reading zip file: " + filename, ioe);
        } finally {
            try {
                if (zip != null)
                    zip.close();
            } catch (IOException ioe) {
                // catch this here because we can continue even if the
                // close failed
                Log.e(LOGTAG, "error closing zip filestream", ioe);
            }
        }
        return str;
    }

    private String readStringFromStream(InputStream fileStream) {
        String str = null;
        try {
            byte[] buf = new byte[32768];
            StringBuffer jsonString = new StringBuffer();
            int read = 0;
            while ((read = fileStream.read(buf, 0, 32768)) != -1)
                jsonString.append(new String(buf, 0, read));
            str = jsonString.toString();
        } catch (IOException ioe) {
            Log.i(LOGTAG, "error reading filestream", ioe);
        } finally {
            try {
                if (fileStream != null)
                    fileStream.close();
            } catch (IOException ioe) {
                // catch this here because we can continue even if the
                // close failed
                Log.e(LOGTAG, "error closing filestream", ioe);
            }
        }
        return str;
    }

    private String getPageUrlFromIconUrl(String iconUrl) {
        // Addon icon URLs come with a query argument that is usually
        // used for expiration purposes. We want the "page URL" here to be
        // stable enough to avoid unnecessary duplicate records of the
        // same addon.
        String pageUrl = iconUrl;

        try {
            URL urlForIcon = new URL(iconUrl);
            URL urlForPage = new URL(urlForIcon.getProtocol(), urlForIcon.getAuthority(), urlForIcon.getPath());
            pageUrl = urlForPage.toString();
        } catch (MalformedURLException e) {
            // Defaults to pageUrl = iconUrl in case of error
        }

        return pageUrl;
    }

    private void readRecommendedAddons(final Activity activity) {
        final String addonsFilename = "recommended-addons.json";
        String jsonString;
        try {
            jsonString = GeckoApp.mAppContext.getProfile().readFile(addonsFilename);
        } catch (IOException ioe) {
            Log.i(LOGTAG, "filestream is null");
            jsonString = readFromZipFile(activity, addonsFilename);
        }

        JSONArray addonsArray = null;
        if (jsonString != null) {
            try {
                addonsArray = new JSONObject(jsonString).getJSONArray("addons");
            } catch (JSONException e) {
                Log.i(LOGTAG, "error reading json file", e);
            }
        }

        final JSONArray array = addonsArray;
        GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
            public void run() {
                try {
                    if (array == null || array.length() == 0) {
                        mAddons.hide();
                        return;
                    }

                    for (int i = 0; i < array.length(); i++) {
                        JSONObject jsonobj = array.getJSONObject(i);

                        final View row = mInflater.inflate(R.layout.abouthome_addon_row, mAddons.getItemsContainer(), false);
                        ((TextView) row.findViewById(R.id.addon_title)).setText(jsonobj.getString("name"));
                        ((TextView) row.findViewById(R.id.addon_version)).setText(jsonobj.getString("version"));

                        String iconUrl = jsonobj.getString("iconURL");
                        String pageUrl = getPageUrlFromIconUrl(iconUrl);

                        final String homepageUrl = jsonobj.getString("homepageURL");
                        row.setOnClickListener(new View.OnClickListener() {
                            public void onClick(View v) {
                                if (mUriLoadCallback != null)
                                    mUriLoadCallback.callback(homepageUrl);
                            }
                        });

                        Favicons favicons = GeckoApp.mAppContext.mFavicons;
                        favicons.loadFavicon(pageUrl, iconUrl,
                                    new Favicons.OnFaviconLoadedListener() {
                            public void onFaviconLoaded(String url, Drawable favicon) {
                                if (favicon != null) {
                                    ImageView icon = (ImageView) row.findViewById(R.id.addon_icon);
                                    icon.setImageDrawable(favicon);
                                }
                            }
                        });

                        mAddons.addItem(row);
                    }

                    mAddons.show();
                } catch (JSONException e) {
                    Log.i(LOGTAG, "error reading json file", e);
                }
            }
        });
    }

    private void readLastTabs(final Activity activity) {
        String jsonString = GeckoApp.mAppContext.getProfile().readSessionFile(GeckoApp.sIsGeckoReady);
        if (jsonString == null) {
            // no previous session data
            return;
        }

        final JSONArray tabs;
        try {
            tabs = new JSONObject(jsonString).getJSONArray("windows")
                                             .getJSONObject(0)
                                             .getJSONArray("tabs");
        } catch (JSONException e) {
            Log.i(LOGTAG, "error reading json file", e);
            return;
        }

        final ArrayList<String> lastTabUrlsList = new ArrayList<String>();

        for (int i = 0; i < tabs.length(); i++) {
            final String title;
            final String url;
            try {
                JSONObject tab = tabs.getJSONObject(i);
                int index = tab.getInt("index");
                JSONArray entries = tab.getJSONArray("entries");
                JSONObject entry = entries.getJSONObject(index - 1);
                url = entry.getString("url");

                String optTitle = entry.optString("title");
                if (optTitle.length() == 0)
                    title = url;
                else
                    title = optTitle;
            } catch (JSONException e) {
                Log.e(LOGTAG, "error reading json file", e);
                continue;
            }

            // don't show last tabs for about pages
            if (url.startsWith("about:"))
                continue;

            ContentResolver resolver = GeckoApp.mAppContext.getContentResolver();
            final BitmapDrawable favicon = BrowserDB.getFaviconForUrl(resolver, url);
            lastTabUrlsList.add(url);

            GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
                public void run() {
                    View container = mInflater.inflate(R.layout.abouthome_last_tabs_row, mLastTabs.getItemsContainer(), false);
                    ((TextView) container.findViewById(R.id.last_tab_title)).setText(title);
                    ((TextView) container.findViewById(R.id.last_tab_url)).setText(url);
                    if (favicon != null)
                        ((ImageView) container.findViewById(R.id.last_tab_favicon)).setImageDrawable(favicon);

                    container.findViewById(R.id.last_tab_row).setOnClickListener(new View.OnClickListener() {
                        public void onClick(View v) {
                            GeckoApp.mAppContext.loadUrlInTab(url);
                        }
                    });

                    mLastTabs.addItem(container);
                }
            });
        }

        final int numLastTabs = lastTabUrlsList.size();
        GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
            public void run() {
                if (numLastTabs > 1) {
                    mLastTabs.showMoreText();
                    mLastTabs.setOnMoreTextClickListener(new View.OnClickListener() {
                        public void onClick(View v) {
                            for (String url : lastTabUrlsList)
                                GeckoApp.mAppContext.loadUrlInTab(url);
                        }
                    });
                    mLastTabs.show();
                } else if (numLastTabs == 1) {
                    mLastTabs.hideMoreText();
                    mLastTabs.show();
                }
            }
        });
    }

    private void loadRemoteTabs(final Activity activity) {
        if (!isSyncSetup()) {
            GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
                public void run() {
                    mRemoteTabs.hide();
                }
            });
            return;
        }

        TabsAccessor.getTabs(getContext(), NUMBER_OF_REMOTE_TABS, this);
    }

    @Override
    public void onQueryTabsComplete(List<TabsAccessor.RemoteTab> tabsList) {
        ArrayList<TabsAccessor.RemoteTab> tabs = new ArrayList<TabsAccessor.RemoteTab> (tabsList);
        if (tabs == null || tabs.size() == 0) {
            mRemoteTabs.hide();
            return;
        }
        
        mRemoteTabs.clear();
        
        String client = null;
        
        for (TabsAccessor.RemoteTab tab : tabs) {
            if (client == null)
                client = tab.name;
            else if (!TextUtils.equals(client, tab.name))
                break;

            final TextView row = (TextView) mInflater.inflate(R.layout.abouthome_remote_tab_row, mRemoteTabs.getItemsContainer(), false);
            row.setText(TextUtils.isEmpty(tab.title) ? tab.url : tab.title);
            row.setTag(tab.url);
            mRemoteTabs.addItem(row);
            row.setOnClickListener(mRemoteTabClickListener);
        }
        
        mRemoteTabs.setSubtitle(client);
        mRemoteTabs.show();
    }

    public static class TopSitesGridView extends GridView {
        /** From layout xml:
         *  80dip image height 
         * + 2dip image paddingTop
         * + 1dip image padding (for bottom)
         * + 3dip marginTop on the TextView
         * +15dip TextView height
         * + 8dip vertical spacing in the GridView
         * ------
         * 109dip total height per top site grid item
         */
        private static final int kTopSiteItemHeight = 109;
        float mDisplayDensity ;

        public TopSitesGridView(Context context, AttributeSet attrs) {
            super(context, attrs);
            mDisplayDensity = GeckoApp.mAppContext.getDisplayMetrics().density;
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            int numRows;

            SimpleCursorAdapter adapter = (SimpleCursorAdapter) getAdapter();
            int nSites = Integer.MAX_VALUE;

            if (adapter != null) {
                Cursor c = adapter.getCursor();
                if (c != null)
                    nSites = c.getCount();
            }

            Configuration config = getContext().getResources().getConfiguration();
            if (config.orientation == Configuration.ORIENTATION_LANDSCAPE) {
                nSites = Math.min(nSites, NUMBER_OF_TOP_SITES_LANDSCAPE);
                numRows = (int) Math.round((double) nSites / NUMBER_OF_COLS_LANDSCAPE);
                setNumColumns(NUMBER_OF_COLS_LANDSCAPE);
            } else {
                nSites = Math.min(nSites, NUMBER_OF_TOP_SITES_PORTRAIT);
                numRows = (int) Math.round((double) nSites / NUMBER_OF_COLS_PORTRAIT);
                setNumColumns(NUMBER_OF_COLS_PORTRAIT);
            }
            int expandedHeightSpec = 
                MeasureSpec.makeMeasureSpec((int)(mDisplayDensity * numRows * kTopSiteItemHeight),
                                            MeasureSpec.EXACTLY);

            super.onMeasure(widthMeasureSpec, expandedHeightSpec);
        }
    }

    public class TopSitesCursorAdapter extends SimpleCursorAdapter {
        public TopSitesCursorAdapter(Context context, int layout, Cursor c,
                                     String[] from, int[] to) {
            super(context, layout, c, from, to);
        }

        @Override
        public int getCount() {
            return Math.min(super.getCount(), getNumberOfTopSites());
        }

        @Override
        protected void onContentChanged () {
            // Don't do anything. We don't want to regenerate every time
            // our history database is updated.
            return;
        }
    }

    class TopSitesViewBinder implements SimpleCursorAdapter.ViewBinder {
        private boolean updateThumbnail(View view, Cursor cursor, int thumbIndex) {
            byte[] b = cursor.getBlob(thumbIndex);
            ImageView thumbnail = (ImageView) view;

            if (b == null) {
                thumbnail.setImageResource(R.drawable.tab_thumbnail_default);
            } else {
                try {
                    Bitmap bitmap = BitmapFactory.decodeByteArray(b, 0, b.length);
                    thumbnail.setImageBitmap(bitmap);
                } catch (OutOfMemoryError oom) {
                    Log.e(LOGTAG, "Unable to load thumbnail bitmap", oom);
                    thumbnail.setImageResource(R.drawable.tab_thumbnail_default);
                }
            }

            return true;
        }

        private boolean updateTitle(View view, Cursor cursor, int titleIndex) {
            String title = cursor.getString(titleIndex);
            TextView titleView = (TextView) view;

            // Use the URL instead of an empty title for consistency with the normal URL
            // bar view - this is the equivalent of getDisplayTitle() in Tab.java
            if (title == null || title.length() == 0) {
                int urlIndex = cursor.getColumnIndexOrThrow(URLColumns.URL);
                title = cursor.getString(urlIndex);
            }

            titleView.setText(title);
            return true;
        }

        @Override
        public boolean setViewValue(View view, Cursor cursor, int columnIndex) {
            int titleIndex = cursor.getColumnIndexOrThrow(URLColumns.TITLE);
            if (columnIndex == titleIndex) {
                return updateTitle(view, cursor, titleIndex);
            }

            int thumbIndex = cursor.getColumnIndexOrThrow(URLColumns.THUMBNAIL);
            if (columnIndex == thumbIndex) {
                return updateThumbnail(view, cursor, thumbIndex);
            }

            // Other columns are handled automatically
            return false;
        }
    }
}
