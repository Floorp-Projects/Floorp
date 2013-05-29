/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.mozilla.gecko.AwesomeBar;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.ThumbnailHelper;
import org.mozilla.gecko.db.BrowserContract.Thumbnails;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserDB.TopSitesCursorWrapper;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.util.ActivityResultHandler;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UiAsyncTask;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.ShapeDrawable;
import android.graphics.drawable.shapes.PathShape;
import android.net.Uri;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AbsListView;
import android.widget.AdapterView;
import android.widget.GridView;
import android.widget.ImageView;
import android.widget.SimpleCursorAdapter;
import android.widget.TextView;
import android.widget.Toast;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class TopSitesView extends GridView {
    private static final String LOGTAG = "GeckoAboutHomeTopSites";

    private static int mNumberOfTopSites;
    private static int mNumberOfCols;

    public static enum UnpinFlags {
        REMOVE_PIN
    }

    private Context mContext;
    private BrowserApp mActivity;
    private AboutHome.UriLoadListener mUriLoadListener;
    private AboutHome.LoadCompleteListener mLoadCompleteListener;

    protected TopSitesCursorAdapter mTopSitesAdapter;

    private static Drawable sPinDrawable = null;
    private int mThumbnailBackground;
    private Map<String, Bitmap> mPendingThumbnails;

    public TopSitesView(Context context) {
        super(context);
        mContext = context;
        mActivity = (BrowserApp) context;

        init();
    }

    public TopSitesView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mActivity = (BrowserApp) context;

        init();
    }

    private void init() {
        mThumbnailBackground = mContext.getResources().getColor(R.color.abouthome_thumbnail_bg);
        setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View v, int position, long id) {
                TopSitesViewHolder holder = (TopSitesViewHolder) v.getTag();
                String spec = holder.getUrl();

                // If we don't have a url, this must be an empty row. Show the edit dialog box
                if (TextUtils.isEmpty(spec)) {
                    editSite(spec, position);
                    return;
                }

                if (mUriLoadListener != null) {
                    // Decode "user-entered" URLs before loading them.
                    mUriLoadListener.onAboutHomeUriLoad(decodeUserEnteredUrl(spec));
                }
            }
        });

        setOnCreateContextMenuListener(new View.OnCreateContextMenuListener() {
            @Override
            public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo) {
                AdapterContextMenuInfo info = (AdapterContextMenuInfo) menuInfo;

                MenuInflater inflater = mActivity.getMenuInflater();
                inflater.inflate(R.menu.abouthome_topsites_contextmenu, menu);

                // If nothing is pinned at all, hide both clear items
                // We can assume that the adapter count and view count are the same in this case because our grid view
                // force all items to be visible all the time
                View view = getChildAt(info.position);
                TopSitesViewHolder holder = (TopSitesViewHolder) view.getTag();
                if (TextUtils.isEmpty(holder.getUrl())) {
                    menu.findItem(R.id.abouthome_open_new_tab).setVisible(false);
                    menu.findItem(R.id.abouthome_open_private_tab).setVisible(false);
                    menu.findItem(R.id.abouthome_topsites_pin).setVisible(false);
                    menu.findItem(R.id.abouthome_topsites_unpin).setVisible(false);
                } else if (holder.isPinned()) {
                    menu.findItem(R.id.abouthome_topsites_pin).setVisible(false);
                } else {
                    menu.findItem(R.id.abouthome_topsites_unpin).setVisible(false);
                }
            }
        });

        setTopSitesConstants();
    }

    public void onDestroy() {
        if (mTopSitesAdapter != null) {
            setAdapter(null);
            final Cursor cursor = mTopSitesAdapter.getCursor();

            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                if (cursor != null && !cursor.isClosed())
                    cursor.close();
                }
            });
        }
    }

    @Override
    public int getColumnWidth() {
        return getColumnWidth(getWidth());
    }

    public int getColumnWidth(int width) {
        // super.getColumnWidth() doesn't always return the correct value.
        return (width - getPaddingLeft() - getPaddingRight()) / mNumberOfCols;
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

    public void loadTopSites() {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                final ContentResolver resolver = mContext.getContentResolver();

                // Swap in the new cursor.
                final Cursor oldCursor = (mTopSitesAdapter != null) ? mTopSitesAdapter.getCursor() : null;
                final Cursor newCursor = BrowserDB.getTopSites(resolver, mNumberOfTopSites);

                post(new Runnable() {
                    @Override
                    public void run() {
                        if (mTopSitesAdapter == null) {
                            mTopSitesAdapter = new TopSitesCursorAdapter(mContext,
                                                                         R.layout.abouthome_topsite_item,
                                                                         newCursor,
                                                                         new String[] { URLColumns.TITLE },
                                                                         new int[] { R.id.title });

                            setAdapter(mTopSitesAdapter);
                        } else {
                            mTopSitesAdapter.changeCursor(newCursor);
                        }

                        if (mTopSitesAdapter.getCount() > 0)
                            loadTopSitesThumbnails(resolver);

                        // Free the old Cursor in the right thread now.
                        if (oldCursor != null && !oldCursor.isClosed())
                            oldCursor.close();

                        // Even if AboutHome isn't necessarily entirely loaded if we
                        // get here, for phones this is the part the user initially sees,
                        // so it's the one we will care about for now.
                        if (mLoadCompleteListener != null)
                            mLoadCompleteListener.onAboutHomeLoadComplete();
                    }
                });
            }
        });
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        super.onLayout(changed, l, t, r, b);

        if (mPendingThumbnails != null) {
            updateTopSitesThumbnails(mPendingThumbnails);
            mPendingThumbnails = null;
        }
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
            thumbnailView.setScaleType(ImageView.ScaleType.FIT_CENTER);
            thumbnailView.setImageResource(R.drawable.abouthome_thumbnail_bg);
            thumbnailView.setBackgroundColor(mThumbnailBackground);
        } else {
            try {
                thumbnailView.setScaleType(ImageView.ScaleType.CENTER_CROP);
                thumbnailView.setImageBitmap(thumbnail);
                thumbnailView.setBackgroundColor(0x0);
            } catch (OutOfMemoryError oom) {
                Log.e(LOGTAG, "Unable to load thumbnail bitmap", oom);
                thumbnailView.setScaleType(ImageView.ScaleType.FIT_CENTER);
                thumbnailView.setImageResource(R.drawable.abouthome_thumbnail_bg);
            }
        }
    }

    private void updateTopSitesThumbnails(Map<String, Bitmap> thumbnails) {
        for (int i = 0; i < mTopSitesAdapter.getCount(); i++) {
            final View view = getChildAt(i);

            // The grid view might get temporarily out of sync with the
            // adapter refreshes (e.g. on device rotation)
            if (view == null)
                continue;

            TopSitesViewHolder holder = (TopSitesViewHolder)view.getTag();
            final String url = holder.getUrl();
            if (TextUtils.isEmpty(url)) {
                holder.thumbnailView.setScaleType(ImageView.ScaleType.FIT_CENTER);
                holder.thumbnailView.setImageResource(R.drawable.abouthome_thumbnail_add);
                holder.thumbnailView.setBackgroundColor(mThumbnailBackground);
            } else {
                displayThumbnail(view, thumbnails.get(url));
            }
        }
    }

    public Map<String, Bitmap> getThumbnailsFromCursor(Cursor c) {
        Map<String, Bitmap> thumbnails = new HashMap<String, Bitmap>();

        try {
            if (c == null || !c.moveToFirst())
                return thumbnails;

            do {
                final String url = c.getString(c.getColumnIndexOrThrow(Thumbnails.URL));
                final byte[] b = c.getBlob(c.getColumnIndexOrThrow(Thumbnails.DATA));
                if (b == null)
                    continue;

                Bitmap thumbnail = null;
                try {
                    thumbnail = BitmapUtils.decodeByteArray(b);
                } catch (IllegalArgumentException e) {
                    Log.e(LOGTAG, "Error decoding thumbnail", e);
                }
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

        (new UiAsyncTask<Void, Void, Map<String, Bitmap> >(ThreadUtils.getBackgroundHandler()) {
            @Override
            public Map<String, Bitmap> doInBackground(Void... params) {
                return getThumbnailsFromCursor(BrowserDB.getThumbnailsForUrls(cr, urls));
            }

            @Override
            public void onPostExecute(Map<String, Bitmap> thumbnails) {
                // If we're waiting for a layout to happen, the GridView may be
                // stale, so store the pending thumbnails here. They will be
                // shown on the next layout pass.
                if (isLayoutRequested()) {
                    mPendingThumbnails = thumbnails;
                } else {
                    updateTopSitesThumbnails(thumbnails);
                }
            }
        }).execute();
    }

    private void setTopSitesConstants() {
        mNumberOfTopSites = getResources().getInteger(R.integer.number_of_top_sites);
        mNumberOfCols = getResources().getInteger(R.integer.number_of_top_sites_cols);
    }

    public void setUriLoadListener(AboutHome.UriLoadListener uriLoadListener) {
        mUriLoadListener = uriLoadListener;
    }

    public void setLoadCompleteListener(AboutHome.LoadCompleteListener listener) {
        mLoadCompleteListener = listener;
    }

    private class TopSitesViewHolder {
        public TextView titleView = null;
        public ImageView thumbnailView = null;
        public ImageView pinnedView = null;
        private String mTitle = null;
        private String mUrl = null;
        private boolean mIsPinned = false;

        public TopSitesViewHolder(View v) {
            titleView = (TextView) v.findViewById(R.id.title);
            thumbnailView = (ImageView) v.findViewById(R.id.thumbnail);
            pinnedView = (ImageView) v.findViewById(R.id.pinned);
        }

        public void setTitle(String title) {
            if (mTitle != null && mTitle.equals(title))
                return;
            mTitle = title;
            updateTitleView();
        }

        public String getTitle() {
            return (!TextUtils.isEmpty(mTitle) ? mTitle : mUrl);
        }

        public void setUrl(String url) {
            if (mUrl != null && mUrl.equals(url)) {
                return;
            }
            mUrl = url;
            updateTitleView();
        }

        public String getUrl() {
            return mUrl;
        }

        public void updateTitleView() {
            String title = getTitle();
            if (!TextUtils.isEmpty(title)) {
                titleView.setText(title);
                titleView.setVisibility(View.VISIBLE);
            } else {
                titleView.setVisibility(View.INVISIBLE);
            }
            titleView.invalidate();
        }

        private Drawable getPinDrawable() {
            if (sPinDrawable == null) {
                int size = mContext.getResources().getDimensionPixelSize(R.dimen.abouthome_topsite_pinsize);

                // Draw a little triangle in the upper right corner
                Path path = new Path();
                path.moveTo(0, 0);
                path.lineTo(size, 0);
                path.lineTo(size, size);
                path.close();

                sPinDrawable = new ShapeDrawable(new PathShape(path, size, size));
                Paint p = ((ShapeDrawable) sPinDrawable).getPaint();
                p.setColor(mContext.getResources().getColor(R.color.abouthome_topsite_pin));
            }
            return sPinDrawable;
        }

        public void setPinned(boolean aPinned) {
            mIsPinned = aPinned;
            pinnedView.setBackgroundDrawable(aPinned ? getPinDrawable() : null);
        }

        public boolean isPinned() {
            return mIsPinned;
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

        private View buildView(String url, String title, boolean pinned, View convertView) {
            TopSitesViewHolder viewHolder;
            if (convertView == null) {
                convertView = LayoutInflater.from(mContext).inflate(R.layout.abouthome_topsite_item, null);

                viewHolder = new TopSitesViewHolder(convertView);
                convertView.setTag(viewHolder);
            } else {
                viewHolder = (TopSitesViewHolder) convertView.getTag();
            }

            viewHolder.setTitle(title);
            viewHolder.setUrl(url);
            viewHolder.setPinned(pinned);

            // Force the view to fit inside this slot in the grid
            convertView.setLayoutParams(new AbsListView.LayoutParams(getColumnWidth(),
                        Math.round(getColumnWidth()*ThumbnailHelper.THUMBNAIL_ASPECT_RATIO)));

            return convertView;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            String url = "";
            String title = "";
            boolean pinned = false;

            Cursor c = getCursor();
            c.moveToPosition(position);
            if (!c.isAfterLast()) {
                url = c.getString(c.getColumnIndex(URLColumns.URL));
                title = c.getString(c.getColumnIndex(URLColumns.TITLE));
                pinned = ((TopSitesCursorWrapper)c).isPinned();
            }

            return buildView(url, title, pinned, convertView);
        }
    }

    private void clearThumbnailsWithUrl(final String url) {
        for (int i = 0; i < mTopSitesAdapter.getCount(); i++) {
            final View view = getChildAt(i);
            final TopSitesViewHolder holder = (TopSitesViewHolder) view.getTag();

            if (holder.getUrl().equals(url)) {
                clearThumbnail(holder);
            }
        }
    }

    private void clearThumbnail(TopSitesViewHolder holder) {
        holder.setTitle("");
        holder.setUrl("");
        holder.thumbnailView.setScaleType(ImageView.ScaleType.FIT_CENTER);
        holder.thumbnailView.setImageResource(R.drawable.abouthome_thumbnail_add);
        holder.thumbnailView.setBackgroundColor(mThumbnailBackground);
        holder.setPinned(false);
    }

    private void openTab(ContextMenuInfo menuInfo, int flags) {
        AdapterContextMenuInfo info = (AdapterContextMenuInfo) menuInfo;
        final TopSitesViewHolder holder = (TopSitesViewHolder) info.targetView.getTag();
        // Decode "user-entered" URLs before loading them.
        final String url = decodeUserEnteredUrl(holder.getUrl());

        Tabs.getInstance().loadUrl(url, flags);
        Toast.makeText(mActivity, R.string.new_tab_opened, Toast.LENGTH_SHORT).show();
    }

    public void openNewTab(ContextMenuInfo menuInfo) {
        openTab(menuInfo, Tabs.LOADURL_NEW_TAB | Tabs.LOADURL_BACKGROUND);
    }

    public void openNewPrivateTab(ContextMenuInfo menuInfo) {
        openTab(menuInfo, Tabs.LOADURL_NEW_TAB | Tabs.LOADURL_PRIVATE | Tabs.LOADURL_BACKGROUND);
    }

    public void unpinSite(ContextMenuInfo menuInfo, final UnpinFlags flags) {
        AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) menuInfo;
        final int position = info.position;

        final View v = getChildAt(position);
        final TopSitesViewHolder holder = (TopSitesViewHolder) v.getTag();
        final String url = holder.getUrl();
        // Quickly update the view so that there isn't as much lag between the request and response
        clearThumbnail(holder);
        (new UiAsyncTask<Void, Void, Void>(ThreadUtils.getBackgroundHandler()) {
            @Override
            public Void doInBackground(Void... params) {
                final ContentResolver resolver = mContext.getContentResolver();
                BrowserDB.unpinSite(resolver, position);
                return null;
            }
        }).execute();
    }

    public void pinSite(ContextMenuInfo menuInfo) {
        AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) menuInfo;
        final int position = info.position;
        View v = getChildAt(position);

        final TopSitesViewHolder holder = (TopSitesViewHolder) v.getTag();
        holder.setPinned(true);

        // update the database on a background thread
        (new UiAsyncTask<Void, Void, Void>(ThreadUtils.getBackgroundHandler()) {
            @Override
            public Void doInBackground(Void... params) {
                final ContentResolver resolver = mContext.getContentResolver();
                BrowserDB.pinSite(resolver, holder.getUrl(), holder.getTitle(), position);
                return null;
            }
        }).execute();
    }

    private static String encodeUserEnteredUrl(String url) {
        return Uri.fromParts("user-entered", url, null).toString();
    }

    private static String decodeUserEnteredUrl(String url) {
        Uri uri = Uri.parse(url);
        if ("user-entered".equals(uri.getScheme())) {
            return uri.getSchemeSpecificPart();
        }
        return url;
    }

    public void editSite(ContextMenuInfo menuInfo) {
        AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) menuInfo;
        int position = info.position;
        View v = getChildAt(position);

        TopSitesViewHolder holder = (TopSitesViewHolder) v.getTag();
        // Decode "user-entered" URLs before showing them to the user to edit.
        editSite(decodeUserEnteredUrl(holder.getUrl()), position);
    }

    // Edit the site at position. Provide a url to start editing with
    public void editSite(String url, final int position) {
        Intent intent = new Intent(mContext, AwesomeBar.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NO_HISTORY);
        intent.putExtra(AwesomeBar.TARGET_KEY, AwesomeBar.Target.PICK_SITE.toString());
        if (url != null && !TextUtils.isEmpty(url)) {
            intent.putExtra(AwesomeBar.CURRENT_URL_KEY, url);
        }

        int requestCode = GeckoAppShell.sActivityHelper.makeRequestCode(new ActivityResultHandler() {
            @Override
            public void onActivityResult(int resultCode, Intent data) {
                if (resultCode == Activity.RESULT_CANCELED || data == null)
                    return;

                final View v = getChildAt(position);
                final TopSitesViewHolder holder = (TopSitesViewHolder) v.getTag();

                String title = data.getStringExtra(AwesomeBar.TITLE_KEY);
                String url = data.getStringExtra(AwesomeBar.URL_KEY);

                // Bail if the user entered an empty string.
                if (TextUtils.isEmpty(url)) {
                    return;
                }

                // If the user manually entered a search term or URL, wrap the value in
                // a special URI until we can get a valid URL for this bookmark.
                if (data.getBooleanExtra(AwesomeBar.USER_ENTERED_KEY, false)) {
                    // Store what the user typed as the bookmark's title.
                    title = url;
                    url = encodeUserEnteredUrl(url);
                }

                clearThumbnailsWithUrl(url);

                holder.setUrl(url);
                holder.setTitle(title);
                holder.setPinned(true);

                // update the database on a background thread
                (new UiAsyncTask<Void, Void, Bitmap>(ThreadUtils.getBackgroundHandler()) {
                    @Override
                    public Bitmap doInBackground(Void... params) {
                        final ContentResolver resolver = mContext.getContentResolver();
                        BrowserDB.pinSite(resolver, holder.getUrl(), holder.getTitle(), position);

                        List<String> urls = new ArrayList<String>();
                        urls.add(holder.getUrl());

                        Cursor c = BrowserDB.getThumbnailsForUrls(resolver, urls);
                        if (c == null || !c.moveToFirst()) {
                            return null;
                        }

                        final byte[] b = c.getBlob(c.getColumnIndexOrThrow(Thumbnails.DATA));
                        Bitmap bitmap = null;
                        if (b != null && b.length > 0) {
                            bitmap = BitmapUtils.decodeByteArray(b);
                        }
                        c.close();

                        return bitmap;
                    }

                    @Override
                    public void onPostExecute(Bitmap b) {
                        displayThumbnail(v, b);
                    }
                }).execute();
            }
        });

        mActivity.startActivityForResult(intent, requestCode);
    }
}
