/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.graphics.drawable.StateListDrawable;
import android.net.Uri;
import android.support.v4.view.ViewPager;
import android.support.v4.view.PagerAdapter;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.widget.TabHost;
import android.widget.TabWidget;

public class AwesomeBarTabs extends TabHost
                            implements LightweightTheme.OnChangeListener { 
    private static final String LOGTAG = "GeckoAwesomeBarTabs";

    private Context mContext;
    private GeckoActivity mActivity;

    private boolean mInflated;
    private LayoutInflater mInflater;
    private OnUrlOpenListener mUrlOpenListener;
    private View.OnTouchListener mListTouchListener;
    private boolean mSearching = false;
    private String mTarget;
    private Background mBackground;
    private ViewPager mViewPager;
    private AwesomePagerAdapter mPagerAdapter;
    
    private AwesomeBarTab mTabs[];

    // FIXME: This value should probably come from a
    // prefs key (just like XUL-based fennec)
    private static final int MAX_RESULTS = 100;

    public interface OnUrlOpenListener {
        public void onUrlOpen(String url, String title);
        public void onSearch(String engine, String text);
        public void onEditSuggestion(String suggestion);
    }

    private class AwesomePagerAdapter extends PagerAdapter {
        public AwesomePagerAdapter() {
            super();
        }

        public Object instantiateItem(ViewGroup group, int index) {
            AwesomeBarTab tab = mTabs[index];
            group.addView(tab.getView());
            return tab;
        }

        public void destroyItem(ViewGroup group, int index, Object obj) {
            AwesomeBarTab tab = (AwesomeBarTab)obj;
            group.removeView(tab.getView());
        }

        public int getCount() {
            if (mSearching)
                return 1;
            return mTabs.length;
        }

        public boolean isViewFromObject(View view, Object object) {
            return getAwesomeBarTabForView(view) == object;
        }
    }

    private AwesomeBarTab getCurrentAwesomeBarTab() {
        int index = mViewPager.getCurrentItem();
        return mTabs[index];
    }

    public AwesomeBarTab getAwesomeBarTabForView(View view) {
        String tag = (String)view.getTag();
        return getAwesomeBarTabForTag(tag);
    }

    public AwesomeBarTab getAwesomeBarTabForTag(String tag) {
        for (AwesomeBarTab tab : mTabs) {
            if (tag.equals(tab.getTag())) {
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
        mActivity = (GeckoActivity) context;

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
                if (event.getActionMasked() == MotionEvent.ACTION_DOWN)
                    hideSoftInput(view);
                return false;
            }
        };

        mBackground = (Background) findViewById(R.id.awesomebar_background);

        mTabs = new AwesomeBarTab[] {
            new AllPagesTab(mContext),
            new BookmarksTab(mContext),
            new HistoryTab(mContext)
        };

        final TabWidget tabWidget = (TabWidget) findViewById(android.R.id.tabs);
        // hide the strip since we aren't using the TabHost...
        tabWidget.setStripEnabled(false);

        mViewPager = (ViewPager) findViewById(R.id.tabviewpager);
        mPagerAdapter = new AwesomePagerAdapter();
        mViewPager.setAdapter(mPagerAdapter);
        mViewPager.setCurrentItem(0);
        mViewPager.setOnPageChangeListener(new ViewPager.OnPageChangeListener() {
            public void onPageScrollStateChanged(int state) { }
            public void onPageScrolled(int position, float positionOffset, int positionOffsetPixels) { }
            public void onPageSelected(int position) {
                tabWidget.setCurrentTab(position);
                styleSelectedTab();
                hideSoftInput(mViewPager);
             }
         });

        for (int i = 0; i < mTabs.length; i++) {
            mTabs[i].setListTouchListener(mListTouchListener);
            addAwesomeTab(mTabs[i].getTag(),
                          mTabs[i].getTitleStringId(),
                          i);
        }

        tabWidget.setCurrentTab(0);

        styleSelectedTab();

        // Initialize "App Pages" list with no filter
        filter("");
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

    @Override
    public void onLightweightThemeChanged() {
        styleSelectedTab();
    }

    @Override
    public void onLightweightThemeReset() {
        styleSelectedTab();
    }

    private void styleSelectedTab() {
        int selIndex = mViewPager.getCurrentItem();
        TabWidget tabWidget = getTabWidget();
        boolean isPrivate = false;

        if (mTarget != null && mTarget.equals(AwesomeBar.Target.CURRENT_TAB.name())) {
            Tab tab = Tabs.getInstance().getSelectedTab();
            if (tab != null)
                isPrivate = tab.isPrivate();
        }

        for (int i = 0; i < tabWidget.getTabCount(); i++) {
            GeckoTextView view = (GeckoTextView) tabWidget.getChildTabViewAt(i);
            if (isPrivate) {
                view.setPrivateMode((i == selIndex) ? false : true);
            } else {
                if (i == selIndex)
                    view.resetTheme();
                else if (mActivity.getLightweightTheme().isEnabled())
                    view.setTheme(mActivity.getLightweightTheme().isLightTheme());
                else
                    view.resetTheme();
            }

            if (i == selIndex)
                continue;

            if (i == (selIndex - 1))
                view.getBackground().setLevel(1);
            else if (i == (selIndex + 1))
                view.getBackground().setLevel(2);
            else
                view.getBackground().setLevel(0);
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


    private View addAwesomeTab(String id, int titleId, final int contentId) {
        GeckoTextView indicatorView = (GeckoTextView) mInflater.inflate(R.layout.awesomebar_tab_indicator, null);
        indicatorView.setText(titleId);

        getTabWidget().addView(indicatorView);

        // this MUST be done after tw.addView to overwrite the listener added by tabWidget
        // which delegates to TabHost (which we don't have)
        indicatorView.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                mViewPager.setCurrentItem(contentId, true);
            }
        });

        return indicatorView;
    }

    private boolean hideSoftInput(View view) {
        InputMethodManager imm =
                (InputMethodManager) mContext.getSystemService(Context.INPUT_METHOD_SERVICE);

        return imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
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
        mSearching = searchTerm.length() != 0;
        // reset the pager adapter to force repopulating the cache
        mViewPager.setAdapter(mPagerAdapter);
        int tabsVisibility = !mSearching ? View.VISIBLE : View.GONE;
        findViewById(R.id.tab_widget_container).setVisibility(tabsVisibility);

        // Perform the actual search
        allPages.filter(searchTerm);
    }

    public boolean isInReadingList() {
        return getBookmarksTab().isInReadingList();
    }

    public void setTarget(String target) {
        mTarget = target;
        styleSelectedTab();
        if (mTarget.equals(AwesomeBar.Target.CURRENT_TAB.name())) {
            Tab tab = Tabs.getInstance().getSelectedTab();
            if (tab != null && tab.isPrivate())
                mBackground.setPrivateMode(true);
        }
    }

    public static class Background extends GeckoLinearLayout
                                   implements LightweightTheme.OnChangeListener { 
        private GeckoActivity mActivity;

        public Background(Context context, AttributeSet attrs) {
            super(context, attrs);
            mActivity = (GeckoActivity) context;
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

        @Override
        public void onLightweightThemeChanged() {
            LightweightThemeDrawable drawable = mActivity.getLightweightTheme().getColorDrawable(this);
            if (drawable == null)
                return;

            drawable.setAlpha(255, 0);

            StateListDrawable stateList = new StateListDrawable();
            stateList.addState(new int[] { R.attr.state_private }, mActivity.getResources().getDrawable(R.drawable.abouthome_bg_pb_repeat));
            stateList.addState(new int[] {}, drawable);

            int[] padding =  new int[] { getPaddingLeft(),
                                         getPaddingTop(),
                                         getPaddingRight(),
                                         getPaddingBottom()
                                       };
            setBackgroundDrawable(stateList);
            setPadding(padding[0], padding[1], padding[2], padding[3]);
        }

        @Override
        public void onLightweightThemeReset() {
            int[] padding =  new int[] { getPaddingLeft(),
                                         getPaddingTop(),
                                         getPaddingRight(),
                                         getPaddingBottom()
                                       };
            setBackgroundResource(R.drawable.awesomebar_tabs_bg);
            setPadding(padding[0], padding[1], padding[2], padding[3]);
        }

        @Override
        protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
            super.onLayout(changed, left, top, right, bottom);
            onLightweightThemeChanged();
        }
    }
}
