/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.GeckoEventListener;

import org.json.JSONException;
import org.json.JSONObject;

import android.graphics.PointF;
import android.os.Handler;
import android.util.Log;

class SubdocumentScrollHelper implements GeckoEventListener {
    private static final String LOGTAG = "GeckoSubdocScroll";

    private static final String MESSAGE_PANNING_OVERRIDE = "Panning:Override";
    private static final String MESSAGE_CANCEL_OVERRIDE = "Panning:CancelOverride";
    private static final String MESSAGE_SCROLL = "Gesture:Scroll";
    private static final String MESSAGE_SCROLL_ACK = "Gesture:ScrollAck";

    private final Handler mUiHandler;
    private final EventDispatcher mEventDispatcher;

    /* This is the amount of displacement we have accepted but not yet sent to JS; this is
     * only valid when mOverrideScrollPending is true. */
    private final PointF mPendingDisplacement;

    /* When this is true, we're sending scroll events to JS to scroll the active subdocument. */
    private boolean mOverridePanning;

    /* When this is true, we have received an ack for the last scroll event we sent to JS, and
     * are ready to send the next scroll event. Note we only ever have one scroll event inflight
     * at a time. */
    private boolean mOverrideScrollAck;

    /* When this is true, we have a pending scroll that we need to send to JS; we were unable
     * to send it when it was initially requested because mOverrideScrollAck was not true. */
    private boolean mOverrideScrollPending;

    /* When this is true, the last scroll event we sent actually did some amount of scrolling on
     * the subdocument; we use this to decide when we have reached the end of the subdocument. */
    private boolean mScrollSucceeded;

    SubdocumentScrollHelper(EventDispatcher eventDispatcher) {
        // mUiHandler will be bound to the UI thread since that's where this constructor runs
        mUiHandler = new Handler();
        mPendingDisplacement = new PointF();

        mEventDispatcher = eventDispatcher;
        mEventDispatcher.registerGeckoThreadListener(this,
            MESSAGE_PANNING_OVERRIDE,
            MESSAGE_CANCEL_OVERRIDE,
            MESSAGE_SCROLL_ACK);
    }

    void destroy() {
        mEventDispatcher.unregisterGeckoThreadListener(this,
            MESSAGE_PANNING_OVERRIDE,
            MESSAGE_CANCEL_OVERRIDE,
            MESSAGE_SCROLL_ACK);
    }

    boolean scrollBy(PointF displacement) {
        if (! mOverridePanning) {
            return false;
        }

        if (! mOverrideScrollAck) {
            mOverrideScrollPending = true;
            mPendingDisplacement.x += displacement.x;
            mPendingDisplacement.y += displacement.y;
            return true;
        }

        JSONObject json = new JSONObject();
        try {
            json.put("x", displacement.x);
            json.put("y", displacement.y);
        } catch (JSONException e) {
            Log.e(LOGTAG, "Error forming subwindow scroll message: ", e);
        }
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent(MESSAGE_SCROLL, json.toString()));

        mOverrideScrollAck = false;
        mOverrideScrollPending = false;
        // clear the |mPendingDisplacement| after serializing |displacement| to
        // JSON because they might be the same object
        mPendingDisplacement.x = 0;
        mPendingDisplacement.y = 0;

        return true;
    }

    void cancel() {
        mOverridePanning = false;
    }

    boolean scrolling() {
        return mOverridePanning;
    }

    boolean lastScrollSucceeded() {
        return mScrollSucceeded;
    }

    // GeckoEventListener implementation

    @Override
    public void handleMessage(final String event, final JSONObject message) {
        // This comes in on the Gecko thread; hand off the handling to the UI thread.
        mUiHandler.post(new Runnable() {
            @Override
            public void run() {
                try {
                    if (MESSAGE_PANNING_OVERRIDE.equals(event)) {
                        mOverridePanning = true;
                        mOverrideScrollAck = true;
                        mOverrideScrollPending = false;
                        mScrollSucceeded = true;
                    } else if (MESSAGE_CANCEL_OVERRIDE.equals(event)) {
                        mOverridePanning = false;
                    } else if (MESSAGE_SCROLL_ACK.equals(event)) {
                        mOverrideScrollAck = true;
                        mScrollSucceeded = message.getBoolean("scrolled");
                        if (mOverridePanning && mOverrideScrollPending) {
                            scrollBy(mPendingDisplacement);
                        }
                    }
                } catch (Exception e) {
                    Log.e(LOGTAG, "Exception handling message", e);
                }
            }
        });
    }
}
