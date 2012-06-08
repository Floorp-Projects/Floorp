/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.ArrayList;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AbsListView.RecyclerListener;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;

public class TabsTray extends LinearLayout 
                      implements TabsPanel.PanelView,
                                 Tabs.OnTabsChangedListener {
    private static final String LOGTAG = "GeckoTabsTray";

    private Context mContext;
    private static boolean mHeightRestricted;

    private static int sPreferredHeight;
    private static int sListItemHeight;
    private static int sListDividerHeight;
    private static ListView mList;
    private static TabsListContainer mListContainer;
    private TabsAdapter mTabsAdapter;
    private boolean mWaitingForClose;

    private static final String ABOUT_HOME = "about:home";

    public TabsTray(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;

        LayoutInflater.from(context).inflate(R.layout.tabs_tray, this);

        mList = (ListView) findViewById(R.id.list);
        mList.setItemsCanFocus(true);

        mList.setRecyclerListener(new RecyclerListener() {
            @Override
            public void onMovedToScrapHeap(View view) {
                TabRow row = (TabRow) view.getTag();
                row.thumbnail.setImageDrawable(null);
            }
        });

        mListContainer = (TabsListContainer) findViewById(R.id.list_container);
    }

    @Override
    public ViewGroup getLayout() {
        return this;
    }

    @Override
    public void setHeightRestriction(boolean isRestricted) {
        mHeightRestricted = isRestricted;
    }

    @Override
    public void show() {
        mWaitingForClose = false;

        DisplayMetrics metrics = new DisplayMetrics();
        GeckoApp.mAppContext.getWindowManager().getDefaultDisplay().getMetrics(metrics);

        Resources resources = mContext.getResources();
        sListItemHeight = (int) (resources.getDimension(R.dimen.local_tab_row_height));
        sListDividerHeight = (int) (resources.getDimension(R.dimen.tabs_list_divider_height));
        sPreferredHeight = (int) ((0.5 * metrics.heightPixels) + (0.33 * sListItemHeight));

        Tabs.registerOnTabsChangedListener(this);
        Tabs.getInstance().refreshThumbnails();
        onTabChanged(null, null);
    }

    @Override
    public void hide() {
        Tabs.unregisterOnTabsChangedListener(this);
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Tab:Screenshot:Cancel",""));
        mTabsAdapter.clear();
        mTabsAdapter.notifyDataSetChanged();
    }

    public void onTabChanged(Tab tab, Tabs.TabEvents msg) {
        if (mTabsAdapter == null) {
            mTabsAdapter = new TabsAdapter(mContext, Tabs.getInstance().getTabsInOrder());
            mList.setAdapter(mTabsAdapter);
            mListContainer.requestLayout();

            int selected = mTabsAdapter.getPositionForTab(Tabs.getInstance().getSelectedTab());
            if (selected == -1)
                return;

            mList.setSelection(selected);
            return;
        }

        int position = mTabsAdapter.getPositionForTab(tab);
        if (position == -1)
            return;

        if (Tabs.getInstance().getIndexOf(tab) == -1) {
            mWaitingForClose = false;
            mTabsAdapter.removeTab(tab);
            mTabsAdapter.notifyDataSetChanged();
        } else {
            View view = mList.getChildAt(position - mList.getFirstVisiblePosition());
            if (view == null)
                return;

            TabRow row = (TabRow) view.getTag();
            mTabsAdapter.assignValues(row, tab);
        }
    }

    void hideTabs() {
        GeckoApp.mAppContext.hideTabs();
    }

    // Tabs List Container holds the ListView and the New Tab button
    public static class TabsListContainer extends LinearLayout {
        public TabsListContainer(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            if (mList.getAdapter() == null || !mHeightRestricted) {
                super.onMeasure(widthMeasureSpec, heightMeasureSpec);
                return;
            }

            int childrenHeight = (mList.getAdapter().getCount() * (sListItemHeight + sListDividerHeight)) - sListDividerHeight;
            int restrictedHeightSpec = MeasureSpec.makeMeasureSpec(Math.min(childrenHeight, sPreferredHeight), MeasureSpec.EXACTLY);
            super.onMeasure(widthMeasureSpec, restrictedHeightSpec);
        }
    }

    // ViewHolder for a row in the list
    private class TabRow {
        int id;
        TextView title;
        ImageView thumbnail;
        ImageButton close;
        LinearLayout info;

        public TabRow(View view) {
            info = (LinearLayout) view;
            title = (TextView) view.findViewById(R.id.title);
            thumbnail = (ImageView) view.findViewById(R.id.thumbnail);
            close = (ImageButton) view.findViewById(R.id.close);
        }
    }

    // Adapter to bind tabs into a list
    private class TabsAdapter extends BaseAdapter {
        private Context mContext;
        private ArrayList<Tab> mTabs;
        private LayoutInflater mInflater;
        private View.OnClickListener mOnInfoClickListener;
        private Button.OnClickListener mOnCloseClickListener;

        public TabsAdapter(Context context, ArrayList<Tab> tabs) {
            mContext = context;
            mInflater = LayoutInflater.from(mContext);
            mTabs = new ArrayList<Tab>();

            if (tabs == null)
                return;

            for (int i = 0; i < tabs.size(); i++) {
                mTabs.add(tabs.get(i));
            }

            mOnInfoClickListener = new View.OnClickListener() {
                public void onClick(View v) {
                    Tabs.getInstance().selectTab(((TabRow) v.getTag()).id);
                    hideTabs();
                }
            };

            mOnCloseClickListener = new Button.OnClickListener() {
                public void onClick(View v) {
                    if (mWaitingForClose)
                        return;

                    mWaitingForClose = true;

                    String tabId = v.getTag().toString();
                    Tabs tabs = Tabs.getInstance();
                    Tab tab = tabs.getTab(Integer.parseInt(tabId));
                    tabs.closeTab(tab);
                }
            };
        }

        public void clear() {
            mTabs = null;
        }

        public int getCount() {
            return (mTabs == null ? 0 : mTabs.size());
        }

        public Tab getItem(int position) {
            return mTabs.get(position);
        }

        public long getItemId(int position) {
            return position;
        }

        public int getPositionForTab(Tab tab) {
            if (mTabs == null || tab == null)
                return -1;

            return mTabs.indexOf(tab);
        }

        public void removeTab(Tab tab) {
            mTabs.remove(tab);
        }

        public void assignValues(TabRow row, Tab tab) {
            if (row == null || tab == null)
                return;

            row.id = tab.getId();

            Drawable thumbnailImage = tab.getThumbnail();
            if (thumbnailImage != null)
                row.thumbnail.setImageDrawable(thumbnailImage);
            else if (TextUtils.equals(tab.getURL(), ABOUT_HOME))
                row.thumbnail.setImageResource(R.drawable.abouthome_thumbnail);
            else
                row.thumbnail.setImageResource(R.drawable.tab_thumbnail_default);

            if (Tabs.getInstance().isSelectedTab(tab))
                row.info.setBackgroundResource(R.drawable.tabs_tray_active_selector);
            else
                row.info.setBackgroundResource(R.drawable.tabs_tray_default_selector);

            row.title.setText(tab.getDisplayTitle());

            row.close.setTag(String.valueOf(row.id));
            row.close.setVisibility(mTabs.size() > 1 ? View.VISIBLE : View.INVISIBLE);
        }

        public View getView(int position, View convertView, ViewGroup parent) {
            TabRow row;

            if (convertView == null) {
                convertView = mInflater.inflate(R.layout.tabs_row, null);
                convertView.setOnClickListener(mOnInfoClickListener);

                row = new TabRow(convertView);
                row.close.setOnClickListener(mOnCloseClickListener);

                convertView.setTag(row);
            } else {
                row = (TabRow) convertView.getTag();
            }

            Tab tab = mTabs.get(position);
            assignValues(row, tab);

            return convertView;
        }
    }
}
