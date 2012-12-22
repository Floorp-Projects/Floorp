/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.PropertyAnimator.Property;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.VelocityTracker;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.widget.AbsListView;
import android.widget.AbsListView.RecyclerListener;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

public class TabsTray extends ListView 
                      implements TabsPanel.PanelView {
    private static final String LOGTAG = "GeckoTabsTray";

    private Context mContext;
    private TabsPanel mTabsPanel;

    private TabsAdapter mTabsAdapter;

    private List<View> mPendingClosedTabs;
    private int mCloseAnimationCount;

    private TabSwipeGestureListener mSwipeListener;

    // Time to animate non-flinged tabs of screen, in milliseconds
    private static final int ANIMATION_DURATION = 250;

    private static final String ABOUT_HOME = "about:home";

    public TabsTray(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;

        mCloseAnimationCount = 0;
        mPendingClosedTabs = new ArrayList<View>();

        setItemsCanFocus(true);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.TabsTray);
        boolean isPrivate = (a.getInt(R.styleable.TabsTray_tabs, 0x0) == 1);
        a.recycle();

        mTabsAdapter = new TabsAdapter(mContext, isPrivate);
        setAdapter(mTabsAdapter);

        mSwipeListener = new TabSwipeGestureListener();
        setOnTouchListener(mSwipeListener);
        setOnScrollListener(mSwipeListener.makeScrollListener());

        setRecyclerListener(new RecyclerListener() {
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
    public void setTabsPanel(TabsPanel panel) {
        mTabsPanel = panel;
    }

    @Override
    public void show() {
        Tabs.getInstance().refreshThumbnails();
        Tabs.registerOnTabsChangedListener(mTabsAdapter);
        mTabsAdapter.refreshTabsData();
    }

    @Override
    public void hide() {
        Tabs.unregisterOnTabsChangedListener(mTabsAdapter);
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Tab:Screenshot:Cancel",""));
        mTabsAdapter.clear();
    }

    private void autoHidePanel() {
        mTabsPanel.autoHidePanel();
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
    private class TabsAdapter extends BaseAdapter implements Tabs.OnTabsChangedListener {
        private Context mContext;
        private boolean mIsPrivate;
        private ArrayList<Tab> mTabs;
        private LayoutInflater mInflater;
        private Button.OnClickListener mOnCloseClickListener;

        public TabsAdapter(Context context, boolean isPrivate) {
            mContext = context;
            mInflater = LayoutInflater.from(mContext);
            mIsPrivate = isPrivate;

            mOnCloseClickListener = new Button.OnClickListener() {
                public void onClick(View v) {
                    TabRow tab = (TabRow) v.getTag();
                    animateClose(tab.info, tab.info.getWidth());
                }
            };
        }

        public void onTabChanged(Tab tab, Tabs.TabEvents msg, Object data) {
            switch (msg) {
                case ADDED:
                    // Refresh the list to make sure the new tab is added in the right position.
                    refreshTabsData();
                    break;

                case CLOSED:
                    removeTab(tab);
                    break;

                case SELECTED:
                    // Update the selected position, then fall through...
                    updateSelectedPosition();
                case UNSELECTED:
                    // We just need to update the style for the unselected tab...
                case THUMBNAIL:
                case TITLE:
                    View view = TabsTray.this.getChildAt(getPositionForTab(tab) - TabsTray.this.getFirstVisiblePosition());
                    if (view == null)
                        return;

                    TabRow row = (TabRow) view.getTag();
                    assignValues(row, tab);
                    break;
            }
        }

        private void refreshTabsData() {
            // Store a different copy of the tabs, so that we don't have to worry about
            // accidentally updating it on the wrong thread.
            mTabs = new ArrayList<Tab>();

            Iterable<Tab> tabs = Tabs.getInstance().getTabsInOrder();
            for (Tab tab : tabs) {
                if (tab.isPrivate() == mIsPrivate)
                    mTabs.add(tab);
            }

            notifyDataSetChanged(); // Be sure to call this whenever mTabs changes.
            updateSelectedPosition();
        }

        // Updates the selected position in the list so that it will be scrolled to the right place.
        private void updateSelectedPosition() {
            int selected = getPositionForTab(Tabs.getInstance().getSelectedTab());
            if (selected == -1)
                return;

            TabsTray.this.setSelection(selected);
        }

        public void clear() {
            mTabs = null;
            notifyDataSetChanged(); // Be sure to call this whenever mTabs changes.
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

        private int getPositionForTab(Tab tab) {
            if (mTabs == null || tab == null)
                return -1;

            return mTabs.indexOf(tab);
        }

        private void removeTab(Tab tab) {
            if (tab.isPrivate() == mIsPrivate) {
                mTabs.remove(tab);
                notifyDataSetChanged(); // Be sure to call this whenever mTabs changes.
            }
        }

        private void assignValues(TabRow row, Tab tab) {
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

            row.close.setTag(row);
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
                row.close.setVisibility(View.VISIBLE);
            }

            Tab tab = mTabs.get(position);
            assignValues(row, tab);

            return convertView;
        }
    }

    private void animateClose(final View view, int x) {
        PropertyAnimator animator = new PropertyAnimator(ANIMATION_DURATION);
        animator.attach(view, Property.ALPHA, 0);
        animator.attach(view, Property.TRANSLATION_X, x);

        mCloseAnimationCount++;
        mPendingClosedTabs.add(view);

        animator.setPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
            public void onPropertyAnimationStart() { }
            public void onPropertyAnimationEnd() {
                mCloseAnimationCount--;
                if (mCloseAnimationCount > 0)
                    return;

                for (View pendingView : mPendingClosedTabs) {
                    animateFinishClose(pendingView);
                }

                mPendingClosedTabs.clear();
            }
        });

        if (mTabsAdapter.getCount() == 1)
            autoHidePanel();

        animator.start();
    }

    private void animateFinishClose(final View view) {
        PropertyAnimator animator = new PropertyAnimator(ANIMATION_DURATION);
        animator.attach(view, Property.HEIGHT, 1);

        TabRow tab = (TabRow)view.getTag();
        final int tabId = tab.id;
        final int originalHeight = view.getHeight();

        animator.setPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
            public void onPropertyAnimationStart() { }
            public void onPropertyAnimationEnd() {
                // Reset view presentation as it will be recycled in the
                // list view by the adapter.
                AnimatorProxy proxy = AnimatorProxy.create(view);
                proxy.setAlpha(1);
                proxy.setTranslationX(0);
                proxy.setHeight(originalHeight);

                Tabs tabs = Tabs.getInstance();
                Tab tab = tabs.getTab(tabId);
                tabs.closeTab(tab);
            }
        });

        animator.start();
    }

    private void animateCancel(final View view) {
        PropertyAnimator animator = new PropertyAnimator(ANIMATION_DURATION);
        animator.attach(view, Property.ALPHA, 1);
        animator.attach(view, Property.TRANSLATION_X, 0);

        animator.setPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
            public void onPropertyAnimationStart() { }
            public void onPropertyAnimationEnd() {
                TabRow tab = (TabRow) view.getTag();
                tab.close.setVisibility(View.VISIBLE);
            }
        });

        animator.start();
    }

    private class TabSwipeGestureListener implements View.OnTouchListener {
        // same value the stock browser uses for after drag animation velocity in pixels/sec
        // http://androidxref.com/4.0.4/xref/packages/apps/Browser/src/com/android/browser/NavTabScroller.java#61
        private static final float MIN_VELOCITY = 750;

        private int mSwipeThreshold;
        private int mMinFlingVelocity;

        private int mMaxFlingVelocity;
        private VelocityTracker mVelocityTracker;

        private int mListWidth = 1;

        private View mSwipeView;
        private AnimatorProxy mSwipeProxy;
        private int mSwipeViewPosition;
        private Runnable mPendingCheckForTap;

        private float mSwipeStart;
        private boolean mSwiping;
        private boolean mEnabled;

        public TabSwipeGestureListener() {
            mSwipeView = null;
            mSwipeProxy = null;
            mSwipeViewPosition = ListView.INVALID_POSITION;
            mSwiping = false;
            mEnabled = true;

            ViewConfiguration vc = ViewConfiguration.get(TabsTray.this.getContext());
            mSwipeThreshold = vc.getScaledTouchSlop();
            mMinFlingVelocity = (int) (getContext().getResources().getDisplayMetrics().density * MIN_VELOCITY);
            mMaxFlingVelocity = vc.getScaledMaximumFlingVelocity();
        }

        public void setEnabled(boolean enabled) {
            mEnabled = enabled;
        }

        public AbsListView.OnScrollListener makeScrollListener() {
            return new AbsListView.OnScrollListener() {
                @Override
                public void onScrollStateChanged(AbsListView absListView, int scrollState) {
                    setEnabled(scrollState != AbsListView.OnScrollListener.SCROLL_STATE_TOUCH_SCROLL);
                }

                @Override
                public void onScroll(AbsListView absListView, int i, int i1, int i2) {
                }
            };
        }

        @Override
        public boolean onTouch(View view, MotionEvent e) {
            if (!mEnabled)
                return false;

            if (mListWidth < 2)
                mListWidth = TabsTray.this.getWidth();

            switch (e.getActionMasked()) {
                case MotionEvent.ACTION_DOWN: {
                    // Check if we should set pressed state on the
                    // touched view after a standard delay.
                    triggerCheckForTap();

                    // Find out which view is being touched
                    mSwipeView = findViewAt(e.getRawX(), e.getRawY());

                    if (mSwipeView != null) {
                        mSwipeStart = e.getRawX();
                        mSwipeViewPosition = TabsTray.this.getPositionForView(mSwipeView);

                        mVelocityTracker = VelocityTracker.obtain();
                        mVelocityTracker.addMovement(e);
                    }

                    view.onTouchEvent(e);
                    return true;
                }

                case MotionEvent.ACTION_UP: {
                    if (mSwipeView == null)
                        break;

                    cancelCheckForTap();
                    mSwipeView.setPressed(false);

                    if (!mSwiping) {
                        TabRow tab = (TabRow) mSwipeView.getTag();
                        Tabs.getInstance().selectTab(tab.id);
                        autoHidePanel();
                        break;
                    }

                    float deltaX = mSwipeProxy.getTranslationX();

                    mVelocityTracker.addMovement(e);
                    mVelocityTracker.computeCurrentVelocity(1000, mMaxFlingVelocity);

                    float velocityX = Math.abs(mVelocityTracker.getXVelocity());
                    float velocityY = Math.abs(mVelocityTracker.getYVelocity());

                    boolean dismiss = false;
                    boolean dismissRight = false;

                    if (Math.abs(deltaX) > mListWidth / 2) {
                        dismiss = true;
                        dismissRight = (deltaX > 0);
                    } else if (mMinFlingVelocity <= velocityX && velocityX <= mMaxFlingVelocity
                            && velocityY < velocityX) {
                        dismiss = mSwiping && (deltaX * mVelocityTracker.getXVelocity() > 0);
                        dismissRight = (mVelocityTracker.getXVelocity() > 0);
                    }

                    if (dismiss)
                        animateClose(mSwipeView, (dismissRight ? mListWidth : -mListWidth));
                    else
                        animateCancel(mSwipeView);

                    mVelocityTracker = null;
                    mSwipeView = null;
                    mSwipeViewPosition = ListView.INVALID_POSITION;
                    mSwipeProxy = null;

                    mSwipeStart = 0;
                    mSwiping = false;

                    break;
                }

                case MotionEvent.ACTION_MOVE: {
                    if (mSwipeView == null)
                        break;

                    mVelocityTracker.addMovement(e);

                    float deltaX = e.getRawX() - mSwipeStart;
                    if (Math.abs(deltaX) > mSwipeThreshold) {
                        // If we're actually swiping, make sure we don't
                        // set pressed state on the swiped view.
                        cancelCheckForTap();

                        mSwiping = true;
                        TabsTray.this.requestDisallowInterceptTouchEvent(true);

                        TabRow tab = (TabRow) mSwipeView.getTag();
                        tab.close.setVisibility(View.INVISIBLE);

                        // Stops listview from highlighting the touched item
                        // in the list when swiping.
                        MotionEvent cancelEvent = MotionEvent.obtain(e);
                        cancelEvent.setAction(MotionEvent.ACTION_CANCEL |
                                (e.getActionIndex() << MotionEvent.ACTION_POINTER_INDEX_SHIFT));
                        TabsTray.this.onTouchEvent(cancelEvent);

                        mSwipeProxy = AnimatorProxy.create(mSwipeView);
                    }

                    if (mSwiping) {
                        mSwipeProxy.setTranslationX(deltaX);
                        mSwipeProxy.setAlpha(Math.max(0.1f, Math.min(1f,
                                1f - 2f * Math.abs(deltaX) / mListWidth)));

                        return true;
                    }

                    break;
                }
            }

            return false;
        }

        private View findViewAt(float rawX, float rawY) {
            Rect rect = new Rect();

            int[] listViewCoords = new int[2];
            TabsTray.this.getLocationOnScreen(listViewCoords);

            int x = (int) rawX - listViewCoords[0];
            int y = (int) rawY - listViewCoords[1];

            for (int i = 0; i < TabsTray.this.getChildCount(); i++) {
                View child = TabsTray.this.getChildAt(i);
                child.getHitRect(rect);

                if (rect.contains(x, y))
                    return child;
            }

            return null;
        }

        private void triggerCheckForTap() {
            if (mPendingCheckForTap == null)
                mPendingCheckForTap = new CheckForTap();

            TabsTray.this.postDelayed(mPendingCheckForTap, ViewConfiguration.getTapTimeout());
        }

        private void cancelCheckForTap() {
            if (mPendingCheckForTap == null)
                return;

            TabsTray.this.removeCallbacks(mPendingCheckForTap);
        }

        private class CheckForTap implements Runnable {
            @Override
            public void run() {
                if (!mSwiping && mSwipeView != null && mEnabled)
                    mSwipeView.setPressed(true);
            }
        }
    }
}
