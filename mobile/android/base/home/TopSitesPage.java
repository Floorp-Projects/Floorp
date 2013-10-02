/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.favicons.Favicons;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.PropertyAnimator.Property;
import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.db.BrowserContract.Thumbnails;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.db.BrowserDB.TopSitesCursorWrapper;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.home.HomeListView.HomeContextMenuInfo;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.home.PinSiteDialog.OnSiteSelectedListener;
import org.mozilla.gecko.home.TopSitesGridView.OnPinSiteListener;
import org.mozilla.gecko.home.TopSitesGridView.TopSitesGridContextMenuInfo;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Configuration;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.LoaderManager;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.AsyncTaskLoader;
import android.support.v4.content.Loader;
import android.support.v4.widget.CursorAdapter;
import android.text.TextUtils;
import android.util.Log;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnTouchListener;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.Toast;

import java.util.EnumSet;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

/**
 * Fragment that displays frecency search results in a ListView.
 */
public class TopSitesPage extends HomeFragment {
    // Logging tag name
    private static final String LOGTAG = "GeckoTopSitesPage";

    // Cursor loader ID for the top sites
    private static final int LOADER_ID_TOP_SITES = 0;

    // Loader ID for thumbnails
    private static final int LOADER_ID_THUMBNAILS = 1;

    // Key for thumbnail urls
    private static final String THUMBNAILS_URLS_KEY = "urls";

    // Adapter for the list of top sites
    private VisitedAdapter mListAdapter;

    // Adapter for the grid of top sites
    private TopSitesGridAdapter mGridAdapter;

    // List of top sites
    private ListView mList;

    // Grid of top sites
    private TopSitesGridView mGrid;

    // Banner to show snippets.
    private HomeBanner mBanner;

    // Raw Y value of the last event that happened on the list view.
    private float mListTouchY = -1;

    // Scrolling direction of the banner.
    private boolean mSnapBannerToTop;

    // Callbacks used for the search and favicon cursor loaders
    private CursorLoaderCallbacks mCursorLoaderCallbacks;

    // Callback for thumbnail loader
    private ThumbnailsLoaderCallbacks mThumbnailsLoaderCallbacks;

    // Listener for pinning sites
    private PinSiteListener mPinSiteListener;

    // On URL open listener
    private OnUrlOpenListener mUrlOpenListener;

    // Max number of entries shown in the grid from the cursor.
    private int mMaxGridEntries;

    // Time in ms until the Gecko thread is reset to normal priority.
    private static final long PRIORITY_RESET_TIMEOUT = 10000;

    /**
     *  Class to hold the bitmap of cached thumbnails/favicons.
     */
    public static class Thumbnail {
        // Thumbnail or favicon.
        private final boolean isThumbnail;

        // Bitmap of thumbnail/favicon.
        private final Bitmap bitmap;

        public Thumbnail(Bitmap bitmap, boolean isThumbnail) {
            this.bitmap = bitmap;
            this.isThumbnail = isThumbnail;
        }
    }

    public static TopSitesPage newInstance() {
        return new TopSitesPage();
    }

    public TopSitesPage() {
        mUrlOpenListener = null;
    }

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);

        mMaxGridEntries = activity.getResources().getInteger(R.integer.number_of_top_sites);

        try {
            mUrlOpenListener = (OnUrlOpenListener) activity;
        } catch (ClassCastException e) {
            throw new ClassCastException(activity.toString()
                    + " must implement HomePager.OnUrlOpenListener");
        }
    }

    @Override
    public void onDetach() {
        super.onDetach();

        mUrlOpenListener = null;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        final View view = inflater.inflate(R.layout.home_top_sites_page, container, false);

        mList = (HomeListView) view.findViewById(R.id.list);

        mGrid = new TopSitesGridView(getActivity());
        mList.addHeaderView(mGrid);

        return view;
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        mPinSiteListener = new PinSiteListener();

        mList.setTag(HomePager.LIST_TAG_TOP_SITES);
        mList.setHeaderDividersEnabled(false);

        mList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                final ListView list = (ListView) parent;
                final int headerCount = list.getHeaderViewsCount();
                if (position < headerCount) {
                    // The click is on a header, don't do anything.
                    return;
                }

                // Absolute position for the adapter.
                position += (mGridAdapter.getCount() - headerCount);

                final Cursor c = mListAdapter.getCursor();
                if (c == null || !c.moveToPosition(position)) {
                    return;
                }

                final String url = c.getString(c.getColumnIndexOrThrow(URLColumns.URL));

                // This item is a TwoLinePageRow, so we allow switch-to-tab.
                mUrlOpenListener.onUrlOpen(url, EnumSet.of(OnUrlOpenListener.Flags.ALLOW_SWITCH_TO_TAB));
            }
        });

        mGrid.setOnUrlOpenListener(mUrlOpenListener);
        mGrid.setOnPinSiteListener(mPinSiteListener);

        registerForContextMenu(mList);
        registerForContextMenu(mGrid);

        mBanner = (HomeBanner) view.findViewById(R.id.home_banner);
        mList.setOnTouchListener(new OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                TopSitesPage.this.handleListTouchEvent(event);
                return false;
            }
        });
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        mList = null;
        mGrid = null;
        mListAdapter = null;
        mGridAdapter = null;
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        // Detach and reattach the fragment as the layout changes.
        if (isVisible()) {
            getFragmentManager().beginTransaction()
                                .detach(this)
                                .attach(this)
                                .commitAllowingStateLoss();
        }
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        final Activity activity = getActivity();

        // Setup the top sites grid adapter.
        mGridAdapter = new TopSitesGridAdapter(activity, null);
        mGrid.setAdapter(mGridAdapter);

        // Setup the top sites list adapter.
        mListAdapter = new VisitedAdapter(activity, null);
        mList.setAdapter(mListAdapter);

        // Create callbacks before the initial loader is started
        mCursorLoaderCallbacks = new CursorLoaderCallbacks();
        mThumbnailsLoaderCallbacks = new ThumbnailsLoaderCallbacks();
        loadIfVisible();
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View view, ContextMenuInfo menuInfo) {
        if (menuInfo == null) {
            return;
        }

        // HomeFragment will handle the default case.
        if (menuInfo instanceof HomeContextMenuInfo) {
            super.onCreateContextMenu(menu, view, menuInfo);
        }

        if (!(menuInfo instanceof TopSitesGridContextMenuInfo)) {
            return;
        }

        MenuInflater inflater = new MenuInflater(view.getContext());
        inflater.inflate(R.menu.top_sites_contextmenu, menu);

        TopSitesGridContextMenuInfo info = (TopSitesGridContextMenuInfo) menuInfo;
        menu.setHeaderTitle(info.getDisplayTitle());

        if (!TextUtils.isEmpty(info.url)) {
            if (info.isPinned) {
                menu.findItem(R.id.top_sites_pin).setVisible(false);
            } else {
                menu.findItem(R.id.top_sites_unpin).setVisible(false);
            }
        } else {
            menu.findItem(R.id.top_sites_open_new_tab).setVisible(false);
            menu.findItem(R.id.top_sites_open_private_tab).setVisible(false);
            menu.findItem(R.id.top_sites_pin).setVisible(false);
            menu.findItem(R.id.top_sites_unpin).setVisible(false);
        }
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        ContextMenuInfo menuInfo = item.getMenuInfo();

        // HomeFragment will handle the default case.
        if (menuInfo == null || !(menuInfo instanceof TopSitesGridContextMenuInfo)) {
            return false;
        }

        TopSitesGridContextMenuInfo info = (TopSitesGridContextMenuInfo) menuInfo;
        final Activity activity = getActivity();

        final int itemId = item.getItemId();
        if (itemId == R.id.top_sites_open_new_tab || itemId == R.id.top_sites_open_private_tab) {
            if (info.url == null) {
                Log.e(LOGTAG, "Can't open in new tab because URL is null");
                return false;
            }

            int flags = Tabs.LOADURL_NEW_TAB | Tabs.LOADURL_BACKGROUND;
            if (item.getItemId() == R.id.top_sites_open_private_tab)
                flags |= Tabs.LOADURL_PRIVATE;

            Tabs.getInstance().loadUrl(info.url, flags);
            Toast.makeText(activity, R.string.new_tab_opened, Toast.LENGTH_SHORT).show();
            return true;
        }

        if (itemId == R.id.top_sites_pin) {
            final String url = info.url;
            final String title = info.title;
            final int position = info.position;
            final Context context = getActivity().getApplicationContext();

            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    BrowserDB.pinSite(context.getContentResolver(), url, title, position);
                }
            });

            return true;
        }

        if (itemId == R.id.top_sites_unpin) {
            final int position = info.position;
            final Context context = getActivity().getApplicationContext();

            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    BrowserDB.unpinSite(context.getContentResolver(), position);
                }
            });

            return true;
        }

        if (itemId == R.id.top_sites_edit) {
            mPinSiteListener.onPinSite(info.position);
            return true;
        }

        return false;
    }

    @Override
    protected void load() {
        getLoaderManager().initLoader(LOADER_ID_TOP_SITES, null, mCursorLoaderCallbacks);

        // Since this is the primary fragment that loads whenever about:home is
        // visited, we want to load it as quickly as possible. Heavy load on
        // the Gecko thread can slow down the time it takes for thumbnails to
        // appear, especially during startup (bug 897162). By minimizing the
        // Gecko thread priority, we ensure that the UI appears quickly. The
        // priority is reset to normal once thumbnails are loaded.
        ThreadUtils.reduceGeckoPriority(PRIORITY_RESET_TIMEOUT);
    }

    /**
     * Listener for pinning sites.
     */
    private class PinSiteListener implements OnPinSiteListener,
                                             OnSiteSelectedListener {
        // Tag for the PinSiteDialog fragment.
        private static final String TAG_PIN_SITE = "pin_site";

        // Position of the pin.
        private int mPosition;

        @Override
        public void onPinSite(int position) {
            mPosition = position;

            final FragmentManager manager = getActivity().getSupportFragmentManager();
            PinSiteDialog dialog = (PinSiteDialog) manager.findFragmentByTag(TAG_PIN_SITE);
            if (dialog == null) {
                dialog = PinSiteDialog.newInstance();
            }

            dialog.setOnSiteSelectedListener(this);
            dialog.show(manager, TAG_PIN_SITE);
        }

        @Override
        public void onSiteSelected(final String url, final String title) {
            final int position = mPosition;
            final Context context = getActivity().getApplicationContext();
            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    BrowserDB.pinSite(context.getContentResolver(), url, title, position);
                }
            });
        }
    }

    private void handleListTouchEvent(MotionEvent event) {
        // Ignore the event if the banner is hidden for this session.
        if (mBanner.isDismissed()) {
            return;
        }

        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN: {
                mListTouchY = event.getRawY();
                break;
            }

            case MotionEvent.ACTION_MOVE: {
                // There is a chance that we won't receive ACTION_DOWN, if the touch event
                // actually started on the Grid instead of the List. Treat this as first event.
                if (mListTouchY == -1) {
                    mListTouchY = event.getRawY();
                    return;
                }

                final float curY = event.getRawY();
                final float delta = mListTouchY - curY;
                mSnapBannerToTop = (delta > 0.0f) ? false : true;

                final float height = mBanner.getHeight();
                float newTranslationY = ViewHelper.getTranslationY(mBanner) + delta;

                // Clamp the values to be between 0 and height.
                if (newTranslationY < 0.0f) {
                    newTranslationY = 0.0f;
                } else if (newTranslationY > height) {
                    newTranslationY = height;
                }

                ViewHelper.setTranslationY(mBanner, newTranslationY);
                mListTouchY = curY;
                break;
            }

            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL: {
                mListTouchY = -1;
                final float y = ViewHelper.getTranslationY(mBanner);
                final float height = mBanner.getHeight();
                if (y > 0.0f && y < height) {
                    final PropertyAnimator animator = new PropertyAnimator(100);
                    animator.attach(mBanner, Property.TRANSLATION_Y, mSnapBannerToTop ? 0 : height);
                    animator.start();
                }
                break;
            }
        }
    }

    private void updateUiFromCursor(Cursor c) {
        mList.setHeaderDividersEnabled(c != null && c.getCount() > mMaxGridEntries);
    }

    private static class TopSitesLoader extends SimpleCursorLoader {
        // Max number of search results
        private static final int SEARCH_LIMIT = 30;
        private int mMaxGridEntries;

        public TopSitesLoader(Context context) {
            super(context);
            mMaxGridEntries = context.getResources().getInteger(R.integer.number_of_top_sites);
        }

        @Override
        public Cursor loadCursor() {
            Log.d(LOGTAG, "TopSitesLoader.loadCursor()");
            return BrowserDB.getTopSites(getContext().getContentResolver(), mMaxGridEntries, SEARCH_LIMIT);
        }
    }

    private class VisitedAdapter extends CursorAdapter {
        public VisitedAdapter(Context context, Cursor cursor) {
            super(context, cursor);
        }

        @Override
        public int getCount() {
            return Math.max(0, super.getCount() - mMaxGridEntries);
        }

        @Override
        public Object getItem(int position) {
            return super.getItem(position + mMaxGridEntries);
        }

        @Override
        public void bindView(View view, Context context, Cursor cursor) {
            final int position = cursor.getPosition();
            cursor.moveToPosition(position + mMaxGridEntries);

            final TwoLinePageRow row = (TwoLinePageRow) view;
            row.updateFromCursor(cursor);
        }

        @Override
        public View newView(Context context, Cursor cursor, ViewGroup parent) {
            return LayoutInflater.from(context).inflate(R.layout.bookmark_item_row, parent, false);
        }
    }

    public class TopSitesGridAdapter extends CursorAdapter {
        // Cache to store the thumbnails.
        private Map<String, Thumbnail> mThumbnails;

        public TopSitesGridAdapter(Context context, Cursor cursor) {
            super(context, cursor);
        }

        @Override
        public int getCount() {
            return Math.min(mMaxGridEntries, super.getCount());
        }

        @Override
        protected void onContentChanged() {
            // Don't do anything. We don't want to regenerate every time
            // our database is updated.
            return;
        }

        /**
         * Update the thumbnails returned by the db.
         *
         * @param thumbnails A map of urls and their thumbnail bitmaps.
         */
        public void updateThumbnails(Map<String, Thumbnail> thumbnails) {
            mThumbnails = thumbnails;
            notifyDataSetChanged();
        }

        @Override
        public void bindView(View bindView, Context context, Cursor cursor) {
            String url = "";
            String title = "";
            boolean pinned = false;

            // Cursor is already moved to required position.
            if (!cursor.isAfterLast()) {
                url = cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.URL));
                title = cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.TITLE));
                pinned = ((TopSitesCursorWrapper) cursor).isPinned();
            }

            TopSitesGridItemView view = (TopSitesGridItemView) bindView;
            view.setTitle(title);
            view.setUrl(url);
            view.setPinned(pinned);

            // If there is no url, then show "add bookmark".
            if (TextUtils.isEmpty(url)) {
                view.displayThumbnail(R.drawable.top_site_add);
            } else {
                // Show the thumbnail.
                Thumbnail thumbnail = (mThumbnails != null ? mThumbnails.get(url) : null);
                if (thumbnail == null) {
                    view.displayThumbnail(null);
                } else if (thumbnail.isThumbnail) {
                    view.displayThumbnail(thumbnail.bitmap);
                } else {
                    view.displayFavicon(thumbnail.bitmap);
                }
            }
        }

        @Override
        public View newView(Context context, Cursor cursor, ViewGroup parent) {
            return new TopSitesGridItemView(context);
        }
    }

    private class CursorLoaderCallbacks implements LoaderCallbacks<Cursor> {
        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            Log.d(LOGTAG, "Creating TopSitesLoader: " + id);
            return new TopSitesLoader(getActivity());
        }

        /**
         * This method is called *twice* in some circumstances.
         *
         * If you try to avoid that through some kind of boolean flag,
         * sometimes (e.g., returning to the activity) you'll *not* be called
         * twice, and thus you'll never draw thumbnails.
         *
         * The root cause is TopSitesLoader.loadCursor being called twice.
         * Why that is... dunno.
         */
        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor c) {
            Log.d(LOGTAG, "onLoadFinished: " + c.getCount() + " rows.");

            mListAdapter.swapCursor(c);
            mGridAdapter.swapCursor(c);
            updateUiFromCursor(c);

            final int col = c.getColumnIndexOrThrow(URLColumns.URL);

            // Load the thumbnails.
            // Even though the cursor we're given is supposed to be fresh,
            // we get a bad first value unless we reset its position.
            // Using move(-1) and moveToNext() doesn't work correctly under
            // rotation, so we use moveToFirst.
            if (!c.moveToFirst()) {
                return;
            }
            
            final ArrayList<String> urls = new ArrayList<String>();
            int i = 1;
            do {
                urls.add(c.getString(col));
            } while (i++ < mMaxGridEntries && c.moveToNext());

            if (urls.isEmpty()) {
                return;
            }

            Bundle bundle = new Bundle();
            bundle.putStringArrayList(THUMBNAILS_URLS_KEY, urls);
            getLoaderManager().restartLoader(LOADER_ID_THUMBNAILS, bundle, mThumbnailsLoaderCallbacks);
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            if (mListAdapter != null) {
                mListAdapter.swapCursor(null);
            }

            if (mGridAdapter != null) {
                mGridAdapter.swapCursor(null);
            }
        }
    }

    /**
     * An AsyncTaskLoader to load the thumbnails from a cursor.
     */
    private static class ThumbnailsLoader extends AsyncTaskLoader<Map<String, Thumbnail>> {
        private Map<String, Thumbnail> mThumbnails;
        private ArrayList<String> mUrls;

        public ThumbnailsLoader(Context context, ArrayList<String> urls) {
            super(context);
            mUrls = urls;
        }

        @Override
        public Map<String, Thumbnail> loadInBackground() {
            if (mUrls == null || mUrls.size() == 0) {
                return null;
            }

            // Query the DB for thumbnails.
            final ContentResolver cr = getContext().getContentResolver();
            final Cursor cursor = BrowserDB.getThumbnailsForUrls(cr, mUrls);

            if (cursor == null) {
                return null;
            }

            final Map<String, Thumbnail> thumbnails = new HashMap<String, Thumbnail>();

            try {
                final int urlIndex = cursor.getColumnIndexOrThrow(Thumbnails.URL);
                final int dataIndex = cursor.getColumnIndexOrThrow(Thumbnails.DATA);

                while (cursor.moveToNext()) {
                    String url = cursor.getString(urlIndex);

                    // This should never be null, but if it is...
                    final byte[] b = cursor.getBlob(dataIndex);
                    if (b == null) {
                        continue;
                    }

                    final Bitmap bitmap = BitmapUtils.decodeByteArray(b);

                    // Our thumbnails are never null, so if we get a null decoded
                    // bitmap, it's because we hit an OOM or some other disaster.
                    // Give up immediately rather than hammering on.
                    if (bitmap == null) {
                        Log.w(LOGTAG, "Aborting thumbnail load; decode failed.");
                        break;
                    }

                    thumbnails.put(url, new Thumbnail(bitmap, true));
                }
            } finally {
                cursor.close();
            }

            // Query the DB for favicons for the urls without thumbnails.
            for (String url : mUrls) {
                if (!thumbnails.containsKey(url)) {
                    final Bitmap bitmap = BrowserDB.getFaviconForUrl(cr, url);
                    if (bitmap != null) {
                        // Favicons.scaleImage can return several different size favicons,
                        // but will at least prevent this from being too large.
                        thumbnails.put(url, new Thumbnail(Favicons.scaleImage(bitmap), false));
                    }
                }
            }

            return thumbnails;
        }

        @Override
        public void deliverResult(Map<String, Thumbnail> thumbnails) {
            if (isReset()) {
                mThumbnails = null;
                return;
            }

            mThumbnails = thumbnails;

            if (isStarted()) {
                super.deliverResult(thumbnails);
            }
        }

        @Override
        protected void onStartLoading() {
            if (mThumbnails != null) {
                deliverResult(mThumbnails);
            }

            if (takeContentChanged() || mThumbnails == null) {
                forceLoad();
            }
        }

        @Override
        protected void onStopLoading() {
            cancelLoad();
        }

        @Override
        public void onCanceled(Map<String, Thumbnail> thumbnails) {
            mThumbnails = null;
        }

        @Override
        protected void onReset() {
            super.onReset();

            // Ensure the loader is stopped.
            onStopLoading();

            mThumbnails = null;
        }
    }

    /**
     * Loader callbacks for the thumbnails on TopSitesGridView.
     */
    private class ThumbnailsLoaderCallbacks implements LoaderCallbacks<Map<String, Thumbnail>> {
        @Override
        public Loader<Map<String, Thumbnail>> onCreateLoader(int id, Bundle args) {
            return new ThumbnailsLoader(getActivity(), args.getStringArrayList(THUMBNAILS_URLS_KEY));
        }

        @Override
        public void onLoadFinished(Loader<Map<String, Thumbnail>> loader, Map<String, Thumbnail> thumbnails) {
            if (mGridAdapter != null) {
                mGridAdapter.updateThumbnails(thumbnails);
            }

            // Once thumbnails have finished loading, the UI is ready. Reset
            // Gecko to normal priority.
            ThreadUtils.resetGeckoPriority();
        }

        @Override
        public void onLoaderReset(Loader<Map<String, Thumbnail>> loader) {
            if (mGridAdapter != null) {
                mGridAdapter.updateThumbnails(null);
            }
        }
    }
}
