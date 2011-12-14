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
import android.graphics.drawable.Drawable;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.Bitmap;
import android.util.DisplayMetrics;
import android.util.Log;
import android.graphics.Bitmap;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

import org.mozilla.gecko.db.BrowserDB;

public class Tab {
    public static enum AgentMode { MOBILE, DESKTOP };
    private static final String LOGTAG = "GeckoTab";
    private static final int kThumbnailSize = 96;

    static int sMinDim = 0;
    private int mId;
    private String mUrl;
    private String mTitle;
    private Drawable mFavicon;
    private String mFaviconUrl;
    private String mSecurityMode;
    private Drawable mThumbnail;
    private List<HistoryEntry> mHistory;
    private int mHistoryIndex;
    private boolean mLoading;
    private boolean mBookmark;
    private HashMap<String, DoorHanger> mDoorHangers;
    private long mFaviconLoadId;
    private AgentMode mAgentMode = AgentMode.MOBILE;
    private String mDocumentURI;
    private String mContentType;

    static class HistoryEntry {
        public final String mUri;   // must never be null
        public String mTitle;       // must never be null

        public HistoryEntry(String uri, String title) {
            mUri = uri;
            mTitle = title;
        }
    }

    public Tab() {
        this(-1, "");
    }

    public Tab(int id, String url) {
        mId = id;
        mUrl = url;
        mTitle = "";
        mFavicon = null;
        mFaviconUrl = null;
        mSecurityMode = "unknown";
        mThumbnail = null;
        mHistory = new ArrayList<HistoryEntry>();
        mHistoryIndex = -1;
        mBookmark = false;
        mDoorHangers = new HashMap<String, DoorHanger>();
        mFaviconLoadId = 0;
        mDocumentURI = "";
        mContentType = "";
    }

    public int getId() {
        return mId;
    }

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

    public void updateThumbnail(final Bitmap b) {
        GeckoAppShell.getHandler().post(new Runnable() {
            public void run() {
                if (sMinDim == 0) {
                    DisplayMetrics metrics = new DisplayMetrics();
                    GeckoApp.mAppContext.getWindowManager().getDefaultDisplay().getMetrics(metrics);
                    sMinDim = Math.min(metrics.widthPixels, metrics.heightPixels);
                }
                if (b != null) {
                    try {
                        Bitmap cropped = Bitmap.createBitmap(b, 0, 0, sMinDim, sMinDim);
                        Bitmap bitmap = Bitmap.createScaledBitmap(cropped, kThumbnailSize, kThumbnailSize, false);
                        mThumbnail = new BitmapDrawable(bitmap);
                        saveThumbnailToDB((BitmapDrawable) mThumbnail);
                    } catch (OutOfMemoryError oom) {
                        Log.e(LOGTAG, "Unable to create/scale bitmap", oom);
                        mThumbnail = null;
                    }
                } else {
                    mThumbnail = null;
                }
            }
        });
    }

    public String getFaviconURL() {
        return mFaviconUrl;
    }

    public String getSecurityMode() {
        return mSecurityMode;
    }

    public boolean isLoading() {
        return mLoading;
    }

    public boolean isBookmark() {
        return mBookmark;
    }

    public void updateURL(String url) {
        if (url != null && url.length() > 0) {
            mUrl = url;
            Log.i(LOGTAG, "Updated url: " + url + " for tab with id: " + mId);
            updateBookmark();
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

        final HistoryEntry he = getLastHistoryEntry();
        if (he != null) {
            he.mTitle = mTitle;
            GeckoAppShell.getHandler().post(new Runnable() {
                public void run() {
                    GlobalHistory.getInstance().update(he.mUri, he.mTitle);
                }
            });
        } else {
            Log.e(LOGTAG, "Requested title update on empty history stack");
        }
    }

    public void setLoading(boolean loading) {
        mLoading = loading;
    }

    private void setBookmark(boolean bookmark) {
        mBookmark = bookmark;
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

    public void updateSecurityMode(String mode) {
        mSecurityMode = mode;
    }

    private void updateBookmark() {
        new CheckBookmarkTask().execute();
    }

    public void addBookmark() {
        new AddBookmarkTask().execute();
    }

    public void removeBookmark() {
        new RemoveBookmarkTask().execute();
    }

    public boolean doReload() {
        if (mHistory.isEmpty())
            return false;
        GeckoEvent e = new GeckoEvent("Session:Reload", "");
        GeckoAppShell.sendEventToGecko(e);
        return true;
    }

    public boolean doBack() {
        if (mHistoryIndex < 1) {
            return false;
        }
        GeckoEvent e = new GeckoEvent("Session:Back", "");
        GeckoAppShell.sendEventToGecko(e);
        return true;
    }

    public boolean doStop() {
        GeckoEvent e = new GeckoEvent("Session:Stop", "");
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
        GeckoEvent e = new GeckoEvent("Session:Forward", "");
        GeckoAppShell.sendEventToGecko(e);
        return true;
    }

    public void addDoorHanger(String value, DoorHanger dh) {
        mDoorHangers.put(value, dh);
    } 

    public void removeDoorHanger(String value) {
        mDoorHangers.remove(value);
    }

    public void removeAllDoorHangers() {
        mDoorHangers = new HashMap<String, DoorHanger>();
    }

    public void removeTransientDoorHangers() {
        for (String value : mDoorHangers.keySet()) {
            DoorHanger dh = mDoorHangers.get(value);
            if (dh.shouldRemove())
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

    private class CheckBookmarkTask extends GeckoAsyncTask<Void, Void, Boolean> {
        @Override
        protected Boolean doInBackground(Void... unused) {
            ContentResolver resolver = Tabs.getInstance().getContentResolver();
            return BrowserDB.isBookmark(resolver, getURL());
        }

        @Override
        protected void onPostExecute(Boolean isBookmark) {
            setBookmark(isBookmark.booleanValue());
        }
    }

    private class AddBookmarkTask extends GeckoAsyncTask<Void, Void, Void> {
        @Override
        protected Void doInBackground(Void... unused) {
            ContentResolver resolver = Tabs.getInstance().getContentResolver();
            BrowserDB.addBookmark(resolver, getTitle(), getURL());
            return null;
        }

        @Override
        protected void onPostExecute(Void unused) {
            setBookmark(true);
        }
    }

    private void saveThumbnailToDB(BitmapDrawable thumbnail) {
        try {
            ContentResolver resolver = Tabs.getInstance().getContentResolver();
            BrowserDB.updateThumbnailForUrl(resolver, getURL(), thumbnail);
        } catch (Exception e) {
            // ignore
        }
    }

    private class RemoveBookmarkTask extends GeckoAsyncTask<Void, Void, Void> {
        @Override
        protected Void doInBackground(Void... unused) {
            ContentResolver resolver = Tabs.getInstance().getContentResolver();
            BrowserDB.removeBookmark(resolver, getURL());
            return null;
        }

        @Override
        protected void onPostExecute(Void unused) {
            setBookmark(false);
        }
    }

    public void setAgentMode(AgentMode agentMode) {
        mAgentMode = agentMode;
    }

    public AgentMode getAgentMode() {
        return mAgentMode;
    }
}
