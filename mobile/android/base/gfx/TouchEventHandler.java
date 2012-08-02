/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.OnInterceptTouchListener;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.ui.PanZoomController;
import org.mozilla.gecko.ui.SimpleScaleGestureDetector;

import android.content.Context;
import android.os.SystemClock;
import android.util.Log;
import android.view.GestureDetector;
import android.view.MotionEvent;

import java.util.LinkedList;
import java.util.Queue;

/**
 * This class handles incoming touch events from the user and sends them to
 * listeners in Gecko and/or performs the "default action" (asynchronous pan/zoom
 * behaviour. EVERYTHING IN THIS CLASS MUST RUN ON THE UI THREAD.
 *
 * In the following code/comments, a "block" of events refers to a contiguous
 * sequence of events that starts with a DOWN or POINTER_DOWN and goes up to
 * but not including the next DOWN or POINTER_DOWN event.
 *
 * "Dispatching" an event refers to performing the default actions for the event,
 * which at our level of abstraction just means sending it off to the gesture
 * detectors and the pan/zoom controller.
 *
 * If an event is "default-prevented" that means one or more listeners in Gecko
 * has called preventDefault() on the event, which means that the default action
 * for that event should not occur. Usually we care about a "block" of events being
 * default-prevented, which means that the DOWN/POINTER_DOWN event that started
 * the block, or the first MOVE event following that, were prevent-defaulted.
 *
 * A "default-prevented notification" is when we here in Java-land receive a notification
 * from gecko as to whether or not a block of events was default-prevented. This happens
 * at some point after the first or second event in the block is processed in Gecko.
 * This code assumes we get EXACTLY ONE default-prevented notification for each block
 * of events.
 *
 * Note that even if all events are default-prevented, we still send specific types
 * of notifications to the pan/zoom controller. The notifications are needed
 * to respond to user actions a timely manner regardless of default-prevention,
 * and fix issues like bug 749384.
 */
public final class TouchEventHandler implements Tabs.OnTabsChangedListener {
    private static final String LOGTAG = "GeckoTouchEventHandler";

    // The time limit for listeners to respond with preventDefault on touchevents
    // before we begin panning the page
    private final int EVENT_LISTENER_TIMEOUT = 200;

    private final LayerView mView;
    private final GestureDetector mGestureDetector;
    private final SimpleScaleGestureDetector mScaleGestureDetector;
    private final PanZoomController mPanZoomController;

    // the queue of events that we are holding on to while waiting for a preventDefault
    // notification
    private final Queue<MotionEvent> mEventQueue;
    private final ListenerTimeoutProcessor mListenerTimeoutProcessor;

    // the listener we use to notify gecko of touch events
    private OnInterceptTouchListener mOnTouchListener;

    // whether or not we should wait for touch listeners to respond (this state is
    // per-tab and is updated when we switch tabs).
    private boolean mWaitForTouchListeners;

    // true if we should hold incoming events in our queue. this is re-set for every
    // block of events, this is cleared once we find out if the block has been
    // default-prevented or not (or we time out waiting for that).
    private boolean mHoldInQueue;

    // true if we should dispatch incoming events to the gesture detector and the pan/zoom
    // controller. if this is false, then the current block of events has been
    // default-prevented, and we should not dispatch these events (although we'll still send
    // them to gecko listeners).
    private boolean mDispatchEvents;

    // this next variable requires some explanation. strap yourself in.
    //
    // for each block of events, we do two things: (1) send the events to gecko and expect
    // exactly one default-prevented notification in return, and (2) kick off a delayed
    // ListenerTimeoutProcessor that triggers in case we don't hear from the listener in
    // a timely fashion.
    // since events are constantly coming in, we need to be able to handle more than one
    // block of events in the queue.
    //
    // this means that there are ordering restrictions on these that we can take advantage of,
    // and need to abide by. blocks of events in the queue will always be in the order that
    // the user generated them. default-prevented notifications we get from gecko will be in
    // the same order as the blocks of events in the queue. the ListenerTimeoutProcessors that
    // have been posted will also fire in the same order as the blocks of events in the queue.
    // HOWEVER, we may get multiple default-prevented notifications interleaved with multiple
    // ListenerTimeoutProcessor firings, and that interleaving is not predictable.
    //
    // therefore, we need to make sure that for each block of events, we process the queued
    // events exactly once, either when we get the default-prevented notification, or when the
    // timeout expires (whichever happens first). there is no way to associate the
    // default-prevented notification with a particular block of events other than via ordering,
    //
    // so what we do to accomplish this is to track a "processing balance", which is the number
    // of default-prevented notifications that we have received, minus the number of ListenerTimeoutProcessors
    // that have fired. (think "balance" as in teeter-totter balance). this value is:
    // - zero when we are in a state where the next default-prevented notification we expect
    //   to receive and the next ListenerTimeoutProcessor we expect to fire both correspond to
    //   the next block of events in the queue.
    // - positive when we are in a state where we have received more default-prevented notifications
    //   than ListenerTimeoutProcessors. This means that the next default-prevented notification
    //   does correspond to the block at the head of the queue, but the next n ListenerTimeoutProcessors
    //   need to be ignored as they are for blocks we have already processed. (n is the absolute value
    //   of the balance.)
    // - negative when we are in a state where we have received more ListenerTimeoutProcessors than
    //   default-prevented notifications. This means that the next ListenerTimeoutProcessor that
    //   we receive does correspond to the block at the head of the queue, but the next n
    //   default-prevented notifications need to be ignored as they are for blocks we have already
    //   processed. (n is the absolute value of the balance.)
    private int mProcessingBalance;

    TouchEventHandler(Context context, LayerView view, LayerController controller) {
        mView = view;

        mEventQueue = new LinkedList<MotionEvent>();
        mGestureDetector = new GestureDetector(context, controller.getGestureListener());
        mScaleGestureDetector = new SimpleScaleGestureDetector(controller.getScaleGestureListener());
        mPanZoomController = controller.getPanZoomController();
        mListenerTimeoutProcessor = new ListenerTimeoutProcessor();
        mDispatchEvents = true;

        mGestureDetector.setOnDoubleTapListener(controller.getDoubleTapListener());

        Tabs.registerOnTabsChangedListener(this);
    }

    /* This function MUST be called on the UI thread */
    public boolean handleEvent(MotionEvent event) {
        // if we don't have gecko listeners, just dispatch the event
        // and be done with it, no extra work needed.
        if (mOnTouchListener == null) {
            dispatchEvent(event);
            return true;
        }

        if (mOnTouchListener.onInterceptTouchEvent(mView, event)) {
            return true;
        }

        // if this is a hover event just notify gecko, we don't have any interest in the java layer.
        if (isHoverEvent(event)) {
            mOnTouchListener.onTouch(mView, event);
            return true;
        }

        if (isDownEvent(event)) {
            // this is the start of a new block of events! whee!
            mHoldInQueue = mWaitForTouchListeners;

            // Set mDispatchEvents to true so that we are guaranteed to either queue these
            // events or dispatch them. The only time we should not do either is once we've
            // heard back from content to preventDefault this block.
            mDispatchEvents = true;
            if (mHoldInQueue) {
                // if the new block we are starting is the current block (i.e. there are no
                // other blocks waiting in the queue, then we should let the pan/zoom controller
                // know we are waiting for the touch listeners to run
                if (mEventQueue.isEmpty()) {
                    mPanZoomController.startingNewEventBlock(event, true);
                }
            } else {
                // we're not going to be holding this block of events in the queue, but we need
                // a marker of some sort so that the processEventBlock loop deals with the blocks
                // in the right order as notifications come in. we use a single null event in
                // the queue as a placeholder for a block of events that has already been dispatched.
                mEventQueue.add(null);
                mPanZoomController.startingNewEventBlock(event, false);
            }

            // set the timeout so that we dispatch these events and update mProcessingBalance
            // if we don't get a default-prevented notification
            mView.postDelayed(mListenerTimeoutProcessor, EVENT_LISTENER_TIMEOUT);
        }

        // if we need to hold the events, add it to the queue. if we need to dispatch
        // it directly, do that. it is possible that both mHoldInQueue and mDispatchEvents
        // are false, in which case we are processing a block of events that we know
        // has been default-prevented. in that case we don't keep the events as we don't
        // need them (but we still pass them to the gecko listener).
        if (mHoldInQueue) {
            mEventQueue.add(MotionEvent.obtain(event));
        } else if (mDispatchEvents) {
            dispatchEvent(event);
        } else if (touchFinished(event)) {
            mPanZoomController.preventedTouchFinished();
        }

        // notify gecko of the event
        mOnTouchListener.onTouch(mView, event);

        return true;
    }

    /**
     * This function is how gecko sends us a default-prevented notification. It is called
     * once gecko knows definitively whether the block of events has had preventDefault
     * called on it (either on the initial down event that starts the block, or on
     * the first event following that down event).
     *
     * This function MUST be called on the UI thread.
     */
    public void handleEventListenerAction(boolean allowDefaultAction) {
        if (mProcessingBalance > 0) {
            // this event listener that triggered this took too long, and the corresponding
            // ListenerTimeoutProcessor runnable already ran for the event in question. the
            // block of events this is for has already been processed, so we don't need to
            // do anything here.
        } else {
            processEventBlock(allowDefaultAction);
        }
        mProcessingBalance--;
    }

    /* This function MUST be called on the UI thread. */
    public void setWaitForTouchListeners(boolean aValue) {
        mWaitForTouchListeners = aValue;
    }

    /* This function MUST be called on the UI thread. */
    public void setOnTouchListener(OnInterceptTouchListener onTouchListener) {
        mOnTouchListener = onTouchListener;
    }

    private boolean isHoverEvent(MotionEvent event) {
        int action = (event.getAction() & MotionEvent.ACTION_MASK);
        return (action == MotionEvent.ACTION_HOVER_ENTER || action == MotionEvent.ACTION_HOVER_MOVE || action == MotionEvent.ACTION_HOVER_EXIT);
    }

    private boolean isDownEvent(MotionEvent event) {
        int action = (event.getAction() & MotionEvent.ACTION_MASK);
        return (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN);
    }

    private boolean touchFinished(MotionEvent event) {
        int action = (event.getAction() & MotionEvent.ACTION_MASK);
        return (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL);
    }

    /**
     * Dispatch the event to the gesture detectors and the pan/zoom controller.
     */
    private void dispatchEvent(MotionEvent event) {
        if (mGestureDetector.onTouchEvent(event)) {
            return;
        }
        mScaleGestureDetector.onTouchEvent(event);
        if (mScaleGestureDetector.isInProgress()) {
            return;
        }
        mPanZoomController.onTouchEvent(event);
    }

    /**
     * Process the block of events at the head of the queue now that we know
     * whether it has been default-prevented or not.
     */
    private void processEventBlock(boolean allowDefaultAction) {
        if (!allowDefaultAction) {
            // if the block has been default-prevented, cancel whatever stuff we had in
            // progress in the gesture detector and pan zoom controller
            long now = SystemClock.uptimeMillis();
            dispatchEvent(MotionEvent.obtain(now, now, MotionEvent.ACTION_CANCEL, 0, 0, 0));
        }

        if (mEventQueue.isEmpty()) {
            Log.e(LOGTAG, "Unexpected empty event queue in processEventBlock!", new Exception());
            return;
        }

        // the odd loop condition is because the first event in the queue will
        // always be a DOWN or POINTER_DOWN event, and we want to process all
        // the events in the queue starting at that one, up to but not including
        // the next DOWN or POINTER_DOWN event.

        MotionEvent event = mEventQueue.poll();
        while (true) {
            // event being null here is valid and represents a block of events
            // that has already been dispatched.

            if (event != null) {
                // for each event we process, only dispatch it if the block hasn't been
                // default-prevented.
                if (allowDefaultAction) {
                    dispatchEvent(event);
                } else if (touchFinished(event)) {
                    mPanZoomController.preventedTouchFinished();
                }
            }
            if (mEventQueue.isEmpty()) {
                // we have processed the backlog of events, and are all caught up.
                // now we can set clear the hold flag and set the dispatch flag so
                // that the handleEvent() function can do the right thing for all
                // remaining events in this block (which is still ongoing) without
                // having to put them in the queue.
                mHoldInQueue = false;
                mDispatchEvents = allowDefaultAction;
                break;
            }
            event = mEventQueue.peek();
            if (event == null || isDownEvent(event)) {
                // we have finished processing the block we were interested in.
                // now we wait for the next call to processEventBlock
                if (event != null) {
                    mPanZoomController.startingNewEventBlock(event, true);
                }
                break;
            }
            // pop the event we peeked above, as it is still part of the block and
            // we want to keep processing
            mEventQueue.remove();
        }
    }

    private class ListenerTimeoutProcessor implements Runnable {
        /* This MUST be run on the UI thread */
        public void run() {
            if (mProcessingBalance < 0) {
                // gecko already responded with default-prevented notification, and so
                // the block of events this ListenerTimeoutProcessor corresponds to have
                // already been removed from the queue.
            } else {
                processEventBlock(true);
            }
            mProcessingBalance++;
        }
    }

    // Tabs.OnTabsChangedListener implementation

    public void onTabChanged(Tab tab, Tabs.TabEvents msg, Object data) {
        if ((Tabs.getInstance().isSelectedTab(tab) && msg == Tabs.TabEvents.STOP) || msg == Tabs.TabEvents.SELECTED) {
            mWaitForTouchListeners = tab.getHasTouchListeners();
        }
    }
}
