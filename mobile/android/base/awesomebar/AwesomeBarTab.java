/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.AwesomeBar.ContextMenuSubject;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.widget.FaviconView;

import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Resources;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.ImageView;
import android.widget.TextView;

import java.util.HashMap;

abstract public class AwesomeBarTab {
    abstract public String getTag();
    abstract public int getTitleStringId();
    abstract public boolean   onBackPressed();
    abstract public ContextMenuSubject getSubject(ContextMenu menu, View view, ContextMenuInfo menuInfo);
    abstract public View getView();

    protected View mView = null;
    protected View.OnTouchListener mListListener;
    private AwesomeBarTabs.OnUrlOpenListener mListener;
    private LayoutInflater mInflater = null;
    private ContentResolver mContentResolver = null;
    private Resources mResources;
    // FIXME: This value should probably come from a prefs key
    public static final int MAX_RESULTS = 100;
    protected Context mContext = null;
    public static HashMap<String, Integer> sOpenTabs;

    public AwesomeBarTab(Context context) {
        mContext = context;
    }

    public void destroy() {
        sOpenTabs = null;
    }

    public void setListTouchListener(View.OnTouchListener listener) {
        mListListener = listener;
        if (mView != null)
            mView.setOnTouchListener(mListListener);
    }

    protected class AwesomeEntryViewHolder {
        public TextView titleView;
        public String url;
        public TextView urlView;
        public FaviconView faviconView;
        public ImageView bookmarkIconView;
    }

    protected LayoutInflater getInflater() {
        if (mInflater == null) {
            mInflater = (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        }
        return mInflater;
    }

    protected AwesomeBarTabs.OnUrlOpenListener getUrlListener() {
        return mListener;
    }

    protected void setUrlListener(AwesomeBarTabs.OnUrlOpenListener listener) {
        mListener = listener;
    }

    protected ContentResolver getContentResolver() {
        if (mContentResolver == null) {
            mContentResolver = mContext.getContentResolver();
        }
        return mContentResolver;
    }

    protected HashMap<String, Integer> getOpenTabs() {
        if (sOpenTabs == null || sOpenTabs.isEmpty()) {
            Iterable<Tab> tabs = Tabs.getInstance().getTabsInOrder();
            sOpenTabs = new HashMap<String, Integer>();
            for (Tab tab : tabs) {
                sOpenTabs.put(tab.getURL(), tab.getId());
            }
        }
        return sOpenTabs;
    }

    protected Resources getResources() {
        if (mResources == null) {
            mResources = mContext.getResources();
        }
        return mResources;
    }

    protected void setupMenu(ContextMenu menu, AwesomeBar.ContextMenuSubject subject) {
        MenuInflater inflater = new MenuInflater(mContext);
        inflater.inflate(R.menu.awesomebar_contextmenu, menu);

        // Show Open Private Tab if we're in private mode, Open New Tab otherwise
        boolean isPrivate = false;
        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab != null) {
            isPrivate = tab.isPrivate();
        }
        menu.findItem(R.id.open_new_tab).setVisible(!isPrivate);
        menu.findItem(R.id.open_private_tab).setVisible(isPrivate);

        // Hide "Remove" item if there isn't a valid history ID
        if (subject.id < 0) {
            menu.findItem(R.id.remove_history).setVisible(false);
        }
        menu.setHeaderTitle(subject.title);
    }

    protected void updateFavicon(FaviconView faviconView, Bitmap bitmap, String key) {
        faviconView.updateImage(bitmap, key);
    }

    protected void updateTitle(TextView titleView, Cursor cursor) {
        int titleIndex = cursor.getColumnIndexOrThrow(URLColumns.TITLE);
        String title = cursor.getString(titleIndex);
        String url = "";

        // Use the URL instead of an empty title for consistency with the normal URL
        // bar view - this is the equivalent of getDisplayTitle() in Tab.java
        if (TextUtils.isEmpty(title)) {
            int urlIndex = cursor.getColumnIndexOrThrow(URLColumns.URL);
            url = cursor.getString(urlIndex);
        }

        updateTitle(titleView, title, url);
    }

    protected void updateTitle(TextView titleView, String title, String url) {
        if (TextUtils.isEmpty(title)) {
            titleView.setText(url);
        } else {
            titleView.setText(title);
        }
    }

    public void sendToListener(String url, String title) {
        AwesomeBarTabs.OnUrlOpenListener listener = getUrlListener();
        if (listener == null)
            return;

        Integer tabId = getOpenTabs().get(url);
        if (tabId != null) {
            listener.onSwitchToTab(tabId);
        } else {
            listener.onUrlOpen(url, title);
        }
    }

    protected void updateUrl(AwesomeEntryViewHolder holder, Cursor cursor) {
        int urlIndex = cursor.getColumnIndexOrThrow(URLColumns.URL);
        String url = cursor.getString(urlIndex);
        updateUrl(holder, url);
    }
    
    protected void updateUrl(AwesomeEntryViewHolder holder, String url) {
        Integer tabId = getOpenTabs().get(url);
        holder.url = url;
        if (tabId != null) {
            holder.urlView.setText(R.string.awesomebar_switch_to_tab);
            holder.urlView.setCompoundDrawablesWithIntrinsicBounds(R.drawable.ic_awesomebar_tab, 0, 0, 0);
        } else {
            holder.urlView.setText(url);
            holder.urlView.setCompoundDrawablesWithIntrinsicBounds(0, 0, 0, 0);
        }
    }

    protected boolean hideSoftInput(View view) {
        InputMethodManager imm =
                (InputMethodManager) mContext.getSystemService(Context.INPUT_METHOD_SERVICE);

        return imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
    }
}
