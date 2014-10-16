/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import java.util.ArrayList;
import java.util.List;

import org.mozilla.gecko.animation.PropertyAnimator.Property;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.tabs.TabsPanel.TabsLayout;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.widget.TwoWayView;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.VelocityTracker;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewConfiguration;
import android.widget.Button;

class TabsListLayout extends TwoWayView
                     implements TabsLayout,
                                Tabs.OnTabsChangedListener {
    private static final String LOGTAG = "Gecko" + TabsListLayout.class.getSimpleName();

    private final Context mContext;
    private TabsPanel mTabsPanel;

    final private boolean mIsPrivate;

    private final TabsLayoutAdapter mTabsAdapter;

    private final List<View> mPendingClosedTabs;
    private int mCloseAnimationCount;
    private int mCloseAllAnimationCount;

    private final TabSwipeGestureListener mSwipeListener;

    // Time to animate non-flinged tabs of screen, in milliseconds
    private static final int ANIMATION_DURATION = 250;

    // Time between starting successive tab animations in closeAllTabs.
    private static final int ANIMATION_CASCADE_DELAY = 75;

    private int mOriginalSize;

    public TabsListLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;

        mPendingClosedTabs = new ArrayList<View>();

        setItemsCanFocus(true);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.TabsTray);
        mIsPrivate = (a.getInt(R.styleable.TabsTray_tabs, 0x0) == 1);
        a.recycle();

        mTabsAdapter = new TabsListLayoutAdapter(mContext);
        setAdapter(mTabsAdapter);

        mSwipeListener = new TabSwipeGestureListener();
        setOnTouchListener(mSwipeListener);
        setOnScrollListener(mSwipeListener.makeScrollListener());

        setRecyclerListener(new RecyclerListener() {
            @Override
            public void onMovedToScrapHeap(View view) {
                TabsLayoutItemView item = (TabsLayoutItemView) view;
                item.setThumbnail(null);
                item.setCloseVisible(true);
            }
        });
    }

    private class TabsListLayoutAdapter extends TabsLayoutAdapter {
        private final Button.OnClickListener mCloseOnClickListener;
        public TabsListLayoutAdapter (Context context) {
            super(context);

            mCloseOnClickListener = new Button.OnClickListener() {
                @Override
                public void onClick(View v) {
                    // The view here is the close button, which has a reference
                    // to the parent TabsLayoutItemView in it's tag, hence the getTag() call
                    TabsLayoutItemView item = (TabsLayoutItemView) v.getTag();
                    final int pos = (isVertical() ? item.getWidth() : 0 - item.getHeight());
                    animateClose(item, pos);
                }
            };
        }

        @Override
        public TabsLayoutItemView newView(int position, ViewGroup parent) {
            TabsLayoutItemView item = super.newView(position, parent);

            item.setCloseOnClickListener(mCloseOnClickListener);

            return item;
        }

        @Override
        public void bindView(TabsLayoutItemView view, Tab tab) {
            super.bindView(view, tab);

            // If we're recycling this view, there's a chance it was transformed during
            // the close animation. Remove any of those properties.
            resetTransforms(view);
        }

    }

    @Override
    public void setTabsPanel(TabsPanel panel) {
        mTabsPanel = panel;
    }

    @Override
    public void show() {
        setVisibility(View.VISIBLE);
        Tabs.getInstance().refreshThumbnails();
        Tabs.registerOnTabsChangedListener(this);
        refreshTabsData();
    }

    @Override
    public void hide() {
        setVisibility(View.GONE);
        Tabs.unregisterOnTabsChangedListener(this);
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Tab:Screenshot:Cancel",""));
        mTabsAdapter.clear();
    }

    @Override
    public boolean shouldExpand() {
        return isVertical();
    }

    private void autoHidePanel() {
        mTabsPanel.autoHidePanel();
    }

    @Override
    public void onTabChanged(Tab tab, Tabs.TabEvents msg, Object data) {
        switch (msg) {
            case ADDED:
                // Refresh the list to make sure the new tab is added in the right position.
                refreshTabsData();
                break;

            case CLOSED:
               if (tab.isPrivate() == mIsPrivate && mTabsAdapter.getCount() > 0) {
                   if (mTabsAdapter.removeTab(tab)) {
                       int selected = mTabsAdapter.getPositionForTab(Tabs.getInstance().getSelectedTab());
                       updateSelectedStyle(selected);
                   }
               }
               break;

            case SELECTED:
                // Update the selected position, then fall through...
                updateSelectedPosition();
            case UNSELECTED:
                // We just need to update the style for the unselected tab...
            case THUMBNAIL:
            case TITLE:
            case RECORDING_CHANGE:
                View view = getChildAt(mTabsAdapter.getPositionForTab(tab) - getFirstVisiblePosition());
                if (view == null)
                    return;

                TabsLayoutItemView item = (TabsLayoutItemView) view;
                item.assignValues(tab);
                break;
        }
    }

    // Updates the selected position in the list so that it will be scrolled to the right place.
    private void updateSelectedPosition() {
        int selected = mTabsAdapter.getPositionForTab(Tabs.getInstance().getSelectedTab());
        updateSelectedStyle(selected);

        if (selected != -1) {
            setSelection(selected);
        }
    }

    /**
     * Updates the selected/unselected style for the tabs.
     *
     * @param selected position of the selected tab
     */
    private void updateSelectedStyle(int selected) {
        for (int i = 0; i < mTabsAdapter.getCount(); i++) {
            setItemChecked(i, (i == selected));
        }
    }

    private void refreshTabsData() {
        // Store a different copy of the tabs, so that we don't have to worry about
        // accidentally updating it on the wrong thread.
        ArrayList<Tab> tabData = new ArrayList<Tab>();

        Iterable<Tab> allTabs = Tabs.getInstance().getTabsInOrder();
        for (Tab tab : allTabs) {
            if (tab.isPrivate() == mIsPrivate)
                tabData.add(tab);
        }

        mTabsAdapter.setTabs(tabData);
        updateSelectedPosition();
    }

    public void resetTransforms(View view) {
        ViewHelper.setAlpha(view, 1);

        if (isVertical()) {
            ViewHelper.setTranslationX(view, 0);
        } else {
            ViewHelper.setTranslationY(view, 0);
        }

        // We only need to reset the height or width after individual tab close animations.
        if (mOriginalSize != 0) {
            if (isVertical()) {
                ViewHelper.setHeight(view, mOriginalSize);
            } else {
                ViewHelper.setWidth(view, mOriginalSize);
            }
        }
    }

    private boolean isVertical() {
        return (getOrientation().compareTo(TwoWayView.Orientation.VERTICAL) == 0);
    }

    @Override
    public void closeAll() {
        final int childCount = getChildCount();

        // Just close the panel if there are no tabs to close.
        if (childCount == 0) {
            autoHidePanel();
            return;
        }

        // Disable the view so that gestures won't interfere wth the tab close animation.
        setEnabled(false);

        // Delay starting each successive animation to create a cascade effect.
        int cascadeDelay = 0;

        for (int i = childCount - 1; i >= 0; i--) {
            final View view = getChildAt(i);
            final PropertyAnimator animator = new PropertyAnimator(ANIMATION_DURATION);
            animator.attach(view, Property.ALPHA, 0);

            if (isVertical()) {
                animator.attach(view, Property.TRANSLATION_X, view.getWidth());
            } else {
                animator.attach(view, Property.TRANSLATION_Y, view.getHeight());
            }

            mCloseAllAnimationCount++;

            animator.addPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
                @Override
                public void onPropertyAnimationStart() { }

                @Override
                public void onPropertyAnimationEnd() {
                    mCloseAllAnimationCount--;
                    if (mCloseAllAnimationCount > 0) {
                        return;
                    }

                    // Hide the panel after the animation is done.
                    autoHidePanel();

                    // Re-enable the view after the animation is done.
                    TabsListLayout.this.setEnabled(true);

                    // Then actually close all the tabs.
                    final Iterable<Tab> tabs = Tabs.getInstance().getTabsInOrder();
                    for (Tab tab : tabs) {
                        // In the normal panel we want to close all tabs (both private and normal),
                        // but in the private panel we only want to close private tabs.
                        if (!mIsPrivate || tab.isPrivate()) {
                            Tabs.getInstance().closeTab(tab, false);
                        }
                    }
                }
            });

            ThreadUtils.getUiHandler().postDelayed(new Runnable() {
                @Override
                public void run() {
                    animator.start();
                }
            }, cascadeDelay);

            cascadeDelay += ANIMATION_CASCADE_DELAY;
        }
    }

    private void animateClose(final View view, int pos) {
        PropertyAnimator animator = new PropertyAnimator(ANIMATION_DURATION);
        animator.attach(view, Property.ALPHA, 0);

        if (isVertical())
            animator.attach(view, Property.TRANSLATION_X, pos);
        else
            animator.attach(view, Property.TRANSLATION_Y, pos);

        mCloseAnimationCount++;
        mPendingClosedTabs.add(view);

        animator.addPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() { }
            @Override
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

        final boolean isVertical = isVertical();
        if (isVertical)
            animator.attach(view, Property.HEIGHT, 1);
        else
            animator.attach(view, Property.WIDTH, 1);

        final int tabId = ((TabsLayoutItemView) view).getTabId();

        // Caching this assumes that all rows are the same height
        if (mOriginalSize == 0) {
            mOriginalSize = (isVertical ? view.getHeight() : view.getWidth());
        }

        animator.addPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() { }
            @Override
            public void onPropertyAnimationEnd() {
                Tabs tabs = Tabs.getInstance();
                Tab tab = tabs.getTab(tabId);
                tabs.closeTab(tab, true);
            }
        });

        animator.start();
    }

    private void animateCancel(final View view) {
        PropertyAnimator animator = new PropertyAnimator(ANIMATION_DURATION);
        animator.attach(view, Property.ALPHA, 1);

        if (isVertical())
            animator.attach(view, Property.TRANSLATION_X, 0);
        else
            animator.attach(view, Property.TRANSLATION_Y, 0);


        animator.addPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() { }
            @Override
            public void onPropertyAnimationEnd() {
                TabsLayoutItemView tab = (TabsLayoutItemView) view;
                tab.setCloseVisible(true);
            }
        });

        animator.start();
    }

    private class TabSwipeGestureListener implements View.OnTouchListener {
        // same value the stock browser uses for after drag animation velocity in pixels/sec
        // http://androidxref.com/4.0.4/xref/packages/apps/Browser/src/com/android/browser/NavTabScroller.java#61
        private static final float MIN_VELOCITY = 750;

        private final int mSwipeThreshold;
        private final int mMinFlingVelocity;

        private final int mMaxFlingVelocity;
        private VelocityTracker mVelocityTracker;

        private int mListWidth = 1;
        private int mListHeight = 1;

        private View mSwipeView;
        private Runnable mPendingCheckForTap;

        private float mSwipeStartX;
        private float mSwipeStartY;
        private boolean mSwiping;
        private boolean mEnabled;

        public TabSwipeGestureListener() {
            mEnabled = true;

            ViewConfiguration vc = ViewConfiguration.get(TabsListLayout.this.getContext());
            mSwipeThreshold = vc.getScaledTouchSlop();
            mMinFlingVelocity = (int) (getContext().getResources().getDisplayMetrics().density * MIN_VELOCITY);
            mMaxFlingVelocity = vc.getScaledMaximumFlingVelocity();
        }

        public void setEnabled(boolean enabled) {
            mEnabled = enabled;
        }

        public TwoWayView.OnScrollListener makeScrollListener() {
            return new TwoWayView.OnScrollListener() {
                @Override
                public void onScrollStateChanged(TwoWayView twoWayView, int scrollState) {
                    setEnabled(scrollState != TwoWayView.OnScrollListener.SCROLL_STATE_TOUCH_SCROLL);
                }

                @Override
                public void onScroll(TwoWayView twoWayView, int i, int i1, int i2) {
                }
            };
        }

        @Override
        public boolean onTouch(View view, MotionEvent e) {
            if (!mEnabled)
                return false;

            if (mListWidth < 2 || mListHeight < 2) {
                mListWidth = TabsListLayout.this.getWidth();
                mListHeight = TabsListLayout.this.getHeight();
            }

            switch (e.getActionMasked()) {
                case MotionEvent.ACTION_DOWN: {
                    // Check if we should set pressed state on the
                    // touched view after a standard delay.
                    triggerCheckForTap();

                    final float x = e.getRawX();
                    final float y = e.getRawY();

                    // Find out which view is being touched
                    mSwipeView = findViewAt(x, y);

                    if (mSwipeView != null) {
                        mSwipeStartX = e.getRawX();
                        mSwipeStartY = e.getRawY();

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
                        TabsLayoutItemView item = (TabsLayoutItemView) mSwipeView;
                        Tabs.getInstance().selectTab(item.getTabId());
                        autoHidePanel();

                        mVelocityTracker.recycle();
                        mVelocityTracker = null;
                        break;
                    }

                    mVelocityTracker.addMovement(e);
                    mVelocityTracker.computeCurrentVelocity(1000, mMaxFlingVelocity);

                    float velocityX = Math.abs(mVelocityTracker.getXVelocity());
                    float velocityY = Math.abs(mVelocityTracker.getYVelocity());

                    boolean dismiss = false;
                    boolean dismissDirection = false;
                    int dismissTranslation = 0;

                    if (isVertical()) {
                        float deltaX = ViewHelper.getTranslationX(mSwipeView);

                        if (Math.abs(deltaX) > mListWidth / 2) {
                            dismiss = true;
                            dismissDirection = (deltaX > 0);
                        } else if (mMinFlingVelocity <= velocityX && velocityX <= mMaxFlingVelocity
                                && velocityY < velocityX) {
                            dismiss = mSwiping && (deltaX * mVelocityTracker.getXVelocity() > 0);
                            dismissDirection = (mVelocityTracker.getXVelocity() > 0);
                        }

                        dismissTranslation = (dismissDirection ? mListWidth : -mListWidth);
                    } else {
                        float deltaY = ViewHelper.getTranslationY(mSwipeView);

                        if (Math.abs(deltaY) > mListHeight / 2) {
                            dismiss = true;
                            dismissDirection = (deltaY > 0);
                        } else if (mMinFlingVelocity <= velocityY && velocityY <= mMaxFlingVelocity
                                && velocityX < velocityY) {
                            dismiss = mSwiping && (deltaY * mVelocityTracker.getYVelocity() > 0);
                            dismissDirection = (mVelocityTracker.getYVelocity() > 0);
                        }

                        dismissTranslation = (dismissDirection ? mListHeight : -mListHeight);
                     }

                    if (dismiss)
                        animateClose(mSwipeView, dismissTranslation);
                    else
                        animateCancel(mSwipeView);

                    mVelocityTracker.recycle();
                    mVelocityTracker = null;
                    mSwipeView = null;

                    mSwipeStartX = 0;
                    mSwipeStartY = 0;
                    mSwiping = false;

                    break;
                }

                case MotionEvent.ACTION_MOVE: {
                    if (mSwipeView == null || mVelocityTracker == null)
                        break;

                    mVelocityTracker.addMovement(e);

                    final boolean isVertical = isVertical();

                    float deltaX = e.getRawX() - mSwipeStartX;
                    float deltaY = e.getRawY() - mSwipeStartY;
                    float delta = (isVertical ? deltaX : deltaY);

                    boolean isScrollingX = Math.abs(deltaX) > mSwipeThreshold;
                    boolean isScrollingY = Math.abs(deltaY) > mSwipeThreshold;
                    boolean isSwipingToClose = (isVertical ? isScrollingX : isScrollingY);

                    // If we're actually swiping, make sure we don't
                    // set pressed state on the swiped view.
                    if (isScrollingX || isScrollingY)
                        cancelCheckForTap();

                    if (isSwipingToClose) {
                        mSwiping = true;
                        TabsListLayout.this.requestDisallowInterceptTouchEvent(true);

                        ((TabsLayoutItemView) mSwipeView).setCloseVisible(false);

                        // Stops listview from highlighting the touched item
                        // in the list when swiping.
                        MotionEvent cancelEvent = MotionEvent.obtain(e);
                        cancelEvent.setAction(MotionEvent.ACTION_CANCEL |
                                (e.getActionIndex() << MotionEvent.ACTION_POINTER_INDEX_SHIFT));
                        TabsListLayout.this.onTouchEvent(cancelEvent);
                        cancelEvent.recycle();
                    }

                    if (mSwiping) {
                        if (isVertical)
                            ViewHelper.setTranslationX(mSwipeView, delta);
                        else
                            ViewHelper.setTranslationY(mSwipeView, delta);

                        ViewHelper.setAlpha(mSwipeView, Math.max(0.1f, Math.min(1f,
                                1f - 2f * Math.abs(delta) / (isVertical ? mListWidth : mListHeight))));

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
            TabsListLayout.this.getLocationOnScreen(listViewCoords);

            int x = (int) rawX - listViewCoords[0];
            int y = (int) rawY - listViewCoords[1];

            for (int i = 0; i < TabsListLayout.this.getChildCount(); i++) {
                View child = TabsListLayout.this.getChildAt(i);
                child.getHitRect(rect);

                if (rect.contains(x, y))
                    return child;
            }

            return null;
        }

        private void triggerCheckForTap() {
            if (mPendingCheckForTap == null)
                mPendingCheckForTap = new CheckForTap();

            TabsListLayout.this.postDelayed(mPendingCheckForTap, ViewConfiguration.getTapTimeout());
        }

        private void cancelCheckForTap() {
            if (mPendingCheckForTap == null)
                return;

            TabsListLayout.this.removeCallbacks(mPendingCheckForTap);
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
