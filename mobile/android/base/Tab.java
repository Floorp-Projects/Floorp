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
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

import android.content.ContentResolver;
import android.database.ContentObserver;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.AsyncTask;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Surface;
import android.view.View;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.gfx.Layer;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.regex.Pattern;
import java.util.regex.Matcher;

public final class Tab {
    private static final String LOGTAG = "GeckoTab";
    private static final int kThumbnailWidth = 136;
    private static final int kThumbnailHeight = 78;

    private static float sDensity = 0.0f;
    private static Pattern sColorPattern;
    private int mId;
    private String mUrl;
    private String mTitle;
    private Drawable mFavicon;
    private String mFaviconUrl;
    private JSONObject mIdentityData;
    private Drawable mThumbnail;
    private List<HistoryEntry> mHistory;
    private int mHistoryIndex;
    private int mParentId;
    private boolean mExternal;
    private boolean mBookmark;
    private HashMap<String, DoorHanger> mDoorHangers;
    private long mFaviconLoadId;
    private String mDocumentURI;
    private String mContentType;
    private boolean mHasTouchListeners;
    private ArrayList<View> mPluginViews;
    private HashMap<Object, Layer> mPluginLayers;
    private ContentResolver mContentResolver;
    private ContentObserver mContentObserver;
    private int mCheckerboardColor = Color.WHITE;
    private int mState;

    public static final int STATE_DELAYED = 0;
    public static final int STATE_LOADING = 1;
    public static final int STATE_SUCCESS = 2;
    public static final int STATE_ERROR = 3;

    public static final class HistoryEntry {
        public String mUri;         // must never be null
        public String mTitle;       // must never be null

        public HistoryEntry(String uri, String title) {
            mUri = uri;
            mTitle = title;
        }
    }

    public Tab(int id, String url, boolean external, int parentId, String title) {
        mId = id;
        mUrl = url;
        mExternal = external;
        mParentId = parentId;
        mTitle = title;
        mFavicon = null;
        mFaviconUrl = null;
        mIdentityData = null;
        mThumbnail = null;
        mHistory = new ArrayList<HistoryEntry>();
        mHistoryIndex = -1;
        mBookmark = false;
        mDoorHangers = new HashMap<String, DoorHanger>();
        mFaviconLoadId = 0;
        mDocumentURI = "";
        mContentType = "";
        mPluginViews = new ArrayList<View>();
        mPluginLayers = new HashMap<Object, Layer>();
        mState = "about:home".equals(url) ? STATE_SUCCESS : STATE_LOADING;
        mContentResolver = Tabs.getInstance().getContentResolver();
        mContentObserver = new ContentObserver(GeckoAppShell.getHandler()) {
            public void onChange(boolean selfChange) {
                updateBookmark();
            }
        };
        BrowserDB.registerBookmarkObserver(mContentResolver, mContentObserver);
    }

    public void onDestroy() {
        mDoorHangers = new HashMap<String, DoorHanger>();
        BrowserDB.unregisterBookmarkObserver(mContentResolver, mContentObserver);
    }

    public int getId() {
        return mId;
    }

    public int getParentId() {
        return mParentId;
    }

    // may be null if user-entered query hasn't yet been resolved to a URI
    public String getURL() {
        return mUrl;
    }

    public String getTitle() {
        return mTitle;
    }

    public String getDisplayTitle() {
        if (mTitle != null && mTitle.length() > 0) {
            return mTitle;
        }

        return mUrl;
    }

    public Drawable getFavicon() {
        return mFavicon;
    }

    public Drawable getThumbnail() {
        return mThumbnail;
    }

    float getDensity() {
        if (sDensity == 0.0f) {
            DisplayMetrics metrics = new DisplayMetrics();
            GeckoApp.mAppContext.getWindowManager().getDefaultDisplay().getMetrics(metrics);
            sDensity = metrics.density;
        }
        return sDensity;
    }

    int getThumbnailWidth() {
        return (int)(kThumbnailWidth * getDensity());
    }

    int getThumbnailHeight() {
        return (int)(kThumbnailHeight * getDensity());
    }

    public void updateThumbnail(final Bitmap b) {
        final Tab tab = this;
        GeckoAppShell.getHandler().post(new Runnable() {
            public void run() {
                if (b != null) {
                    try {
                        Bitmap bitmap = Bitmap.createScaledBitmap(b, getThumbnailWidth(), getThumbnailHeight(), false);

                        if (mState == Tab.STATE_SUCCESS)
                            saveThumbnailToDB(new BitmapDrawable(bitmap));

                        mThumbnail = new BitmapDrawable(bitmap);
                        b.recycle();
                    } catch (OutOfMemoryError oom) {
                        Log.e(LOGTAG, "Unable to create/scale bitmap", oom);
                        mThumbnail = null;
                    }
                } else {
                    mThumbnail = null;
                }
                GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
                    public void run() {
                        Tabs.getInstance().notifyListeners(tab, Tabs.TabEvents.THUMBNAIL);
                    }
                });
            }
        });
    }

    public String getFaviconURL() {
        return mFaviconUrl;
    }

    public String getSecurityMode() {
        try {
            return mIdentityData.getString("mode");
        } catch (Exception e) {
            // If mIdentityData is null, or we get a JSONException
            return SiteIdentityPopup.UNKNOWN;
        }
    }

    public JSONObject getIdentityData() {
        return mIdentityData;
    }

    public boolean isBookmark() {
        return mBookmark;
    }

    public boolean isExternal() {
        return mExternal;
    }

    public void updateURL(String url) {
        if (url != null && url.length() > 0) {
            mUrl = url;
            Log.i(LOGTAG, "Updated url: " + url + " for tab with id: " + mId);
            updateBookmark();
            updateHistoryEntry(mUrl, mTitle);
        }
    }

    public void setDocumentURI(String documentURI) {
        mDocumentURI = documentURI;
    }

    public String getDocumentURI() {
        return mDocumentURI;
    }

    public void setContentType(String contentType) {
        mContentType = contentType;
    }

    public String getContentType() {
        return mContentType;
    }

    public void updateTitle(String title) {
        mTitle = (title == null ? "" : title);

        Log.i(LOGTAG, "Updated title: " + mTitle + " for tab with id: " + mId);
        updateHistoryEntry(mUrl, mTitle);
    }

    private void updateHistoryEntry(final String uri, final String title) {
        final HistoryEntry he = getLastHistoryEntry();
        if (he != null) {
            he.mUri = uri;
            he.mTitle = title;
            GeckoAppShell.getHandler().post(new Runnable() {
                public void run() {
                    GlobalHistory.getInstance().update(uri, title);
                }
            });
        } else {
            Log.e(LOGTAG, "Requested title update on empty history stack");
        }
    }

    public void setState(int state) {
        mState = state;
    }

    public int getState() {
        return mState;
    }

    public void setHasTouchListeners(boolean aValue) {
        mHasTouchListeners = aValue;
    }

    public boolean getHasTouchListeners() {
        return mHasTouchListeners;
    }

    public void setFaviconLoadId(long faviconLoadId) {
        mFaviconLoadId = faviconLoadId;
    }

    public long getFaviconLoadId() {
        return mFaviconLoadId;
    }

    public HistoryEntry getLastHistoryEntry() {
        if (mHistory.isEmpty())
            return null;
        return mHistory.get(mHistoryIndex);
    }

    public void updateFavicon(Drawable favicon) {
        mFavicon = favicon;
        Log.i(LOGTAG, "Updated favicon for tab with id: " + mId);
    }

    public void updateFaviconURL(String faviconUrl) {
        mFaviconUrl = faviconUrl;
        Log.i(LOGTAG, "Updated favicon URL for tab with id: " + mId);
    }


    public void updateIdentityData(JSONObject identityData) {
        mIdentityData = identityData;
    }

    private void updateBookmark() {
        final String url = getURL();
        if (url == null)
            return;

        GeckoBackgroundThread.getHandler().post(new Runnable() {
            public void run() {
                boolean bookmark = BrowserDB.isBookmark(mContentResolver, url);
                if (url.equals(getURL())) {
                    mBookmark = bookmark;
                }
            }
        });
    }

    public void addBookmark() {
        GeckoAppShell.getHandler().post(new Runnable() {
            public void run() {
                String url = getURL();
                if (url == null)
                    return;

                BrowserDB.addBookmark(mContentResolver, getTitle(), url);
            }
        });
    }

    public void removeBookmark() {
        GeckoAppShell.getHandler().post(new Runnable() {
            public void run() {
                String url = getURL();
                if (url == null)
                    return;

                BrowserDB.removeBookmarksWithURL(mContentResolver, url);
            }
        });
    }

    public boolean doReload() {
        GeckoEvent e = GeckoEvent.createBroadcastEvent("Session:Reload", "");
        GeckoAppShell.sendEventToGecko(e);
        return true;
    }

    public boolean doBack() {
        if (mHistoryIndex < 1) {
            return false;
        }
        GeckoEvent e = GeckoEvent.createBroadcastEvent("Session:Back", "");
        GeckoAppShell.sendEventToGecko(e);
        return true;
    }

    public boolean doStop() {
        GeckoEvent e = GeckoEvent.createBroadcastEvent("Session:Stop", "");
        GeckoAppShell.sendEventToGecko(e);
        return true;
    }

    public boolean canDoForward() {
        return (mHistoryIndex + 1 < mHistory.size());
    }

    public boolean doForward() {
        if (mHistoryIndex + 1 >= mHistory.size()) {
            return false;
        }
        GeckoEvent e = GeckoEvent.createBroadcastEvent("Session:Forward", "");
        GeckoAppShell.sendEventToGecko(e);
        return true;
    }

    public void addDoorHanger(String value, DoorHanger dh) {
        mDoorHangers.put(value, dh);
    }

    public void removeDoorHanger(String value) {
        mDoorHangers.remove(value);
    }

    public void removeTransientDoorHangers() {
        // Make a temporary set to avoid a ConcurrentModificationException
        final HashSet<String> valuesToRemove = new HashSet<String>(); 

        for (String value : mDoorHangers.keySet()) {
            DoorHanger dh = mDoorHangers.get(value);
            if (dh.shouldRemove())
                valuesToRemove.add(value);
        }

        for (String value : valuesToRemove) {
            mDoorHangers.remove(value);
        }
    }

    public DoorHanger getDoorHanger(String value) {
        if (mDoorHangers == null)
            return null;

        if (mDoorHangers.containsKey(value))
            return mDoorHangers.get(value);

        return null;
    }

    public HashMap<String, DoorHanger> getDoorHangers() {
        return mDoorHangers;
    }

    void handleSessionHistoryMessage(String event, JSONObject message) throws JSONException {
        if (event.equals("New")) {
            final String uri = message.getString("uri");
            mHistoryIndex++;
            while (mHistory.size() > mHistoryIndex) {
                mHistory.remove(mHistoryIndex);
            }
            HistoryEntry he = new HistoryEntry(uri, "");
            mHistory.add(he);
            GeckoAppShell.getHandler().post(new Runnable() {
                public void run() {
                    GlobalHistory.getInstance().add(uri);
                }
            });
        } else if (event.equals("Back")) {
            if (mHistoryIndex - 1 < 0) {
                Log.e(LOGTAG, "Received unexpected back notification");
                return;
            }
            mHistoryIndex--;
        } else if (event.equals("Forward")) {
            if (mHistoryIndex + 1 >= mHistory.size()) {
                Log.e(LOGTAG, "Received unexpected forward notification");
                return;
            }
            mHistoryIndex++;
        } else if (event.equals("Goto")) {
            int index = message.getInt("index");
            if (index < 0 || index >= mHistory.size()) {
                Log.e(LOGTAG, "Received unexpected history-goto notification");
                return;
            }
            mHistoryIndex = index;
        } else if (event.equals("Purge")) {
            mHistory.clear();
            mHistoryIndex = -1;
        }
    }

    private void saveThumbnailToDB(BitmapDrawable thumbnail) {
        try {
            String url = getURL();
            if (url == null)
                return;

            BrowserDB.updateThumbnailForUrl(mContentResolver, url, thumbnail);
        } catch (Exception e) {
            // ignore
        }
    }

    public void addPluginView(View view) {
        mPluginViews.add(view);
    }

    public void removePluginView(View view) {
        mPluginViews.remove(view);
    }

    public View[] getPluginViews() {
        return mPluginViews.toArray(new View[mPluginViews.size()]);
    }

    public void addPluginLayer(Object surfaceOrView, Layer layer) {
        mPluginLayers.put(surfaceOrView, layer);
    }

    public Layer getPluginLayer(Object surfaceOrView) {
        return mPluginLayers.get(surfaceOrView);
    }

    public Collection<Layer> getPluginLayers() {
        return mPluginLayers.values();
    }

    public Layer removePluginLayer(Object surfaceOrView) {
        return mPluginLayers.remove(surfaceOrView);
    }

    public int getCheckerboardColor() {
        return mCheckerboardColor;
    }

    /** Sets a new color for the checkerboard. */
    public void setCheckerboardColor(int color) {
        mCheckerboardColor = color;
    }

    /** Parses and sets a new color for the checkerboard. */
    public void setCheckerboardColor(String newColor) {
        setCheckerboardColor(parseColorFromGecko(newColor));
    }

    // Parses a color from an RGB triple of the form "rgb([0-9]+, [0-9]+, [0-9]+)". If the color
    // cannot be parsed, returns white.
    private static int parseColorFromGecko(String string) {
        if (sColorPattern == null) {
            sColorPattern = Pattern.compile("rgb\\((\\d+),\\s*(\\d+),\\s*(\\d+)\\)");
        }

        Matcher matcher = sColorPattern.matcher(string);
        if (!matcher.matches()) {
            return Color.WHITE;
        }

        int r = Integer.parseInt(matcher.group(1));
        int g = Integer.parseInt(matcher.group(2));
        int b = Integer.parseInt(matcher.group(3));
        return Color.rgb(r, g, b);
    }
}
