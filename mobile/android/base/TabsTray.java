/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.ArrayList;

import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.Build;
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
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.mozilla.gecko.sync.setup.SyncAccounts;

public class TabsTray extends GeckoActivity implements Tabs.OnTabsChangedListener {

    private static int sPreferredHeight;
    private static int sMaxHeight;
    private static int sListItemHeight;
    private static int sAddTabHeight;
    private static ListView mList;
    private static TabsListContainer mListContainer;
    private static LinkTextView mRemoteTabs;
    private TabsAdapter mTabsAdapter;
    private boolean mWaitingForClose;

    // 100 for item + 2 for divider
    private static final int TABS_LIST_ITEM_HEIGHT = 102;
    private static final int TABS_ADD_TAB_HEIGHT = 50;

    private static final String ABOUT_HOME = "about:home";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        LayoutInflater.from(this).setFactory(GeckoViewsFactory.getInstance());

        setContentView(R.layout.tabs_tray);

        mWaitingForClose = false;

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

        ImageButton addTab = (ImageButton) findViewById(R.id.add_tab);
        addTab.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                GeckoApp.mAppContext.addTab();
                finishActivity();
            }
        });

        mRemoteTabs = (LinkTextView) findViewById(R.id.remote_tabs);
        mRemoteTabs.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                showRemoteTabs();
            }
        });

        RelativeLayout toolbar = (RelativeLayout) findViewById(R.id.toolbar);
        toolbar.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                // Consume the click event to avoid enclosing container consuming it
            }
        });

        LinearLayout container = (LinearLayout) findViewById(R.id.container);
        container.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                finishActivity();
            }
        });

        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(metrics);

        sListItemHeight = (int) (TABS_LIST_ITEM_HEIGHT * metrics.density);
        sAddTabHeight = (int) (TABS_ADD_TAB_HEIGHT * metrics.density);
        sPreferredHeight = (int) (0.67 * metrics.heightPixels);
        sMaxHeight = (int) (sPreferredHeight + (0.33 * sListItemHeight));

        Tabs.registerOnTabsChangedListener(this);
        Tabs.getInstance().refreshThumbnails();
        onTabChanged(null, null);

        // If Sync is set up, query the database for remote clients.
        final Context context = getApplicationContext();
        new SyncAccounts.AccountsExistTask() {
            @Override
            protected void onPostExecute(Boolean result) {
                if (!result.booleanValue()) {
                    return;
                }
                TabsAccessor.areClientsAvailable(context, new TabsAccessor.OnClientsAvailableListener() {
                    @Override
                    public void areAvailable(boolean available) {
                        final int visibility = available ? View.VISIBLE : View.GONE;
                        mRemoteTabs.setVisibility(visibility);
                    }
                });
            }
        }.execute(context);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Tabs.unregisterOnTabsChangedListener(this);
    }

    public void onTabChanged(Tab tab, Tabs.TabEvents msg) {
        if (Tabs.getInstance().getCount() == 1)
            finishActivity();

        if (mTabsAdapter == null) {
            mTabsAdapter = new TabsAdapter(this, Tabs.getInstance().getTabsInOrder());
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

    void finishActivity() {
        finish();
        overridePendingTransition(0, R.anim.shrink_fade_out);
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Tab:Screenshot:Cancel",""));
    }

    void showRemoteTabs() {
        Intent intent = new Intent(this, RemoteTabs.class);
        intent.putExtra("exit-to-tabs-tray", true);
        startActivity(intent);
        overridePendingTransition(R.anim.grow_fade_in, R.anim.shrink_fade_out);
        finishActivity();
    }

    // Tabs List Container holds the ListView and the New Tab button
    public static class TabsListContainer extends LinearLayout {
        public TabsListContainer(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            if (mList.getAdapter() == null) {
                super.onMeasure(widthMeasureSpec, heightMeasureSpec);
                return;
            }

            int restrictedHeightSpec;
            int childrenHeight = (mList.getAdapter().getCount() * sListItemHeight) + sAddTabHeight;

            if (childrenHeight <= sPreferredHeight) {
                restrictedHeightSpec = MeasureSpec.makeMeasureSpec(childrenHeight, MeasureSpec.EXACTLY);
            } else {
                if (((childrenHeight - sAddTabHeight) % sListItemHeight == 0) && (childrenHeight >= sMaxHeight))
                    restrictedHeightSpec = MeasureSpec.makeMeasureSpec(sMaxHeight, MeasureSpec.EXACTLY);
                else
                    restrictedHeightSpec = MeasureSpec.makeMeasureSpec(sPreferredHeight, MeasureSpec.EXACTLY);
            }

            super.onMeasure(widthMeasureSpec, restrictedHeightSpec);
        }
    }

    // ViewHolder for a row in the list
    private class TabRow {
        int id;
        TextView title;
        ImageView thumbnail;
        ImageView selectedIndicator;
        ImageButton close;

        public TabRow(View view) {
            title = (TextView) view.findViewById(R.id.title);
            thumbnail = (ImageView) view.findViewById(R.id.thumbnail);
            selectedIndicator = (ImageView) view.findViewById(R.id.selected_indicator);
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
                    finishActivity();
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

        public int getCount() {
            return mTabs.size();
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
                row.selectedIndicator.setVisibility(View.VISIBLE);
            else
                row.selectedIndicator.setVisibility(View.GONE);

            row.title.setText(tab.getDisplayTitle());

            row.close.setTag(String.valueOf(row.id));
            row.close.setVisibility(mTabs.size() > 1 ? View.VISIBLE : View.GONE);
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
