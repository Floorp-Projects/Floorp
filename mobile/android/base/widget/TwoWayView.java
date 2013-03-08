/*
 * Copyright (C) 2013 Lucas Rocha
 *
 * This code is based on bits and pieces of Android's AbsListView,
 * Listview, and StaggeredGridView.
 *
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.mozilla.gecko.widget;

import org.mozilla.gecko.R;

import java.util.ArrayList;
import java.util.List;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.res.TypedArray;
import android.database.DataSetObserver;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.TransitionDrawable;
import android.os.Build;
import android.os.Parcel;
import android.os.Parcelable;
import android.os.SystemClock;
import android.support.v4.util.LongSparseArray;
import android.support.v4.util.SparseArrayCompat;
import android.support.v4.view.MotionEventCompat;
import android.support.v4.view.VelocityTrackerCompat;
import android.support.v4.view.ViewCompat;
import android.support.v4.widget.EdgeEffectCompat;
import android.util.AttributeSet;
import android.util.Log;
import android.util.SparseBooleanArray;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.HapticFeedbackConstants;
import android.view.MotionEvent;
import android.view.VelocityTracker;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.ViewTreeObserver;
import android.widget.AdapterView;
import android.widget.Checkable;
import android.widget.ListAdapter;
import android.widget.Scroller;

/*
 * Implementation Notes:
 *
 * Some terminology:
 *
 *     index    - index of the items that are currently visible
 *     position - index of the items in the cursor
 *
 * Given the bi-directional nature of this view, the source code
 * usually names variables with 'start' to mean 'top' or 'left'; and
 * 'end' to mean 'bottom' or 'right', depending on the current
 * orientation of the widget.
 */

/**
 * A view that shows items in a vertical or horizontal scrolling list.
 * The items come from the {@link ListAdapter} associated with this view.
 */
public class TwoWayView extends AdapterView<ListAdapter> implements
        ViewTreeObserver.OnTouchModeChangeListener {
    private static final String LOGTAG = "TwoWayView";

    private static final int NO_POSITION = -1;
    private static final int INVALID_POINTER = -1;

    public static final int[] STATE_NOTHING = new int[] { 0 };

    private static final int TOUCH_MODE_REST = -1;
    private static final int TOUCH_MODE_DOWN = 0;
    private static final int TOUCH_MODE_TAP = 1;
    private static final int TOUCH_MODE_DONE_WAITING = 2;
    private static final int TOUCH_MODE_DRAGGING = 3;
    private static final int TOUCH_MODE_FLINGING = 4;
    private static final int TOUCH_MODE_OVERSCROLL = 5;

    private static final int TOUCH_MODE_UNKNOWN = -1;
    private static final int TOUCH_MODE_ON = 0;
    private static final int TOUCH_MODE_OFF = 1;

    private static final int LAYOUT_NORMAL = 0;
    private static final int LAYOUT_FORCE_TOP = 1;
    private static final int LAYOUT_SET_SELECTION = 2;
    private static final int LAYOUT_FORCE_BOTTOM = 3;
    private static final int LAYOUT_SPECIFIC = 4;
    private static final int LAYOUT_SYNC = 5;
    private static final int LAYOUT_MOVE_SELECTION = 6;

    private static final int SYNC_SELECTED_POSITION = 0;
    private static final int SYNC_FIRST_POSITION = 1;

    private static final int SYNC_MAX_DURATION_MILLIS = 100;

    private static final int CHECK_POSITION_SEARCH_DISTANCE = 20;

    public static enum ChoiceMode {
        NONE,
        SINGLE,
        MULTIPLE
    }

    public static enum Orientation {
        HORIZONTAL,
        VERTICAL;
    };

    private ListAdapter mAdapter;

    private boolean mIsVertical;

    private int mItemMargin;

    private boolean mInLayout;
    private boolean mBlockLayoutRequests;

    private boolean mIsAttached;

    private final RecycleBin mRecycler;
    private AdapterDataSetObserver mDataSetObserver;

    final boolean[] mIsScrap = new boolean[1];

    private boolean mDataChanged;
    private int mItemCount;
    private int mOldItemCount;
    private boolean mHasStableIds;
    private boolean mAreAllItemsSelectable;

    private int mFirstPosition;
    private int mSpecificStart;

    private SavedState mPendingSync;

    private final int mTouchSlop;
    private final int mMaximumVelocity;
    private final int mFlingVelocity;
    private float mLastTouchPos;
    private float mTouchRemainderPos;
    private int mActivePointerId;

    private Rect mTouchFrame;
    private int mMotionPosition;
    private CheckForTap mPendingCheckForTap;
    private CheckForLongPress mPendingCheckForLongPress;
    private PerformClick mPerformClick;
    private Runnable mTouchModeReset;
    private int mResurrectToPosition;

    private boolean mIsChildViewEnabled;

    private boolean mDrawSelectorOnTop;
    private Drawable mSelector;
    private int mSelectorPosition;
    private final Rect mSelectorRect;

    private int mOverScroll;
    private final int mOverscrollDistance;

    private SelectionNotifier mSelectionNotifier;

    private boolean mNeedSync;
    private int mSyncMode;
    private int mSyncPosition;
    private long mSyncRowId;
    private long mSyncHeight;
    private int mSelectedStart;

    private int mNextSelectedPosition;
    private long mNextSelectedRowId;
    private int mSelectedPosition;
    private long mSelectedRowId;
    private int mOldSelectedPosition;
    private long mOldSelectedRowId;

    private ChoiceMode mChoiceMode;
    private int mCheckedItemCount;
    private SparseBooleanArray mCheckStates;
    LongSparseArray<Integer> mCheckedIdStates;

    private ContextMenuInfo mContextMenuInfo;

    private int mLayoutMode;
    private int mTouchMode;
    private int mLastTouchMode;
    private VelocityTracker mVelocityTracker;
    private final Scroller mScroller;

    private EdgeEffectCompat mStartEdge;
    private EdgeEffectCompat mEndEdge;

    private OnScrollListener mOnScrollListener;
    private int mLastScrollState;

    public interface OnScrollListener {

        /**
         * The view is not scrolling. Note navigating the list using the trackball counts as
         * being in the idle state since these transitions are not animated.
         */
        public static int SCROLL_STATE_IDLE = 0;

        /**
         * The user is scrolling using touch, and their finger is still on the screen
         */
        public static int SCROLL_STATE_TOUCH_SCROLL = 1;

        /**
         * The user had previously been scrolling using touch and had performed a fling. The
         * animation is now coasting to a stop
         */
        public static int SCROLL_STATE_FLING = 2;

        /**
         * Callback method to be invoked while the list view or grid view is being scrolled. If the
         * view is being scrolled, this method will be called before the next frame of the scroll is
         * rendered. In particular, it will be called before any calls to
         * {@link Adapter#getView(int, View, ViewGroup)}.
         *
         * @param view The view whose scroll state is being reported
         *
         * @param scrollState The current scroll state. One of {@link #SCROLL_STATE_IDLE},
         * {@link #SCROLL_STATE_TOUCH_SCROLL} or {@link #SCROLL_STATE_IDLE}.
         */
        public void onScrollStateChanged(TwoWayView view, int scrollState);

        /**
         * Callback method to be invoked when the list or grid has been scrolled. This will be
         * called after the scroll has completed
         * @param view The view whose scroll state is being reported
         * @param firstVisibleItem the index of the first visible cell (ignore if
         *        visibleItemCount == 0)
         * @param visibleItemCount the number of visible cells
         * @param totalItemCount the number of items in the list adaptor
         */
        public void onScroll(TwoWayView view, int firstVisibleItem, int visibleItemCount,
                int totalItemCount);
    }

    /**
     * A RecyclerListener is used to receive a notification whenever a View is placed
     * inside the RecycleBin's scrap heap. This listener is used to free resources
     * associated to Views placed in the RecycleBin.
     *
     * @see TwoWayView.RecycleBin
     * @see TwoWayView#setRecyclerListener(TwoWayView.RecyclerListener)
     */
    public static interface RecyclerListener {
        /**
         * Indicates that the specified View was moved into the recycler's scrap heap.
         * The view is not displayed on screen any more and any expensive resource
         * associated with the view should be discarded.
         *
         * @param view
         */
        void onMovedToScrapHeap(View view);
    }

    public TwoWayView(Context context) {
        this(context, null);
    }

    public TwoWayView(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public TwoWayView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        mNeedSync = false;
        mVelocityTracker = null;

        mLayoutMode = LAYOUT_NORMAL;
        mTouchMode = TOUCH_MODE_REST;
        mLastTouchMode = TOUCH_MODE_UNKNOWN;

        mIsAttached = false;

        mContextMenuInfo = null;

        mOnScrollListener = null;
        mLastScrollState = OnScrollListener.SCROLL_STATE_IDLE;

        final ViewConfiguration vc = ViewConfiguration.get(context);
        mTouchSlop = vc.getScaledTouchSlop();
        mMaximumVelocity = vc.getScaledMaximumFlingVelocity();
        mFlingVelocity = vc.getScaledMinimumFlingVelocity();
        mOverscrollDistance = getScaledOverscrollDistance(vc);

        mOverScroll = 0;

        mScroller = new Scroller(context);

        mIsVertical = true;

        mSelectorPosition = INVALID_POSITION;

        mSelectorRect = new Rect();
        mSelectedStart = 0;

        mResurrectToPosition = INVALID_POSITION;

        mSelectedStart = 0;
        mNextSelectedPosition = INVALID_POSITION;
        mNextSelectedRowId = INVALID_ROW_ID;
        mSelectedPosition = INVALID_POSITION;
        mSelectedRowId = INVALID_ROW_ID;
        mOldSelectedPosition = INVALID_POSITION;
        mOldSelectedRowId = INVALID_ROW_ID;

        mChoiceMode = ChoiceMode.NONE;
        mCheckedItemCount = 0;
        mCheckedIdStates = null;
        mCheckStates = null;

        mRecycler = new RecycleBin();
        mDataSetObserver = new AdapterDataSetObserver();

        mAreAllItemsSelectable = true;

        mStartEdge = null;
        mEndEdge = null;

        setClickable(true);
        setFocusableInTouchMode(true);
        setWillNotDraw(false);
        setAlwaysDrawnWithCacheEnabled(false);
        setWillNotDraw(false);
        setClipToPadding(false);

        ViewCompat.setOverScrollMode(this, ViewCompat.OVER_SCROLL_IF_CONTENT_SCROLLS);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.TwoWayView, defStyle, 0);
        initializeScrollbars(a);

        mDrawSelectorOnTop = a.getBoolean(
                R.styleable.TwoWayView_android_drawSelectorOnTop, false);

        Drawable d = a.getDrawable(R.styleable.TwoWayView_android_listSelector);
        if (d != null) {
            setSelector(d);
        }

        int orientation = a.getInt(R.styleable.TwoWayView_android_orientation, -1);
        if (orientation >= 0) {
            setOrientation(Orientation.values()[orientation]);
        }

        int choiceMode = a.getInt(R.styleable.TwoWayView_android_choiceMode, -1);
        if (choiceMode >= 0) {
            setChoiceMode(ChoiceMode.values()[choiceMode]);
        }

        a.recycle();

        updateScrollbarsDirection();
    }

    public void setOrientation(Orientation orientation) {
        final boolean isVertical = (orientation.compareTo(Orientation.VERTICAL) == 0);
        if (mIsVertical == isVertical) {
            return;
        }

        mIsVertical = isVertical;

        updateScrollbarsDirection();
        resetState();
        mRecycler.clear();

        requestLayout();
    }

    public Orientation getOrientation() {
        return (mIsVertical ? Orientation.VERTICAL : Orientation.HORIZONTAL);
    }

    public void setItemMargin(int itemMargin) {
        if (mItemMargin == itemMargin) {
            return;
        }

        mItemMargin = itemMargin;
        requestLayout();
    }

    public int getItemMargin() {
        return mItemMargin;
    }

    /**
     * Set the listener that will receive notifications every time the list scrolls.
     *
     * @param l the scroll listener
     */
    public void setOnScrollListener(OnScrollListener l) {
        mOnScrollListener = l;
        invokeOnItemScrollListener();
    }

    /**
     * Sets the recycler listener to be notified whenever a View is set aside in
     * the recycler for later reuse. This listener can be used to free resources
     * associated to the View.
     *
     * @param listener The recycler listener to be notified of views set aside
     *        in the recycler.
     *
     * @see TwoWayView.RecycleBin
     * @see TwoWayView.RecyclerListener
     */
    public void setRecyclerListener(RecyclerListener l) {
        mRecycler.mRecyclerListener = l;
    }

    /**
     * Controls whether the selection highlight drawable should be drawn on top of the item or
     * behind it.
     *
     * @param onTop If true, the selector will be drawn on the item it is highlighting. The default
     *        is false.
     *
     * @attr ref android.R.styleable#AbsListView_drawSelectorOnTop
     */
    public void setDrawSelectorOnTop(boolean drawSelectorOnTop) {
        mDrawSelectorOnTop = drawSelectorOnTop;
    }

    /**
     * Set a Drawable that should be used to highlight the currently selected item.
     *
     * @param resID A Drawable resource to use as the selection highlight.
     *
     * @attr ref android.R.styleable#AbsListView_listSelector
     */
    public void setSelector(int resID) {
        setSelector(getResources().getDrawable(resID));
    }

    /**
     * Set a Drawable that should be used to highlight the currently selected item.
     *
     * @param selector A Drawable to use as the selection highlight.
     *
     * @attr ref android.R.styleable#AbsListView_listSelector
     */
    public void setSelector(Drawable selector) {
        if (mSelector != null) {
            mSelector.setCallback(null);
            unscheduleDrawable(mSelector);
        }

        mSelector = selector;
        Rect padding = new Rect();
        selector.getPadding(padding);

        selector.setCallback(this);
        updateSelectorState();
    }

    /**
     * Returns the selector {@link android.graphics.drawable.Drawable} that is used to draw the
     * selection in the list.
     *
     * @return the drawable used to display the selector
     */
    public Drawable getSelector() {
        return mSelector;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public int getSelectedItemPosition() {
        return mNextSelectedPosition;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public long getSelectedItemId() {
        return mNextSelectedRowId;
    }

    /**
     * Returns the number of items currently selected. This will only be valid
     * if the choice mode is not {@link #CHOICE_MODE_NONE} (default).
     *
     * <p>To determine the specific items that are currently selected, use one of
     * the <code>getChecked*</code> methods.
     *
     * @return The number of items currently selected
     *
     * @see #getCheckedItemPosition()
     * @see #getCheckedItemPositions()
     * @see #getCheckedItemIds()
     */
    public int getCheckedItemCount() {
        return mCheckedItemCount;
    }

    /**
     * Returns the checked state of the specified position. The result is only
     * valid if the choice mode has been set to {@link #CHOICE_MODE_SINGLE}
     * or {@link #CHOICE_MODE_MULTIPLE}.
     *
     * @param position The item whose checked state to return
     * @return The item's checked state or <code>false</code> if choice mode
     *         is invalid
     *
     * @see #setChoiceMode(int)
     */
    public boolean isItemChecked(int position) {
        if (mChoiceMode.compareTo(ChoiceMode.NONE) == 0 && mCheckStates != null) {
            return mCheckStates.get(position);
        }

        return false;
    }

    /**
     * Returns the currently checked item. The result is only valid if the choice
     * mode has been set to {@link #CHOICE_MODE_SINGLE}.
     *
     * @return The position of the currently checked item or
     *         {@link #INVALID_POSITION} if nothing is selected
     *
     * @see #setChoiceMode(int)
     */
    public int getCheckedItemPosition() {
        if (mChoiceMode.compareTo(ChoiceMode.SINGLE) == 0 &&
                mCheckStates != null && mCheckStates.size() == 1) {
            return mCheckStates.keyAt(0);
        }

        return INVALID_POSITION;
    }

    /**
     * Returns the set of checked items in the list. The result is only valid if
     * the choice mode has not been set to {@link #CHOICE_MODE_NONE}.
     *
     * @return  A SparseBooleanArray which will return true for each call to
     *          get(int position) where position is a position in the list,
     *          or <code>null</code> if the choice mode is set to
     *          {@link #CHOICE_MODE_NONE}.
     */
    public SparseBooleanArray getCheckedItemPositions() {
        if (mChoiceMode.compareTo(ChoiceMode.NONE) != 0) {
            return mCheckStates;
        }

        return null;
    }

    /**
     * Returns the set of checked items ids. The result is only valid if the
     * choice mode has not been set to {@link #CHOICE_MODE_NONE} and the adapter
     * has stable IDs. ({@link ListAdapter#hasStableIds()} == {@code true})
     *
     * @return A new array which contains the id of each checked item in the
     *         list.
     */
    public long[] getCheckedItemIds() {
        if (mChoiceMode.compareTo(ChoiceMode.NONE) == 0 ||
                mCheckedIdStates == null || mAdapter == null) {
            return new long[0];
        }

        final LongSparseArray<Integer> idStates = mCheckedIdStates;
        final int count = idStates.size();
        final long[] ids = new long[count];

        for (int i = 0; i < count; i++) {
            ids[i] = idStates.keyAt(i);
        }

        return ids;
    }

    /**
     * Sets the checked state of the specified position. The is only valid if
     * the choice mode has been set to {@link #CHOICE_MODE_SINGLE} or
     * {@link #CHOICE_MODE_MULTIPLE}.
     *
     * @param position The item whose checked state is to be checked
     * @param value The new checked state for the item
     */
    public void setItemChecked(int position, boolean value) {
        if (mChoiceMode.compareTo(ChoiceMode.NONE) == 0) {
            return;
        }

        if (mChoiceMode.compareTo(ChoiceMode.MULTIPLE) == 0) {
            boolean oldValue = mCheckStates.get(position);
            mCheckStates.put(position, value);

            if (mCheckedIdStates != null && mAdapter.hasStableIds()) {
                if (value) {
                    mCheckedIdStates.put(mAdapter.getItemId(position), position);
                } else {
                    mCheckedIdStates.delete(mAdapter.getItemId(position));
                }
            }

            if (oldValue != value) {
                if (value) {
                    mCheckedItemCount++;
                } else {
                    mCheckedItemCount--;
                }
            }
        } else {
            boolean updateIds = mCheckedIdStates != null && mAdapter.hasStableIds();

            // Clear all values if we're checking something, or unchecking the currently
            // selected item
            if (value || isItemChecked(position)) {
                mCheckStates.clear();

                if (updateIds) {
                    mCheckedIdStates.clear();
                }
            }

            // This may end up selecting the value we just cleared but this way
            // we ensure length of mCheckStates is 1, a fact getCheckedItemPosition relies on
            if (value) {
                mCheckStates.put(position, true);

                if (updateIds) {
                    mCheckedIdStates.put(mAdapter.getItemId(position), position);
                }

                mCheckedItemCount = 1;
            } else if (mCheckStates.size() == 0 || !mCheckStates.valueAt(0)) {
                mCheckedItemCount = 0;
            }
        }

        // Do not generate a data change while we are in the layout phase
        if (!mInLayout && !mBlockLayoutRequests) {
            mDataChanged = true;
            rememberSyncState();
            requestLayout();
        }
    }

    /**
     * Clear any choices previously set
     */
    public void clearChoices() {
        if (mCheckStates != null) {
            mCheckStates.clear();
        }

        if (mCheckedIdStates != null) {
            mCheckedIdStates.clear();
        }

        mCheckedItemCount = 0;
    }

    /**
     * @see #setChoiceMode(int)
     *
     * @return The current choice mode
     */
    public ChoiceMode getChoiceMode() {
        return mChoiceMode;
    }

    /**
     * Defines the choice behavior for the List. By default, Lists do not have any choice behavior
     * ({@link #CHOICE_MODE_NONE}). By setting the choiceMode to {@link #CHOICE_MODE_SINGLE}, the
     * List allows up to one item to  be in a chosen state. By setting the choiceMode to
     * {@link #CHOICE_MODE_MULTIPLE}, the list allows any number of items to be chosen.
     *
     * @param choiceMode One of {@link #CHOICE_MODE_NONE}, {@link #CHOICE_MODE_SINGLE}, or
     * {@link #CHOICE_MODE_MULTIPLE}
     */
    public void setChoiceMode(ChoiceMode choiceMode) {
        mChoiceMode = choiceMode;

        if (mChoiceMode.compareTo(ChoiceMode.NONE) != 0) {
            if (mCheckStates == null) {
                mCheckStates = new SparseBooleanArray();
            }

            if (mCheckedIdStates == null && mAdapter != null && mAdapter.hasStableIds()) {
                mCheckedIdStates = new LongSparseArray<Integer>();
            }
        }
    }

    @Override
    public ListAdapter getAdapter() {
        return mAdapter;
    }

    @Override
    public void setAdapter(ListAdapter adapter) {
        if (mAdapter != null) {
            mAdapter.unregisterDataSetObserver(mDataSetObserver);
        }

        resetState();
        mRecycler.clear();

        mAdapter = adapter;
        mDataChanged = true;

        mOldSelectedPosition = INVALID_POSITION;
        mOldSelectedRowId = INVALID_ROW_ID;

        if (mCheckStates != null) {
            mCheckStates.clear();
        }

        if (mCheckedIdStates != null) {
            mCheckedIdStates.clear();
        }

        if (mAdapter != null) {
            mOldItemCount = mItemCount;
            mItemCount = adapter.getCount();

            mAdapter.registerDataSetObserver(mDataSetObserver);
            mRecycler.setViewTypeCount(adapter.getViewTypeCount());

            mHasStableIds = adapter.hasStableIds();
            mAreAllItemsSelectable = adapter.areAllItemsEnabled();

            if (mChoiceMode.compareTo(ChoiceMode.NONE) != 0 && mHasStableIds &&
                    mCheckedIdStates == null) {
                mCheckedIdStates = new LongSparseArray<Integer>();
            }

            final int position = lookForSelectablePosition(0);
            setSelectedPositionInt(position);
            setNextSelectedPositionInt(position);

            if (mItemCount == 0) {
                checkSelectionChanged();
            }
        } else {
            mItemCount = 0;
            mHasStableIds = false;
            mAreAllItemsSelectable = true;

            checkSelectionChanged();
        }

        requestLayout();
    }

    @Override
    public int getFirstVisiblePosition() {
        return mFirstPosition;
    }

    @Override
    public int getLastVisiblePosition() {
        return mFirstPosition + getChildCount() - 1;
    }

    @Override
    public int getPositionForView(View view) {
        View child = view;
        try {
            View v;
            while (!(v = (View) child.getParent()).equals(this)) {
                child = v;
            }
        } catch (ClassCastException e) {
            // We made it up to the window without find this list view
            return INVALID_POSITION;
        }

        // Search the children for the list item
        final int childCount = getChildCount();
        for (int i = 0; i < childCount; i++) {
            if (getChildAt(i).equals(child)) {
                return mFirstPosition + i;
            }
        }

        // Child not found!
        return INVALID_POSITION;
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();

        final ViewTreeObserver treeObserver = getViewTreeObserver();
        treeObserver.addOnTouchModeChangeListener(this);

        if (mAdapter != null && mDataSetObserver == null) {
            mDataSetObserver = new AdapterDataSetObserver();
            mAdapter.registerDataSetObserver(mDataSetObserver);

            // Data may have changed while we were detached. Refresh.
            mDataChanged = true;
            mOldItemCount = mItemCount;
            mItemCount = mAdapter.getCount();
        }

        mIsAttached = true;
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        // Detach any view left in the scrap heap
        mRecycler.clear();

        final ViewTreeObserver treeObserver = getViewTreeObserver();
        treeObserver.removeOnTouchModeChangeListener(this);

        if (mAdapter != null) {
            mAdapter.unregisterDataSetObserver(mDataSetObserver);
            mDataSetObserver = null;
        }

        if (mPerformClick != null) {
            removeCallbacks(mPerformClick);
        }

        if (mTouchModeReset != null) {
            removeCallbacks(mTouchModeReset);
            mTouchModeReset.run();
        }

        mIsAttached = false;
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        super.onWindowFocusChanged(hasWindowFocus);

        final int touchMode = isInTouchMode() ? TOUCH_MODE_ON : TOUCH_MODE_OFF;

        if (!hasWindowFocus) {
            if (touchMode == TOUCH_MODE_OFF) {
                // Remember the last selected element
                mResurrectToPosition = mSelectedPosition;
            }
        } else {
            // If we changed touch mode since the last time we had focus
            if (touchMode != mLastTouchMode && mLastTouchMode != TOUCH_MODE_UNKNOWN) {
                // If we come back in trackball mode, we bring the selection back
                if (touchMode == TOUCH_MODE_OFF) {
                    // This will trigger a layout
                    resurrectSelection();

                // If we come back in touch mode, then we want to hide the selector
                } else {
                    hideSelector();
                    mLayoutMode = LAYOUT_NORMAL;
                    layoutChildren();
                }
            }
        }

        mLastTouchMode = touchMode;
    }

    @Override
    protected void onOverScrolled(int scrollX, int scrollY, boolean clampedX, boolean clampedY) {
        boolean needsInvalidate = false;

        if (mIsVertical && mOverScroll != scrollY) {
            onScrollChanged(getScrollX(), scrollY, getScrollX(), mOverScroll);
            mOverScroll = scrollY;
            needsInvalidate = true;
        } else if (!mIsVertical && mOverScroll != scrollX) {
            onScrollChanged(scrollX, getScrollY(), mOverScroll, getScrollY());
            mOverScroll = scrollX;
            needsInvalidate = true;
        }

        if (needsInvalidate) {
            invalidate();
            awakenScrollbarsInternal();
        }
    }

    @TargetApi(9)
    private boolean overScrollByInternal(int deltaX, int deltaY,
            int scrollX, int scrollY,
            int scrollRangeX, int scrollRangeY,
            int maxOverScrollX, int maxOverScrollY,
            boolean isTouchEvent) {
        if (Build.VERSION.SDK_INT < 9) {
            return false;
        }

        return super.overScrollBy(deltaX, deltaY, scrollX, scrollY, scrollRangeX,
                scrollRangeY, maxOverScrollX, maxOverScrollY, isTouchEvent);
    }

    @Override
    @TargetApi(9)
    public void setOverScrollMode(int mode) {
        if (Build.VERSION.SDK_INT < 9) {
            return;
        }

        if (mode != ViewCompat.OVER_SCROLL_NEVER) {
            if (mStartEdge == null) {
                Context context = getContext();

                mStartEdge = new EdgeEffectCompat(context);
                mEndEdge = new EdgeEffectCompat(context);
            }
        } else {
            mStartEdge = null;
            mEndEdge = null;
        }

        super.setOverScrollMode(mode);
    }

    public int pointToPosition(int x, int y) {
        Rect frame = mTouchFrame;
        if (frame == null) {
            mTouchFrame = new Rect();
            frame = mTouchFrame;
        }

        final int count = getChildCount();
        for (int i = count - 1; i >= 0; i--) {
            final View child = getChildAt(i);

            if (child.getVisibility() == View.VISIBLE) {
                child.getHitRect(frame);

                if (frame.contains(x, y)) {
                    return mFirstPosition + i;
                }
            }
        }
        return INVALID_POSITION;
    }

    @Override
    protected int computeVerticalScrollExtent() {
        final int count = getChildCount();
        if (count == 0) {
            return 0;
        }

        int extent = count * 100;

        View child = getChildAt(0);
        final int childTop = child.getTop();

        int childHeight = child.getHeight();
        if (childHeight > 0) {
            extent += (childTop * 100) / childHeight;
        }

        child = getChildAt(count - 1);
        final int childBottom = child.getBottom();

        childHeight = child.getHeight();
        if (childHeight > 0) {
            extent -= ((childBottom - getHeight()) * 100) / childHeight;
        }

        return extent;
    }

    @Override
    protected int computeHorizontalScrollExtent() {
        final int count = getChildCount();
        if (count == 0) {
            return 0;
        }

        int extent = count * 100;

        View child = getChildAt(0);
        final int childLeft = child.getLeft();

        int childWidth = child.getWidth();
        if (childWidth > 0) {
            extent += (childLeft * 100) / childWidth;
        }

        child = getChildAt(count - 1);
        final int childRight = child.getRight();

        childWidth = child.getWidth();
        if (childWidth > 0) {
            extent -= ((childRight - getWidth()) * 100) / childWidth;
        }

        return extent;
    }

    @Override
    protected int computeVerticalScrollOffset() {
        final int firstPosition = mFirstPosition;
        final int childCount = getChildCount();

        if (firstPosition < 0 || childCount == 0) {
            return 0;
        }

        final View child = getChildAt(0);
        final int childTop = child.getTop();

        int childHeight = child.getHeight();
        if (childHeight > 0) {
            return Math.max(firstPosition * 100 - (childTop * 100) / childHeight, 0);
        }

        return 0;
    }

    @Override
    protected int computeHorizontalScrollOffset() {
        final int firstPosition = mFirstPosition;
        final int childCount = getChildCount();

        if (firstPosition < 0 || childCount == 0) {
            return 0;
        }

        final View child = getChildAt(0);
        final int childLeft = child.getLeft();

        int childWidth = child.getWidth();
        if (childWidth > 0) {
            return Math.max(firstPosition * 100 - (childLeft * 100) / childWidth, 0);
        }

        return 0;
    }

    @Override
    protected int computeVerticalScrollRange() {
        int result = Math.max(mItemCount * 100, 0);

        if (mIsVertical && mOverScroll != 0) {
            // Compensate for overscroll
            result += Math.abs((int) ((float) mOverScroll / getHeight() * mItemCount * 100));
        }

        return result;
    }

    @Override
    protected int computeHorizontalScrollRange() {
        int result = Math.max(mItemCount * 100, 0);

        if (!mIsVertical && mOverScroll != 0) {
            // Compensate for overscroll
            result += Math.abs((int) ((float) mOverScroll / getWidth() * mItemCount * 100));
        }

        return result;
    }

    @Override
    public boolean showContextMenuForChild(View originalView) {
        final int longPressPosition = getPositionForView(originalView);
        if (longPressPosition >= 0) {
            final long longPressId = mAdapter.getItemId(longPressPosition);
            boolean handled = false;

            OnItemLongClickListener listener = getOnItemLongClickListener();
            if (listener != null) {
                handled = listener.onItemLongClick(TwoWayView.this, originalView,
                        longPressPosition, longPressId);
            }

            if (!handled) {
                mContextMenuInfo = createContextMenuInfo(
                        getChildAt(longPressPosition - mFirstPosition),
                        longPressPosition, longPressId);

                handled = super.showContextMenuForChild(originalView);
            }

            return handled;
        }

        return false;
    }

    @Override
    public void requestDisallowInterceptTouchEvent(boolean disallowIntercept) {
        if (disallowIntercept) {
            recycleVelocityTracker();
        }

        super.requestDisallowInterceptTouchEvent(disallowIntercept);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        if (!mIsAttached) {
            return false;
        }

        final int action = ev.getAction() & MotionEventCompat.ACTION_MASK;
        switch (action) {
        case MotionEvent.ACTION_DOWN:
            initOrResetVelocityTracker();
            mVelocityTracker.addMovement(ev);

            mScroller.abortAnimation();

            final float x = ev.getX();
            final float y = ev.getY();

            mLastTouchPos = (mIsVertical ? y : x);

            final int motionPosition = findMotionRowOrColumn((int) mLastTouchPos);

            mActivePointerId = MotionEventCompat.getPointerId(ev, 0);
            mTouchRemainderPos = 0;

            if (mTouchMode == TOUCH_MODE_FLINGING) {
                return true;
            } else if (motionPosition >= 0) {
                mMotionPosition = motionPosition;
                mTouchMode = TOUCH_MODE_DOWN;
            }

            break;

        case MotionEvent.ACTION_MOVE: {
            if (mTouchMode != TOUCH_MODE_DOWN) {
                break;
            }

            initVelocityTrackerIfNotExists();
            mVelocityTracker.addMovement(ev);

            final int index = MotionEventCompat.findPointerIndex(ev, mActivePointerId);
            if (index < 0) {
                Log.e(LOGTAG, "onInterceptTouchEvent could not find pointer with id " +
                        mActivePointerId + " - did TwoWayView receive an inconsistent " +
                        "event stream?");
                return false;
            }

            final float pos;
            if (mIsVertical) {
                pos = MotionEventCompat.getY(ev, index);
            } else {
                pos = MotionEventCompat.getX(ev, index);
            }

            final float diff = pos - mLastTouchPos + mTouchRemainderPos;
            final int delta = (int) diff;
            mTouchRemainderPos = diff - delta;

            if (maybeStartScrolling(delta)) {
                return true;
            }
        }

        case MotionEvent.ACTION_CANCEL:
        case MotionEvent.ACTION_UP:
            mActivePointerId = INVALID_POINTER;
            mTouchMode = TOUCH_MODE_REST;
            recycleVelocityTracker();
            reportScrollStateChange(OnScrollListener.SCROLL_STATE_IDLE);

            break;
        }

        return false;
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        if (!isEnabled()) {
            // A disabled view that is clickable still consumes the touch
            // events, it just doesn't respond to them.
            return isClickable() || isLongClickable();
        }

        if (!mIsAttached) {
            return false;
        }

        boolean needsInvalidate = false;

        initVelocityTrackerIfNotExists();
        mVelocityTracker.addMovement(ev);

        final int action = ev.getAction() & MotionEventCompat.ACTION_MASK;
        switch (action) {
        case MotionEvent.ACTION_DOWN: {
            if (mDataChanged) {
                break;
            }

            mVelocityTracker.clear();
            mScroller.abortAnimation();

            final float x = ev.getX();
            final float y = ev.getY();

            mLastTouchPos = (mIsVertical ? y : x);

            int motionPosition = pointToPosition((int) x, (int) y);

            mActivePointerId = MotionEventCompat.getPointerId(ev, 0);
            mTouchRemainderPos = 0;

            if (mDataChanged) {
                break;
            }

            if (mTouchMode == TOUCH_MODE_FLINGING) {
                mTouchMode = TOUCH_MODE_DRAGGING;
                reportScrollStateChange(OnScrollListener.SCROLL_STATE_TOUCH_SCROLL);
                motionPosition = findMotionRowOrColumn((int) mLastTouchPos);
                return true;
            } else if (mMotionPosition >= 0 && mAdapter.isEnabled(mMotionPosition)) {
                mTouchMode = TOUCH_MODE_DOWN;
                triggerCheckForTap();
            }

            mMotionPosition = motionPosition;

            break;
        }

        case MotionEvent.ACTION_MOVE: {
            final int index = MotionEventCompat.findPointerIndex(ev, mActivePointerId);
            if (index < 0) {
                Log.e(LOGTAG, "onInterceptTouchEvent could not find pointer with id " +
                        mActivePointerId + " - did TwoWayView receive an inconsistent " +
                        "event stream?");
                return false;
            }

            final float pos;
            if (mIsVertical) {
                pos = MotionEventCompat.getY(ev, index);
            } else {
                pos = MotionEventCompat.getX(ev, index);
            }

            if (mDataChanged) {
                // Re-sync everything if data has been changed
                // since the scroll operation can query the adapter.
                layoutChildren();
            }

            final float diff = pos - mLastTouchPos + mTouchRemainderPos;
            final int delta = (int) diff;
            mTouchRemainderPos = diff - delta;

            switch (mTouchMode) {
            case TOUCH_MODE_DOWN:
            case TOUCH_MODE_TAP:
            case TOUCH_MODE_DONE_WAITING:
                // Check if we have moved far enough that it looks more like a
                // scroll than a tap
                maybeStartScrolling(delta);
                break;

            case TOUCH_MODE_DRAGGING:
            case TOUCH_MODE_OVERSCROLL:
                mLastTouchPos = pos;
                maybeScroll(delta);
                break;
            }

            break;
        }

        case MotionEvent.ACTION_CANCEL:
            cancelCheckForTap();
            mTouchMode = TOUCH_MODE_REST;
            reportScrollStateChange(OnScrollListener.SCROLL_STATE_IDLE);

            setPressed(false);
            View motionView = this.getChildAt(mMotionPosition - mFirstPosition);
            if (motionView != null) {
                motionView.setPressed(false);
            }

            if (mStartEdge != null && mEndEdge != null) {
                needsInvalidate = mStartEdge.onRelease() | mEndEdge.onRelease();
            }

            recycleVelocityTracker();

            break;

        case MotionEvent.ACTION_UP: {
            switch (mTouchMode) {
            case TOUCH_MODE_DOWN:
            case TOUCH_MODE_TAP:
            case TOUCH_MODE_DONE_WAITING: {
                final int motionPosition = mMotionPosition;
                final View child = getChildAt(motionPosition - mFirstPosition);

                final float x = ev.getX();
                final float y = ev.getY();

                boolean inList = false;
                if (mIsVertical) {
                    inList = x > getPaddingLeft() && x < getWidth() - getPaddingRight();
                } else {
                    inList = y > getPaddingTop() && y < getHeight() - getPaddingBottom();
                }

                if (child != null && !child.hasFocusable() && inList) {
                    if (mTouchMode != TOUCH_MODE_DOWN) {
                        child.setPressed(false);
                    }

                    if (mPerformClick == null) {
                        mPerformClick = new PerformClick();
                    }

                    final PerformClick performClick = mPerformClick;
                    performClick.mClickMotionPosition = motionPosition;
                    performClick.rememberWindowAttachCount();

                    mResurrectToPosition = motionPosition;

                    if (mTouchMode == TOUCH_MODE_DOWN || mTouchMode == TOUCH_MODE_TAP) {
                        if (mTouchMode == TOUCH_MODE_DOWN) {
                            cancelCheckForTap();
                        } else {
                            cancelCheckForLongPress();
                        }

                        mLayoutMode = LAYOUT_NORMAL;

                        if (!mDataChanged && mAdapter.isEnabled(motionPosition)) {
                            mTouchMode = TOUCH_MODE_TAP;

                            setPressed(true);
                            positionSelector(mMotionPosition, child);
                            child.setPressed(true);

                            if (mSelector != null) {
                                Drawable d = mSelector.getCurrent();
                                if (d != null && d instanceof TransitionDrawable) {
                                    ((TransitionDrawable) d).resetTransition();
                                }
                            }

                            if (mTouchModeReset != null) {
                                removeCallbacks(mTouchModeReset);
                            }

                            mTouchModeReset = new Runnable() {
                                @Override
                                public void run() {
                                    mTouchMode = TOUCH_MODE_REST;

                                    setPressed(false);
                                    child.setPressed(false);

                                    if (!mDataChanged) {
                                        performClick.run();
                                    }

                                    mTouchModeReset = null;
                                }
                            };

                            postDelayed(mTouchModeReset,
                                    ViewConfiguration.getPressedStateDuration());
                        } else {
                            mTouchMode = TOUCH_MODE_REST;
                            updateSelectorState();
                        }
                    } else if (!mDataChanged && mAdapter.isEnabled(motionPosition)) {
                        performClick.run();
                    }
                }

                mTouchMode = TOUCH_MODE_REST;
                updateSelectorState();

                break;
            }

            case TOUCH_MODE_DRAGGING:
                if (contentFits()) {
                    mTouchMode = TOUCH_MODE_REST;
                    reportScrollStateChange(OnScrollListener.SCROLL_STATE_IDLE);
                    break;
                }

                mVelocityTracker.computeCurrentVelocity(1000, mMaximumVelocity);

                final float velocity;
                if (mIsVertical) {
                    velocity = VelocityTrackerCompat.getYVelocity(mVelocityTracker,
                            mActivePointerId);
                } else {
                    velocity = VelocityTrackerCompat.getXVelocity(mVelocityTracker,
                            mActivePointerId);
                }

                if (Math.abs(velocity) >= mFlingVelocity) {
                    mTouchMode = TOUCH_MODE_FLINGING;
                    reportScrollStateChange(OnScrollListener.SCROLL_STATE_FLING);

                    mScroller.fling(0, 0,
                                    (int) (mIsVertical ? 0 : velocity),
                                    (int) (mIsVertical ? velocity : 0),
                                    (mIsVertical ? 0 : Integer.MIN_VALUE),
                                    (mIsVertical ? 0 : Integer.MAX_VALUE),
                                    (mIsVertical ? Integer.MIN_VALUE : 0),
                                    (mIsVertical ? Integer.MAX_VALUE : 0));

                    mLastTouchPos = 0;
                    needsInvalidate = true;
                } else {
                    mTouchMode = TOUCH_MODE_REST;
                    reportScrollStateChange(OnScrollListener.SCROLL_STATE_IDLE);
                }

                break;
            }

            cancelCheckForTap();
            cancelCheckForLongPress();
            setPressed(false);

            if (mStartEdge != null && mEndEdge != null) {
                needsInvalidate |= mStartEdge.onRelease() | mEndEdge.onRelease();
            }

            recycleVelocityTracker();

            break;
        }
        }

        if (needsInvalidate) {
            ViewCompat.postInvalidateOnAnimation(this);
        }

        return true;
    }

    @Override
    public void onTouchModeChanged(boolean isInTouchMode) {
        if (isInTouchMode) {
            // Get rid of the selection when we enter touch mode
            hideSelector();

            // Layout, but only if we already have done so previously.
            // (Otherwise may clobber a LAYOUT_SYNC layout that was requested to restore
            // state.)
            if (getWidth() > 0 && getHeight() > 0 && getChildCount() > 0) {
                layoutChildren();
            }

            updateSelectorState();
        } else {
            final int touchMode = mTouchMode;
            if (touchMode == TOUCH_MODE_OVERSCROLL) {
                if (mOverScroll != 0) {
                    mOverScroll = 0;
                    finishEdgeGlows();
                    invalidate();
                }
            }
        }
    }

    private void initOrResetVelocityTracker() {
        if (mVelocityTracker == null) {
            mVelocityTracker = VelocityTracker.obtain();
        } else {
            mVelocityTracker.clear();
        }
    }

    private void initVelocityTrackerIfNotExists() {
        if (mVelocityTracker == null) {
            mVelocityTracker = VelocityTracker.obtain();
        }
    }

    private void recycleVelocityTracker() {
        if (mVelocityTracker != null) {
            mVelocityTracker.recycle();
            mVelocityTracker = null;
        }
    }

    /**
     * Notify our scroll listener (if there is one) of a change in scroll state
     */
    private void invokeOnItemScrollListener() {
        if (mOnScrollListener != null) {
            mOnScrollListener.onScroll(this, mFirstPosition, getChildCount(), mItemCount);
        }

        // Dummy values, View's implementation does not use these.
        onScrollChanged(0, 0, 0, 0);
    }

    private void reportScrollStateChange(int newState) {
        if (newState == mLastScrollState) {
            return;
        }

        if (mOnScrollListener != null) {
            mLastScrollState = newState;
            mOnScrollListener.onScrollStateChanged(this, newState);
        }
    }

    private boolean maybeStartScrolling(int delta) {
        final boolean isOverScroll = (mOverScroll != 0);
        if (Math.abs(delta) <= mTouchSlop && !isOverScroll) {
            return false;
        }

        if (isOverScroll) {
            mTouchMode = TOUCH_MODE_OVERSCROLL;
        } else {
            mTouchMode = TOUCH_MODE_DRAGGING;
        }

        // Time to start stealing events! Once we've stolen them, don't
        // let anyone steal from us.
        final ViewParent parent = getParent();
        if (parent != null) {
            parent.requestDisallowInterceptTouchEvent(true);
        }

        cancelCheckForLongPress();

        setPressed(false);
        View motionView = getChildAt(mMotionPosition - mFirstPosition);
        if (motionView != null) {
            motionView.setPressed(false);
        }

        reportScrollStateChange(OnScrollListener.SCROLL_STATE_TOUCH_SCROLL);

        return true;
    }

    private void maybeScroll(int delta) {
        if (mTouchMode == TOUCH_MODE_DRAGGING) {
            handleDragChange(delta);
        } else if (mTouchMode == TOUCH_MODE_OVERSCROLL) {
            handleOverScrollChange(delta);
        }
    }

    private void handleDragChange(int delta) {
        // Time to start stealing events! Once we've stolen them, don't
        // let anyone steal from us.
        final ViewParent parent = getParent();
        if (parent != null) {
            parent.requestDisallowInterceptTouchEvent(true);
        }

        final int motionIndex;
        if (mMotionPosition >= 0) {
            motionIndex = mMotionPosition - mFirstPosition;
        } else {
            // If we don't have a motion position that we can reliably track,
            // pick something in the middle to make a best guess at things below.
            motionIndex = getChildCount() / 2;
        }

        int motionViewPrevStart = 0;
        View motionView = this.getChildAt(motionIndex);
        if (motionView != null) {
            motionViewPrevStart = (mIsVertical ? motionView.getTop() : motionView.getLeft());
        }

        boolean atEdge = trackMotionScroll(delta);

        motionView = this.getChildAt(motionIndex);
        if (motionView != null) {
            final int motionViewRealStart =
                    (mIsVertical ? motionView.getTop() : motionView.getLeft());

            if (atEdge) {
                final int overscroll = -delta - (motionViewRealStart - motionViewPrevStart);
                updateOverScrollState(delta, overscroll);
            }
        }
    }

    private void updateOverScrollState(int delta, int overscroll) {
        overScrollByInternal((mIsVertical ? 0 : overscroll),
                             (mIsVertical ? overscroll : 0),
                             (mIsVertical ? 0 : mOverScroll),
                             (mIsVertical ? mOverScroll : 0),
                             0, 0,
                             (mIsVertical ? 0 : mOverscrollDistance),
                             (mIsVertical ? mOverscrollDistance : 0),
                             true);

        if (Math.abs(mOverscrollDistance) == Math.abs(mOverScroll)) {
            // Break fling velocity if we impacted an edge
            if (mVelocityTracker != null) {
                mVelocityTracker.clear();
            }
        }

        final int overscrollMode = ViewCompat.getOverScrollMode(this);
        if (overscrollMode == ViewCompat.OVER_SCROLL_ALWAYS ||
                (overscrollMode == ViewCompat.OVER_SCROLL_IF_CONTENT_SCROLLS && !contentFits())) {
            mTouchMode = TOUCH_MODE_OVERSCROLL;

            float pull = (float) overscroll / (mIsVertical ? getHeight() : getWidth());
            if (delta > 0) {
                mStartEdge.onPull(pull);

                if (!mEndEdge.isFinished()) {
                    mEndEdge.onRelease();
                }
            } else if (delta < 0) {
                mEndEdge.onPull(pull);

                if (!mStartEdge.isFinished()) {
                    mStartEdge.onRelease();
                }
            }

            if (delta != 0) {
                ViewCompat.postInvalidateOnAnimation(this);
            }
        }
    }

    private void handleOverScrollChange(int delta) {
        final int oldOverScroll = mOverScroll;
        final int newOverScroll = oldOverScroll - delta;

        int overScrollDistance = -delta;
        if ((newOverScroll < 0 && oldOverScroll >= 0) ||
                (newOverScroll > 0 && oldOverScroll <= 0)) {
            overScrollDistance = -oldOverScroll;
            delta += overScrollDistance;
        } else {
            delta = 0;
        }

        if (overScrollDistance != 0) {
            updateOverScrollState(delta, overScrollDistance);
        }

        if (delta != 0) {
            if (mOverScroll != 0) {
                mOverScroll = 0;
                ViewCompat.postInvalidateOnAnimation(this);
            }

            trackMotionScroll(delta);
            mTouchMode = TOUCH_MODE_DRAGGING;

            // We did not scroll the full amount. Treat this essentially like the
            // start of a new touch scroll
            mMotionPosition = findClosestMotionRowOrColumn((int) mLastTouchPos);
            mTouchRemainderPos = 0;
        }
    }

    int findMotionRowOrColumn(int motionPos) {
        int childCount = getChildCount();
        if (childCount == 0) {
            return INVALID_POSITION;
        }

        for (int i = 0; i < childCount; i++) {
            View v = getChildAt(i);

            if ((mIsVertical && motionPos <= v.getBottom()) ||
                    (!mIsVertical && motionPos <= v.getRight())) {
                return mFirstPosition + i;
            }
        }

        return INVALID_POSITION;
    }

    private int findClosestMotionRowOrColumn(int motionPos) {
        final int childCount = getChildCount();
        if (childCount == 0) {
            return INVALID_POSITION;
        }

        final int motionRow = findMotionRowOrColumn(motionPos);
        if (motionRow != INVALID_POSITION) {
            return motionRow;
        } else {
            return mFirstPosition + childCount - 1;
        }
    }

    @TargetApi(9)
    private int getScaledOverscrollDistance(ViewConfiguration vc) {
        if (Build.VERSION.SDK_INT < 9) {
            return 0;
        }

        return vc.getScaledOverscrollDistance();
    }

    private boolean contentFits() {
        final int childCount = getChildCount();
        if (childCount == 0) {
            return true;
        }

        if (childCount != mItemCount) {
            return false;
        }

        View first = getChildAt(0);
        View last = getChildAt(childCount - 1);

        if (mIsVertical) {
            return first.getTop() >= getPaddingTop() &&
                    last.getBottom() <= getHeight() - getPaddingBottom();
        } else {
            return first.getLeft() >= getPaddingLeft() &&
                    last.getRight() <= getWidth() - getPaddingRight();
        }
    }

    private void updateScrollbarsDirection() {
        setHorizontalScrollBarEnabled(!mIsVertical);
        setVerticalScrollBarEnabled(mIsVertical);
    }

    private void triggerCheckForTap() {
        if (mPendingCheckForTap == null) {
            mPendingCheckForTap = new CheckForTap();
        }

        postDelayed(mPendingCheckForTap, ViewConfiguration.getTapTimeout());
    }

    private void cancelCheckForTap() {
        if (mPendingCheckForTap == null) {
            return;
        }

        removeCallbacks(mPendingCheckForTap);
    }

    private void triggerCheckForLongPress() {
        if (mPendingCheckForLongPress == null) {
            mPendingCheckForLongPress = new CheckForLongPress();
        }

        mPendingCheckForLongPress.rememberWindowAttachCount();

        postDelayed(mPendingCheckForLongPress,
                ViewConfiguration.getLongPressTimeout());
    }

    private void cancelCheckForLongPress() {
        if (mPendingCheckForLongPress == null) {
            return;
        }

        removeCallbacks(mPendingCheckForLongPress);
    }

    boolean trackMotionScroll(int incrementalDelta) {
        final int childCount = getChildCount();
        if (childCount == 0) {
            return true;
        }

        final View first = getChildAt(0);
        final int firstStart = (mIsVertical ? first.getTop() : first.getLeft());

        final View last = getChildAt(childCount - 1);
        final int lastEnd = (mIsVertical ? last.getBottom() : last.getRight());

        final int paddingTop = getPaddingTop();
        final int paddingBottom = getPaddingBottom();
        final int paddingLeft = getPaddingLeft();
        final int paddingRight = getPaddingRight();

        final int paddingStart = (mIsVertical ? paddingTop : paddingLeft);

        final int spaceBefore = paddingStart - firstStart;
        final int end = (mIsVertical ? getHeight() - paddingBottom :
            getWidth() - paddingRight);
        final int spaceAfter = lastEnd - end;

        final int size;
        if (mIsVertical) {
            size = getHeight() - paddingBottom - paddingTop;
        } else {
            size = getWidth() - paddingRight - paddingLeft;
        }

        if (incrementalDelta < 0) {
            incrementalDelta = Math.max(-(size - 1), incrementalDelta);
        } else {
            incrementalDelta = Math.min(size - 1, incrementalDelta);
        }

        final int firstPosition = mFirstPosition;

        final boolean cannotScrollDown = (firstPosition == 0 &&
                firstStart >= paddingStart && incrementalDelta >= 0);
        final boolean cannotScrollUp = (firstPosition + childCount == mItemCount &&
                lastEnd <= end && incrementalDelta <= 0);

        if (cannotScrollDown || cannotScrollUp) {
            return incrementalDelta != 0;
        }

        final boolean inTouchMode = isInTouchMode();
        if (inTouchMode) {
            hideSelector();
        }

        int start = 0;
        int count = 0;

        final boolean down = (incrementalDelta < 0);
        if (down) {
            int childrenStart = -incrementalDelta + paddingStart;

            for (int i = 0; i < childCount; i++) {
                final View child = getChildAt(i);
                final int childEnd = (mIsVertical ? child.getBottom() : child.getRight());

                if (childEnd >= childrenStart) {
                    break;
                }

                count++;
                mRecycler.addScrapView(child, firstPosition + i);
            }
        } else {
            int childrenEnd = end - incrementalDelta;

            for (int i = childCount - 1; i >= 0; i--) {
                final View child = getChildAt(i);
                final int childStart = (mIsVertical ? child.getTop() : child.getLeft());

                if (childStart <= childrenEnd) {
                    break;
                }

                start = i;
                count++;
                mRecycler.addScrapView(child, firstPosition + i);
            }
        }

        mBlockLayoutRequests = true;

        if (count > 0) {
            detachViewsFromParent(start, count);
        }

        // invalidate before moving the children to avoid unnecessary invalidate
        // calls to bubble up from the children all the way to the top
        if (!awakenScrollbarsInternal()) {
           invalidate();
        }

        offsetChildren(incrementalDelta);

        if (down) {
            mFirstPosition += count;
        }

        final int absIncrementalDelta = Math.abs(incrementalDelta);
        if (spaceBefore < absIncrementalDelta || spaceAfter < absIncrementalDelta) {
            fillGap(down);
        }

        if (!inTouchMode && mSelectedPosition != INVALID_POSITION) {
            final int childIndex = mSelectedPosition - mFirstPosition;
            if (childIndex >= 0 && childIndex < getChildCount()) {
                positionSelector(mSelectedPosition, getChildAt(childIndex));
            }
        } else if (mSelectorPosition != INVALID_POSITION) {
            final int childIndex = mSelectorPosition - mFirstPosition;
            if (childIndex >= 0 && childIndex < getChildCount()) {
                positionSelector(INVALID_POSITION, getChildAt(childIndex));
            }
        } else {
            mSelectorRect.setEmpty();
        }

        mBlockLayoutRequests = false;

        invokeOnItemScrollListener();

        return false;
    }

    @TargetApi(14)
    private final float getCurrVelocity() {
        if (Build.VERSION.SDK_INT >= 14) {
            return mScroller.getCurrVelocity();
        }

        return 0;
    }

    @TargetApi(5)
    private boolean awakenScrollbarsInternal() {
        if (Build.VERSION.SDK_INT >= 5) {
            return super.awakenScrollBars();
        } else {
            return false;
        }
    }

    @Override
    public void computeScroll() {
        if (!mScroller.computeScrollOffset()) {
            return;
        }

        final int pos;
        if (mIsVertical) {
            pos = mScroller.getCurrY();
        } else {
            pos = mScroller.getCurrX();
        }

        final int diff = (int) (pos - mLastTouchPos);
        mLastTouchPos = pos;

        final boolean stopped = trackMotionScroll(diff);

        if (!stopped && !mScroller.isFinished()) {
            ViewCompat.postInvalidateOnAnimation(this);
        } else {
            if (stopped) {
                final int overScrollMode = ViewCompat.getOverScrollMode(this);
                if (overScrollMode != ViewCompat.OVER_SCROLL_NEVER) {
                    final EdgeEffectCompat edge =
                            (diff > 0 ? mStartEdge : mEndEdge);

                    boolean needsInvalidate =
                            edge.onAbsorb(Math.abs((int) getCurrVelocity()));

                    if (needsInvalidate) {
                        ViewCompat.postInvalidateOnAnimation(this);
                    }
                }

                mScroller.abortAnimation();
            }

            mTouchMode = TOUCH_MODE_REST;
            reportScrollStateChange(OnScrollListener.SCROLL_STATE_IDLE);
        }
    }

    private void finishEdgeGlows() {
        if (mStartEdge != null) {
            mStartEdge.finish();
        }

        if (mEndEdge != null) {
            mEndEdge.finish();
        }
    }

    private boolean drawStartEdge(Canvas canvas) {
        if (mStartEdge.isFinished()) {
            return false;
        }

        if (mIsVertical) {
            return mStartEdge.draw(canvas);
        }

        final int restoreCount = canvas.save();
        final int height = getHeight() - getPaddingTop() - getPaddingBottom();

        canvas.translate(0, height);
        canvas.rotate(270);

        final boolean needsInvalidate = mStartEdge.draw(canvas);
        canvas.restoreToCount(restoreCount);
        return needsInvalidate;
    }

    private boolean drawEndEdge(Canvas canvas) {
        if (mEndEdge.isFinished()) {
            return false;
        }

        final int restoreCount = canvas.save();
        final int width = getWidth() - getPaddingLeft() - getPaddingRight();
        final int height = getHeight() - getPaddingTop() - getPaddingBottom();

        if (mIsVertical) {
            canvas.translate(-width, height);
            canvas.rotate(180, width, 0);
        } else {
            canvas.translate(width, 0);
            canvas.rotate(90);
        }

        final boolean needsInvalidate = mEndEdge.draw(canvas);
        canvas.restoreToCount(restoreCount);
        return needsInvalidate;
    }

    private void drawSelector(Canvas canvas) {
        if (!mSelectorRect.isEmpty()) {
            final Drawable selector = mSelector;
            selector.setBounds(mSelectorRect);
            selector.draw(canvas);
        }
    }

    private void useDefaultSelector() {
        setSelector(getResources().getDrawable(
                android.R.drawable.list_selector_background));
    }

    private boolean shouldShowSelector() {
        return (hasFocus() && !isInTouchMode()) || touchModeDrawsInPressedState();
    }

    private void positionSelector(int position, View selected) {
        if (position != INVALID_POSITION) {
            mSelectorPosition = position;
        }

        mSelectorRect.set(selected.getLeft(), selected.getTop(), selected.getRight(),
                selected.getBottom());

        final boolean isChildViewEnabled = mIsChildViewEnabled;
        if (selected.isEnabled() != isChildViewEnabled) {
            mIsChildViewEnabled = !isChildViewEnabled;

            if (getSelectedItemPosition() != INVALID_POSITION) {
                refreshDrawableState();
            }
        }
    }

    private void hideSelector() {
        if (mSelectedPosition != INVALID_POSITION) {
            if (mLayoutMode != LAYOUT_SPECIFIC) {
                mResurrectToPosition = mSelectedPosition;
            }

            if (mNextSelectedPosition >= 0 && mNextSelectedPosition != mSelectedPosition) {
                mResurrectToPosition = mNextSelectedPosition;
            }

            setSelectedPositionInt(INVALID_POSITION);
            setNextSelectedPositionInt(INVALID_POSITION);

            mSelectedStart = 0;
        }
    }

    private void setSelectedPositionInt(int position) {
        mSelectedPosition = position;
        mSelectedRowId = getItemIdAtPosition(position);
    }

    private void setSelectionInt(int position) {
        setNextSelectedPositionInt(position);
        boolean awakeScrollbars = false;

        final int selectedPosition = mSelectedPosition;
        if (selectedPosition >= 0) {
            if (position == selectedPosition - 1) {
                awakeScrollbars = true;
            } else if (position == selectedPosition + 1) {
                awakeScrollbars = true;
            }
        }

        layoutChildren();

        if (awakeScrollbars) {
            awakenScrollbarsInternal();
        }
    }

    private void setNextSelectedPositionInt(int position) {
        mNextSelectedPosition = position;
        mNextSelectedRowId = getItemIdAtPosition(position);

        // If we are trying to sync to the selection, update that too
        if (mNeedSync && mSyncMode == SYNC_SELECTED_POSITION && position >= 0) {
            mSyncPosition = position;
            mSyncRowId = mNextSelectedRowId;
        }
    }

    private boolean touchModeDrawsInPressedState() {
        switch (mTouchMode) {
        case TOUCH_MODE_TAP:
        case TOUCH_MODE_DONE_WAITING:
            return true;
        default:
            return false;
        }
    }

    private void updateSelectorState() {
        if (mSelector != null) {
            if (shouldShowSelector()) {
                mSelector.setState(getDrawableState());
            } else {
                mSelector.setState(STATE_NOTHING);
            }
        }
    }

    private void checkSelectionChanged() {
        if ((mSelectedPosition != mOldSelectedPosition) || (mSelectedRowId != mOldSelectedRowId)) {
            selectionChanged();
            mOldSelectedPosition = mSelectedPosition;
            mOldSelectedRowId = mSelectedRowId;
        }
    }

    private void selectionChanged() {
        OnItemSelectedListener listener = getOnItemSelectedListener();
        if (listener == null) {
            return;
        }

        if (mInLayout || mBlockLayoutRequests) {
            // If we are in a layout traversal, defer notification
            // by posting. This ensures that the view tree is
            // in a consistent state and is able to accommodate
            // new layout or invalidate requests.
            if (mSelectionNotifier == null) {
                mSelectionNotifier = new SelectionNotifier();
            }

            post(mSelectionNotifier);
        } else {
            fireOnSelected();
        }
    }

    private void fireOnSelected() {
        OnItemSelectedListener listener = getOnItemSelectedListener();
        if (listener == null) {
            return;
        }

        final int selection = getSelectedItemPosition();
        if (selection >= 0) {
            View v = getSelectedView();
            listener.onItemSelected(this, v, selection,
                    mAdapter.getItemId(selection));
        } else {
            listener.onNothingSelected(this);
        }
    }

    private int lookForSelectablePosition(int position) {
        return lookForSelectablePosition(position, true);
    }

    private int lookForSelectablePosition(int position, boolean lookDown) {
        final ListAdapter adapter = mAdapter;
        if (adapter == null || isInTouchMode()) {
            return INVALID_POSITION;
        }

        final int itemCount = mItemCount;
        if (!mAreAllItemsSelectable) {
            if (lookDown) {
                position = Math.max(0, position);
                while (position < itemCount && !adapter.isEnabled(position)) {
                    position++;
                }
            } else {
                position = Math.min(position, itemCount - 1);
                while (position >= 0 && !adapter.isEnabled(position)) {
                    position--;
                }
            }

            if (position < 0 || position >= itemCount) {
                return INVALID_POSITION;
            }

            return position;
        } else {
            if (position < 0 || position >= itemCount) {
                return INVALID_POSITION;
            }

            return position;
        }
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();
        updateSelectorState();
    }

    @Override
    protected int[] onCreateDrawableState(int extraSpace) {
        // If the child view is enabled then do the default behavior.
        if (mIsChildViewEnabled) {
            // Common case
            return super.onCreateDrawableState(extraSpace);
        }

        // The selector uses this View's drawable state. The selected child view
        // is disabled, so we need to remove the enabled state from the drawable
        // states.
        final int enabledState = ENABLED_STATE_SET[0];

        // If we don't have any extra space, it will return one of the static state arrays,
        // and clearing the enabled state on those arrays is a bad thing!  If we specify
        // we need extra space, it will create+copy into a new array that safely mutable.
        int[] state = super.onCreateDrawableState(extraSpace + 1);
        int enabledPos = -1;
        for (int i = state.length - 1; i >= 0; i--) {
            if (state[i] == enabledState) {
                enabledPos = i;
                break;
            }
        }

        // Remove the enabled state
        if (enabledPos >= 0) {
            System.arraycopy(state, enabledPos + 1, state, enabledPos,
                    state.length - enabledPos - 1);
        }

        return state;
    }

    @Override
    protected boolean canAnimate() {
        return (super.canAnimate() && mItemCount > 0);
    }

    @Override
    protected void dispatchDraw(Canvas canvas) {
        final boolean drawSelectorOnTop = mDrawSelectorOnTop;
        if (!drawSelectorOnTop) {
            drawSelector(canvas);
        }

        super.dispatchDraw(canvas);

        if (drawSelectorOnTop) {
            drawSelector(canvas);
        }
    }

    @Override
    public void draw(Canvas canvas) {
        super.draw(canvas);

        boolean needsInvalidate = false;

        if (mStartEdge != null) {
            needsInvalidate |= drawStartEdge(canvas);
        }

        if (mEndEdge != null) {
            needsInvalidate |= drawEndEdge(canvas);
        }

        if (needsInvalidate) {
            ViewCompat.postInvalidateOnAnimation(this);
        }
    }

    @Override
    public void requestLayout() {
        if (!mInLayout && !mBlockLayoutRequests) {
            super.requestLayout();
        }
    }

    @Override
    public View getSelectedView() {
        if (mItemCount > 0 && mSelectedPosition >= 0) {
            return getChildAt(mSelectedPosition - mFirstPosition);
        } else {
            return null;
        }
    }

    @Override
    public void setSelection(int position) {
        setSelectionFromOffset(position, 0);
    }

    public void setSelectionFromOffset(int position, int offset) {
        if (mAdapter == null) {
            return;
        }

        if (!isInTouchMode()) {
            position = lookForSelectablePosition(position);
            if (position >= 0) {
                setNextSelectedPositionInt(position);
            }
        } else {
            mResurrectToPosition = position;
        }

        if (position >= 0) {
            mLayoutMode = LAYOUT_SPECIFIC;

            if (mIsVertical) {
                mSpecificStart = getPaddingTop() + offset;
            } else {
                mSpecificStart = getPaddingLeft() + offset;
            }

            if (mNeedSync) {
                mSyncPosition = position;
                mSyncRowId = mAdapter.getItemId(position);
            }

            requestLayout();
        }
    }

    @Override
    protected void dispatchSetPressed(boolean pressed) {
        // Don't dispatch setPressed to our children. We call setPressed on ourselves to
        // get the selector in the right state, but we don't want to press each child.
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mSelector == null) {
            useDefaultSelector();
        }

        int widthMode = MeasureSpec.getMode(widthMeasureSpec);
        int heightMode = MeasureSpec.getMode(heightMeasureSpec);
        int widthSize = MeasureSpec.getSize(widthMeasureSpec);
        int heightSize = MeasureSpec.getSize(heightMeasureSpec);

        int childWidth = 0;
        int childHeight = 0;

        mItemCount = (mAdapter == null ? 0 : mAdapter.getCount());
        if (mItemCount > 0 && (widthMode == MeasureSpec.UNSPECIFIED ||
                heightMode == MeasureSpec.UNSPECIFIED)) {
            final View child = obtainView(0, mIsScrap);

            final int secondaryMeasureSpec =
                    (mIsVertical ? widthMeasureSpec : heightMeasureSpec);

            measureScrapChild(child, 0, secondaryMeasureSpec);

            childWidth = child.getMeasuredWidth();
            childHeight = child.getMeasuredHeight();

            if (recycleOnMeasure()) {
                mRecycler.addScrapView(child, -1);
            }
        }

        if (widthMode == MeasureSpec.UNSPECIFIED) {
            widthSize = getPaddingLeft() + getPaddingRight() + childWidth;
            if (mIsVertical) {
                widthSize += getVerticalScrollbarWidth();
            }
        }

        if (heightMode == MeasureSpec.UNSPECIFIED) {
            heightSize = getPaddingTop() + getPaddingBottom() + childHeight;
            if (!mIsVertical) {
                heightSize += getHorizontalScrollbarHeight();
            }
        }

        if (mIsVertical && heightMode == MeasureSpec.AT_MOST) {
            heightSize = measureHeightOfChildren(widthMeasureSpec, 0, NO_POSITION, heightSize, -1);
        }

        if (!mIsVertical && widthMode == MeasureSpec.AT_MOST) {
            widthSize = measureWidthOfChildren(heightMeasureSpec, 0, NO_POSITION, widthSize, -1);
        }

        setMeasuredDimension(widthSize, heightSize);
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        mInLayout = true;

        if (changed) {
            final int childCount = getChildCount();
            for (int i = 0; i < childCount; i++) {
                getChildAt(i).forceLayout();
            }

            mRecycler.markChildrenDirty();
        }

        layoutChildren();

        mInLayout = false;

        final int width = r - l - getPaddingLeft() - getPaddingRight();
        final int height = b - t - getPaddingTop() - getPaddingBottom();

        if (mStartEdge != null && mEndEdge != null) {
            if (mIsVertical) {
                mStartEdge.setSize(width, height);
                mEndEdge.setSize(width, height);
            } else {
                mStartEdge.setSize(height, width);
                mEndEdge.setSize(height, width);
            }
        }
    }

    private void layoutChildren() {
        if (getWidth() == 0 || getHeight() == 0) {
            return;
        }

        final boolean blockLayoutRequests = mBlockLayoutRequests;
        if (!blockLayoutRequests) {
            mBlockLayoutRequests = true;
        } else {
            return;
        }

        try {
            invalidate();

            if (mAdapter == null) {
                resetState();
                return;
            }

            final int start = (mIsVertical ? getPaddingTop() : getPaddingLeft());
            final int end =
                    (mIsVertical ? getHeight() - getPaddingBottom() : getWidth() - getPaddingRight());

            int childCount = getChildCount();
            int index = 0;
            int delta = 0;

            View selected = null;
            View oldSelected = null;
            View newSelected = null;
            View oldFirstChild = null;

            switch (mLayoutMode) {
            case LAYOUT_SET_SELECTION:
                index = mNextSelectedPosition - mFirstPosition;
                if (index >= 0 && index < childCount) {
                    newSelected = getChildAt(index);
                }

                break;

            case LAYOUT_FORCE_TOP:
            case LAYOUT_FORCE_BOTTOM:
            case LAYOUT_SPECIFIC:
            case LAYOUT_SYNC:
                break;

            case LAYOUT_MOVE_SELECTION:
            default:
                // Remember the previously selected view
                index = mSelectedPosition - mFirstPosition;
                if (index >= 0 && index < childCount) {
                    oldSelected = getChildAt(index);
                }

                // Remember the previous first child
                oldFirstChild = getChildAt(0);

                if (mNextSelectedPosition >= 0) {
                    delta = mNextSelectedPosition - mSelectedPosition;
                }

                // Caution: newSelected might be null
                newSelected = getChildAt(index + delta);
            }

            final boolean dataChanged = mDataChanged;
            if (dataChanged) {
                handleDataChanged();
            }

            // Handle the empty set by removing all views that are visible
            // and calling it a day
            if (mItemCount == 0) {
                resetState();
                return;
            } else if (mItemCount != mAdapter.getCount()) {
                throw new IllegalStateException("The content of the adapter has changed but "
                        + "TwoWayView did not receive a notification. Make sure the content of "
                        + "your adapter is not modified from a background thread, but only "
                        + "from the UI thread. [in TwoWayView(" + getId() + ", " + getClass()
                        + ") with Adapter(" + mAdapter.getClass() + ")]");
            }

            setSelectedPositionInt(mNextSelectedPosition);

            // Pull all children into the RecycleBin.
            // These views will be reused if possible
            final int firstPosition = mFirstPosition;
            final RecycleBin recycleBin = mRecycler;

            if (dataChanged) {
                for (int i = 0; i < childCount; i++) {
                    recycleBin.addScrapView(getChildAt(i), firstPosition + i);
                }
            } else {
                recycleBin.fillActiveViews(childCount, firstPosition);
            }

            detachAllViewsFromParent();

            switch (mLayoutMode) {
            case LAYOUT_SET_SELECTION:
                if (newSelected != null) {
                    final int newSelectedStart =
                            (mIsVertical ? newSelected.getTop() : newSelected.getLeft());

                    selected = fillFromSelection(newSelectedStart, start, end);
                } else {
                    selected = fillFromMiddle(start, end);
                }

                break;

            case LAYOUT_SYNC:
                selected = fillSpecific(mSyncPosition, mSpecificStart);
                break;

            case LAYOUT_FORCE_BOTTOM:
                selected = fillBefore(mItemCount - 1, end);
                adjustViewsStartOrEnd();
                break;

            case LAYOUT_FORCE_TOP:
                mFirstPosition = 0;
                selected = fillFromOffset(start);
                adjustViewsStartOrEnd();
                break;

            case LAYOUT_SPECIFIC:
                selected = fillSpecific(reconcileSelectedPosition(), mSpecificStart);
                break;

            case LAYOUT_MOVE_SELECTION:
                selected = moveSelection(oldSelected, newSelected, delta, start, end);
                break;

            default:
                if (childCount == 0) {
                    final int position = lookForSelectablePosition(0);
                    setSelectedPositionInt(position);
                    selected = fillFromOffset(start);
                } else {
                    if (mSelectedPosition >= 0 && mSelectedPosition < mItemCount) {
                        int offset = start;
                        if (oldSelected != null) {
                            offset = (mIsVertical ? oldSelected.getTop() : oldSelected.getLeft());
                        }
                        selected = fillSpecific(mSelectedPosition, offset);
                    } else if (mFirstPosition < mItemCount) {
                        int offset = start;
                        if (oldFirstChild != null) {
                            offset = (mIsVertical ? oldFirstChild.getTop() : oldFirstChild.getLeft());
                        }

                        selected = fillSpecific(mFirstPosition, offset);
                    } else {
                        selected = fillSpecific(0, start);
                    }
                }

                break;

            }

            recycleBin.scrapActiveViews();

            if (selected != null) {
                positionSelector(INVALID_POSITION, selected);
                mSelectedStart = (mIsVertical ? selected.getTop() : selected.getLeft());
            } else {
                if (mTouchMode > TOUCH_MODE_DOWN && mTouchMode < TOUCH_MODE_DRAGGING) {
                    View child = getChildAt(mMotionPosition - mFirstPosition);

                    if (child != null) {
                        positionSelector(mMotionPosition, child);
                    }
                } else {
                    mSelectedStart = 0;
                    mSelectorRect.setEmpty();
                }
            }

            mLayoutMode = LAYOUT_NORMAL;
            mDataChanged = false;
            mNeedSync = false;

            setNextSelectedPositionInt(mSelectedPosition);
            if (mItemCount > 0) {
                checkSelectionChanged();
            }

            invokeOnItemScrollListener();
        } finally {
            if (!blockLayoutRequests) {
                mBlockLayoutRequests = false;
                mDataChanged = false;
            }
        }
    }

    protected boolean recycleOnMeasure() {
        return true;
    }

    private void offsetChildren(int offset) {
        final int childCount = getChildCount();

        for (int i = 0; i < childCount; i++) {
            final View child = getChildAt(i);

            if (mIsVertical) {
                child.offsetTopAndBottom(offset);
            } else {
                child.offsetLeftAndRight(offset);
            }
        }
    }

    private View moveSelection(View oldSelected, View newSelected, int delta, int start,
            int end) {
        final int selectedPosition = mSelectedPosition;

        final int oldSelectedStart = (mIsVertical ? oldSelected.getTop() : oldSelected.getLeft());
        final int oldSelectedEnd = (mIsVertical ? oldSelected.getBottom() : oldSelected.getRight());

        View selected = null;

        if (delta > 0) {
            /*
             * Case 1: Scrolling down.
             */

            /*
             *     Before           After
             *    |       |        |       |
             *    +-------+        +-------+
             *    |   A   |        |   A   |
             *    |   1   |   =>   +-------+
             *    +-------+        |   B   |
             *    |   B   |        |   2   |
             *    +-------+        +-------+
             *    |       |        |       |
             *
             *    Try to keep the top of the previously selected item where it was.
             *    oldSelected = A
             *    selected = B
             */

            // Put oldSelected (A) where it belongs
            oldSelected = makeAndAddView(selectedPosition - 1, oldSelectedStart, true, false);

            final int itemMargin = mItemMargin;

            // Now put the new selection (B) below that
            selected = makeAndAddView(selectedPosition, oldSelectedEnd + itemMargin, true, true);

            final int selectedStart = (mIsVertical ? selected.getTop() : selected.getLeft());
            final int selectedEnd = (mIsVertical ? selected.getBottom() : selected.getRight());

            // Some of the newly selected item extends below the bottom of the list
            if (selectedEnd > end) {
                // Find space available above the selection into which we can scroll upwards
                final int spaceBefore = selectedStart - start;

                // Find space required to bring the bottom of the selected item fully into view
                final int spaceAfter = selectedEnd - end;

                // Don't scroll more than half the size of the list
                final int halfSpace = (end - start) / 2;
                int offset = Math.min(spaceBefore, spaceAfter);
                offset = Math.min(offset, halfSpace);

                if (mIsVertical) {
                    oldSelected.offsetTopAndBottom(-offset);
                    selected.offsetTopAndBottom(-offset);
                } else {
                    oldSelected.offsetLeftAndRight(-offset);
                    selected.offsetLeftAndRight(-offset);
                }
            }

            // Fill in views before and after
            fillBefore(mSelectedPosition - 2, selectedStart - itemMargin);
            adjustViewsStartOrEnd();
            fillAfter(mSelectedPosition + 1, selectedEnd + itemMargin);
        } else if (delta < 0) {
            /*
             * Case 2: Scrolling up.
             */

            /*
             *     Before           After
             *    |       |        |       |
             *    +-------+        +-------+
             *    |   A   |        |   A   |
             *    +-------+   =>   |   1   |
             *    |   B   |        +-------+
             *    |   2   |        |   B   |
             *    +-------+        +-------+
             *    |       |        |       |
             *
             *    Try to keep the top of the item about to become selected where it was.
             *    newSelected = A
             *    olSelected = B
             */

            if (newSelected != null) {
                // Try to position the top of newSel (A) where it was before it was selected
                final int newSelectedStart = (mIsVertical ? newSelected.getTop() : newSelected.getLeft());
                selected = makeAndAddView(selectedPosition, newSelectedStart, true, true);
            } else {
                // If (A) was not on screen and so did not have a view, position
                // it above the oldSelected (B)
                selected = makeAndAddView(selectedPosition, oldSelectedStart, false, true);
            }

            final int selectedStart = (mIsVertical ? selected.getTop() : selected.getLeft());
            final int selectedEnd = (mIsVertical ? selected.getBottom() : selected.getRight());

            // Some of the newly selected item extends above the top of the list
            if (selectedStart < start) {
                // Find space required to bring the top of the selected item fully into view
                final int spaceBefore = start - selectedStart;

               // Find space available below the selection into which we can scroll downwards
                final int spaceAfter = end - selectedEnd;

                // Don't scroll more than half the height of the list
                final int halfSpace = (end - start) / 2;
                int offset = Math.min(spaceBefore, spaceAfter);
                offset = Math.min(offset, halfSpace);

                if (mIsVertical) {
                    selected.offsetTopAndBottom(offset);
                } else {
                    selected.offsetLeftAndRight(offset);
                }
            }

            // Fill in views above and below
            fillBeforeAndAfter(selected, selectedPosition);
        } else {
            /*
             * Case 3: Staying still
             */

            selected = makeAndAddView(selectedPosition, oldSelectedStart, true, true);

            final int selectedStart = (mIsVertical ? selected.getTop() : selected.getLeft());
            final int selectedEnd = (mIsVertical ? selected.getBottom() : selected.getRight());

            // We're staying still...
            if (oldSelectedStart < start) {
                // ... but the top of the old selection was off screen.
                // (This can happen if the data changes size out from under us)
                int newEnd = selectedEnd;
                if (newEnd < start + 20) {
                    // Not enough visible -- bring it onscreen
                    if (mIsVertical) {
                        selected.offsetTopAndBottom(start - selectedStart);
                    } else {
                        selected.offsetLeftAndRight(start - selectedStart);
                    }
                }
            }

            // Fill in views above and below
            fillBeforeAndAfter(selected, selectedPosition);
        }

        return selected;
    }

    void confirmCheckedPositionsById() {
        // Clear out the positional check states, we'll rebuild it below from IDs.
        mCheckStates.clear();

        for (int checkedIndex = 0; checkedIndex < mCheckedIdStates.size(); checkedIndex++) {
            final long id = mCheckedIdStates.keyAt(checkedIndex);
            final int lastPos = mCheckedIdStates.valueAt(checkedIndex);

            final long lastPosId = mAdapter.getItemId(lastPos);
            if (id != lastPosId) {
                // Look around to see if the ID is nearby. If not, uncheck it.
                final int start = Math.max(0, lastPos - CHECK_POSITION_SEARCH_DISTANCE);
                final int end = Math.min(lastPos + CHECK_POSITION_SEARCH_DISTANCE, mItemCount);
                boolean found = false;

                for (int searchPos = start; searchPos < end; searchPos++) {
                    final long searchId = mAdapter.getItemId(searchPos);
                    if (id == searchId) {
                        found = true;
                        mCheckStates.put(searchPos, true);
                        mCheckedIdStates.setValueAt(checkedIndex, searchPos);
                        break;
                    }
                }

                if (!found) {
                    mCheckedIdStates.delete(id);
                    checkedIndex--;
                    mCheckedItemCount--;
                }
            } else {
                mCheckStates.put(lastPos, true);
            }
        }
    }

    private void handleDataChanged() {
        if (mChoiceMode.compareTo(ChoiceMode.NONE) != 0 && mAdapter != null && mAdapter.hasStableIds()) {
            confirmCheckedPositionsById();
        }

        mRecycler.clearTransientStateViews();

        final int itemCount = mItemCount;
        if (itemCount > 0) {
            int newPos;
            int selectablePos;

            // Find the row we are supposed to sync to
            if (mNeedSync) {
                // Update this first, since setNextSelectedPositionInt inspects it
                mNeedSync = false;
                mPendingSync = null;

                switch (mSyncMode) {
                case SYNC_SELECTED_POSITION:
                    if (isInTouchMode()) {
                        // We saved our state when not in touch mode. (We know this because
                        // mSyncMode is SYNC_SELECTED_POSITION.) Now we are trying to
                        // restore in touch mode. Just leave mSyncPosition as it is (possibly
                        // adjusting if the available range changed) and return.
                        mLayoutMode = LAYOUT_SYNC;
                        mSyncPosition = Math.min(Math.max(0, mSyncPosition), itemCount - 1);

                        return;
                    } else {
                        // See if we can find a position in the new data with the same
                        // id as the old selection. This will change mSyncPosition.
                        newPos = findSyncPosition();
                        if (newPos >= 0) {
                            // Found it. Now verify that new selection is still selectable
                            selectablePos = lookForSelectablePosition(newPos, true);
                            if (selectablePos == newPos) {
                                // Same row id is selected
                                mSyncPosition = newPos;

                                if (mSyncHeight == getHeight()) {
                                    // If we are at the same height as when we saved state, try
                                    // to restore the scroll position too.
                                    mLayoutMode = LAYOUT_SYNC;
                                } else {
                                    // We are not the same height as when the selection was saved, so
                                    // don't try to restore the exact position
                                    mLayoutMode = LAYOUT_SET_SELECTION;
                                }

                                // Restore selection
                                setNextSelectedPositionInt(newPos);
                                return;
                            }
                        }
                    }
                    break;

                case SYNC_FIRST_POSITION:
                    // Leave mSyncPosition as it is -- just pin to available range
                    mLayoutMode = LAYOUT_SYNC;
                    mSyncPosition = Math.min(Math.max(0, mSyncPosition), itemCount - 1);

                    return;
                }
            }

            if (!isInTouchMode()) {
                // We couldn't find matching data -- try to use the same position
                newPos = getSelectedItemPosition();

                // Pin position to the available range
                if (newPos >= itemCount) {
                    newPos = itemCount - 1;
                }
                if (newPos < 0) {
                    newPos = 0;
                }

                // Make sure we select something selectable -- first look down
                selectablePos = lookForSelectablePosition(newPos, true);

                if (selectablePos >= 0) {
                    setNextSelectedPositionInt(selectablePos);
                    return;
                } else {
                    // Looking down didn't work -- try looking up
                    selectablePos = lookForSelectablePosition(newPos, false);
                    if (selectablePos >= 0) {
                        setNextSelectedPositionInt(selectablePos);
                        return;
                    }
                }
            } else {
                // We already know where we want to resurrect the selection
                if (mResurrectToPosition >= 0) {
                    return;
                }
            }
        }

        // Nothing is selected. Give up and reset everything.
        mLayoutMode = LAYOUT_FORCE_TOP;
        mSelectedPosition = INVALID_POSITION;
        mSelectedRowId = INVALID_ROW_ID;
        mNextSelectedPosition = INVALID_POSITION;
        mNextSelectedRowId = INVALID_ROW_ID;
        mNeedSync = false;
        mPendingSync = null;
        mSelectorPosition = INVALID_POSITION;

        checkSelectionChanged();

    }

    private int reconcileSelectedPosition() {
        int position = mSelectedPosition;
        if (position < 0) {
            position = mResurrectToPosition;
        }

        position = Math.max(0, position);
        position = Math.min(position, mItemCount - 1);

        return position;
    }

    boolean resurrectSelection() {
        final int childCount = getChildCount();
        if (childCount <= 0) {
            return false;
        }

        int selectedStart = 0;
        int selectedPosition;

        final int start = (mIsVertical ? getPaddingTop() : getPaddingLeft());
        final int end =
                (mIsVertical ? getHeight() - getPaddingBottom() : getWidth() - getPaddingRight());

        final int firstPosition = mFirstPosition;
        final int toPosition = mResurrectToPosition;
        boolean down = true;

        if (toPosition >= firstPosition && toPosition < firstPosition + childCount) {
            selectedPosition = toPosition;

            final View selected = getChildAt(selectedPosition - mFirstPosition);
            selectedStart = (mIsVertical ? selected.getTop() : selected.getLeft());
        } else if (toPosition < firstPosition) {
            // Default to selecting whatever is first
            selectedPosition = firstPosition;

            for (int i = 0; i < childCount; i++) {
                final View child = getChildAt(i);
                final int childStart = (mIsVertical ? child.getTop() : child.getLeft());

                if (i == 0) {
                    // Remember the position of the first item
                    selectedStart = childStart;
                }

                if (childStart >= start) {
                    // Found a view whose top is fully visible
                    selectedPosition = firstPosition + i;
                    selectedStart = childStart;
                    break;
                }
            }
        } else {
            selectedPosition = firstPosition + childCount - 1;
            down = false;

            for (int i = childCount - 1; i >= 0; i--) {
                final View child = getChildAt(i);
                final int childStart = (mIsVertical ? child.getTop() : child.getLeft());
                final int childEnd = (mIsVertical ? child.getBottom() : child.getRight());

                if (i == childCount - 1) {
                    selectedStart = childStart;
                }

                if (childEnd <= end) {
                    selectedPosition = firstPosition + i;
                    selectedStart = childStart;
                    break;
                }
            }
        }

        mResurrectToPosition = INVALID_POSITION;
        mTouchMode = TOUCH_MODE_REST;
        reportScrollStateChange(OnScrollListener.SCROLL_STATE_IDLE);

        mSpecificStart = selectedStart;

        selectedPosition = lookForSelectablePosition(selectedPosition, down);
        if (selectedPosition >= firstPosition && selectedPosition <= getLastVisiblePosition()) {
            mLayoutMode = LAYOUT_SPECIFIC;
            updateSelectorState();
            setSelectionInt(selectedPosition);
            invokeOnItemScrollListener();
        } else {
            selectedPosition = INVALID_POSITION;
        }

        return selectedPosition >= 0;
    }

    private int getChildWidthMeasureSpec(LayoutParams lp) {
        if (!mIsVertical && lp.width == LayoutParams.WRAP_CONTENT) {
            return MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        } else if (mIsVertical) {
            final int maxWidth = getWidth() - getPaddingLeft() - getPaddingRight();
            return MeasureSpec.makeMeasureSpec(maxWidth, MeasureSpec.EXACTLY);
        } else {
            return MeasureSpec.makeMeasureSpec(lp.width, MeasureSpec.EXACTLY);
        }
    }

    private int getChildHeightMeasureSpec(LayoutParams lp) {
        if (mIsVertical && lp.height == LayoutParams.WRAP_CONTENT) {
            return MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        } else if (!mIsVertical) {
            final int maxHeight = getHeight() - getPaddingTop() - getPaddingBottom();
            return MeasureSpec.makeMeasureSpec(maxHeight, MeasureSpec.EXACTLY);
        } else {
            return MeasureSpec.makeMeasureSpec(lp.height, MeasureSpec.EXACTLY);
        }
    }

    private void measureChild(View child, LayoutParams lp) {
        final int widthSpec = getChildWidthMeasureSpec(lp);
        final int heightSpec = getChildHeightMeasureSpec(lp);
        child.measure(widthSpec, heightSpec);
    }

    private void measureScrapChild(View scrapChild, int position, int secondaryMeasureSpec) {
        LayoutParams lp = (LayoutParams) scrapChild.getLayoutParams();
        if (lp == null) {
            lp = generateDefaultLayoutParams();
            scrapChild.setLayoutParams(lp);
        }

        lp.viewType = mAdapter.getItemViewType(position);
        lp.forceAdd = true;

        final int widthMeasureSpec;
        final int heightMeasureSpec;
        if (mIsVertical) {
            widthMeasureSpec = secondaryMeasureSpec;
            heightMeasureSpec = getChildHeightMeasureSpec(lp);
        } else {
            widthMeasureSpec = getChildWidthMeasureSpec(lp);
            heightMeasureSpec = secondaryMeasureSpec;
        }

        scrapChild.measure(widthMeasureSpec, heightMeasureSpec);
    }

    /**
     * Measures the height of the given range of children (inclusive) and
     * returns the height with this TwoWayView's padding and item margin heights
     * included. If maxHeight is provided, the measuring will stop when the
     * current height reaches maxHeight.
     *
     * @param widthMeasureSpec The width measure spec to be given to a child's
     *            {@link View#measure(int, int)}.
     * @param startPosition The position of the first child to be shown.
     * @param endPosition The (inclusive) position of the last child to be
     *            shown. Specify {@link #NO_POSITION} if the last child should be
     *            the last available child from the adapter.
     * @param maxHeight The maximum height that will be returned (if all the
     *            children don't fit in this value, this value will be
     *            returned).
     * @param disallowPartialChildPosition In general, whether the returned
     *            height should only contain entire children. This is more
     *            powerful--it is the first inclusive position at which partial
     *            children will not be allowed. Example: it looks nice to have
     *            at least 3 completely visible children, and in portrait this
     *            will most likely fit; but in landscape there could be times
     *            when even 2 children can not be completely shown, so a value
     *            of 2 (remember, inclusive) would be good (assuming
     *            startPosition is 0).
     * @return The height of this TwoWayView with the given children.
     */
    private int measureHeightOfChildren(int widthMeasureSpec, int startPosition, int endPosition,
            final int maxHeight, int disallowPartialChildPosition) {

        final int paddingTop = getPaddingTop();
        final int paddingBottom = getPaddingBottom();

        final ListAdapter adapter = mAdapter;
        if (adapter == null) {
            return paddingTop + paddingBottom;
        }

        // Include the padding of the list
        int returnedHeight = paddingTop + paddingBottom;
        final int itemMargin = mItemMargin;

        // The previous height value that was less than maxHeight and contained
        // no partial children
        int prevHeightWithoutPartialChild = 0;
        int i;
        View child;

        // mItemCount - 1 since endPosition parameter is inclusive
        endPosition = (endPosition == NO_POSITION) ? adapter.getCount() - 1 : endPosition;
        final RecycleBin recycleBin = mRecycler;
        final boolean shouldRecycle = recycleOnMeasure();
        final boolean[] isScrap = mIsScrap;

        for (i = startPosition; i <= endPosition; ++i) {
            child = obtainView(i, isScrap);

            measureScrapChild(child, i, widthMeasureSpec);

            if (i > 0) {
                // Count the item margin for all but one child
                returnedHeight += itemMargin;
            }

            // Recycle the view before we possibly return from the method
            if (shouldRecycle) {
                recycleBin.addScrapView(child, -1);
            }

            returnedHeight += child.getMeasuredHeight();

            if (returnedHeight >= maxHeight) {
                // We went over, figure out which height to return.  If returnedHeight > maxHeight,
                // then the i'th position did not fit completely.
                return (disallowPartialChildPosition >= 0) // Disallowing is enabled (> -1)
                            && (i > disallowPartialChildPosition) // We've past the min pos
                            && (prevHeightWithoutPartialChild > 0) // We have a prev height
                            && (returnedHeight != maxHeight) // i'th child did not fit completely
                        ? prevHeightWithoutPartialChild
                        : maxHeight;
            }

            if ((disallowPartialChildPosition >= 0) && (i >= disallowPartialChildPosition)) {
                prevHeightWithoutPartialChild = returnedHeight;
            }
        }

        // At this point, we went through the range of children, and they each
        // completely fit, so return the returnedHeight
        return returnedHeight;
    }

    /**
     * Measures the width of the given range of children (inclusive) and
     * returns the width with this TwoWayView's padding and item margin widths
     * included. If maxWidth is provided, the measuring will stop when the
     * current width reaches maxWidth.
     *
     * @param heightMeasureSpec The height measure spec to be given to a child's
     *            {@link View#measure(int, int)}.
     * @param startPosition The position of the first child to be shown.
     * @param endPosition The (inclusive) position of the last child to be
     *            shown. Specify {@link #NO_POSITION} if the last child should be
     *            the last available child from the adapter.
     * @param maxWidth The maximum width that will be returned (if all the
     *            children don't fit in this value, this value will be
     *            returned).
     * @param disallowPartialChildPosition In general, whether the returned
     *            width should only contain entire children. This is more
     *            powerful--it is the first inclusive position at which partial
     *            children will not be allowed. Example: it looks nice to have
     *            at least 3 completely visible children, and in portrait this
     *            will most likely fit; but in landscape there could be times
     *            when even 2 children can not be completely shown, so a value
     *            of 2 (remember, inclusive) would be good (assuming
     *            startPosition is 0).
     * @return The width of this TwoWayView with the given children.
     */
    private int measureWidthOfChildren(int heightMeasureSpec, int startPosition, int endPosition,
            final int maxWidth, int disallowPartialChildPosition) {

        final int paddingLeft = getPaddingLeft();
        final int paddingRight = getPaddingRight();

        final ListAdapter adapter = mAdapter;
        if (adapter == null) {
            return paddingLeft + paddingRight;
        }

        // Include the padding of the list
        int returnedWidth = paddingLeft + paddingRight;
        final int itemMargin = mItemMargin;

        // The previous height value that was less than maxHeight and contained
        // no partial children
        int prevWidthWithoutPartialChild = 0;
        int i;
        View child;

        // mItemCount - 1 since endPosition parameter is inclusive
        endPosition = (endPosition == NO_POSITION) ? adapter.getCount() - 1 : endPosition;
        final RecycleBin recycleBin = mRecycler;
        final boolean shouldRecycle = recycleOnMeasure();
        final boolean[] isScrap = mIsScrap;

        for (i = startPosition; i <= endPosition; ++i) {
            child = obtainView(i, isScrap);

            measureScrapChild(child, i, heightMeasureSpec);

            if (i > 0) {
                // Count the item margin for all but one child
                returnedWidth += itemMargin;
            }

            // Recycle the view before we possibly return from the method
            if (shouldRecycle) {
                recycleBin.addScrapView(child, -1);
            }

            returnedWidth += child.getMeasuredHeight();

            if (returnedWidth >= maxWidth) {
                // We went over, figure out which width to return.  If returnedWidth > maxWidth,
                // then the i'th position did not fit completely.
                return (disallowPartialChildPosition >= 0) // Disallowing is enabled (> -1)
                            && (i > disallowPartialChildPosition) // We've past the min pos
                            && (prevWidthWithoutPartialChild > 0) // We have a prev width
                            && (returnedWidth != maxWidth) // i'th child did not fit completely
                        ? prevWidthWithoutPartialChild
                        : maxWidth;
            }

            if ((disallowPartialChildPosition >= 0) && (i >= disallowPartialChildPosition)) {
                prevWidthWithoutPartialChild = returnedWidth;
            }
        }

        // At this point, we went through the range of children, and they each
        // completely fit, so return the returnedWidth
        return returnedWidth;
    }

    private View makeAndAddView(int position, int offset, boolean flow, boolean selected) {
        final int top;
        final int left;

        if (mIsVertical) {
            top = offset;
            left = getPaddingLeft();
        } else {
            top = getPaddingTop();
            left = offset;
        }

        if (!mDataChanged) {
            // Try to use an existing view for this position
            final View activeChild = mRecycler.getActiveView(position);
            if (activeChild != null) {
                // Found it -- we're using an existing child
                // This just needs to be positioned
                setupChild(activeChild, position, top, left, flow, selected, true);

                return activeChild;
            }
        }

        // Make a new view for this position, or convert an unused view if possible
        final View child = obtainView(position, mIsScrap);

        // This needs to be positioned and measured
        setupChild(child, position, top, left, flow, selected, mIsScrap[0]);

        return child;
    }

    @TargetApi(11)
    private void setupChild(View child, int position, int top, int left,
            boolean flow, boolean selected, boolean recycled) {
        final boolean isSelected = selected && shouldShowSelector();
        final boolean updateChildSelected = isSelected != child.isSelected();
        final int touchMode = mTouchMode;

        final boolean isPressed = touchMode > TOUCH_MODE_DOWN && touchMode < TOUCH_MODE_DRAGGING &&
                mMotionPosition == position;

        final boolean updateChildPressed = isPressed != child.isPressed();
        final boolean needToMeasure = !recycled || updateChildSelected || child.isLayoutRequested();

        // Respect layout params that are already in the view. Otherwise make some up...
        LayoutParams lp = (LayoutParams) child.getLayoutParams();
        if (lp == null) {
            lp = generateDefaultLayoutParams();
        }

        lp.viewType = mAdapter.getItemViewType(position);

        if (recycled && !lp.forceAdd) {
            attachViewToParent(child, (flow ? -1 : 0), lp);
        } else {
            lp.forceAdd = false;
            addViewInLayout(child, (flow ? -1 : 0), lp, true);
        }

        if (updateChildSelected) {
            child.setSelected(isSelected);
        }

        if (updateChildPressed) {
            child.setPressed(isPressed);
        }

        if (mChoiceMode.compareTo(ChoiceMode.NONE) != 0 && mCheckStates != null) {
            if (child instanceof Checkable) {
                ((Checkable) child).setChecked(mCheckStates.get(position));
            } else if (getContext().getApplicationInfo().targetSdkVersion
                    >= Build.VERSION_CODES.HONEYCOMB) {
                child.setActivated(mCheckStates.get(position));
            }
        }

        if (needToMeasure) {
            measureChild(child, lp);
        } else {
            cleanupLayoutState(child);
        }

        final int w = child.getMeasuredWidth();
        final int h = child.getMeasuredHeight();

        final int childTop = (mIsVertical && !flow ? top - h : top);
        final int childLeft = (!mIsVertical && !flow ? left - w : left);

        if (needToMeasure) {
            final int childRight = childLeft + w;
            final int childBottom = childTop + h;

            child.layout(childLeft, childTop, childRight, childBottom);
        } else {
            child.offsetLeftAndRight(childLeft - child.getLeft());
            child.offsetTopAndBottom(childTop - child.getTop());
        }
    }

    void fillGap(boolean down) {
        final int childCount = getChildCount();

        if (down) {
            final int paddingStart = (mIsVertical ? getPaddingTop() : getPaddingLeft());

            final int lastEnd;
            if (mIsVertical) {
                lastEnd = getChildAt(childCount - 1).getBottom();
            } else {
                lastEnd = getChildAt(childCount - 1).getRight();
            }

            final int offset = (childCount > 0 ? lastEnd + mItemMargin : paddingStart);
            fillAfter(mFirstPosition + childCount, offset);
            correctTooHigh(getChildCount());
        } else {
            final int end;
            final int firstStart;

            if (mIsVertical) {
                end = getHeight() - getPaddingBottom();
                firstStart = getChildAt(0).getTop();
            } else {
                end = getWidth() - getPaddingRight();
                firstStart = getChildAt(0).getLeft();
            }

            final int offset = (childCount > 0 ? firstStart - mItemMargin : end);
            fillBefore(mFirstPosition - 1, offset);
            correctTooLow(getChildCount());
        }
    }

    private View fillBefore(int pos, int nextOffset) {
        View selectedView = null;

        final int start = (mIsVertical ? getPaddingTop() : getPaddingLeft());

        while (nextOffset > start && pos >= 0) {
            boolean isSelected = (pos == mSelectedPosition);
            View child = makeAndAddView(pos, nextOffset, false, isSelected);

            if (mIsVertical) {
                nextOffset = child.getTop() - mItemMargin;
            } else {
                nextOffset = child.getLeft() - mItemMargin;
            }

            if (isSelected) {
                selectedView = child;
            }

            pos--;
        }

        mFirstPosition = pos + 1;

        return selectedView;
    }

    private View fillAfter(int pos, int nextOffset) {
        View selectedView = null;

        final int end =
                (mIsVertical ? getHeight() - getPaddingBottom() : getWidth() - getPaddingRight());

        while (nextOffset < end && pos < mItemCount) {
            boolean selected = (pos == mSelectedPosition);

            View child = makeAndAddView(pos, nextOffset, true, selected);

            if (mIsVertical) {
                nextOffset = child.getBottom() + mItemMargin;
            } else {
                nextOffset = child.getRight() + mItemMargin;
            }

            if (selected) {
                selectedView = child;
            }

            pos++;
        }

        return selectedView;
    }

    private View fillSpecific(int position, int top) {
        final boolean tempIsSelected = (position == mSelectedPosition);
        View temp = makeAndAddView(position, top, true, tempIsSelected);

        // Possibly changed again in fillBefore if we add rows above this one.
        mFirstPosition = position;

        final int itemMargin = mItemMargin;

        final int offsetBefore;
        if (mIsVertical) {
            offsetBefore = temp.getTop() - itemMargin;
        } else {
            offsetBefore = temp.getLeft() - itemMargin;
        }
        final View before = fillBefore(position - 1, offsetBefore);

        // This will correct for the top of the first view not touching the top of the list
        adjustViewsStartOrEnd();

        final int offsetAfter;
        if (mIsVertical) {
            offsetAfter = temp.getBottom() + itemMargin;
        } else {
            offsetAfter = temp.getRight() + itemMargin;
        }
        final View after = fillAfter(position + 1, offsetAfter);

        final int childCount = getChildCount();
        if (childCount > 0) {
            correctTooHigh(childCount);
        }

        if (tempIsSelected) {
            return temp;
        } else if (before != null) {
            return before;
        } else {
            return after;
        }
    }

    private View fillFromOffset(int nextOffset) {
        mFirstPosition = Math.min(mFirstPosition, mSelectedPosition);
        mFirstPosition = Math.min(mFirstPosition, mItemCount - 1);

        if (mFirstPosition < 0) {
            mFirstPosition = 0;
        }

        return fillAfter(mFirstPosition, nextOffset);
    }

    private View fillFromMiddle(int start, int end) {
        final int size = end - start;
        int position = reconcileSelectedPosition();

        View selected = makeAndAddView(position, start, true, true);
        mFirstPosition = position;

        if (mIsVertical) {
            int selectedHeight = selected.getMeasuredHeight();
            if (selectedHeight <= size) {
                selected.offsetTopAndBottom((size - selectedHeight) / 2);
            }
        } else {
            int selectedWidth = selected.getMeasuredWidth();
            if (selectedWidth <= size) {
                selected.offsetLeftAndRight((size - selectedWidth) / 2);
            }
        }

        fillBeforeAndAfter(selected, position);
        correctTooHigh(getChildCount());

        return selected;
    }

    private void fillBeforeAndAfter(View selected, int position) {
        final int itemMargin = mItemMargin;

        final int offsetBefore;
        if (mIsVertical) {
            offsetBefore = selected.getTop() - itemMargin;
        } else {
            offsetBefore = selected.getLeft() - itemMargin;
        }

        fillBefore(position - 1, offsetBefore);

        adjustViewsStartOrEnd();

        final int offsetAfter;
        if (mIsVertical) {
            offsetAfter = selected.getBottom() + itemMargin;
        } else {
            offsetAfter = selected.getRight() + itemMargin;
        }

        fillAfter(position + 1, offsetAfter);
    }

    private View fillFromSelection(int selectedTop, int start, int end) {
        final int selectedPosition = mSelectedPosition;
        View selected;

        selected = makeAndAddView(selectedPosition, selectedTop, true, true);

        final int selectedStart = (mIsVertical ? selected.getTop() : selected.getLeft());
        final int selectedEnd = (mIsVertical ? selected.getBottom() : selected.getRight());

        // Some of the newly selected item extends below the bottom of the list
        if (selectedEnd > end) {
            // Find space available above the selection into which we can scroll
            // upwards
            final int spaceAbove = selectedStart - start;

            // Find space required to bring the bottom of the selected item
            // fully into view
            final int spaceBelow = selectedEnd - end;

            final int offset = Math.min(spaceAbove, spaceBelow);

            // Now offset the selected item to get it into view
            selected.offsetTopAndBottom(-offset);
        } else if (selectedStart < start) {
            // Find space required to bring the top of the selected item fully
            // into view
            final int spaceAbove = start - selectedStart;

            // Find space available below the selection into which we can scroll
            // downwards
            final int spaceBelow = end - selectedEnd;

            final int offset = Math.min(spaceAbove, spaceBelow);

            // Offset the selected item to get it into view
            selected.offsetTopAndBottom(offset);
        }

        // Fill in views above and below
        fillBeforeAndAfter(selected, selectedPosition);
        correctTooHigh(getChildCount());

        return selected;
    }

    private void correctTooHigh(int childCount) {
        // First see if the last item is visible. If it is not, it is OK for the
        // top of the list to be pushed up.
        int lastPosition = mFirstPosition + childCount - 1;
        if (lastPosition != mItemCount - 1 || childCount == 0) {
            return;
        }

        // Get the last child ...
        final View lastChild = getChildAt(childCount - 1);

        // ... and its end edge
        final int lastEnd;
        if (mIsVertical) {
            lastEnd = lastChild.getBottom();
        } else {
            lastEnd = lastChild.getRight();
        }

        // This is bottom of our drawable area
        final int start = (mIsVertical ? getPaddingTop() : getPaddingLeft());
        final int end =
                (mIsVertical ? getHeight() - getPaddingBottom() : getWidth() - getPaddingRight());

        // This is how far the end edge of the last view is from the end of the
        // drawable area
        int endOffset = end - lastEnd;

        View firstChild = getChildAt(0);
        int firstStart = (mIsVertical ? firstChild.getTop() : firstChild.getLeft());

        // Make sure we are 1) Too high, and 2) Either there are more rows above the
        // first row or the first row is scrolled off the top of the drawable area
        if (endOffset > 0 && (mFirstPosition > 0 || firstStart < start))  {
            if (mFirstPosition == 0) {
                // Don't pull the top too far down
                endOffset = Math.min(endOffset, start - firstStart);
            }

            // Move everything down
            offsetChildren(endOffset);

            if (mFirstPosition > 0) {
                firstStart = (mIsVertical ? firstChild.getTop() : firstChild.getLeft());

                // Fill the gap that was opened above mFirstPosition with more rows, if
                // possible
                fillBefore(mFirstPosition - 1, firstStart - mItemMargin);

                // Close up the remaining gap
                adjustViewsStartOrEnd();
            }
        }
    }

    private void correctTooLow(int childCount) {
        // First see if the first item is visible. If it is not, it is OK for the
        // bottom of the list to be pushed down.
        if (mFirstPosition != 0 || childCount == 0) {
            return;
        }

        final View first = getChildAt(0);
        final int firstStart = (mIsVertical ? first.getTop() : first.getLeft());

        final int start = (mIsVertical ? getPaddingTop() : getPaddingLeft());

        final int end;
        if (mIsVertical) {
            end = getHeight() - getPaddingBottom();
        } else {
            end = getWidth() - getPaddingRight();
        }

        // This is how far the start edge of the first view is from the start of the
        // drawable area
        int startOffset = firstStart - start;

        View last = getChildAt(childCount - 1);
        int lastEnd = (mIsVertical ? last.getBottom() : last.getRight());

        int lastPosition = mFirstPosition + childCount - 1;

        // Make sure we are 1) Too low, and 2) Either there are more columns/rows below the
        // last column/row or the last column/row is scrolled off the end of the
        // drawable area
        if (startOffset > 0) {
            if (lastPosition < mItemCount - 1 || lastEnd > end)  {
                if (lastPosition == mItemCount - 1) {
                    // Don't pull the bottom too far up
                    startOffset = Math.min(startOffset, lastEnd - end);
                }

                // Move everything up
                offsetChildren(-startOffset);

                if (lastPosition < mItemCount - 1) {
                    lastEnd = (mIsVertical ? last.getBottom() : last.getRight());

                    // Fill the gap that was opened below the last position with more rows, if
                    // possible
                    fillAfter(lastPosition + 1, lastEnd + mItemMargin);

                    // Close up the remaining gap
                    adjustViewsStartOrEnd();
                }
            } else if (lastPosition == mItemCount - 1) {
                adjustViewsStartOrEnd();
            }
        }
    }

    private void adjustViewsStartOrEnd() {
        if (getChildCount() == 0) {
            return;
        }

        final View firstChild = getChildAt(0);

        int delta;
        if (mIsVertical) {
            delta = firstChild.getTop() - getPaddingTop() - mItemMargin;
        } else {
            delta = firstChild.getLeft() - getPaddingLeft() - mItemMargin;
        }

        if (delta < 0) {
            // We only are looking to see if we are too low, not too high
            delta = 0;
        }

        if (delta != 0) {
            offsetChildren(-delta);
        }
    }

    @TargetApi(14)
    private SparseBooleanArray cloneCheckStates() {
        if (mCheckStates == null) {
            return null;
        }

        SparseBooleanArray checkedStates;

        if (Build.VERSION.SDK_INT >= 14) {
            checkedStates = mCheckStates.clone();
        } else {
            checkedStates = new SparseBooleanArray();

            for (int i = 0; i < mCheckStates.size(); i++) {
                checkedStates.put(mCheckStates.keyAt(i), mCheckStates.valueAt(i));
            }
        }

        return checkedStates;
    }

    private int findSyncPosition() {
        int itemCount = mItemCount;

        if (itemCount == 0) {
            return INVALID_POSITION;
        }

        final long idToMatch = mSyncRowId;

        // If there isn't a selection don't hunt for it
        if (idToMatch == INVALID_ROW_ID) {
            return INVALID_POSITION;
        }

        // Pin seed to reasonable values
        int seed = mSyncPosition;
        seed = Math.max(0, seed);
        seed = Math.min(itemCount - 1, seed);

        long endTime = SystemClock.uptimeMillis() + SYNC_MAX_DURATION_MILLIS;

        long rowId;

        // first position scanned so far
        int first = seed;

        // last position scanned so far
        int last = seed;

        // True if we should move down on the next iteration
        boolean next = false;

        // True when we have looked at the first item in the data
        boolean hitFirst;

        // True when we have looked at the last item in the data
        boolean hitLast;

        // Get the item ID locally (instead of getItemIdAtPosition), so
        // we need the adapter
        final ListAdapter adapter = mAdapter;
        if (adapter == null) {
            return INVALID_POSITION;
        }

        while (SystemClock.uptimeMillis() <= endTime) {
            rowId = adapter.getItemId(seed);
            if (rowId == idToMatch) {
                // Found it!
                return seed;
            }

            hitLast = (last == itemCount - 1);
            hitFirst = (first == 0);

            if (hitLast && hitFirst) {
                // Looked at everything
                break;
            }

            if (hitFirst || (next && !hitLast)) {
                // Either we hit the top, or we are trying to move down
                last++;
                seed = last;

                // Try going up next time
                next = false;
            } else if (hitLast || (!next && !hitFirst)) {
                // Either we hit the bottom, or we are trying to move up
                first--;
                seed = first;

                // Try going down next time
                next = true;
            }
        }

        return INVALID_POSITION;
    }

    View obtainView(int position, boolean[] isScrap) {
        isScrap[0] = false;
        View scrapView;

        scrapView = mRecycler.getTransientStateView(position);
        if (scrapView != null) {
            return scrapView;
        }

        scrapView = mRecycler.getScrapView(position);

        final View child;
        if (scrapView != null) {
            child = mAdapter.getView(position, scrapView, this);

            if (child != scrapView) {
                mRecycler.addScrapView(scrapView, position);
            } else {
                isScrap[0] = true;
            }
        } else {
            child = mAdapter.getView(position, null, this);
        }

        if (mHasStableIds) {
            LayoutParams lp = (LayoutParams) child.getLayoutParams();

            if (lp == null) {
                lp = generateDefaultLayoutParams();
            } else if (!checkLayoutParams(lp)) {
                lp = generateLayoutParams(lp);
            }

            lp.id = mAdapter.getItemId(position);

            child.setLayoutParams(lp);
        }

        return child;
    }

    void resetState() {
        removeAllViewsInLayout();

        mSelectedStart = 0;
        mFirstPosition = 0;
        mDataChanged = false;
        mNeedSync = false;
        mPendingSync = null;
        mOldSelectedPosition = INVALID_POSITION;
        mOldSelectedRowId = INVALID_ROW_ID;

        mOverScroll = 0;

        setSelectedPositionInt(INVALID_POSITION);
        setNextSelectedPositionInt(INVALID_POSITION);

        mSelectorPosition = INVALID_POSITION;
        mSelectorRect.setEmpty();

        invalidate();
    }

    private void rememberSyncState() {
        if (getChildCount() == 0) {
            return;
        }

        mNeedSync = true;

        if (mSelectedPosition >= 0) {
            View child = getChildAt(mSelectedPosition - mFirstPosition);

            mSyncRowId = mNextSelectedRowId;
            mSyncPosition = mNextSelectedPosition;

            if (child != null) {
                mSpecificStart = (mIsVertical ? child.getTop() : child.getLeft());
            }

            mSyncMode = SYNC_SELECTED_POSITION;
        } else {
            // Sync the based on the offset of the first view
            View child = getChildAt(0);
            ListAdapter adapter = getAdapter();

            if (mFirstPosition >= 0 && mFirstPosition < adapter.getCount()) {
                mSyncRowId = adapter.getItemId(mFirstPosition);
            } else {
                mSyncRowId = NO_ID;
            }

            mSyncPosition = mFirstPosition;

            if (child != null) {
                mSpecificStart = child.getTop();
            }

            mSyncMode = SYNC_FIRST_POSITION;
        }
    }

    private ContextMenuInfo createContextMenuInfo(View view, int position, long id) {
        return new AdapterContextMenuInfo(view, position, id);
    }

    @TargetApi(11)
    private void updateOnScreenCheckedViews() {
        final int firstPos = mFirstPosition;
        final int count = getChildCount();

        final boolean useActivated = getContext().getApplicationInfo().targetSdkVersion
                >= Build.VERSION_CODES.HONEYCOMB;

        for (int i = 0; i < count; i++) {
            final View child = getChildAt(i);
            final int position = firstPos + i;

            if (child instanceof Checkable) {
                ((Checkable) child).setChecked(mCheckStates.get(position));
            } else if (useActivated) {
                child.setActivated(mCheckStates.get(position));
            }
        }
    }

    @Override
    public boolean performItemClick(View view, int position, long id) {
        boolean checkedStateChanged = false;

        if (mChoiceMode.compareTo(ChoiceMode.MULTIPLE) == 0) {
            boolean checked = !mCheckStates.get(position, false);
            mCheckStates.put(position, checked);

            if (mCheckedIdStates != null && mAdapter.hasStableIds()) {
                if (checked) {
                    mCheckedIdStates.put(mAdapter.getItemId(position), position);
                } else {
                    mCheckedIdStates.delete(mAdapter.getItemId(position));
                }
            }

            if (checked) {
                mCheckedItemCount++;
            } else {
                mCheckedItemCount--;
            }

            checkedStateChanged = true;
        } else if (mChoiceMode.compareTo(ChoiceMode.SINGLE) == 0) {
            boolean checked = !mCheckStates.get(position, false);
            if (checked) {
                mCheckStates.clear();
                mCheckStates.put(position, true);

                if (mCheckedIdStates != null && mAdapter.hasStableIds()) {
                    mCheckedIdStates.clear();
                    mCheckedIdStates.put(mAdapter.getItemId(position), position);
                }

                mCheckedItemCount = 1;
            } else if (mCheckStates.size() == 0 || !mCheckStates.valueAt(0)) {
                mCheckedItemCount = 0;
            }

            checkedStateChanged = true;
        }

        if (checkedStateChanged) {
            updateOnScreenCheckedViews();
        }

        return super.performItemClick(view, position, id);
    }

    private boolean performLongPress(final View child,
            final int longPressPosition, final long longPressId) {
        // CHOICE_MODE_MULTIPLE_MODAL takes over long press.
        boolean handled = false;

        OnItemLongClickListener listener = getOnItemLongClickListener();
        if (listener != null) {
            handled = listener.onItemLongClick(TwoWayView.this, child,
                    longPressPosition, longPressId);
        }

        if (!handled) {
            mContextMenuInfo = createContextMenuInfo(child, longPressPosition, longPressId);
            handled = super.showContextMenuForChild(TwoWayView.this);
        }

        if (handled) {
            performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);
        }

        return handled;
    }

    @Override
    protected LayoutParams generateDefaultLayoutParams() {
        if (mIsVertical) {
            return new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
        } else {
            return new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.MATCH_PARENT);
        }
    }

    @Override
    protected LayoutParams generateLayoutParams(ViewGroup.LayoutParams lp) {
        return new LayoutParams(lp);
    }

    @Override
    protected boolean checkLayoutParams(ViewGroup.LayoutParams lp) {
        return lp instanceof LayoutParams;
    }

    @Override
    public ViewGroup.LayoutParams generateLayoutParams(AttributeSet attrs) {
        return new LayoutParams(getContext(), attrs);
    }

    @Override
    protected ContextMenuInfo getContextMenuInfo() {
        return mContextMenuInfo;
    }

    @Override
    public Parcelable onSaveInstanceState() {
        Parcelable superState = super.onSaveInstanceState();
        SavedState ss = new SavedState(superState);

        if (mPendingSync != null) {
            ss.selectedId = mPendingSync.selectedId;
            ss.firstId = mPendingSync.firstId;
            ss.viewStart = mPendingSync.viewStart;
            ss.position = mPendingSync.position;
            ss.height = mPendingSync.height;

            return ss;
        }

        boolean haveChildren = (getChildCount() > 0 && mItemCount > 0);
        long selectedId = getSelectedItemId();
        ss.selectedId = selectedId;
        ss.height = getHeight();

        if (selectedId >= 0) {
            ss.viewStart = mSelectedStart;
            ss.position = getSelectedItemPosition();
            ss.firstId = INVALID_POSITION;
        } else if (haveChildren && mFirstPosition > 0) {
            // Remember the position of the first child.
            // We only do this if we are not currently at the top of
            // the list, for two reasons:
            //
            // (1) The list may be in the process of becoming empty, in
            // which case mItemCount may not be 0, but if we try to
            // ask for any information about position 0 we will crash.
            //
            // (2) Being "at the top" seems like a special case, anyway,
            // and the user wouldn't expect to end up somewhere else when
            // they revisit the list even if its content has changed.

            View child = getChildAt(0);
            ss.viewStart = (mIsVertical ? child.getTop() : child.getLeft());

            int firstPos = mFirstPosition;
            if (firstPos >= mItemCount) {
                firstPos = mItemCount - 1;
            }

            ss.position = firstPos;
            ss.firstId = mAdapter.getItemId(firstPos);
        } else {
            ss.viewStart = 0;
            ss.firstId = INVALID_POSITION;
            ss.position = 0;
        }

        if (mCheckStates != null) {
            ss.checkState = cloneCheckStates();
        }

        if (mCheckedIdStates != null) {
            final LongSparseArray<Integer> idState = new LongSparseArray<Integer>();

            final int count = mCheckedIdStates.size();
            for (int i = 0; i < count; i++) {
                idState.put(mCheckedIdStates.keyAt(i), mCheckedIdStates.valueAt(i));
            }

            ss.checkIdState = idState;
        }

        ss.checkedItemCount = mCheckedItemCount;

        return ss;
    }

    @Override
    public void onRestoreInstanceState(Parcelable state) {
        SavedState ss = (SavedState) state;
        super.onRestoreInstanceState(ss.getSuperState());

        mDataChanged = true;
        mSyncHeight = ss.height;

        if (ss.selectedId >= 0) {
            mNeedSync = true;
            mPendingSync = ss;
            mSyncRowId = ss.selectedId;
            mSyncPosition = ss.position;
            mSpecificStart = ss.viewStart;
            mSyncMode = SYNC_SELECTED_POSITION;
        } else if (ss.firstId >= 0) {
            setSelectedPositionInt(INVALID_POSITION);

            // Do this before setting mNeedSync since setNextSelectedPosition looks at mNeedSync
            setNextSelectedPositionInt(INVALID_POSITION);

            mSelectorPosition = INVALID_POSITION;
            mNeedSync = true;
            mPendingSync = ss;
            mSyncRowId = ss.firstId;
            mSyncPosition = ss.position;
            mSpecificStart = ss.viewStart;
            mSyncMode = SYNC_FIRST_POSITION;
        }

        if (ss.checkState != null) {
            mCheckStates = ss.checkState;
        }

        if (ss.checkIdState != null) {
            mCheckedIdStates = ss.checkIdState;
        }

        mCheckedItemCount = ss.checkedItemCount;

        requestLayout();
    }

    public static class LayoutParams extends ViewGroup.LayoutParams {
        /**
         * Type of this view as reported by the adapter
         */
        int viewType;

        /**
         * The stable ID of the item this view displays
         */
        long id = -1;

        /**
         * The position the view was removed from when pulled out of the
         * scrap heap.
         * @hide
         */
        int scrappedFromPosition;

        /**
         * When a TwoWayView is measured with an AT_MOST measure spec, it needs
         * to obtain children views to measure itself. When doing so, the children
         * are not attached to the window, but put in the recycler which assumes
         * they've been attached before. Setting this flag will force the reused
         * view to be attached to the window rather than just attached to the
         * parent.
         */
        boolean forceAdd;

        public LayoutParams(int width, int height) {
            super(width, height);

            if (this.width == MATCH_PARENT) {
                Log.w(LOGTAG, "Constructing LayoutParams with width FILL_PARENT " +
                        "does not make much sense as the view might change orientation. " +
                        "Falling back to WRAP_CONTENT");
                this.width = WRAP_CONTENT;
            }

            if (this.height == MATCH_PARENT) {
                Log.w(LOGTAG, "Constructing LayoutParams with height FILL_PARENT " +
                        "does not make much sense as the view might change orientation. " +
                        "Falling back to WRAP_CONTENT");
                this.height = WRAP_CONTENT;
            }
        }

        public LayoutParams(Context c, AttributeSet attrs) {
            super(c, attrs);

            if (this.width == MATCH_PARENT) {
                Log.w(LOGTAG, "Inflation setting LayoutParams width to MATCH_PARENT - " +
                        "does not make much sense as the view might change orientation. " +
                        "Falling back to WRAP_CONTENT");
                this.width = MATCH_PARENT;
            }

            if (this.height == MATCH_PARENT) {
                Log.w(LOGTAG, "Inflation setting LayoutParams height to MATCH_PARENT - " +
                        "does not make much sense as the view might change orientation. " +
                        "Falling back to WRAP_CONTENT");
                this.height = WRAP_CONTENT;
            }
        }

        public LayoutParams(ViewGroup.LayoutParams other) {
            super(other);

            if (this.width == MATCH_PARENT) {
                Log.w(LOGTAG, "Constructing LayoutParams with height MATCH_PARENT - " +
                        "does not make much sense as the view might change orientation. " +
                        "Falling back to WRAP_CONTENT");
                this.width = WRAP_CONTENT;
            }

            if (this.height == MATCH_PARENT) {
                Log.w(LOGTAG, "Constructing LayoutParams with height MATCH_PARENT - " +
                        "does not make much sense as the view might change orientation. " +
                        "Falling back to WRAP_CONTENT");
                this.height = WRAP_CONTENT;
            }
        }
    }

    class RecycleBin {
        private RecyclerListener mRecyclerListener;
        private int mFirstActivePosition;
        private View[] mActiveViews = new View[0];
        private ArrayList<View>[] mScrapViews;
        private int mViewTypeCount;
        private ArrayList<View> mCurrentScrap;
        private SparseArrayCompat<View> mTransientStateViews;

        public void setViewTypeCount(int viewTypeCount) {
            if (viewTypeCount < 1) {
                throw new IllegalArgumentException("Can't have a viewTypeCount < 1");
            }

            @SuppressWarnings({"unchecked", "rawtypes"})
            ArrayList<View>[] scrapViews = new ArrayList[viewTypeCount];
            for (int i = 0; i < viewTypeCount; i++) {
                scrapViews[i] = new ArrayList<View>();
            }

            mViewTypeCount = viewTypeCount;
            mCurrentScrap = scrapViews[0];
            mScrapViews = scrapViews;
        }

        public void markChildrenDirty() {
            if (mViewTypeCount == 1) {
                final ArrayList<View> scrap = mCurrentScrap;
                final int scrapCount = scrap.size();

                for (int i = 0; i < scrapCount; i++) {
                    scrap.get(i).forceLayout();
                }
            } else {
                final int typeCount = mViewTypeCount;
                for (int i = 0; i < typeCount; i++) {
                    final ArrayList<View> scrap = mScrapViews[i];
                    final int scrapCount = scrap.size();

                    for (int j = 0; j < scrapCount; j++) {
                        scrap.get(j).forceLayout();
                    }
                }
            }

            if (mTransientStateViews != null) {
                final int count = mTransientStateViews.size();
                for (int i = 0; i < count; i++) {
                    mTransientStateViews.valueAt(i).forceLayout();
                }
            }
        }

        void clear() {
            if (mViewTypeCount == 1) {
                final ArrayList<View> scrap = mCurrentScrap;
                final int scrapCount = scrap.size();

                for (int i = 0; i < scrapCount; i++) {
                    removeDetachedView(scrap.remove(scrapCount - 1 - i), false);
                }
            } else {
                final int typeCount = mViewTypeCount;
                for (int i = 0; i < typeCount; i++) {
                    final ArrayList<View> scrap = mScrapViews[i];
                    final int scrapCount = scrap.size();

                    for (int j = 0; j < scrapCount; j++) {
                        removeDetachedView(scrap.remove(scrapCount - 1 - j), false);
                    }
                }
            }

            if (mTransientStateViews != null) {
                mTransientStateViews.clear();
            }
        }

        void fillActiveViews(int childCount, int firstActivePosition) {
            if (mActiveViews.length < childCount) {
                mActiveViews = new View[childCount];
            }

            mFirstActivePosition = firstActivePosition;

            final View[] activeViews = mActiveViews;
            for (int i = 0; i < childCount; i++) {
                View child = getChildAt(i);

                // Note:  We do place AdapterView.ITEM_VIEW_TYPE_IGNORE in active views.
                //        However, we will NOT place them into scrap views.
                activeViews[i] = child;
            }
        }

        View getActiveView(int position) {
            final int index = position - mFirstActivePosition;
            final View[] activeViews = mActiveViews;

            if (index >= 0 && index < activeViews.length) {
                final View match = activeViews[index];
                activeViews[index] = null;

                return match;
            }

            return null;
        }

        View getTransientStateView(int position) {
            if (mTransientStateViews == null) {
                return null;
            }

            final int index = mTransientStateViews.indexOfKey(position);
            if (index < 0) {
                return null;
            }

            final View result = mTransientStateViews.valueAt(index);
            mTransientStateViews.removeAt(index);

            return result;
        }

        void clearTransientStateViews() {
            if (mTransientStateViews != null) {
                mTransientStateViews.clear();
            }
        }

        View getScrapView(int position) {
            if (mViewTypeCount == 1) {
                return retrieveFromScrap(mCurrentScrap, position);
            } else {
                int whichScrap = mAdapter.getItemViewType(position);
                if (whichScrap >= 0 && whichScrap < mScrapViews.length) {
                    return retrieveFromScrap(mScrapViews[whichScrap], position);
                }
            }

            return null;
        }

        void addScrapView(View scrap, int position) {
            LayoutParams lp = (LayoutParams) scrap.getLayoutParams();
            if (lp == null) {
                return;
            }

            lp.scrappedFromPosition = position;

            if (mViewTypeCount == 1) {
                mCurrentScrap.add(scrap);
            } else {
                mScrapViews[lp.viewType].add(scrap);
            }

            if (mRecyclerListener != null) {
                mRecyclerListener.onMovedToScrapHeap(scrap);
            }
        }

        void scrapActiveViews() {
            final View[] activeViews = mActiveViews;
            final boolean multipleScraps = (mViewTypeCount > 1);

            ArrayList<View> scrapViews = mCurrentScrap;
            final int count = activeViews.length;

            for (int i = count - 1; i >= 0; i--) {
                final View victim = activeViews[i];
                if (victim != null) {
                    final LayoutParams lp = (LayoutParams) victim.getLayoutParams();
                    int whichScrap = lp.viewType;

                    activeViews[i] = null;

                    final boolean scrapHasTransientState = ViewCompat.hasTransientState(victim);
                    if (scrapHasTransientState) {
                        if (scrapHasTransientState) {
                            removeDetachedView(victim, false);

                            if (mTransientStateViews == null) {
                                mTransientStateViews = new SparseArrayCompat<View>();
                            }

                            mTransientStateViews.put(mFirstActivePosition + i, victim);
                        }

                        continue;
                    }

                    if (multipleScraps) {
                        scrapViews = mScrapViews[whichScrap];
                    }

                    lp.scrappedFromPosition = mFirstActivePosition + i;
                    scrapViews.add(victim);

                    if (mRecyclerListener != null) {
                        mRecyclerListener.onMovedToScrapHeap(victim);
                    }
                }
            }

            pruneScrapViews();
        }

        private void pruneScrapViews() {
            final int maxViews = mActiveViews.length;
            final int viewTypeCount = mViewTypeCount;
            final ArrayList<View>[] scrapViews = mScrapViews;

            for (int i = 0; i < viewTypeCount; ++i) {
                final ArrayList<View> scrapPile = scrapViews[i];
                int size = scrapPile.size();
                final int extras = size - maxViews;

                size--;

                for (int j = 0; j < extras; j++) {
                    removeDetachedView(scrapPile.remove(size--), false);
                }
            }

            if (mTransientStateViews != null) {
                for (int i = 0; i < mTransientStateViews.size(); i++) {
                    final View v = mTransientStateViews.valueAt(i);
                    if (!ViewCompat.hasTransientState(v)) {
                        mTransientStateViews.removeAt(i);
                        i--;
                    }
                }
            }
        }

        void reclaimScrapViews(List<View> views) {
            if (mViewTypeCount == 1) {
                views.addAll(mCurrentScrap);
            } else {
                final int viewTypeCount = mViewTypeCount;
                final ArrayList<View>[] scrapViews = mScrapViews;

                for (int i = 0; i < viewTypeCount; ++i) {
                    final ArrayList<View> scrapPile = scrapViews[i];
                    views.addAll(scrapPile);
                }
            }
        }

        View retrieveFromScrap(ArrayList<View> scrapViews, int position) {
            int size = scrapViews.size();
            if (size <= 0) {
                return null;
            }

            for (int i = 0; i < size; i++) {
                final View scrapView = scrapViews.get(i);
                final LayoutParams lp = (LayoutParams) scrapView.getLayoutParams();

                if (lp.scrappedFromPosition == position) {
                    scrapViews.remove(i);
                    return scrapView;
                }
            }

            return scrapViews.remove(size - 1);
        }
    }

    private class AdapterDataSetObserver extends DataSetObserver {
        private Parcelable mInstanceState = null;

        @Override
        public void onChanged() {
            mDataChanged = true;
            mOldItemCount = mItemCount;
            mItemCount = getAdapter().getCount();

            // Detect the case where a cursor that was previously invalidated has
            // been re-populated with new data.
            if (TwoWayView.this.mHasStableIds && mInstanceState != null
                    && mOldItemCount == 0 && mItemCount > 0) {
                TwoWayView.this.onRestoreInstanceState(mInstanceState);
                mInstanceState = null;
            } else {
                rememberSyncState();
            }

            requestLayout();
        }

        @Override
        public void onInvalidated() {
            mDataChanged = true;

            if (TwoWayView.this.mHasStableIds) {
                // Remember the current state for the case where our hosting activity is being
                // stopped and later restarted
                mInstanceState = TwoWayView.this.onSaveInstanceState();
            }

            // Data is invalid so we should reset our state
            mOldItemCount = mItemCount;
            mItemCount = 0;

            mSelectedPosition = INVALID_POSITION;
            mSelectedRowId = INVALID_ROW_ID;

            mNextSelectedPosition = INVALID_POSITION;
            mNextSelectedRowId = INVALID_ROW_ID;

            mNeedSync = false;

            requestLayout();
        }
    }

    static class SavedState extends BaseSavedState {
        long selectedId;
        long firstId;
        int viewStart;
        int position;
        int height;
        int checkedItemCount;
        SparseBooleanArray checkState;
        LongSparseArray<Integer> checkIdState;

        /**
         * Constructor called from {@link TwoWayView#onSaveInstanceState()}
         */
        SavedState(Parcelable superState) {
            super(superState);
        }

        /**
         * Constructor called from {@link #CREATOR}
         */
        private SavedState(Parcel in) {
            super(in);

            selectedId = in.readLong();
            firstId = in.readLong();
            viewStart = in.readInt();
            position = in.readInt();
            height = in.readInt();

            checkedItemCount = in.readInt();
            checkState = in.readSparseBooleanArray();

            final int N = in.readInt();
            if (N > 0) {
                checkIdState = new LongSparseArray<Integer>();
                for (int i = 0; i < N; i++) {
                    final long key = in.readLong();
                    final int value = in.readInt();
                    checkIdState.put(key, value);
                }
            }
        }

        @Override
        public void writeToParcel(Parcel out, int flags) {
            super.writeToParcel(out, flags);

            out.writeLong(selectedId);
            out.writeLong(firstId);
            out.writeInt(viewStart);
            out.writeInt(position);
            out.writeInt(height);

            out.writeInt(checkedItemCount);
            out.writeSparseBooleanArray(checkState);

            final int N = checkIdState != null ? checkIdState.size() : 0;
            out.writeInt(N);

            for (int i = 0; i < N; i++) {
                out.writeLong(checkIdState.keyAt(i));
                out.writeInt(checkIdState.valueAt(i));
            }
        }

        @Override
        public String toString() {
            return "TwoWayView.SavedState{"
                    + Integer.toHexString(System.identityHashCode(this))
                    + " selectedId=" + selectedId
                    + " firstId=" + firstId
                    + " viewStart=" + viewStart
                    + " height=" + height
                    + " position=" + position
                    + " checkState=" + checkState + "}";
        }

        public static final Parcelable.Creator<SavedState> CREATOR
                = new Parcelable.Creator<SavedState>() {
            @Override
            public SavedState createFromParcel(Parcel in) {
                return new SavedState(in);
            }

            @Override
            public SavedState[] newArray(int size) {
                return new SavedState[size];
            }
        };
    }

    private class SelectionNotifier implements Runnable {
        @Override
        public void run() {
            if (mDataChanged) {
                // Data has changed between when this SelectionNotifier
                // was posted and now. We need to wait until the AdapterView
                // has been synched to the new data.
                if (mAdapter != null) {
                    post(this);
                }
            } else {
                fireOnSelected();
            }
        }
    }

    private class WindowRunnnable {
        private int mOriginalAttachCount;

        public void rememberWindowAttachCount() {
            mOriginalAttachCount = getWindowAttachCount();
        }

        public boolean sameWindow() {
            return hasWindowFocus() && getWindowAttachCount() == mOriginalAttachCount;
        }
    }

    private class PerformClick extends WindowRunnnable implements Runnable {
        int mClickMotionPosition;

        @Override
        public void run() {
            if (mDataChanged) {
                return;
            }

            final ListAdapter adapter = mAdapter;
            final int motionPosition = mClickMotionPosition;

            if (adapter != null && mItemCount > 0 &&
                motionPosition != INVALID_POSITION &&
                motionPosition < adapter.getCount() && sameWindow()) {

                final View child = getChildAt(motionPosition - mFirstPosition);
                if (child != null) {
                    performItemClick(child, motionPosition, adapter.getItemId(motionPosition));
                }
            }
        }
    }

    private final class CheckForTap implements Runnable {
        @Override
        public void run() {
            if (mTouchMode != TOUCH_MODE_DOWN) {
                return;
            }

            mTouchMode = TOUCH_MODE_TAP;

            final View child = getChildAt(mMotionPosition - mFirstPosition);
            if (child != null && !child.hasFocusable()) {
                mLayoutMode = LAYOUT_NORMAL;

                if (!mDataChanged) {
                    setPressed(true);
                    child.setPressed(true);

                    layoutChildren();
                    positionSelector(mMotionPosition, child);
                    refreshDrawableState();

                    positionSelector(mMotionPosition, child);
                    refreshDrawableState();

                    final boolean longClickable = isLongClickable();

                    if (mSelector != null) {
                        Drawable d = mSelector.getCurrent();

                        if (d != null && d instanceof TransitionDrawable) {
                            if (longClickable) {
                                final int longPressTimeout = ViewConfiguration.getLongPressTimeout();
                                ((TransitionDrawable) d).startTransition(longPressTimeout);
                            } else {
                                ((TransitionDrawable) d).resetTransition();
                            }
                        }
                    }

                    if (longClickable) {
                        triggerCheckForLongPress();
                    } else {
                        mTouchMode = TOUCH_MODE_DONE_WAITING;
                    }
                } else {
                    mTouchMode = TOUCH_MODE_DONE_WAITING;
                }
            }
        }
    }

    private class CheckForLongPress extends WindowRunnnable implements Runnable {
        @Override
        public void run() {
            final int motionPosition = mMotionPosition;
            final View child = getChildAt(motionPosition - mFirstPosition);

            if (child != null) {
                final long longPressId = mAdapter.getItemId(mMotionPosition);

                boolean handled = false;
                if (sameWindow() && !mDataChanged) {
                    handled = performLongPress(child, motionPosition, longPressId);
                }

                if (handled) {
                    mTouchMode = TOUCH_MODE_REST;
                    setPressed(false);
                    child.setPressed(false);
                } else {
                    mTouchMode = TOUCH_MODE_DONE_WAITING;
                }
            }
        }
    }
}
