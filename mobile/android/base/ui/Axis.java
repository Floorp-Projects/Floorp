/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick Walton <pcwalton@mozilla.com>
 *   Kartikaya Gupta <kgupta@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko.ui;

import org.mozilla.gecko.FloatUtils;

/**
 * This class represents the physics for one axis of movement (i.e. either
 * horizontal or vertical). It tracks the different properties of movement
 * like displacement, velocity, viewport dimensions, etc. pertaining to
 * a particular axis.
 */
abstract class Axis {
    // This fraction of velocity remains after every animation frame when the velocity is low.
    private static final float FRICTION_SLOW = 0.85f;
    // This fraction of velocity remains after every animation frame when the velocity is high.
    private static final float FRICTION_FAST = 0.97f;
    // Below this velocity (in pixels per frame), the friction starts increasing from FRICTION_FAST
    // to FRICTION_SLOW.
    private static final float VELOCITY_THRESHOLD = 10.0f;
    // The maximum velocity change factor between events, per ms, in %.
    // Direction changes are excluded.
    private static final float MAX_EVENT_ACCELERATION = 0.012f;

    // The rate of deceleration when the surface has overscrolled.
    private static final float OVERSCROLL_DECEL_RATE = 0.04f;
    // The percentage of the surface which can be overscrolled before it must snap back.
    private static final float SNAP_LIMIT = 0.75f;

    // The minimum amount of space that must be present for an axis to be considered scrollable,
    // in pixels.
    private static final float MIN_SCROLLABLE_DISTANCE = 0.5f;
    // The number of milliseconds per frame assuming 60 fps
    private static final float MS_PER_FRAME = 1000.0f / 60.0f;

    private enum FlingStates {
        STOPPED,
        PANNING,
        FLINGING,
    }

    private enum Overscroll {
        NONE,
        MINUS,      // Overscrolled in the negative direction
        PLUS,       // Overscrolled in the positive direction
        BOTH,       // Overscrolled in both directions (page is zoomed to smaller than screen)
    }

    private final SubdocumentScrollHelper mSubscroller;

    private float mFirstTouchPos;           /* Position of the first touch event on the current drag. */
    private float mTouchPos;                /* Position of the most recent touch event on the current drag. */
    private float mLastTouchPos;            /* Position of the touch event before touchPos. */
    private float mVelocity;                /* Velocity in this direction; pixels per animation frame. */
    private boolean mLocked;                /* Whether movement on this axis is locked. */
    private boolean mDisableSnap;           /* Whether overscroll snapping is disabled. */
    private float mDisplacement;

    private FlingStates mFlingState;        /* The fling state we're in on this axis. */

    protected abstract float getOrigin();
    protected abstract float getViewportLength();
    protected abstract float getPageLength();

    Axis(SubdocumentScrollHelper subscroller) {
        mSubscroller = subscroller;
    }

    private float getViewportEnd() {
        return getOrigin() + getViewportLength();
    }

    void startTouch(float pos) {
        mVelocity = 0.0f;
        mLocked = false;
        mFirstTouchPos = mTouchPos = mLastTouchPos = pos;
    }

    float panDistance(float currentPos) {
        return currentPos - mFirstTouchPos;
    }

    void setLocked(boolean locked) {
        mLocked = locked;
    }

    void saveTouchPos() {
        mLastTouchPos = mTouchPos;
    }

    void updateWithTouchAt(float pos, float timeDelta) {
        float newVelocity = (mTouchPos - pos) / timeDelta * MS_PER_FRAME;

        // If there's a direction change, or current velocity is very low,
        // allow setting of the velocity outright. Otherwise, use the current
        // velocity and a maximum change factor to set the new velocity.
        boolean curVelocityIsLow = Math.abs(mVelocity) < 1.0f;
        boolean directionChange = (mVelocity > 0) != (newVelocity > 0);
        if (curVelocityIsLow || (directionChange && !FloatUtils.fuzzyEquals(newVelocity, 0.0f))) {
            mVelocity = newVelocity;
        } else {
            float maxChange = Math.abs(mVelocity * timeDelta * MAX_EVENT_ACCELERATION);
            mVelocity = Math.min(mVelocity + maxChange, Math.max(mVelocity - maxChange, newVelocity));
        }

        mTouchPos = pos;
    }

    boolean overscrolled() {
        return getOverscroll() != Overscroll.NONE;
    }

    private Overscroll getOverscroll() {
        boolean minus = (getOrigin() < 0.0f);
        boolean plus = (getViewportEnd() > getPageLength());
        if (minus && plus) {
            return Overscroll.BOTH;
        } else if (minus) {
            return Overscroll.MINUS;
        } else if (plus) {
            return Overscroll.PLUS;
        } else {
            return Overscroll.NONE;
        }
    }

    // Returns the amount that the page has been overscrolled. If the page hasn't been
    // overscrolled on this axis, returns 0.
    private float getExcess() {
        switch (getOverscroll()) {
        case MINUS:     return -getOrigin();
        case PLUS:      return getViewportEnd() - getPageLength();
        case BOTH:      return getViewportEnd() - getPageLength() - getOrigin();
        default:        return 0.0f;
        }
    }

    /*
     * Returns true if the page is zoomed in to some degree along this axis such that scrolling
     * is possible. Otherwise, returns false.
     */
    private boolean scrollable() {
        return getViewportLength() <= getPageLength() - MIN_SCROLLABLE_DISTANCE;
    }

    /*
     * Returns the resistance, as a multiplier, that should be taken into account when
     * tracking or pinching.
     */
    float getEdgeResistance() {
        float excess = getExcess();
        return (excess > 0.0f) ? SNAP_LIMIT - excess / getViewportLength() : 1.0f;
    }

    /* Returns the velocity. If the axis is locked, returns 0. */
    float getRealVelocity() {
        return mLocked ? 0.0f : mVelocity;
    }

    void startPan() {
        mFlingState = FlingStates.PANNING;
    }

    void startFling(boolean stopped) {
        mDisableSnap = mSubscroller.scrolling();

        if (stopped) {
            mFlingState = FlingStates.STOPPED;
        } else {
            mFlingState = FlingStates.FLINGING;
        }
    }

    /* Advances a fling animation by one step. */
    boolean advanceFling() {
        if (mFlingState != FlingStates.FLINGING) {
            return false;
        }
        if (mSubscroller.scrolling() && !mSubscroller.lastScrollSucceeded()) {
            // if the subdocument stopped scrolling, it's because it reached the end
            // of the subdocument. we don't do overscroll on subdocuments, so there's
            // no point in continuing this fling.
            return false;
        }

        float excess = getExcess();
        if (mDisableSnap || FloatUtils.fuzzyEquals(excess, 0.0f)) {
            // If we aren't overscrolled, just apply friction.
            if (Math.abs(mVelocity) >= VELOCITY_THRESHOLD) {
                mVelocity *= FRICTION_FAST;
            } else {
                float t = mVelocity / VELOCITY_THRESHOLD;
                mVelocity *= FloatUtils.interpolate(FRICTION_SLOW, FRICTION_FAST, t);
            }
        } else {
            // Otherwise, decrease the velocity linearly.
            float elasticity = 1.0f - excess / (getViewportLength() * SNAP_LIMIT);
            if (getOverscroll() == Overscroll.MINUS) {
                mVelocity = Math.min((mVelocity + OVERSCROLL_DECEL_RATE) * elasticity, 0.0f);
            } else { // must be Overscroll.PLUS
                mVelocity = Math.max((mVelocity - OVERSCROLL_DECEL_RATE) * elasticity, 0.0f);
            }
        }

        return true;
    }

    void stopFling() {
        mVelocity = 0.0f;
        mFlingState = FlingStates.STOPPED;
    }

    // Performs displacement of the viewport position according to the current velocity.
    void displace() {
        if (!mSubscroller.scrolling() && (mLocked || !scrollable()))
            return;

        if (mFlingState == FlingStates.PANNING)
            mDisplacement += (mLastTouchPos - mTouchPos) * getEdgeResistance();
        else
            mDisplacement += mVelocity;
    }

    float resetDisplacement() {
        float d = mDisplacement;
        mDisplacement = 0.0f;
        return d;
    }
}
