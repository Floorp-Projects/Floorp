/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.widget.TabHost;
import android.widget.TabWidget;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserDB.URLColumns;

public class AwesomeBarTabs extends TabHost {
    private static final String LOGTAG = "GeckoAwesomeBarTabs";

    private Context mContext;
    private boolean mInflated;
    private LayoutInflater mInflater;
    private OnUrlOpenListener mUrlOpenListener;
    private View.OnTouchListener mListTouchListener;
    
    private AwesomeBarTab mTabs[];

    // FIXME: This value should probably come from a
    // prefs key (just like XUL-based fennec)
    private static final int MAX_RESULTS = 100;

    public interface OnUrlOpenListener {
        public void onUrlOpen(String url);
        public void onSearch(String engine, String text);
        public void onEditSuggestion(String suggestion);
    }

    private AwesomeBarTab getCurrentAwesomeBarTab() {
        String tag = getCurrentTabTag();
        return getAwesomeBarTabForTag(tag);
    }

    public AwesomeBarTab getAwesomeBarTabForView(View view) {
        String tag = (String)view.getTag();
        return getAwesomeBarTabForTag(tag);
    }

    public AwesomeBarTab getAwesomeBarTabForTag(String tag) {
        for (AwesomeBarTab tab : mTabs) {
            if (tag == tab.getTag()) {
                return tab;
            }
        }
        return null;
    }

    public boolean onBackPressed() {
        AwesomeBarTab tab = getCurrentAwesomeBarTab();
        if (tab == null)
             return false;
        return tab.onBackPressed();
    }

    public AwesomeBarTabs(Context context, AttributeSet attrs) {
        super(context, attrs);

        Log.d(LOGTAG, "Creating AwesomeBarTabs");

        mContext = context;
        mInflated = false;
        mInflater = (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
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

        // This should be called before adding any tabs
        // to the TabHost.
        setup();

        mListTouchListener = new View.OnTouchListener() {
            public boolean onTouch(View view, MotionEvent event) {
                if (event.getAction() == MotionEvent.ACTION_DOWN)
                    hideSoftInput(view);
                return false;
            }
        };

        mTabs = new AwesomeBarTab[] {
            new AllPagesTab(mContext),
            new BookmarksTab(mContext),
            new HistoryTab(mContext)
        };

        for (AwesomeBarTab tab : mTabs) {
            addAwesomeTab(tab);
        }

        styleSelectedTab();

         setOnTabChangedListener(new TabHost.OnTabChangeListener() {
             public void onTabChanged(String tabId) {
                 styleSelectedTab();
             }
         });

        // Initialize "App Pages" list with no filter
        filter("");
    }

    private void styleSelectedTab() {
        int selIndex = getCurrentTab();
        TabWidget tabWidget = getTabWidget();
        for (int i = 0; i < tabWidget.getTabCount(); i++) {
             if (i == selIndex)
                 continue;

             if (i == (selIndex - 1))
                 tabWidget.getChildTabViewAt(i).getBackground().setLevel(1);
             else if (i == (selIndex + 1))
                 tabWidget.getChildTabViewAt(i).getBackground().setLevel(2);
             else
                 tabWidget.getChildTabViewAt(i).getBackground().setLevel(0);
        }

        if (selIndex == 0)
            findViewById(R.id.tab_widget_left).getBackground().setLevel(1);
        else
            findViewById(R.id.tab_widget_left).getBackground().setLevel(0);

        if (selIndex == (tabWidget.getTabCount() - 1))
            findViewById(R.id.tab_widget_right).getBackground().setLevel(2);
        else
            findViewById(R.id.tab_widget_right).getBackground().setLevel(0);
    }


    private void addAwesomeTab(AwesomeBarTab tab) {
        TabSpec tabspec = getTabSpec(tab.getTag(), tab.getTitleStringId());
        tabspec.setContent(tab.getFactory());
        addTab(tabspec);
        tab.setListTouchListener(mListTouchListener);
 
        return;
    }

    private TabSpec getTabSpec(String id, int titleId) {
        TabSpec tab = newTabSpec(id);

        TextView indicatorView = (TextView) mInflater.inflate(R.layout.awesomebar_tab_indicator, null);
        indicatorView.setText(titleId);

        tab.setIndicator(indicatorView);
        return tab;
    }

    private boolean hideSoftInput(View view) {
        InputMethodManager imm =
                (InputMethodManager) mContext.getSystemService(Context.INPUT_METHOD_SERVICE);

        return imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
    }

    private String getReaderForUrl(String url) {
        // FIXME: still need to define the final way to open items from
        // reading list. For now, we're using an about:reader page.
        return "about:reader?url=" + url;
    }

    public void setOnUrlOpenListener(OnUrlOpenListener listener) {
        mUrlOpenListener = listener;
        for (AwesomeBarTab tab : mTabs) {
            tab.setUrlListener(listener);
        }
    }

    public void destroy() {
        for (AwesomeBarTab tab : mTabs) {
            tab.destroy();
        }
    }

    public AllPagesTab getAllPagesTab() {
        return (AllPagesTab)getAwesomeBarTabForTag("allPages");
    }

    public BookmarksTab getBookmarksTab() {
        return (BookmarksTab)getAwesomeBarTabForTag("bookmarks");
    }

    public HistoryTab getHistoryTab() {
        return (HistoryTab)getAwesomeBarTabForTag("history");
    }

    public void filter(String searchTerm) {
        // Don't let the tab's content steal focus on tab switch
        setDescendantFocusability(ViewGroup.FOCUS_BLOCK_DESCENDANTS);

        // Ensure the 'All Pages' tab is selected
        AllPagesTab allPages = getAllPagesTab();
        setCurrentTabByTag(allPages.getTag());

        // Restore normal focus behavior on tab host
        setDescendantFocusability(ViewGroup.FOCUS_AFTER_DESCENDANTS);

        // The tabs should only be visible if there's no on-going search
        int tabsVisibility = (searchTerm.length() == 0 ? View.VISIBLE : View.GONE);
        findViewById(R.id.tab_widget_container).setVisibility(tabsVisibility);

        // Perform the actual search
        allPages.filter(searchTerm);
    }

    /**
     * Sets suggestions associated with the current suggest engine.
     * If there is no suggest engine, this does nothing.
     */
    public void setSuggestions(final ArrayList<String> suggestions) {
        GeckoAppShell.getMainHandler().post(new Runnable() {
            public void run() {
                getAllPagesTab().setSuggestions(suggestions);
            }
        });
    }

    /**
     * Sets search engines to be shown for user-entered queries.
     */
    public void setSearchEngines(final String suggestEngineName, final JSONArray engines) {
        GeckoAppShell.getMainHandler().post(new Runnable() {
            public void run() {
                getAllPagesTab().setSearchEngines(suggestEngineName, engines);
            }
        });
    }

    public boolean isInReadingList() {
        return getBookmarksTab().isInReadingList();
    }
}
