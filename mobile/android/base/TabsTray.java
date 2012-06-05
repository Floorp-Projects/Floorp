/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.ArrayList;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.PointF;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.GestureDetector.SimpleOnGestureListener;
import android.view.LayoutInflater;
import android.view.MotionEvent;
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

import org.mozilla.gecko.gfx.PointUtils;
import org.mozilla.gecko.PropertyAnimator.Property;

public class TabsTray extends LinearLayout 
                      implements TabsPanel.PanelView,
                                 Tabs.OnTabsChangedListener {
    private static final String LOGTAG = "GeckoTabsTray";

    private Context mContext;

    private static ListView mList;
    private TabsAdapter mTabsAdapter;
    private boolean mWaitingForClose;

    private GestureDetector mGestureDetector;
    private TabSwipeGestureListener mListener;
    private static final int SWIPE_CLOSE_VELOCITY = 1000;
    private static final int MAX_ANIMATION_TIME = 250;
    private static enum DragDirection {
        UNKNOWN,
        HORIZONTAL,
        VERTICAL
    }

    private static final String ABOUT_HOME = "about:home";

    public TabsTray(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;

        LayoutInflater.from(context).inflate(R.layout.tabs_tray, this);

        mList = (ListView) findViewById(R.id.list);
        mList.setItemsCanFocus(true);
        mListener = new TabSwipeGestureListener(mList);
        mGestureDetector = new GestureDetector(context, mListener);

        mList.setOnTouchListener(new View.OnTouchListener() {
            public  boolean onTouch(View v, MotionEvent event) {
                boolean result = mGestureDetector.onTouchEvent(event);

                // if this is an touch end event, we need to reset the state
                // of the gesture listener
                switch (event.getAction() & MotionEvent.ACTION_MASK) {
                    case MotionEvent.ACTION_UP:
                      mListener.onTouchEnd(event);
                }

                return result;
            }
        });

        mList.setRecyclerListener(new RecyclerListener() {
            @Override
            public void onMovedToScrapHeap(View view) {
                TabRow row = (TabRow) view.getTag();
                row.thumbnail.setImageDrawable(null);
            }
        });
    }

    @Override
    public ViewGroup getLayout() {
        return this;
    }

    @Override
    public void show() {
        mWaitingForClose = false;

        Tabs.registerOnTabsChangedListener(this);
        Tabs.getInstance().refreshThumbnails();
        onTabChanged(null, null, null);
    }

    @Override
    public void hide() {
        Tabs.unregisterOnTabsChangedListener(this);
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Tab:Screenshot:Cancel",""));
        mTabsAdapter.clear();
        mTabsAdapter.notifyDataSetChanged();
    }

    public void onTabChanged(Tab tab, Tabs.TabEvents msg, Object data) {
        if (mTabsAdapter == null) {
            mTabsAdapter = new TabsAdapter(mContext, Tabs.getInstance().getTabsInOrder());
            mList.setAdapter(mTabsAdapter);

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

            mOnCloseClickListener = new Button.OnClickListener() {
                public void onClick(View v) {
                    TabRow tab = (TabRow) v.getTag();
                    animateTo(tab.info, tab.info.getWidth(), MAX_ANIMATION_TIME);
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

            // this may be a recycled view that was animated off screen
            // reset the scroll state here
            row.info.scrollTo(0,0);

            row.title.setText(tab.getDisplayTitle());

            row.close.setTag(row);
            row.close.setVisibility(mTabs.size() > 1 ? View.VISIBLE : View.INVISIBLE);
        }

        public View getView(int position, View convertView, ViewGroup parent) {
            TabRow row;

            if (convertView == null) {
                convertView = mInflater.inflate(R.layout.tabs_row, null);

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

    private void animateTo(final View view, int x, int duration) {
        PropertyAnimator pa = new PropertyAnimator(duration);
        pa.attach(view, Property.SLIDE_LEFT, x);
        if (x != 0 && !mWaitingForClose) {
            mWaitingForClose = true;

            TabRow tab = (TabRow)view.getTag();
            final int tabId = tab.id;

            pa.setPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
                public void onPropertyAnimationStart() { }
                public void onPropertyAnimationEnd() {
                    Tabs tabs = Tabs.getInstance();
                    Tab tab = tabs.getTab(tabId);
                    tabs.closeTab(tab);
                }
            });
        }
        pa.start();
    }

    private class TabSwipeGestureListener extends SimpleOnGestureListener {
        private View mList = null;
        private View mView = null;
        private PointF start = null;
        private DragDirection dir = DragDirection.UNKNOWN;

        public TabSwipeGestureListener(View v) {
            mList = v;
        }

        @Override
        public boolean onDown(MotionEvent e) {
            mView = findViewAt((int)e.getX(), (int)e.getY());
            if (mView == null)
                return false;

            mView.setPressed(true);
            start = new PointF(e.getX(), e.getY());
            return false;
        }

        public boolean onTouchEnd(MotionEvent e) {
            if (mView != null) {

                // if the user was dragging horizontally, check to see if we should close the tab
                if (dir == DragDirection.HORIZONTAL) {
    
                    int finalPos = 0;
                    if ((start.x > mList.getWidth()/2 && e.getX() < mList.getWidth()/2)) {
                        finalPos = -1 * mView.getWidth();
                    } else if (start.x < mList.getWidth()/2 && e.getX() > mList.getWidth()/2) {
                        finalPos = mView.getWidth();
                    }
    
                    animateTo(mView, finalPos, MAX_ANIMATION_TIME);
                } else if (mView != null && dir == DragDirection.UNKNOWN) {
                    // the user didn't attempt to scroll the view, so select the row
                    TabRow tab = (TabRow)mView.getTag();
                    int tabId = tab.id;
                    Tabs.getInstance().selectTab(tabId);
                    hideTabs();
                }
            }

            mView = null;
            start = null;
            dir = DragDirection.UNKNOWN;
            return false;
        }

        @Override
        public boolean onScroll(MotionEvent event1, MotionEvent event2, float distanceX, float distanceY) {
            if (mView == null)
                return false;

            // if there is only one tab left, we want to recognize the scroll and
            // stop any click/selection events, but not scroll/close the view
            if (Tabs.getInstance().getCount() == 1) {
                mView.setPressed(false);
                mView = null;
                return false;
            }

            if (dir == DragDirection.UNKNOWN) {
                if (Math.abs(distanceX) > Math.abs(distanceY)) {
                    dir = DragDirection.HORIZONTAL;
                } else {
                    dir = DragDirection.VERTICAL;
                }
                mView.setPressed(false);
            }

            if (dir == DragDirection.HORIZONTAL) {
                mView.scrollBy((int) distanceX, 0);
                return true;
            }

            return false;
        }

        @Override
        public boolean onFling(MotionEvent event1, MotionEvent event2, float velocityX, float velocityY) {
            if (mView == null || Tabs.getInstance().getCount() == 1)
                return false;

            if (Math.abs(velocityX)/GeckoAppShell.getDpi() > SWIPE_CLOSE_VELOCITY) {
                float d = (velocityX > 0 ? 1 : -1) * mView.getWidth();
                animateTo(mView, (int)d, (int)(d/velocityX));
            }

            return false; 
        }

        private View findViewAt(int x, int y) {
            if (mList == null)
                return null;

            ListView list = (ListView)mList;
            x += list.getScrollX();
            y += list.getScrollY();

            final int count = list.getChildCount();
            for (int i = count - 1; i >= 0; i--) {
                View child = list.getChildAt(i);
                if (child.getVisibility() == View.VISIBLE) {
                    if ((x >= child.getLeft()) && (x < child.getRight())
                            && (y >= child.getTop()) && (y < child.getBottom())) {
                        return child;
                    }
                }
            }
            return null;
        }
    }
}

