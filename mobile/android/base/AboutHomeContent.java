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

import android.app.Activity;
import android.os.Bundle;
import android.widget.*;
import android.database.*;
import android.view.*;
import android.graphics.*;
import android.content.*;
import android.provider.Browser;
import android.util.Log;
import java.util.Date;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;
import android.util.Log;
import android.view.View;
import android.widget.ListAdapter;
import android.widget.GridView;
import android.widget.SimpleCursorAdapter;
import java.io.*;
import java.util.zip.*;
import android.os.Handler;
import org.json.*;
import android.util.AttributeSet;

public class AboutHomeContent extends LinearLayout {
    public interface UriLoadCallback {
        public void callback(String uriSpec);
    }

    UriLoadCallback mUriLoadCallback = null;

    void setUriLoadCallback(UriLoadCallback uriLoadCallback) {
        mUriLoadCallback = uriLoadCallback;
    }

    public AboutHomeContent(Context context, AttributeSet attrs) {
        super(context, attrs);

        LayoutInflater inflater = (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        inflater.inflate(R.layout.abouthome_content, this);
    }

    private static final String LOGTAG = "GeckoAboutHome";
    private static final String TITLE_KEY = "title";
    private static final String kAbouthomeWhereClause = Browser.BookmarkColumns.BOOKMARK + " = 1";
    private static final int kTileWidth = 122;

    private Cursor mCursor;
    private Uri mUri;
    private String mTitle;

    protected ListAdapter mGridAdapter;
    protected ArrayAdapter<String> mAddonAdapter;
    protected GridView mGrid;
    protected ListView mAddonList;
    private Handler mHandler = new Handler();

    public void onActivityContentChanged(Activity activity) {
        mGrid = (GridView)findViewById(R.id.grid);
        if (mGrid == null)
            return;

        mGrid.setOnItemClickListener(mGridOnClickListener);

        // we want to do this: mGrid.setNumColumns(GridView.AUTO_FIT); but it doesn't work
        Display display = ((WindowManager) activity.getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay();
        int width = display.getWidth();
        mGrid.setNumColumns((int) Math.floor(width / kTileWidth));
        mAddonList = (ListView)findViewById(R.id.recommended_addon_list);
        GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
            public void run() {
                mGrid.setAdapter(mGridAdapter);
            }
        });
    }


    private AdapterView.OnItemClickListener mGridOnClickListener = new AdapterView.OnItemClickListener() {
        public void onItemClick(AdapterView<?> parent, View v, int position, long id)
        {
            onGridItemClick((GridView)parent, v, position, id);
        }
    };

    void init(final Activity activity) {
        GeckoAppShell.getHandler().post(new Runnable() {
            public void run() {
                mCursor = activity.managedQuery(Browser.BOOKMARKS_URI,
                                                null, kAbouthomeWhereClause, null, null);
                activity.startManagingCursor(mCursor);

                onActivityContentChanged(activity);
                mAddonAdapter = new ArrayAdapter<String>(activity, R.layout.abouthome_addon_list_item);
                GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
                    public void run() {
                        final SimpleCursorAdapter gridAdapter =
                            new SimpleCursorAdapter(activity, R.layout.abouthome_grid_box, mCursor,
                                                    new String[] {Browser.BookmarkColumns.TITLE,
                                                                  Browser.BookmarkColumns.FAVICON,
                                                                  Browser.BookmarkColumns.URL,
                                                                  "thumbnail"},
                                                    new int[] {R.id.bookmark_title, R.id.bookmark_icon, R.id.bookmark_url, R.id.screenshot});
                        mGrid.setAdapter(gridAdapter);
                        gridAdapter.setViewBinder(new AwesomeCursorViewBinder());
                        mAddonList.setAdapter(mAddonAdapter);
                    }
                });
                readRecommendedAddons(activity);
            }
        });
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
        ZipEntry fileEntry = zip.getEntry("recommended-addons.json");
        return zip.getInputStream(fileEntry);
    }

    void readRecommendedAddons(final Activity activity) {
        GeckoAppShell.getHandler().post(new Runnable() {
            public void run() {
                try {
                    byte[] buf = new byte[32768];
                    InputStream fileStream = getRecommendedAddonsStream(activity);
                    StringBuffer jsonString = new StringBuffer();
                    int read = 0;
                    while ((read = fileStream.read(buf, 0, 32768)) != -1) {
                        jsonString.append(new String(buf, 0, read));
                    }
                    final JSONArray array = new JSONObject(jsonString.toString()).getJSONArray("addons");
                    GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
                        public void run() {
                            try {
                                for (int i = 0; i < array.length(); i++) {
                                    JSONObject jsonobj = array.getJSONObject(i);
                                    mAddonAdapter.add(jsonobj.getString("name"));
                                    Log.i("GeckoAddons", "addon #" + i +": " + jsonobj.getString("name"));
                                }
                            } catch (Exception e) {
                                Log.i("GeckoAddons", "error reading json file", e);
                            }
                        }
                    });
                } catch (Exception e) {
                    Log.i("GeckoAddons", "error reading json file", e);
                }
            }
        });
    }

    protected void onGridItemClick(GridView l, View v, int position, long id) {
        mCursor.moveToPosition(position);
        String spec = mCursor.getString(mCursor.getColumnIndex(Browser.BookmarkColumns.URL));
        Log.i(LOGTAG, "clicked: " + spec);
        if (mUriLoadCallback != null)
            mUriLoadCallback.callback(spec);
    }

}
class AwesomeCursorViewBinder implements SimpleCursorAdapter.ViewBinder {
    private boolean updateImage(View view, Cursor cursor, int faviconIndex) {
        byte[] b = cursor.getBlob(faviconIndex);
        ImageView favicon = (ImageView) view;

        if (b == null) {
            favicon.setImageResource(R.drawable.favicon);
        } else {
            Bitmap bitmap = BitmapFactory.decodeByteArray(b, 0, b.length);
            favicon.setImageBitmap(bitmap);
        }

        return true;
    }

    private boolean updateTitle(View view, Cursor cursor, int titleIndex) {
        String title = cursor.getString(titleIndex);
        TextView titleView = (TextView)view;
        // Use the URL instead of an empty title for consistency with the normal URL
        // bar view - this is the equivalent of getDisplayTitle() in Tab.java
        if (title == null || title.length() == 0) {
            int urlIndex = cursor.getColumnIndexOrThrow(Browser.BookmarkColumns.URL);
            title = cursor.getString(urlIndex);
        }

        titleView.setText(title);
        return true;
    }

    private boolean updateUrl(View view, Cursor cursor, int urlIndex) {
        String title = cursor.getString(urlIndex);
        TextView urlView = (TextView)view;
        if (title != null) {
            int index;
            if ((index = title.indexOf("://")) != -1)
                title = title.substring(index + 3);
            if (title.startsWith("www."))
                title = title.substring(4);
            if (title.endsWith("/"))
                title = title.substring(0, title.length() -1);
        }
        urlView.setText(title);
        return true;
    }

    @Override
    public boolean setViewValue(View view, Cursor cursor, int columnIndex) {
        int faviconIndex = cursor.getColumnIndexOrThrow(Browser.BookmarkColumns.FAVICON);
        if (columnIndex == faviconIndex) {
            return updateImage(view, cursor, faviconIndex);
        }

        int titleIndex = cursor.getColumnIndexOrThrow(Browser.BookmarkColumns.TITLE);
        if (columnIndex == titleIndex) {
            return updateTitle(view, cursor, titleIndex);
        }

        int urlIndex = cursor.getColumnIndexOrThrow(Browser.BookmarkColumns.URL);
        if (columnIndex == urlIndex) {
            return updateUrl(view, cursor, urlIndex);
        }

        int thumbIndex = cursor.getColumnIndexOrThrow("thumbnail");

        if (columnIndex == thumbIndex) {
            return updateImage(view, cursor, thumbIndex);
        }

        // Other columns are handled automatically
        return false;
    }

}
