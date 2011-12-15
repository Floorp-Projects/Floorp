/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
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
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brad Lassey <blassey@mozilla.com>
 *   Lucas Rocha <lucasr@mozilla.com>
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

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.InputStream;
import java.io.IOException;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import org.json.JSONArray;
import org.json.JSONObject;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserDB.URLColumns;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Configuration;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.text.SpannableString;
import android.text.style.UnderlineSpan;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.GridView;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.ScrollView;
import android.widget.SimpleCursorAdapter;
import android.widget.TextView;

public class AboutHomeContent extends ScrollView {
    private static final String LOGTAG = "GeckoAboutHome";

    private static final int NUMBER_OF_TOP_SITES_PORTRAIT = 4;
    private static final int NUMBER_OF_TOP_SITES_LANDSCAPE = 3;

    private static final int NUMBER_OF_COLS_PORTRAIT = 2;
    private static final int NUMBER_OF_COLS_LANDSCAPE = 3;

    private boolean mInflated;

    private Cursor mCursor;
    UriLoadCallback mUriLoadCallback = null;

    protected SimpleCursorAdapter mTopSitesAdapter;
    protected GridView mTopSitesGrid;

    protected ArrayAdapter<String> mAddonsAdapter;
    protected ListView mAddonsList;

    public interface UriLoadCallback {
        public void callback(String uriSpec);
    }

    public AboutHomeContent(Context context, AttributeSet attrs) {
        super(context, attrs);
        mInflated = false;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        // HACK: Without this, the onFinishInflate is called twice
        // This issue is due to a bug when Android inflates a layout with a
        // parent. Fixed in Honeycomb
        if (mInflated)
            return;

        mInflated = true;

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

        mAddonsList = (ListView) findViewById(R.id.recommended_addons_list);

        TextView allTopSitesText = (TextView) findViewById(R.id.all_top_sites_text);
        allTopSitesText.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                GeckoApp.mAppContext.showAwesomebar(AwesomeBar.Type.EDIT);
            }
        });

        TextView allAddonsText = (TextView) findViewById(R.id.all_addons_text);
        allAddonsText.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                if (mUriLoadCallback != null)
                    mUriLoadCallback.callback("about:addons");
            }
        });
    }

    private int getNumberOfTopSites() {
        Configuration config = getContext().getResources().getConfiguration();
        if (config.orientation == Configuration.ORIENTATION_LANDSCAPE)
            return NUMBER_OF_TOP_SITES_LANDSCAPE;
        else
            return NUMBER_OF_TOP_SITES_PORTRAIT;
    }

    private int getNumberOfColumns() {
        Configuration config = getContext().getResources().getConfiguration();
        if (config.orientation == Configuration.ORIENTATION_LANDSCAPE)
            return NUMBER_OF_COLS_LANDSCAPE;
        else
            return NUMBER_OF_COLS_PORTRAIT;
    }

    void init(final Activity activity) {
        GeckoAppShell.getHandler().post(new Runnable() {
            public void run() {
                if (mCursor != null)
                    activity.stopManagingCursor(mCursor);

                ContentResolver resolver = GeckoApp.mAppContext.getContentResolver();
                mCursor = BrowserDB.filter(resolver, "", NUMBER_OF_TOP_SITES_PORTRAIT);
                activity.startManagingCursor(mCursor);

                mTopSitesAdapter = new TopSitesCursorAdapter(activity,
                                                             R.layout.abouthome_topsite_item,
                                                             mCursor,
                                                             new String[] { URLColumns.TITLE,
                                                                            URLColumns.THUMBNAIL },
                                                             new int[] { R.id.title, R.id.thumbnail });

                if (mAddonsAdapter == null)
                    mAddonsAdapter = new ArrayAdapter<String>(activity, R.layout.abouthome_addon_row);

                GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
                    public void run() {
                        mTopSitesGrid.setNumColumns(getNumberOfColumns());

                        mTopSitesGrid.setAdapter(mTopSitesAdapter);
                        mTopSitesAdapter.setViewBinder(new TopSitesViewBinder());

                        mAddonsList.setAdapter(mAddonsAdapter);
                    }
                });

                readRecommendedAddons(activity);
            }
        });
    }

    public void setUriLoadCallback(UriLoadCallback uriLoadCallback) {
        mUriLoadCallback = uriLoadCallback;
    }

    public void onActivityContentChanged(Activity activity) {
        GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
            public void run() {
                mTopSitesGrid.setAdapter(mTopSitesAdapter);
            }
        });
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        mTopSitesGrid.setNumColumns(getNumberOfColumns());
        mTopSitesAdapter.notifyDataSetChanged();

        super.onConfigurationChanged(newConfig);
    }

    InputStream getProfileRecommendedAddonsStream() {
        try {
            File profileDir = GeckoApp.mAppContext.getProfileDir();
            if (profileDir == null)
                return null;
            File recommendedAddonsFile = new File(profileDir, "recommended-addons.json");
            if (!recommendedAddonsFile.exists())
                return null;
            return new FileInputStream(recommendedAddonsFile);
        } catch (FileNotFoundException fnfe) {
            // ignore
        }
        return null;
    }

    InputStream getRecommendedAddonsStream(Activity activity) throws Exception{
        InputStream is = getProfileRecommendedAddonsStream();
        if (is != null)
            return is;
        File applicationPackage = new File(activity.getApplication().getPackageResourcePath());
        ZipFile zip = new ZipFile(applicationPackage);
        if (zip == null)
            return null;
        ZipEntry fileEntry = zip.getEntry("recommended-addons.json");
        if (fileEntry == null)
            return null;
        return zip.getInputStream(fileEntry);
    }

    void readRecommendedAddons(final Activity activity) {
        GeckoAppShell.getHandler().post(new Runnable() {
            public void run() {
                try {
                    byte[] buf = new byte[32768];
                    InputStream fileStream = getRecommendedAddonsStream(activity);
                    if (fileStream == null)
                        return;
                    StringBuffer jsonString = new StringBuffer();
                    try {
                        int read = 0;
                        while ((read = fileStream.read(buf, 0, 32768)) != -1) {
                            jsonString.append(new String(buf, 0, read));
                        }
                    } finally {
                        try {
                            fileStream.close();
                        } catch (IOException ioe) {
                            // catch this here because we can continue even if the
                            // close failed
                            Log.i(LOGTAG, "error closing json file", ioe);
                        }
                    }
                    final JSONArray array = new JSONObject(jsonString.toString()).getJSONArray("addons");
                    GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
                        public void run() {
                            try {
                                for (int i = 0; i < array.length(); i++) {
                                    JSONObject jsonobj = array.getJSONObject(i);
                                    mAddonsAdapter.add(jsonobj.getString("name"));
                                    Log.i(LOGTAG, "addon #" + i +": " + jsonobj.getString("name"));
                                }
                            } catch (Exception e) {
                                Log.i(LOGTAG, "error reading json file", e);
                            }
                        }
                    });
                } catch (Exception e) {
                    Log.i(LOGTAG, "error reading json file", e);
                }
            }
        });
    }

    public static class TopSitesGridView extends GridView {
        public TopSitesGridView(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            // This is to ensure that the GridView always has a size that shows
            // all items with no need for scrolling.
            int expandedHeightSpec = MeasureSpec.makeMeasureSpec(Integer.MAX_VALUE >> 2,
                    MeasureSpec.AT_MOST);
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
    }

    class TopSitesViewBinder implements SimpleCursorAdapter.ViewBinder {
        private boolean updateThumbnail(View view, Cursor cursor, int thumbIndex) {
            byte[] b = cursor.getBlob(thumbIndex);
            ImageView thumbnail = (ImageView) view;

            if (b == null) {
                thumbnail.setImageResource(R.drawable.abouthome_topsite_placeholder);
            } else {
                try {
                    Bitmap bitmap = BitmapFactory.decodeByteArray(b, 0, b.length);
                    thumbnail.setImageBitmap(bitmap);
                } catch (OutOfMemoryError oom) {
                    Log.e(LOGTAG, "Unable to load thumbnail bitmap", oom);
                    thumbnail.setImageResource(R.drawable.abouthome_topsite_placeholder);
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

    public static class AddonsListView extends ListView {
        public AddonsListView(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            // This is to ensure that the ListView always has a size that shows
            // all items with no need for scrolling.
            int expandedHeightSpec = MeasureSpec.makeMeasureSpec(Integer.MAX_VALUE >> 2,
                    MeasureSpec.AT_MOST);
            super.onMeasure(widthMeasureSpec, expandedHeightSpec);
        }
    }

    public static class LinkTextView extends TextView {
        public LinkTextView(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        @Override
        public void setText(CharSequence text, BufferType type) {
            SpannableString content = new SpannableString(text + " \u00BB");
            content.setSpan(new UnderlineSpan(), 0, text.length(), 0);

            super.setText(content, BufferType.SPANNABLE);
        }
    }
}
