/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.db.BrowserContract.Thumbnails;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.sync.setup.SyncAccounts;
import org.mozilla.gecko.sync.setup.activities.SetupSyncActivity;
import org.mozilla.gecko.util.GeckoAsyncTask;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.OnAccountsUpdateListener;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Path;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.RectF;
import android.os.SystemClock;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.style.StyleSpan;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AbsListView;
import android.widget.GridView;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.ScrollView;
import android.widget.SimpleCursorAdapter;
import android.widget.TextView;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.EnumSet;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

public class AboutHomeContent extends ScrollView
                              implements TabsAccessor.OnQueryTabsCompleteListener,
                                         LightweightTheme.OnChangeListener {
    private static final String LOGTAG = "GeckoAboutHome";

    private static final int NUMBER_OF_REMOTE_TABS = 5;

    private static int mNumberOfTopSites;
    private static int mNumberOfCols;

    static enum UpdateFlags {
        TOP_SITES,
        PREVIOUS_TABS,
        RECOMMENDED_ADDONS,
        REMOTE_TABS;

        public static final EnumSet<UpdateFlags> ALL = EnumSet.allOf(UpdateFlags.class);
    }

    private Context mContext;
    private BrowserApp mActivity;
    private Cursor mCursor;
    UriLoadCallback mUriLoadCallback = null;
    VoidCallback mLoadCompleteCallback = null;
    private LayoutInflater mInflater;

    private AccountManager mAccountManager;
    private OnAccountsUpdateListener mAccountListener = null;

    protected SimpleCursorAdapter mTopSitesAdapter;
    protected TopSitesGridView mTopSitesGrid;

    private AboutHomePromoBox mPromoBox;
    private AboutHomePromoBox.Type mPrelimPromoBoxType;
    protected AboutHomeSection mAddons;
    protected AboutHomeSection mLastTabs;
    protected AboutHomeSection mRemoteTabs;

    private View.OnClickListener mRemoteTabClickListener;

    public interface UriLoadCallback {
        public void callback(String uriSpec);
    }

    public interface VoidCallback {
        public void callback();
    }

    public AboutHomeContent(Context context) {
        super(context);
        mContext = context;
    }

    public AboutHomeContent(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mActivity = (BrowserApp) context;
    }

    public void init() {
        inflate();

        mAccountManager = AccountManager.get(mContext);

        // The listener will run on the background thread (see 2nd argument)
        mAccountManager.addOnAccountsUpdatedListener(mAccountListener = new OnAccountsUpdateListener() {
            public void onAccountsUpdated(Account[] accounts) {
                updateLayoutForSync();
            }
        }, GeckoAppShell.getHandler(), false);

        mRemoteTabClickListener = new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Tabs.getInstance().loadUrl((String) v.getTag(), Tabs.LOADURL_NEW_TAB);
            }
        };

        mPrelimPromoBoxType = (new Random()).nextFloat() < 0.5 ? AboutHomePromoBox.Type.SYNC :
                AboutHomePromoBox.Type.APPS;
    }

    private void inflate() {
        mInflater = (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mInflater.inflate(R.layout.abouthome_content, this);

        mTopSitesGrid = (TopSitesGridView)findViewById(R.id.top_sites_grid);
        mTopSitesGrid.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            public void onItemClick(AdapterView<?> parent, View v, int position, long id) {
                Cursor c = (Cursor) parent.getItemAtPosition(position);

                String spec = c.getString(c.getColumnIndex(URLColumns.URL));
                Log.i(LOGTAG, "clicked: " + spec);

                if (mUriLoadCallback != null)
                    mUriLoadCallback.callback(spec);
            }
        });

        mPromoBox = (AboutHomePromoBox) findViewById(R.id.promo_box);
        mAddons = (AboutHomeSection) findViewById(R.id.recommended_addons);
        mLastTabs = (AboutHomeSection) findViewById(R.id.last_tabs);
        mRemoteTabs = (AboutHomeSection) findViewById(R.id.remote_tabs);

        mAddons.setOnMoreTextClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                if (mUriLoadCallback != null)
                    mUriLoadCallback.callback("https://addons.mozilla.org/android");
            }
        });

        mRemoteTabs.setOnMoreTextClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                mActivity.showRemoteTabs();
            }
        });

        setTopSitesConstants();
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        mActivity.getLightweightTheme().addListener(this);
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mActivity.getLightweightTheme().removeListener(this);
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

    private void setTopSitesVisibility(boolean hasTopSites) {
        int visibility = hasTopSites ? View.VISIBLE : View.GONE;

        findViewById(R.id.top_sites_title).setVisibility(visibility);
        findViewById(R.id.top_sites_grid).setVisibility(visibility);
    }

    private void updateLayout(boolean syncIsSetup) {
        boolean hasTopSites = mTopSitesAdapter.getCount() > 0;
        setTopSitesVisibility(hasTopSites);

        AboutHomePromoBox.Type type = mPrelimPromoBoxType;
        if (syncIsSetup && type == AboutHomePromoBox.Type.SYNC)
            type = AboutHomePromoBox.Type.APPS;

        mPromoBox.show(type);
    }

    private void updateLayoutForSync() {
        final GeckoApp.StartupMode startupMode = mActivity.getStartupMode();
        final boolean syncIsSetup = SyncAccounts.syncAccountsExist(mContext);

        post(new Runnable() {
            public void run() {
                // The listener might run before the UI is initially updated.
                // In this case, we should simply wait for the initial setup
                // to happen.
                if (mTopSitesAdapter != null)
                    updateLayout(syncIsSetup);
            }
        });
    }

    private void loadTopSites() {
        // The SyncAccounts.syncAccountsExist method should not be called on
        // UI thread as it touches disk to access a sqlite DB.
        final boolean syncIsSetup = SyncAccounts.syncAccountsExist(mActivity);

        final ContentResolver resolver = mActivity.getContentResolver();
        final Cursor oldCursor = mCursor;
        // Swap in the new cursor.
        mCursor = BrowserDB.getTopSites(resolver, mNumberOfTopSites);

        post(new Runnable() {
            public void run() {
                if (mTopSitesAdapter == null) {
                    mTopSitesAdapter = new TopSitesCursorAdapter(mActivity,
                                                                 R.layout.abouthome_topsite_item,
                                                                 mCursor,
                                                                 new String[] { URLColumns.TITLE },
                                                                 new int[] { R.id.title });

                    mTopSitesAdapter.setViewBinder(new TopSitesViewBinder());
                    mTopSitesGrid.setAdapter(mTopSitesAdapter);
                } else {
                    mTopSitesAdapter.changeCursor(mCursor);
                }

                if (mTopSitesAdapter.getCount() > 0)
                    loadTopSitesThumbnails(resolver);

                updateLayout(syncIsSetup);

                // Free the old Cursor in the right thread now.
                if (oldCursor != null && !oldCursor.isClosed())
                    oldCursor.close();

                // Even if AboutHome isn't necessarily entirely loaded if we
                // get here, for phones this is the part the user initially sees,
                // so it's the one we will care about for now.
                if (mLoadCompleteCallback != null)
                    mLoadCompleteCallback.callback();
            }
        });
    }

    private List<String> getTopSitesUrls() {
        List<String> urls = new ArrayList<String>();

        Cursor c = mTopSitesAdapter.getCursor();
        if (c == null || !c.moveToFirst())
            return urls;

        do {
            final String url = c.getString(c.getColumnIndexOrThrow(URLColumns.URL));
            urls.add(url);
        } while (c.moveToNext());

        return urls;
    }

    private void displayThumbnail(View view, Bitmap thumbnail) {
        ImageView thumbnailView = (ImageView) view.findViewById(R.id.thumbnail);

        if (thumbnail == null) {
            thumbnailView.setImageResource(R.drawable.abouthome_thumbnail_bg);
            thumbnailView.setScaleType(ImageView.ScaleType.FIT_CENTER);
        } else {
            try {
                thumbnailView.setImageBitmap(thumbnail);
                thumbnailView.setScaleType(ImageView.ScaleType.CENTER_CROP);
            } catch (OutOfMemoryError oom) {
                Log.e(LOGTAG, "Unable to load thumbnail bitmap", oom);
                thumbnailView.setImageResource(R.drawable.abouthome_thumbnail_bg);
                thumbnailView.setScaleType(ImageView.ScaleType.FIT_CENTER);
            }
        }
    }

    private void updateTopSitesThumbnails(Map<String, Bitmap> thumbnails) {
        for (int i = 0; i < mTopSitesAdapter.getCount(); i++) {
            final View view = mTopSitesGrid.getChildAt(i);

            // The grid view might get temporarily out of sync with the
            // adapter refreshes (e.g. on device rotation)
            if (view == null)
                continue;

            Cursor c = (Cursor) mTopSitesAdapter.getItem(i);
            final String url = c.getString(c.getColumnIndex(URLColumns.URL));

            displayThumbnail(view, thumbnails.get(url));
        }

        mTopSitesGrid.invalidate();
    }

    public Map<String, Bitmap> getTopSitesThumbnails(Cursor c) {
        Map<String, Bitmap> thumbnails = new HashMap<String, Bitmap>();

        try {
            if (c == null || !c.moveToFirst())
                return thumbnails;

            do {
                final String url = c.getString(c.getColumnIndexOrThrow(Thumbnails.URL));
                final byte[] b = c.getBlob(c.getColumnIndexOrThrow(Thumbnails.DATA));
                if (b == null)
                    continue;

                Bitmap thumbnail = BitmapFactory.decodeByteArray(b, 0, b.length);
                if (thumbnail == null)
                    continue;

                thumbnails.put(url, thumbnail);
            } while (c.moveToNext());
        } finally {
            if (c != null)
                c.close();
        }

        return thumbnails;
    }

    private void loadTopSitesThumbnails(final ContentResolver cr) {
        final List<String> urls = getTopSitesUrls();
        if (urls.size() == 0)
            return;

        (new GeckoAsyncTask<Void, Void, Cursor>(GeckoApp.mAppContext, GeckoAppShell.getHandler()) {
            @Override
            public Cursor doInBackground(Void... params) {
                return BrowserDB.getThumbnailsForUrls(cr, urls);
            }

            @Override
            public void onPostExecute(Cursor c) {
                updateTopSitesThumbnails(getTopSitesThumbnails(c));
            }
        }).execute();
    }

    void update(final EnumSet<UpdateFlags> flags) {
        GeckoAppShell.getHandler().post(new Runnable() {
            public void run() {
                if (flags.contains(UpdateFlags.TOP_SITES))
                    loadTopSites();

                if (flags.contains(UpdateFlags.PREVIOUS_TABS))
                    readLastTabs();

                if (flags.contains(UpdateFlags.RECOMMENDED_ADDONS))
                    readRecommendedAddons();

                if (flags.contains(UpdateFlags.REMOTE_TABS))
                    loadRemoteTabs();
            }
        });
    }

    public void setUriLoadCallback(UriLoadCallback uriLoadCallback) {
        mUriLoadCallback = uriLoadCallback;
    }

    public void setLoadCompleteCallback(VoidCallback callback) {
        mLoadCompleteCallback = callback;
    }

    public void onActivityContentChanged() {
        update(EnumSet.of(UpdateFlags.TOP_SITES));
    }

    private void setTopSitesConstants() {
        mNumberOfTopSites = getResources().getInteger(R.integer.number_of_top_sites);
        mNumberOfCols = getResources().getInteger(R.integer.number_of_top_sites_cols);
    }

    /**
     * Reinflates and updates all components of this view.
     */
    public void refresh() {
        if (mTopSitesAdapter != null)
            mTopSitesAdapter.notifyDataSetChanged();

        removeAllViews(); // We must remove the currently inflated view to allow for reinflation.
        inflate();
        mTopSitesGrid.setAdapter(mTopSitesAdapter); // mTopSitesGrid is a new instance (from loadTopSites()).
        update(AboutHomeContent.UpdateFlags.ALL); // Refresh all elements.
    }

    private String readFromZipFile(String filename) {
        ZipFile zip = null;
        String str = null;
        try {
            InputStream fileStream = null;
            File applicationPackage = new File(mActivity.getApplication().getPackageResourcePath());
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

    private void readRecommendedAddons() {
        final String addonsFilename = "recommended-addons.json";
        String jsonString;
        try {
            jsonString = mActivity.getProfile().readFile(addonsFilename);
        } catch (IOException ioe) {
            Log.i(LOGTAG, "filestream is null");
            jsonString = readFromZipFile(addonsFilename);
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
        post(new Runnable() {
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

                        Favicons favicons = Favicons.getInstance();
                        favicons.loadFavicon(pageUrl, iconUrl, true,
                                    new Favicons.OnFaviconLoadedListener() {
                            public void onFaviconLoaded(String url, Bitmap favicon) {
                                if (favicon != null) {
                                    ImageView icon = (ImageView) row.findViewById(R.id.addon_icon);
                                    icon.setImageBitmap(favicon);
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

    private void readLastTabs() {
        String jsonString = mActivity.getProfile().readSessionFile(true);
        if (jsonString == null) {
            // no previous session data
            return;
        }

        final ArrayList<String> lastTabUrlsList = new ArrayList<String>();
        new SessionParser() {
            @Override
            public void onTabRead(final SessionTab tab) {
                final String url = tab.getSelectedUrl();
                // don't show last tabs for about:home
                if (url.equals("about:home")) {
                    return;
                }

                ContentResolver resolver = mActivity.getContentResolver();
                final Bitmap favicon = BrowserDB.getFaviconForUrl(resolver, url);
                lastTabUrlsList.add(url);

                AboutHomeContent.this.post(new Runnable() {
                    public void run() {
                        View container = mInflater.inflate(R.layout.abouthome_last_tabs_row, mLastTabs.getItemsContainer(), false);
                        ((TextView) container.findViewById(R.id.last_tab_title)).setText(tab.getSelectedTitle());
                        ((TextView) container.findViewById(R.id.last_tab_url)).setText(tab.getSelectedUrl());
                        if (favicon != null) {
                            ((ImageView) container.findViewById(R.id.last_tab_favicon)).setImageBitmap(favicon);
                        }

                        container.setOnClickListener(new View.OnClickListener() {
                            public void onClick(View v) {
                                Tabs.getInstance().loadUrlInTab(url);
                            }
                        });

                        mLastTabs.addItem(container);
                    }
                });
            }
        }.parse(jsonString);

        final int numLastTabs = lastTabUrlsList.size();
        if (numLastTabs >= 1) {
            post(new Runnable() {
                public void run() {
                    if (numLastTabs > 1) {
                        mLastTabs.showMoreText();
                        mLastTabs.setOnMoreTextClickListener(new View.OnClickListener() {
                            public void onClick(View v) {
                                for (String url : lastTabUrlsList) {
                                    Tabs.getInstance().loadUrlInTab(url);
                                }
                            }
                        });
                    } else if (numLastTabs == 1) {
                        mLastTabs.hideMoreText();
                    }
                    mLastTabs.show();
                }
            });
        }
    }

    private void loadRemoteTabs() {
        if (!SyncAccounts.syncAccountsExist(mActivity)) {
            post(new Runnable() {
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

            final RelativeLayout row = (RelativeLayout) mInflater.inflate(R.layout.abouthome_remote_tab_row, mRemoteTabs.getItemsContainer(), false);
            ((TextView) row.findViewById(R.id.remote_tab_title)).setText(TextUtils.isEmpty(tab.title) ? tab.url : tab.title);
            row.setTag(tab.url);
            mRemoteTabs.addItem(row);
            row.setOnClickListener(mRemoteTabClickListener);
        }
        
        mRemoteTabs.setSubtitle(client);
        mRemoteTabs.show();
    }

    @Override
    public void onLightweightThemeChanged() {
        LightweightThemeDrawable drawable = mActivity.getLightweightTheme().getColorDrawable(this);
        if (drawable == null)
            return;

         drawable.setAlpha(255, 0);
         setBackgroundDrawable(drawable);

         boolean isLight = mActivity.getLightweightTheme().isLightTheme();

         if (mAddons != null) {
             mAddons.setTheme(isLight);
             mLastTabs.setTheme(isLight);
             mRemoteTabs.setTheme(isLight);
             ((GeckoImageView) findViewById(R.id.abouthome_logo)).setTheme(isLight);
             ((GeckoTextView) findViewById(R.id.top_sites_title)).setTheme(isLight);
         }
    }

    @Override
    public void onLightweightThemeReset() {
        setBackgroundResource(R.drawable.abouthome_bg_repeat);

        if (mAddons != null) {
            mAddons.resetTheme();
            mLastTabs.resetTheme();
            mRemoteTabs.resetTheme();
            ((GeckoImageView) findViewById(R.id.abouthome_logo)).resetTheme();
            ((GeckoTextView) findViewById(R.id.top_sites_title)).resetTheme();
        }
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        onLightweightThemeChanged();
    }

    public static class TopSitesGridView extends GridView {
        public TopSitesGridView(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        public int getColumnWidth() {
            return getColumnWidth(getWidth());
        }

        public int getColumnWidth(int width) {
            int s = -1;
            if (android.os.Build.VERSION.SDK_INT >= 16)
                s= super.getColumnWidth();
            else
                s = (width - getPaddingLeft() - getPaddingRight()) / mNumberOfCols;

            return s;
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            int measuredWidth = View.MeasureSpec.getSize(widthMeasureSpec);
            int numRows;

            SimpleCursorAdapter adapter = (SimpleCursorAdapter) getAdapter();
            int nSites = Integer.MAX_VALUE;

            if (adapter != null) {
                Cursor c = adapter.getCursor();
                if (c != null)
                    nSites = c.getCount();
            }

            nSites = Math.min(nSites, mNumberOfTopSites);
            numRows = (int) Math.round((double) nSites / mNumberOfCols);
            setNumColumns(mNumberOfCols);

            // Just using getWidth() will use incorrect values during onMeasure when rotating the device
            // Instead we pass in the measuredWidth, which is correct
            int w = getColumnWidth(measuredWidth);
            ThumbnailHelper.getInstance().setThumbnailWidth(w);
            heightMeasureSpec = MeasureSpec.makeMeasureSpec((int)(w*ThumbnailHelper.THUMBNAIL_ASPECT_RATIO*numRows) + getPaddingTop() + getPaddingBottom(),
                                                                 MeasureSpec.EXACTLY);
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }
    }

    public class TopSitesCursorAdapter extends SimpleCursorAdapter {
        public TopSitesCursorAdapter(Context context, int layout, Cursor c,
                                     String[] from, int[] to) {
            super(context, layout, c, from, to);
        }

        @Override
        public int getCount() {
            return Math.min(super.getCount(), mNumberOfTopSites);
        }

        @Override
        protected void onContentChanged () {
            // Don't do anything. We don't want to regenerate every time
            // our history database is updated.
            return;
        }

        @Override
        public void bindView(View view, Context context, Cursor cursor) {
            super.bindView(view, context, cursor);
            view.setLayoutParams(new AbsListView.LayoutParams(mTopSitesGrid.getColumnWidth(),
                                                            Math.round(mTopSitesGrid.getColumnWidth()*ThumbnailHelper.THUMBNAIL_ASPECT_RATIO)));
        }
    }

    class TopSitesViewBinder implements SimpleCursorAdapter.ViewBinder {
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

            // Other columns are handled automatically
            return false;
        }
    }
}
