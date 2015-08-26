/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.tabs.TabsPanel.TabsLayout;
import org.mozilla.gecko.widget.themed.ThemedRelativeLayout;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.PointF;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.util.SparseArray;
import android.view.MotionEvent;
import android.view.VelocityTracker;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.animation.DecelerateInterpolator;
import android.widget.AbsListView;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.GridView;
import com.nineoldandroids.animation.Animator;
import com.nineoldandroids.animation.AnimatorSet;
import com.nineoldandroids.animation.ObjectAnimator;
import com.nineoldandroids.animation.PropertyValuesHolder;
import com.nineoldandroids.animation.ValueAnimator;

import java.util.ArrayList;
import java.util.List;

/**
 * A tabs layout implementation for the tablet redesign (bug 1014156).
 * Expected to replace TabsListLayout once complete.
 */

class TabsGridLayout extends GridView
                     implements TabsLayout,
                                Tabs.OnTabsChangedListener {
    private static final String LOGTAG = "Gecko" + TabsGridLayout.class.getSimpleName();

    public static final int ANIM_DELAY_MULTIPLE_MS = 20;
    private static final int ANIM_TIME_MS = 200;
    private static final DecelerateInterpolator ANIM_INTERPOLATOR = new DecelerateInterpolator();

    private final Context mContext;
    private final SparseArray<PointF> mTabLocations = new SparseArray<PointF>();
    private final boolean mIsPrivate;
    private final TabsLayoutAdapter mTabsAdapter;
    private final int mColumnWidth;
    private TabsPanel mTabsPanel;
    private int lastSelectedTabId;

    public TabsGridLayout(Context context, AttributeSet attrs) {
        super(context, attrs, R.attr.tabGridLayoutViewStyle);
        mContext = context;

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.TabsLayout);
        mIsPrivate = (a.getInt(R.styleable.TabsLayout_tabs, 0x0) == 1);
        a.recycle();

        mTabsAdapter = new TabsGridLayoutAdapter(mContext);
        setAdapter(mTabsAdapter);

        setRecyclerListener(new RecyclerListener() {
            @Override
            public void onMovedToScrapHeap(View view) {
                TabsLayoutItemView item = (TabsLayoutItemView) view;
                item.setThumbnail(null);
            }
        });

        // The clipToPadding setting in the styles.xml doesn't seem to be working (bug 1101784)
        // so lets set it manually in code for the moment as it's needed for the padding animation
        setClipToPadding(false);

        final Resources resources = getResources();
        mColumnWidth = resources.getDimensionPixelSize(R.dimen.tablet_tab_panel_column_width);

        final int padding = resources.getDimensionPixelSize(R.dimen.tablet_tab_panel_grid_padding);
        final int paddingTop = resources.getDimensionPixelSize(R.dimen.tablet_tab_panel_grid_padding_top);

        // Lets set double the top padding on the bottom so that the last row shows up properly!
        // Your demise, GridView, cannot come fast enough.
        final int paddingBottom = paddingTop * 2;

        setPadding(padding, paddingTop, padding, paddingBottom);

        setOnItemClickListener(new OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                TabsLayoutItemView tab = (TabsLayoutItemView) view;
                Tabs.getInstance().selectTab(tab.getTabId());
                autoHidePanel();
            }
        });

        TabSwipeGestureListener mSwipeListener = new TabSwipeGestureListener();
        setOnTouchListener(mSwipeListener);
        setOnScrollListener(mSwipeListener.makeScrollListener());
    }

    private void populateTabLocations(final Tab removedTab) {
        mTabLocations.clear();

        final int firstPosition = getFirstVisiblePosition();
        final int lastPosition = getLastVisiblePosition();
        final int numberOfColumns = getNumColumns();
        final int childCount = getChildCount();
        final int removedPosition = mTabsAdapter.getPositionForTab(removedTab);

        for (int x = 1, i = (removedPosition - firstPosition) + 1; i < childCount; i++, x++) {
            final View child = getChildAt(i);
            if (child != null) {
                // Reset the transformations here in case the user is swiping tabs away fast and they swipe a tab
                // before the last animation has finished (bug 1179195).
                resetTransforms(child);
                mTabLocations.append(x, new PointF(child.getX(), child.getY()));
            }
        }

        final boolean firstChildOffScreen = ((firstPosition > 0) || getChildAt(0).getY() < 0);
        final boolean lastChildVisible = (lastPosition - childCount == firstPosition - 1);
        final boolean oneItemOnLastRow = (lastPosition % numberOfColumns == 0);
        if (firstChildOffScreen && lastChildVisible && oneItemOnLastRow) {
            // We need to set the view's bottom padding to prevent a sudden jump as the
            // last item in the row is being removed. We then need to remove the padding
            // via a sweet animation

            final int removedHeight = getChildAt(0).getMeasuredHeight();
            final int verticalSpacing =
                    getResources().getDimensionPixelOffset(R.dimen.tablet_tab_panel_grid_vspacing);

            ValueAnimator paddingAnimator = ValueAnimator.ofInt(getPaddingBottom() + removedHeight + verticalSpacing, getPaddingBottom());
            paddingAnimator.setDuration(ANIM_TIME_MS * 2);

            paddingAnimator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {

                @Override
                public void onAnimationUpdate(ValueAnimator animation) {
                    setPadding(getPaddingLeft(), getPaddingTop(), getPaddingRight(), (Integer) animation.getAnimatedValue());
                }
            });
            paddingAnimator.start();
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

        Tab currentlySelectedTab = Tabs.getInstance().getSelectedTab();
        if (lastSelectedTabId != currentlySelectedTab.getId()) {
            smoothScrollToPosition(mTabsAdapter.getPositionForTab(currentlySelectedTab));
        }
    }

    @Override
    public void hide() {
        lastSelectedTabId = Tabs.getInstance().getSelectedTab().getId();
        setVisibility(View.GONE);
        Tabs.unregisterOnTabsChangedListener(this);
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Tab:Screenshot:Cancel", ""));
        mTabsAdapter.clear();
    }

    @Override
    public boolean shouldExpand() {
        return true;
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
                if (mTabsAdapter.getCount() > 0) {
                    animateRemoveTab(tab);
                }

                final Tabs tabsInstance = Tabs.getInstance();

                if (mTabsAdapter.removeTab(tab)) {
                    if (tab.isPrivate() == mIsPrivate && mTabsAdapter.getCount() > 0) {
                        int selected = mTabsAdapter.getPositionForTab(tabsInstance.getSelectedTab());
                        updateSelectedStyle(selected);
                    }
                    if (!tab.isPrivate()) {
                        // Make sure we always have at least one normal tab
                        final Iterable<Tab> tabs = tabsInstance.getTabsInOrder();
                        boolean removedTabIsLastNormalTab = true;
                        for (Tab singleTab : tabs) {
                            if (!singleTab.isPrivate()) {
                                removedTabIsLastNormalTab = false;
                                break;
                            }
                        }
                        if (removedTabIsLastNormalTab) {
                            tabsInstance.addTab();
                        }
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
            case AUDIO_PLAYING_CHANGE:
                View view = getChildAt(mTabsAdapter.getPositionForTab(tab) - getFirstVisiblePosition());
                if (view == null)
                    return;

                ((TabsLayoutItemView) view).assignValues(tab);
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
        ArrayList<Tab> tabData = new ArrayList<>();

        Iterable<Tab> allTabs = Tabs.getInstance().getTabsInOrder();
        for (Tab tab : allTabs) {
            if (tab.isPrivate() == mIsPrivate)
                tabData.add(tab);
        }

        mTabsAdapter.setTabs(tabData);
        updateSelectedPosition();
    }

    private void resetTransforms(View view) {
        ViewHelper.setAlpha(view, 1);
        ViewHelper.setTranslationX(view, 0);
        ViewHelper.setTranslationY(view, 0);

        ((TabsLayoutItemView) view).setCloseVisible(true);
    }

    @Override
    public void closeAll() {

        autoHidePanel();

        if (getChildCount() == 0) {
            return;
        }

        final Iterable<Tab> tabs = Tabs.getInstance().getTabsInOrder();
        for (Tab tab : tabs) {
            // In the normal panel we want to close all tabs (both private and normal),
            // but in the private panel we only want to close private tabs.
            if (!mIsPrivate || tab.isPrivate()) {
                Tabs.getInstance().closeTab(tab, false);
            }
        }
    }

    private View getViewForTab(Tab tab) {
        final int position = mTabsAdapter.getPositionForTab(tab);
        return getChildAt(position - getFirstVisiblePosition());
    }

    void closeTab(View v) {
        TabsLayoutItemView itemView = (TabsLayoutItemView) v.getTag();
        Tab tab = Tabs.getInstance().getTab(itemView.getTabId());

        Tabs.getInstance().closeTab(tab, true);
    }

    private void animateRemoveTab(final Tab removedTab) {
        final int removedPosition = mTabsAdapter.getPositionForTab(removedTab);

        final View removedView = getViewForTab(removedTab);

        // The removed position might not have a matching child view
        // when it's not within the visible range of positions in the strip.
        if (removedView == null) {
            return;
        }
        final int removedHeight = removedView.getMeasuredHeight();

        populateTabLocations(removedTab);

        getViewTreeObserver().addOnPreDrawListener(new ViewTreeObserver.OnPreDrawListener() {
            @Override
            public boolean onPreDraw() {
                getViewTreeObserver().removeOnPreDrawListener(this);
                // We don't animate the removed child view (it just disappears)
                // but we still need its size to animate all affected children
                // within the visible viewport.
                final int childCount = getChildCount();
                final int firstPosition = getFirstVisiblePosition();
                final int numberOfColumns = getNumColumns();

                final List<Animator> childAnimators = new ArrayList<>();

                PropertyValuesHolder translateX, translateY;
                for (int x = 0, i = removedPosition - firstPosition; i < childCount; i++, x++) {
                    final View child = getChildAt(i);
                    ObjectAnimator animator;

                    if (i % numberOfColumns == numberOfColumns - 1) {
                        // Animate X & Y
                        translateX = PropertyValuesHolder.ofFloat("translationX", -(mColumnWidth * numberOfColumns), 0);
                        translateY = PropertyValuesHolder.ofFloat("translationY", removedHeight, 0);
                        animator = ObjectAnimator.ofPropertyValuesHolder(child, translateX, translateY);
                    } else {
                        // Just animate X
                        translateX = PropertyValuesHolder.ofFloat("translationX", mColumnWidth, 0);
                        animator = ObjectAnimator.ofPropertyValuesHolder(child, translateX);
                    }
                    animator.setStartDelay(x * ANIM_DELAY_MULTIPLE_MS);
                    childAnimators.add(animator);
                }

                final AnimatorSet animatorSet = new AnimatorSet();
                animatorSet.playTogether(childAnimators);
                animatorSet.setDuration(ANIM_TIME_MS);
                animatorSet.setInterpolator(ANIM_INTERPOLATOR);
                animatorSet.start();

                // Set the starting position of the child views - because we are delaying the start
                // of the animation, we need to prevent the items being drawn in their final position
                // prior to the animation starting
                for (int x = 1, i = (removedPosition - firstPosition) + 1; i < childCount; i++, x++) {
                    final View child = getChildAt(i);

                    final PointF targetLocation = mTabLocations.get(x + 1);
                    if (targetLocation == null) {
                        continue;
                    }

                    child.setX(targetLocation.x);
                    child.setY(targetLocation.y);
                }

                return true;
            }
        });
    }

    private void animateCancel(final View view) {
        PropertyAnimator animator = new PropertyAnimator(ANIM_TIME_MS);
        animator.attach(view, PropertyAnimator.Property.ALPHA, 1);
        animator.attach(view, PropertyAnimator.Property.TRANSLATION_X, 0);

        animator.addPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() {
            }

            @Override
            public void onPropertyAnimationEnd() {
                TabsLayoutItemView tab = (TabsLayoutItemView) view;
                tab.setCloseVisible(true);
            }
        });

        animator.start();
    }

    private class TabsGridLayoutAdapter extends TabsLayoutAdapter {

        final private Button.OnClickListener mCloseClickListener;

        public TabsGridLayoutAdapter(Context context) {
            super(context, R.layout.tabs_layout_item_view);

            mCloseClickListener = new Button.OnClickListener() {
                @Override
                public void onClick(View v) {
                    closeTab(v);
                }
            };
        }

        @Override
        TabsLayoutItemView newView(int position, ViewGroup parent) {
            final TabsLayoutItemView item = super.newView(position, parent);

            item.setCloseOnClickListener(mCloseClickListener);
            ((ThemedRelativeLayout) item.findViewById(R.id.wrapper)).setPrivateMode(mIsPrivate);

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

    private class TabSwipeGestureListener implements View.OnTouchListener {
        // same value the stock browser uses for after drag animation velocity in pixels/sec
        // http://androidxref.com/4.0.4/xref/packages/apps/Browser/src/com/android/browser/NavTabScroller.java#61
        private static final float MIN_VELOCITY = 750;

        private final int mSwipeThreshold;
        private final int mMinFlingVelocity;

        private final int mMaxFlingVelocity;
        private VelocityTracker mVelocityTracker;

        private int mTabWidth = 1;

        private View mSwipeView;
        private Runnable mPendingCheckForTap;

        private float mSwipeStartX;
        private boolean mSwiping;
        private boolean mEnabled;

        public TabSwipeGestureListener() {
            mEnabled = true;

            ViewConfiguration vc = ViewConfiguration.get(TabsGridLayout.this.getContext());
            mSwipeThreshold = vc.getScaledTouchSlop();
            mMinFlingVelocity = (int) (TabsGridLayout.this.getContext().getResources().getDisplayMetrics().density * MIN_VELOCITY);
            mMaxFlingVelocity = vc.getScaledMaximumFlingVelocity();
        }

        public void setEnabled(boolean enabled) {
            mEnabled = enabled;
        }

        public OnScrollListener makeScrollListener() {
            return new OnScrollListener() {
                @Override
                public void onScrollStateChanged(AbsListView view, int scrollState) {
                    setEnabled(scrollState != GridView.OnScrollListener.SCROLL_STATE_TOUCH_SCROLL);
                }

                @Override
                public void onScroll(AbsListView view, int firstVisibleItem, int visibleItemCount, int totalItemCount) {

                }
            };
        }

        @Override
        public boolean onTouch(View view, MotionEvent e) {
            if (!mEnabled) {
                return false;
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
                        if (mTabWidth < 2) {
                            mTabWidth = mSwipeView.getWidth();
                        }

                        mSwipeStartX = e.getRawX();

                        mVelocityTracker = VelocityTracker.obtain();
                        mVelocityTracker.addMovement(e);
                    }

                    view.onTouchEvent(e);
                    return true;
                }

                case MotionEvent.ACTION_UP: {
                    if (mSwipeView == null) {
                        break;
                    }

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

                    boolean dismiss = false;

                    float deltaX = ViewHelper.getTranslationX(mSwipeView);

                    if (Math.abs(deltaX) > mTabWidth / 2) {
                        dismiss = true;
                    } else if (mMinFlingVelocity <= velocityX && velocityX <= mMaxFlingVelocity) {
                        dismiss = mSwiping && (deltaX * mVelocityTracker.getYVelocity() > 0);
                    }
                    if (dismiss) {
                        closeTab(mSwipeView.findViewById(R.id.close));
                    } else {
                        animateCancel(mSwipeView);
                    }
                    mVelocityTracker.recycle();
                    mVelocityTracker = null;
                    mSwipeView = null;

                    mSwipeStartX = 0;
                    mSwiping = false;
                }

                case MotionEvent.ACTION_MOVE: {
                    if (mSwipeView == null || mVelocityTracker == null) {
                        break;
                    }

                    mVelocityTracker.addMovement(e);

                    float delta = e.getRawX() - mSwipeStartX;

                    boolean isScrollingX = Math.abs(delta) > mSwipeThreshold;
                    boolean isSwipingToClose = isScrollingX;

                    // If we're actually swiping, make sure we don't
                    // set pressed state on the swiped view.
                    if (isScrollingX) {
                        cancelCheckForTap();
                    }

                    if (isSwipingToClose) {
                        mSwiping = true;
                        TabsGridLayout.this.requestDisallowInterceptTouchEvent(true);

                        ((TabsLayoutItemView) mSwipeView).setCloseVisible(false);

                        // Stops listview from highlighting the touched item
                        // in the list when swiping.
                        MotionEvent cancelEvent = MotionEvent.obtain(e);
                        cancelEvent.setAction(MotionEvent.ACTION_CANCEL |
                                (e.getActionIndex() << MotionEvent.ACTION_POINTER_INDEX_SHIFT));
                        TabsGridLayout.this.onTouchEvent(cancelEvent);
                        cancelEvent.recycle();
                    }

                    if (mSwiping) {
                        ViewHelper.setTranslationX(mSwipeView, delta);

                        ViewHelper.setAlpha(mSwipeView, Math.min(1f,
                                1f - 2f * Math.abs(delta) / mTabWidth));

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
            TabsGridLayout.this.getLocationOnScreen(listViewCoords);

            int x = (int) rawX - listViewCoords[0];
            int y = (int) rawY - listViewCoords[1];

            for (int i = 0; i < TabsGridLayout.this.getChildCount(); i++) {
                View child = TabsGridLayout.this.getChildAt(i);
                child.getHitRect(rect);

                if (rect.contains(x, y)) {
                    return child;
                }
            }

            return null;
        }

        private void triggerCheckForTap() {
            if (mPendingCheckForTap == null) {
                mPendingCheckForTap = new CheckForTap();
            }

            TabsGridLayout.this.postDelayed(mPendingCheckForTap, ViewConfiguration.getTapTimeout());
        }

        private void cancelCheckForTap() {
            if (mPendingCheckForTap == null) {
                return;
            }

            TabsGridLayout.this.removeCallbacks(mPendingCheckForTap);
        }

        private class CheckForTap implements Runnable {
            @Override
            public void run() {
                if (!mSwiping && mSwipeView != null && mEnabled) {
                    mSwipeView.setPressed(true);
                }
            }
        }
    }
}
