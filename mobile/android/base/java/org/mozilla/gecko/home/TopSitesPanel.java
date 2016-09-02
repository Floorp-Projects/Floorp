/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import static org.mozilla.gecko.db.URLMetadataTable.TILE_COLOR_COLUMN;
import static org.mozilla.gecko.db.URLMetadataTable.TILE_IMAGE_URL_COLUMN;

import java.util.ArrayList;
import java.util.Collections;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Future;

import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.BrowserContract.Thumbnails;
import org.mozilla.gecko.db.BrowserContract.TopSites;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.home.HomeContextMenuInfo.RemoveItemType;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.home.PinSiteDialog.OnSiteSelectedListener;
import org.mozilla.gecko.home.TopSitesGridView.OnEditPinnedSiteListener;
import org.mozilla.gecko.home.TopSitesGridView.TopSitesGridContextMenuInfo;
import org.mozilla.gecko.icons.IconCallback;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.mozilla.gecko.restrictions.Restrictable;
import org.mozilla.gecko.restrictions.Restrictions;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.os.SystemClock;
import android.support.v4.app.FragmentManager;
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
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ListView;

/**
 * Fragment that displays frecency search results in a ListView.
 */
public class TopSitesPanel extends HomeFragment {
    // Logging tag name
    private static final String LOGTAG = "GeckoTopSitesPanel";

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
    private HomeListView mList;

    // Grid of top sites
    private TopSitesGridView mGrid;

    // Callbacks used for the search and favicon cursor loaders
    private CursorLoaderCallbacks mCursorLoaderCallbacks;

    // Callback for thumbnail loader
    private ThumbnailsLoaderCallbacks mThumbnailsLoaderCallbacks;

    // Listener for editing pinned sites.
    private EditPinnedSiteListener mEditPinnedSiteListener;

    // Max number of entries shown in the grid from the cursor.
    private int mMaxGridEntries;

    // Time in ms until the Gecko thread is reset to normal priority.
    private static final long PRIORITY_RESET_TIMEOUT = 10000;

    public static TopSitesPanel newInstance() {
        return new TopSitesPanel();
    }

    private static final boolean logDebug = Log.isLoggable(LOGTAG, Log.DEBUG);
    private static final boolean logVerbose = Log.isLoggable(LOGTAG, Log.VERBOSE);

    private static void debug(final String message) {
        if (logDebug) {
            Log.d(LOGTAG, message);
        }
    }

    private static void trace(final String message) {
        if (logVerbose) {
            Log.v(LOGTAG, message);
        }
    }

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);

        mMaxGridEntries = activity.getResources().getInteger(R.integer.number_of_top_sites);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        final View view = inflater.inflate(R.layout.home_top_sites_panel, container, false);

        mList = (HomeListView) view.findViewById(R.id.list);

        mGrid = new TopSitesGridView(getActivity());
        mList.addHeaderView(mGrid);

        return view;
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        mEditPinnedSiteListener = new EditPinnedSiteListener();

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

                final String url = c.getString(c.getColumnIndexOrThrow(TopSites.URL));

                Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.LIST_ITEM, "top_sites");

                // This item is a TwoLinePageRow, so we allow switch-to-tab.
                mUrlOpenListener.onUrlOpen(url, EnumSet.of(OnUrlOpenListener.Flags.ALLOW_SWITCH_TO_TAB));
            }
        });

        mList.setContextMenuInfoFactory(new HomeContextMenuInfo.Factory() {
            @Override
            public HomeContextMenuInfo makeInfoForCursor(View view, int position, long id, Cursor cursor) {
                final HomeContextMenuInfo info = new HomeContextMenuInfo(view, position, id);
                info.url = cursor.getString(cursor.getColumnIndexOrThrow(TopSites.URL));
                info.title = cursor.getString(cursor.getColumnIndexOrThrow(TopSites.TITLE));
                info.historyId = cursor.getInt(cursor.getColumnIndexOrThrow(TopSites.HISTORY_ID));
                info.itemType = RemoveItemType.HISTORY;
                final int bookmarkIdCol = cursor.getColumnIndexOrThrow(TopSites.BOOKMARK_ID);
                if (cursor.isNull(bookmarkIdCol)) {
                    // If this is a combined cursor, we may get a history item without a
                    // bookmark, in which case the bookmarks ID column value will be null.
                    info.bookmarkId =  -1;
                } else {
                    info.bookmarkId = cursor.getInt(bookmarkIdCol);
                }
                return info;
            }
        });

        mGrid.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                TopSitesGridItemView item = (TopSitesGridItemView) view;

                // Decode "user-entered" URLs before loading them.
                String url = StringUtils.decodeUserEnteredUrl(item.getUrl());
                int type = item.getType();

                // If the url is empty, the user can pin a site.
                // If not, navigate to the page given by the url.
                if (type != TopSites.TYPE_BLANK) {
                    if (mUrlOpenListener != null) {
                        final TelemetryContract.Method method;
                        if (type == TopSites.TYPE_SUGGESTED) {
                            method = TelemetryContract.Method.SUGGESTION;
                        } else {
                            method = TelemetryContract.Method.GRID_ITEM;
                        }

                        String extra = Integer.toString(position);
                        if (type == TopSites.TYPE_PINNED) {
                            extra += "-pinned";
                        }

                        Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, method, extra);

                        mUrlOpenListener.onUrlOpen(url, EnumSet.of(OnUrlOpenListener.Flags.NO_READER_VIEW));
                    }
                } else {
                    if (mEditPinnedSiteListener != null) {
                        mEditPinnedSiteListener.onEditPinnedSite(position, "");
                    }
                }
            }
        });

        mGrid.setOnItemLongClickListener(new AdapterView.OnItemLongClickListener() {
            @Override
            public boolean onItemLongClick(AdapterView<?> parent, View view, int position, long id) {

                Cursor cursor = (Cursor) parent.getItemAtPosition(position);

                TopSitesGridItemView item = (TopSitesGridItemView) view;
                if (cursor == null || item.getType() == TopSites.TYPE_BLANK) {
                    mGrid.setContextMenuInfo(null);
                    return false;
                }

                TopSitesGridContextMenuInfo contextMenuInfo = new TopSitesGridContextMenuInfo(view, position, id);
                updateContextMenuFromCursor(contextMenuInfo, cursor);
                mGrid.setContextMenuInfo(contextMenuInfo);
                return mGrid.showContextMenuForChild(mGrid);
            }

            /*
             * Update the fields of a TopSitesGridContextMenuInfo object
             * from a cursor.
             *
             * @param  info    context menu info object to be updated
             * @param  cursor  used to update the context menu info object
             */
            private void updateContextMenuFromCursor(TopSitesGridContextMenuInfo info, Cursor cursor) {
                info.url = cursor.getString(cursor.getColumnIndexOrThrow(TopSites.URL));
                info.title = cursor.getString(cursor.getColumnIndexOrThrow(TopSites.TITLE));
                info.type = cursor.getInt(cursor.getColumnIndexOrThrow(TopSites.TYPE));
                info.historyId = cursor.getInt(cursor.getColumnIndexOrThrow(TopSites.HISTORY_ID));
            }
        });

        registerForContextMenu(mList);
        registerForContextMenu(mGrid);
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();

        // Discard any additional item clicks on the list as the
        // panel is getting destroyed (see bugs 930160 & 1096958).
        mList.setOnItemClickListener(null);
        mGrid.setOnItemClickListener(null);

        mList = null;
        mGrid = null;
        mListAdapter = null;
        mGridAdapter = null;
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

        if (!(menuInfo instanceof TopSitesGridContextMenuInfo)) {
            // Long pressed item was not a Top Sites GridView item. Superclass
            // can handle this.
            super.onCreateContextMenu(menu, view, menuInfo);

            if (!Restrictions.isAllowed(view.getContext(), Restrictable.CLEAR_HISTORY)) {
                menu.findItem(R.id.home_remove).setVisible(false);
            }

            return;
        }

        final Context context = view.getContext();

        // Long pressed item was a Top Sites GridView item, handle it.
        MenuInflater inflater = new MenuInflater(context);
        inflater.inflate(R.menu.home_contextmenu, menu);

        // Hide unused menu items.
        menu.findItem(R.id.home_edit_bookmark).setVisible(false);

        menu.findItem(R.id.home_remove).setVisible(Restrictions.isAllowed(context, Restrictable.CLEAR_HISTORY));

        TopSitesGridContextMenuInfo info = (TopSitesGridContextMenuInfo) menuInfo;
        menu.setHeaderTitle(info.getDisplayTitle());

        if (info.type != TopSites.TYPE_BLANK) {
            if (info.type == TopSites.TYPE_PINNED) {
                menu.findItem(R.id.top_sites_pin).setVisible(false);
            } else {
                menu.findItem(R.id.top_sites_unpin).setVisible(false);
            }
        } else {
            menu.findItem(R.id.home_open_new_tab).setVisible(false);
            menu.findItem(R.id.home_open_private_tab).setVisible(false);
            menu.findItem(R.id.top_sites_pin).setVisible(false);
            menu.findItem(R.id.top_sites_unpin).setVisible(false);
        }

        if (!StringUtils.isShareableUrl(info.url) || GeckoProfile.get(getActivity()).inGuestMode()) {
            menu.findItem(R.id.home_share).setVisible(false);
        }

        if (!Restrictions.isAllowed(context, Restrictable.PRIVATE_BROWSING)) {
            menu.findItem(R.id.home_open_private_tab).setVisible(false);
        }
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        if (super.onContextItemSelected(item)) {
            // HomeFragment was able to handle to selected item.
            return true;
        }

        ContextMenuInfo menuInfo = item.getMenuInfo();

        if (!(menuInfo instanceof TopSitesGridContextMenuInfo)) {
            return false;
        }

        TopSitesGridContextMenuInfo info = (TopSitesGridContextMenuInfo) menuInfo;

        final int itemId = item.getItemId();
        final BrowserDB db = GeckoProfile.get(getActivity()).getDB();

        if (itemId == R.id.top_sites_pin) {
            final String url = info.url;
            final String title = info.title;
            final int position = info.position;
            final Context context = getActivity().getApplicationContext();

            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    db.pinSite(context.getContentResolver(), url, title, position);
                }
            });

            Telemetry.sendUIEvent(TelemetryContract.Event.PIN);
            return true;
        }

        if (itemId == R.id.top_sites_unpin) {
            final int position = info.position;
            final Context context = getActivity().getApplicationContext();

            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    db.unpinSite(context.getContentResolver(), position);
                }
            });

            Telemetry.sendUIEvent(TelemetryContract.Event.UNPIN);

            return true;
        }

        if (itemId == R.id.top_sites_edit) {
            // Decode "user-entered" URLs before showing them.
            mEditPinnedSiteListener.onEditPinnedSite(info.position,
                                                     StringUtils.decodeUserEnteredUrl(info.url));

            Telemetry.sendUIEvent(TelemetryContract.Event.EDIT);
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
     * Listener for editing pinned sites.
     */
    private class EditPinnedSiteListener implements OnEditPinnedSiteListener,
                                                    OnSiteSelectedListener {
        // Tag for the PinSiteDialog fragment.
        private static final String TAG_PIN_SITE = "pin_site";

        // Position of the pin.
        private int mPosition;

        @Override
        public void onEditPinnedSite(int position, String searchTerm) {
            final FragmentManager manager = getChildFragmentManager();
            PinSiteDialog dialog = (PinSiteDialog) manager.findFragmentByTag(TAG_PIN_SITE);
            if (dialog == null) {
                mPosition = position;

                dialog = PinSiteDialog.newInstance();
                dialog.setOnSiteSelectedListener(this);
                dialog.setSearchTerm(searchTerm);
                dialog.show(manager, TAG_PIN_SITE);
            }
        }

        @Override
        public void onSiteSelected(final String url, final String title) {
            final int position = mPosition;
            final Context context = getActivity().getApplicationContext();
            final BrowserDB db = GeckoProfile.get(getActivity()).getDB();
            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    db.pinSite(context.getContentResolver(), url, title, position);
                }
            });
        }
    }

    private void updateUiFromCursor(Cursor c) {
        mList.setHeaderDividersEnabled(c != null && c.getCount() > mMaxGridEntries);
    }

    private void updateUiWithThumbnails(Map<String, ThumbnailInfo> thumbnails) {
        if (mGridAdapter != null) {
            mGridAdapter.updateThumbnails(thumbnails);
        }

        // Once thumbnails have finished loading, the UI is ready. Reset
        // Gecko to normal priority.
        ThreadUtils.resetGeckoPriority();
    }

    private static class TopSitesLoader extends SimpleCursorLoader {
        // Max number of search results.
        private static final int SEARCH_LIMIT = 30;
        private static final String TELEMETRY_HISTOGRAM_LOAD_CURSOR = "FENNEC_TOPSITES_LOADER_TIME_MS";
        private final BrowserDB mDB;
        private final int mMaxGridEntries;

        public TopSitesLoader(Context context) {
            super(context);
            mMaxGridEntries = context.getResources().getInteger(R.integer.number_of_top_sites);
            mDB = GeckoProfile.get(context).getDB();
        }

        @Override
        public Cursor loadCursor() {
            final long start = SystemClock.uptimeMillis();
            final Cursor cursor = mDB.getTopSites(getContext().getContentResolver(), mMaxGridEntries, SEARCH_LIMIT);
            final long end = SystemClock.uptimeMillis();
            final long took = end - start;
            Telemetry.addToHistogram(TELEMETRY_HISTOGRAM_LOAD_CURSOR, (int) Math.min(took, Integer.MAX_VALUE));
            return cursor;
        }
    }

    private class VisitedAdapter extends CursorAdapter {
        public VisitedAdapter(Context context, Cursor cursor) {
            super(context, cursor, 0);
        }

        @Override
        public int getCount() {
            return Math.max(0, super.getCount() - mMaxGridEntries);
        }

        @Override
        public Object getItem(int position) {
            return super.getItem(position + mMaxGridEntries);
        }

        /**
         * We have to override default getItemId implementation, since for a given position, it returns
         * value of the _id column. In our case _id is always 0 (see Combined view).
         */
        @Override
        public long getItemId(int position) {
            final int adjustedPosition = position + mMaxGridEntries;
            final Cursor cursor = getCursor();

            cursor.moveToPosition(adjustedPosition);
            return getItemIdForTopSitesCursor(cursor);
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
        private final BrowserDB mDB;
        // Cache to store the thumbnails.
        // Ensure that this is only accessed from the UI thread.
        private Map<String, ThumbnailInfo> mThumbnailInfos;

        public TopSitesGridAdapter(Context context, Cursor cursor) {
            super(context, cursor, 0);
            mDB = GeckoProfile.get(context).getDB();
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
        public void updateThumbnails(Map<String, ThumbnailInfo> thumbnails) {
            mThumbnailInfos = thumbnails;

            final int count = mGrid.getChildCount();
            for (int i = 0; i < count; i++) {
                TopSitesGridItemView gridItem = (TopSitesGridItemView) mGrid.getChildAt(i);

                // All the views have already got their initial state at this point.
                // This will force each view to load favicons for the missing
                // thumbnails if necessary.
                gridItem.markAsDirty();
            }

            notifyDataSetChanged();
        }

        /**
         * We have to override default getItemId implementation, since for a given position, it returns
         * value of the _id column. In our case _id is always 0 (see Combined view).
         */
        @Override
        public long getItemId(int position) {
            final Cursor cursor = getCursor();
            cursor.moveToPosition(position);

            return getItemIdForTopSitesCursor(cursor);
        }

        @Override
        public void bindView(View bindView, Context context, Cursor cursor) {
            final String url = cursor.getString(cursor.getColumnIndexOrThrow(TopSites.URL));
            final String title = cursor.getString(cursor.getColumnIndexOrThrow(TopSites.TITLE));
            final int type = cursor.getInt(cursor.getColumnIndexOrThrow(TopSites.TYPE));

            final TopSitesGridItemView view = (TopSitesGridItemView) bindView;

            // If there is no url, then show "add bookmark".
            if (type == TopSites.TYPE_BLANK) {
                view.blankOut();
                return;
            }

            // Show the thumbnail, if any.
            ThumbnailInfo thumbnail = (mThumbnailInfos != null ? mThumbnailInfos.get(url) : null);

            // Debounce bindView calls to avoid redundant redraws and favicon
            // fetches.
            final boolean updated = view.updateState(title, url, type, thumbnail);

            // Thumbnails are delivered late, so we can't short-circuit any
            // sooner than this. But we can avoid a duplicate favicon
            // fetch...
            if (!updated) {
                debug("bindView called twice for same values; short-circuiting.");
                return;
            }

            // Make sure we query suggested images without the user-entered wrapper.
            final String decodedUrl = StringUtils.decodeUserEnteredUrl(url);

            // Suggested images have precedence over thumbnails, no need to wait
            // for them to be loaded. See: CursorLoaderCallbacks.onLoadFinished()
            final String imageUrl = mDB.getSuggestedImageUrlForUrl(decodedUrl);
            if (!TextUtils.isEmpty(imageUrl)) {
                final int bgColor = mDB.getSuggestedBackgroundColorForUrl(decodedUrl);
                view.displayThumbnail(imageUrl, bgColor);
                return;
            }

            // If thumbnails are still being loaded, don't try to load favicons
            // just yet. If we sent in a thumbnail, we're done now.
            if (mThumbnailInfos == null || thumbnail != null) {
                return;
            }

            view.loadFavicon(url);
        }

        @Override
        public View newView(Context context, Cursor cursor, ViewGroup parent) {
            return new TopSitesGridItemView(context);
        }
    }

    private class CursorLoaderCallbacks implements LoaderCallbacks<Cursor> {
        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            trace("Creating TopSitesLoader: " + id);
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
        public void onLoadFinished(Loader<Cursor> loader, Cursor c) {
            debug("onLoadFinished: " + c.getCount() + " rows.");

            mListAdapter.swapCursor(c);
            mGridAdapter.swapCursor(c);
            updateUiFromCursor(c);

            final int col = c.getColumnIndexOrThrow(TopSites.URL);

            // Load the thumbnails.
            // Even though the cursor we're given is supposed to be fresh,
            // we getIcon a bad first value unless we reset its position.
            // Using move(-1) and moveToNext() doesn't work correctly under
            // rotation, so we use moveToFirst.
            if (!c.moveToFirst()) {
                return;
            }

            final ArrayList<String> urls = new ArrayList<String>();
            int i = 1;
            do {
                final String url = c.getString(col);

                // Only try to fetch thumbnails for non-empty URLs that
                // don't have an associated suggested image URL.
                final GeckoProfile profile = GeckoProfile.get(getActivity());
                if (TextUtils.isEmpty(url) || profile.getDB().hasSuggestedImageUrl(url)) {
                    continue;
                }

                urls.add(url);
            } while (i++ < mMaxGridEntries && c.moveToNext());

            if (urls.isEmpty()) {
                // Short-circuit empty results to the UI.
                updateUiWithThumbnails(new HashMap<String, ThumbnailInfo>());
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

    static class ThumbnailInfo {
        public final Bitmap bitmap;
        public final String imageUrl;
        public final int bgColor;

        public ThumbnailInfo(final Bitmap bitmap) {
            this.bitmap = bitmap;
            this.imageUrl = null;
            this.bgColor = Color.TRANSPARENT;
        }

        public ThumbnailInfo(final String imageUrl, final int bgColor) {
            this.bitmap = null;
            this.imageUrl = imageUrl;
            this.bgColor = bgColor;
        }

        public static ThumbnailInfo fromMetadata(final Map<String, Object> data) {
            if (data == null) {
                return null;
            }

            final String imageUrl = (String) data.get(TILE_IMAGE_URL_COLUMN);
            if (imageUrl == null) {
                return null;
            }

            int bgColor = Color.WHITE;
            final String colorString = (String) data.get(TILE_COLOR_COLUMN);
            try {
                bgColor = Color.parseColor(colorString);
            } catch (Exception ex) {
            }

            return new ThumbnailInfo(imageUrl, bgColor);
        }
    }

    /**
     * An AsyncTaskLoader to load the thumbnails from a cursor.
     */
    static class ThumbnailsLoader extends AsyncTaskLoader<Map<String, ThumbnailInfo>> {
        private final BrowserDB mDB;
        private Map<String, ThumbnailInfo> mThumbnailInfos;
        private final ArrayList<String> mUrls;

        private static final List<String> COLUMNS;
        static {
            final ArrayList<String> tempColumns = new ArrayList<>(2);
            tempColumns.add(TILE_IMAGE_URL_COLUMN);
            tempColumns.add(TILE_COLOR_COLUMN);
            COLUMNS = Collections.unmodifiableList(tempColumns);
        }

        public ThumbnailsLoader(Context context, ArrayList<String> urls) {
            super(context);
            mUrls = urls;
            mDB = GeckoProfile.get(context).getDB();
        }

        @Override
        public Map<String, ThumbnailInfo> loadInBackground() {
            final Map<String, ThumbnailInfo> thumbnails = new HashMap<String, ThumbnailInfo>();
            if (mUrls == null || mUrls.size() == 0) {
                return thumbnails;
            }

            // We need to query metadata based on the URL without any refs, hence we create a new
            // mapping and list of these URLs (we need to preserve the original URL for display purposes)
            final Map<String, String> queryURLs = new HashMap<>();
            for (final String pageURL : mUrls) {
                queryURLs.put(pageURL, StringUtils.stripRef(pageURL));
            }

            // Query the DB for tile images.
            final ContentResolver cr = getContext().getContentResolver();
            // Use the stripped URLs for querying the DB
            final Map<String, Map<String, Object>> metadata = mDB.getURLMetadata().getForURLs(cr, queryURLs.values(), COLUMNS);

            // Keep a list of urls that don't have tiles images. We'll use thumbnails for them instead.
            final List<String> thumbnailUrls = new ArrayList<String>();
            for (final String pageURL : mUrls) {
                final String queryURL = queryURLs.get(pageURL);

                ThumbnailInfo info = ThumbnailInfo.fromMetadata(metadata.get(queryURL));
                if (info == null) {
                    // If we didn't find metadata, we'll look for a thumbnail for this url.
                    thumbnailUrls.add(pageURL);
                    continue;
                }

                thumbnails.put(pageURL, info);
            }

            if (thumbnailUrls.size() == 0) {
                return thumbnails;
            }

            // Query the DB for tile thumbnails.
            final Cursor cursor = mDB.getThumbnailsForUrls(cr, thumbnailUrls);
            if (cursor == null) {
                return thumbnails;
            }

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

                    // Our thumbnails are never null, so if we getIcon a null decoded
                    // bitmap, it's because we hit an OOM or some other disaster.
                    // Give up immediately rather than hammering on.
                    if (bitmap == null) {
                        Log.w(LOGTAG, "Aborting thumbnail load; decode failed.");
                        break;
                    }

                    thumbnails.put(url, new ThumbnailInfo(bitmap));
                }
            } finally {
                cursor.close();
            }

            return thumbnails;
        }

        @Override
        public void deliverResult(Map<String, ThumbnailInfo> thumbnails) {
            if (isReset()) {
                mThumbnailInfos = null;
                return;
            }

            mThumbnailInfos = thumbnails;

            if (isStarted()) {
                super.deliverResult(thumbnails);
            }
        }

        @Override
        protected void onStartLoading() {
            if (mThumbnailInfos != null) {
                deliverResult(mThumbnailInfos);
            }

            if (takeContentChanged() || mThumbnailInfos == null) {
                forceLoad();
            }
        }

        @Override
        protected void onStopLoading() {
            cancelLoad();
        }

        @Override
        public void onCanceled(Map<String, ThumbnailInfo> thumbnails) {
            mThumbnailInfos = null;
        }

        @Override
        protected void onReset() {
            super.onReset();

            // Ensure the loader is stopped.
            onStopLoading();

            mThumbnailInfos = null;
        }
    }

    /**
     * Loader callbacks for the thumbnails on TopSitesGridView.
     */
    private class ThumbnailsLoaderCallbacks implements LoaderCallbacks<Map<String, ThumbnailInfo>> {
        @Override
        public Loader<Map<String, ThumbnailInfo>> onCreateLoader(int id, Bundle args) {
            return new ThumbnailsLoader(getActivity(), args.getStringArrayList(THUMBNAILS_URLS_KEY));
        }

        @Override
        public void onLoadFinished(Loader<Map<String, ThumbnailInfo>> loader, Map<String, ThumbnailInfo> thumbnails) {
            updateUiWithThumbnails(thumbnails);
        }

        @Override
        public void onLoaderReset(Loader<Map<String, ThumbnailInfo>> loader) {
            if (mGridAdapter != null) {
                mGridAdapter.updateThumbnails(null);
            }
        }
    }

    /**
     * We are trying to return stable IDs so that Android can recycle views appropriately:
     * - If we have a history ID then we return it
     * - If we only have a bookmark ID then we negate it and return it. We negate it in order
     *   to avoid clashing/conflicting with history IDs.
     *
     * @param cursorInPosition Cursor already moved to position for which we're getting a stable ID
     * @return Stable ID for a given cursor
     */
    private static long getItemIdForTopSitesCursor(final Cursor cursorInPosition) {
        final int historyIdCol = cursorInPosition.getColumnIndexOrThrow(TopSites.HISTORY_ID);
        final long historyId = cursorInPosition.getLong(historyIdCol);
        if (historyId != 0) {
            return historyId;
        }

        final int bookmarkIdCol = cursorInPosition.getColumnIndexOrThrow(TopSites.BOOKMARK_ID);
        final long bookmarkId = cursorInPosition.getLong(bookmarkIdCol);
        return -1 * bookmarkId;
    }
}
