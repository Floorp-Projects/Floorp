// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.mozilla.gecko.gfx;

import android.content.Context;
import android.hardware.SensorManager;
import android.util.Log;
import android.view.ViewConfiguration;

import org.mozilla.gecko.annotation.WrapForJNI;

/**
 * This class is vastly copied from {@link android.widget.OverScroller} but decouples the time
 * from the app time so it can be specified manually.
 */
@WrapForJNI(allowMultithread = true, catchException = true)
public class StackScroller {
    private int mMode;

    private final SplineStackScroller mScrollerX;
    private final SplineStackScroller mScrollerY;

    private final boolean mFlywheel;

    private static final int SCROLL_MODE = 0;
    private static final int FLING_MODE = 1;

    private static float sViscousFluidScale;
    private static float sViscousFluidNormalize;

    /**
     * Creates an StackScroller with a viscous fluid scroll interpolator and flywheel.
     * @param context
     */
    public StackScroller(Context context) {
        mFlywheel = true;
        mScrollerX = new SplineStackScroller(context);
        mScrollerY = new SplineStackScroller(context);
        initContants();
    }

    private static void initContants() {
        // This controls the viscous fluid effect (how much of it)
        sViscousFluidScale = 8.0f;
        // must be set to 1.0 (used in viscousFluid())
        sViscousFluidNormalize = 1.0f;
        sViscousFluidNormalize = 1.0f / viscousFluid(1.0f);
    }

    /**
     *
     * Returns whether the scroller has finished scrolling.
     *
     * @return True if the scroller has finished scrolling, false otherwise.
     */
    public final boolean isFinished() {
        return mScrollerX.mFinished && mScrollerY.mFinished;
    }

    /**
     * Force the finished field to a particular value. Contrary to
     * {@link #abortAnimation()}, forcing the animation to finished
     * does NOT cause the scroller to move to the final x and y
     * position.
     *
     * @param finished The new finished value.
     */
    public final void forceFinished(boolean finished) {
        mScrollerX.mFinished = mScrollerY.mFinished = finished;
    }

    /**
     * Returns the current X offset in the scroll.
     *
     * @return The new X offset as an absolute distance from the origin.
     */
    public final int getCurrX() {
        return mScrollerX.mCurrentPosition;
    }

    /**
     * Returns the current Y offset in the scroll.
     *
     * @return The new Y offset as an absolute distance from the origin.
     */
    public final int getCurrY() {
        return mScrollerY.mCurrentPosition;
    }

    /**
     * Returns where the scroll will end. Valid only for "fling" scrolls.
     *
     * @return The final X offset as an absolute distance from the origin.
     */
    public final int getFinalX() {
        return mScrollerX.mFinal;
    }

    public final float getCurrSpeedX() {
        return mScrollerX.mCurrVelocity;
    }

    public final float getCurrSpeedY() {
        return mScrollerY.mCurrVelocity;
    }

    /**
     * Returns where the scroll will end. Valid only for "fling" scrolls.
     *
     * @return The final Y offset as an absolute distance from the origin.
     */
    public final int getFinalY() {
        return mScrollerY.mFinal;
    }

    /**
     * Sets where the scroll will end.  Valid only for "fling" scrolls.
     *
     * @param x The final X offset as an absolute distance from the origin.
     */
    public final void setFinalX(int x) {
        mScrollerX.setFinalPosition(x);
    }

    private static float viscousFluid(float x) {
        x *= sViscousFluidScale;
        if (x < 1.0f) {
            x -= (1.0f - (float) Math.exp(-x));
        } else {
            float start = 0.36787944117f; // 1/e == exp(-1)
            x = 1.0f - (float) Math.exp(1.0f - x);
            x = start + x * (1.0f - start);
        }
        x *= sViscousFluidNormalize;
        return x;
    }

    /**
     * Call this when you want to know the new location. If it returns true, the
     * animation is not yet finished.
     */
    public boolean computeScrollOffset(long time) {
        if (isFinished()) {
            return false;
        }

        switch (mMode) {
            case SCROLL_MODE:
                // Any scroller can be used for time, since they were started
                // together in scroll mode. We use X here.
                final long elapsedTime = time - mScrollerX.mStartTime;

                final int duration = mScrollerX.mDuration;
                if (elapsedTime < duration) {
                    float q = (float) (elapsedTime) / duration;
                    q = viscousFluid(q);
                    mScrollerX.updateScroll(q);
                    mScrollerY.updateScroll(q);
                } else {
                    abortAnimation();
                }
                break;

            case FLING_MODE:
                if (!mScrollerX.mFinished) {
                    if (!mScrollerX.update(time)) {
                        if (!mScrollerX.continueWhenFinished(time)) {
                            mScrollerX.finish();
                        }
                    }
                }

                if (!mScrollerY.mFinished) {
                    if (!mScrollerY.update(time)) {
                        if (!mScrollerY.continueWhenFinished(time)) {
                            mScrollerY.finish();
                        }
                    }
                }

                break;

            default:
                break;
        }

        return true;
    }

    /**
     * Start scrolling by providing a starting point and the distance to travel.
     *
     * @param startX Starting horizontal scroll offset in pixels. Positive
     *        numbers will scroll the content to the left.
     * @param startY Starting vertical scroll offset in pixels. Positive numbers
     *        will scroll the content up.
     * @param dx Horizontal distance to travel. Positive numbers will scroll the
     *        content to the left.
     * @param dy Vertical distance to travel. Positive numbers will scroll the
     *        content up.
     * @param duration Duration of the scroll in milliseconds.
     */
    public void startScroll(int startX, int startY, int dx, int dy, long startTime, int duration) {
        mMode = SCROLL_MODE;
        mScrollerX.startScroll(startX, dx, startTime, duration);
        mScrollerY.startScroll(startY, dy, startTime, duration);
    }

    /**
     * Call this when you want to 'spring back' into a valid coordinate range.
     *
     * @param startX Starting X coordinate
     * @param startY Starting Y coordinate
     * @param minX Minimum valid X value
     * @param maxX Maximum valid X value
     * @param minY Minimum valid Y value
     * @param maxY Minimum valid Y value
     * @return true if a springback was initiated, false if startX and startY were
     *          already within the valid range.
     */
    public boolean springBack(
            int startX, int startY, int minX, int maxX, int minY, int maxY, long time) {
        mMode = FLING_MODE;

        // Make sure both methods are called.
        final boolean spingbackX = mScrollerX.springback(startX, minX, maxX, time);
        final boolean spingbackY = mScrollerY.springback(startY, minY, maxY, time);
        return spingbackX || spingbackY;
    }

    /**
     * Start scrolling based on a fling gesture. The distance traveled will
     * depend on the initial velocity of the fling.
     *
     * @param startX Starting point of the scroll (X)
     * @param startY Starting point of the scroll (Y)
     * @param velocityX Initial velocity of the fling (X) measured in pixels per second.
     * @param velocityY Initial velocity of the fling (Y) measured in pixels per second
     * @param minX Minimum X value. The scroller will not scroll past this point
     *            unless overX > 0. If overfling is allowed, it will use minX as
     *            a springback boundary.
     * @param maxX Maximum X value. The scroller will not scroll past this point
     *            unless overX > 0. If overfling is allowed, it will use maxX as
     *            a springback boundary.
     * @param minY Minimum Y value. The scroller will not scroll past this point
     *            unless overY > 0. If overfling is allowed, it will use minY as
     *            a springback boundary.
     * @param maxY Maximum Y value. The scroller will not scroll past this point
     *            unless overY > 0. If overfling is allowed, it will use maxY as
     *            a springback boundary.
     * @param overX Overfling range. If > 0, horizontal overfling in either
     *            direction will be possible.
     * @param overY Overfling range. If > 0, vertical overfling in either
     *            direction will be possible.
     */
    public void fling(int startX, int startY, int velocityX, int velocityY, int minX, int maxX,
            int minY, int maxY, int overX, int overY, long time) {
        // Continue a scroll or fling in progress
        if (mFlywheel && !isFinished()) {
            float oldVelocityX = mScrollerX.mCurrVelocity;
            float oldVelocityY = mScrollerY.mCurrVelocity;
            if (Math.signum(velocityX) == Math.signum(oldVelocityX)
                    && Math.signum(velocityY) == Math.signum(oldVelocityY)) {
                velocityX += oldVelocityX;
                velocityY += oldVelocityY;
            }
        }

        mMode = FLING_MODE;
        mScrollerX.fling(startX, velocityX, minX, maxX, overX, time);
        mScrollerY.fling(startY, velocityY, minY, maxY, overY, time);
    }

    /**
     * Stops the animation. Contrary to {@link #forceFinished(boolean)},
     * aborting the animating causes the scroller to move to the final x and y
     * positions.
     *
     * @see #forceFinished(boolean)
     */
    public void abortAnimation() {
        mScrollerX.finish();
        mScrollerY.finish();
    }

    static class SplineStackScroller {
        // Initial position
        private int mStart;

        // Current position
        private int mCurrentPosition;

        // Final position
        private int mFinal;

        // Initial velocity
        private int mVelocity;

        // Current velocity
        private float mCurrVelocity;

        // Constant current deceleration
        private float mDeceleration;

        // Animation starting time, in system milliseconds
        private long mStartTime;

        // Animation duration, in milliseconds
        private int mDuration;

        // Duration to complete spline component of animation
        private int mSplineDuration;

        // Distance to travel along spline animation
        private int mSplineDistance;

        // Whether the animation is currently in progress
        private boolean mFinished;

        // The allowed overshot distance before boundary is reached.
        private int mOver;

        // Fling friction
        private final float mFlingFriction = ViewConfiguration.getScrollFriction();

        // Current state of the animation.
        private int mState = SPLINE;

        // Constant gravity value, used in the deceleration phase.
        private static final float GRAVITY = 2000.0f;

        // A context-specific coefficient adjusted to physical values.
        private final float mPhysicalCoeff;

        private static final float DECELERATION_RATE = (float) (Math.log(0.78) / Math.log(0.9));
        private static final float INFLEXION = 0.35f; // Tension lines cross at (INFLEXION, 1)
        private static final float START_TENSION = 0.5f;
        private static final float END_TENSION = 1.0f;
        private static final float P1 = START_TENSION * INFLEXION;
        private static final float P2 = 1.0f - END_TENSION * (1.0f - INFLEXION);

        private static final int NB_SAMPLES = 100;
        private static final float[] SPLINE_POSITION = new float[NB_SAMPLES + 1];
        private static final float[] SPLINE_TIME = new float[NB_SAMPLES + 1];

        private static final int SPLINE = 0;
        private static final int CUBIC = 1;
        private static final int BALLISTIC = 2;

        static {
            float xMin = 0.0f;
            float yMin = 0.0f;
            for (int i = 0; i < NB_SAMPLES; i++) {
                final float alpha = (float) i / NB_SAMPLES;

                float xMax = 1.0f;
                float x, tx, coef;
                while (true) {
                    x = xMin + (xMax - xMin) / 2.0f;
                    coef = 3.0f * x * (1.0f - x);
                    tx = coef * ((1.0f - x) * P1 + x * P2) + x * x * x;
                    if (Math.abs(tx - alpha) < 1E-5) break;
                    if (tx > alpha) {
                        xMax = x;
                    } else {
                        xMin = x;
                    }
                }
                SPLINE_POSITION[i] = coef * ((1.0f - x) * START_TENSION + x) + x * x * x;

                float yMax = 1.0f;
                float y, dy;
                while (true) {
                    y = yMin + (yMax - yMin) / 2.0f;
                    coef = 3.0f * y * (1.0f - y);
                    dy = coef * ((1.0f - y) * START_TENSION + y) + y * y * y;
                    if (Math.abs(dy - alpha) < 1E-5) break;
                    if (dy > alpha) {
                        yMax = y;
                    } else {
                        yMin = y;
                    }
                }
                SPLINE_TIME[i] = coef * ((1.0f - y) * P1 + y * P2) + y * y * y;
            }
            SPLINE_POSITION[NB_SAMPLES] = SPLINE_TIME[NB_SAMPLES] = 1.0f;
        }

        SplineStackScroller(Context context) {
            mFinished = true;
            final float ppi = context.getResources().getDisplayMetrics().density * 160.0f;
            mPhysicalCoeff = SensorManager.GRAVITY_EARTH // g (m/s^2)
                    * 39.37f // inch/meter
                    * ppi * 0.84f; // look and feel tuning
        }

        void updateScroll(float q) {
            mCurrentPosition = mStart + Math.round(q * (mFinal - mStart));
        }

        /*
         * Get a signed deceleration that will reduce the velocity.
         */
        private static float getDeceleration(int velocity) {
            return velocity > 0 ? -GRAVITY : GRAVITY;
        }

        /*
         * Modifies mDuration to the duration it takes to get from start to newFinal using the
         * spline interpolation. The previous duration was needed to get to oldFinal.
         */
        private void adjustDuration(int start, int oldFinal, int newFinal) {
            final int oldDistance = oldFinal - start;
            final int newDistance = newFinal - start;
            final float x = Math.abs((float) newDistance / oldDistance);
            final int index = (int) (NB_SAMPLES * x);
            if (index < NB_SAMPLES) {
                final float xInf = (float) index / NB_SAMPLES;
                final float xSup = (float) (index + 1) / NB_SAMPLES;
                final float tInf = SPLINE_TIME[index];
                final float tSup = SPLINE_TIME[index + 1];
                final float timeCoef = tInf + (x - xInf) / (xSup - xInf) * (tSup - tInf);
                mDuration *= timeCoef;
            }
        }

        void startScroll(int start, int distance, long startTime, int duration) {
            mFinished = false;

            mStart = start;
            mFinal = start + distance;

            mStartTime = startTime;
            mDuration = duration;

            // Unused
            mDeceleration = 0.0f;
            mVelocity = 0;
        }

        void finish() {
            mCurrentPosition = mFinal;
            // Not reset since WebView relies on this value for fast fling.
            // TODO: restore when WebView uses the fast fling implemented in this class.
            // mCurrVelocity = 0.0f;
            mFinished = true;
        }

        void setFinalPosition(int position) {
            mFinal = position;
            mFinished = false;
        }

        boolean springback(int start, int min, int max, long time) {
            mFinished = true;

            mStart = mFinal = start;
            mVelocity = 0;

            mStartTime = time;
            mDuration = 0;

            if (start < min) {
                startSpringback(start, min, 0);
            } else if (start > max) {
                startSpringback(start, max, 0);
            }

            return !mFinished;
        }

        private void startSpringback(int start, int end, int velocity) {
            // mStartTime has been set
            mFinished = false;
            mState = CUBIC;
            mStart = start;
            mFinal = end;
            final int delta = start - end;
            mDeceleration = getDeceleration(delta);
            // TODO take velocity into account
            mVelocity = -delta; // only sign is used
            mOver = Math.abs(delta);
            mDuration = (int) (1000.0 * Math.sqrt(-2.0 * delta / mDeceleration));
        }

        void fling(int start, int velocity, int min, int max, int over, long time) {
            mOver = over;
            mFinished = false;
            mCurrVelocity = mVelocity = velocity;
            mDuration = mSplineDuration = 0;
            mStartTime = time;
            mCurrentPosition = mStart = start;

            if (start > max || start < min) {
                startAfterEdge(start, min, max, velocity, time);
                return;
            }

            mState = SPLINE;
            double totalDistance = 0.0;

            if (velocity != 0) {
                mDuration = mSplineDuration = getSplineFlingDuration(velocity);
                totalDistance = getSplineFlingDistance(velocity);
            }

            mSplineDistance = (int) (totalDistance * Math.signum(velocity));
            mFinal = start + mSplineDistance;

            // Clamp to a valid final position
            if (mFinal < min) {
                adjustDuration(mStart, mFinal, min);
                mFinal = min;
            }

            if (mFinal > max) {
                adjustDuration(mStart, mFinal, max);
                mFinal = max;
            }
        }

        private double getSplineDeceleration(int velocity) {
            return Math.log(INFLEXION * Math.abs(velocity) / (mFlingFriction * mPhysicalCoeff));
        }

        private double getSplineFlingDistance(int velocity) {
            final double l = getSplineDeceleration(velocity);
            final double decelMinusOne = DECELERATION_RATE - 1.0;
            return mFlingFriction * mPhysicalCoeff
                    * Math.exp(DECELERATION_RATE / decelMinusOne * l);
        }

        /* Returns the duration, expressed in milliseconds */
        private int getSplineFlingDuration(int velocity) {
            final double l = getSplineDeceleration(velocity);
            final double decelMinusOne = DECELERATION_RATE - 1.0;
            return (int) (1000.0 * Math.exp(l / decelMinusOne));
        }

        private void fitOnBounceCurve(int start, int end, int velocity) {
            // Simulate a bounce that started from edge
            final float durationToApex = -velocity / mDeceleration;
            final float distanceToApex = velocity * velocity / 2.0f / Math.abs(mDeceleration);
            final float distanceToEdge = Math.abs(end - start);
            final float totalDuration = (float) Math.sqrt(
                    2.0 * (distanceToApex + distanceToEdge) / Math.abs(mDeceleration));
            mStartTime -= (int) (1000.0f * (totalDuration - durationToApex));
            mStart = end;
            mVelocity = (int) (-mDeceleration * totalDuration);
        }

        private void startBounceAfterEdge(int start, int end, int velocity) {
            mDeceleration = getDeceleration(velocity == 0 ? start - end : velocity);
            fitOnBounceCurve(start, end, velocity);
            onEdgeReached();
        }

        private void startAfterEdge(int start, int min, int max, int velocity, long time) {
            if (start > min && start < max) {
                Log.e("StackScroller", "startAfterEdge called from a valid position");
                mFinished = true;
                return;
            }
            final boolean positive = start > max;
            final int edge = positive ? max : min;
            final int overDistance = start - edge;
            boolean keepIncreasing = overDistance * velocity >= 0;
            if (keepIncreasing) {
                // Will result in a bounce or a to_boundary depending on velocity.
                startBounceAfterEdge(start, edge, velocity);
            } else {
                final double totalDistance = getSplineFlingDistance(velocity);
                if (totalDistance > Math.abs(overDistance)) {
                    fling(start, velocity, positive ? min : start, positive ? start : max, mOver,
                            time);
                } else {
                    startSpringback(start, edge, velocity);
                }
            }
        }

        private void onEdgeReached() {
            // mStart, mVelocity and mStartTime were adjusted to their values when edge was reached.
            float distance = mVelocity * mVelocity / (2.0f * Math.abs(mDeceleration));
            final float sign = Math.signum(mVelocity);

            if (distance > mOver) {
                // Default deceleration is not sufficient to slow us down before boundary
                mDeceleration = -sign * mVelocity * mVelocity / (2.0f * mOver);
                distance = mOver;
            }

            mOver = (int) distance;
            mState = BALLISTIC;
            mFinal = mStart + (int) (mVelocity > 0 ? distance : -distance);
            mDuration = -(int) (1000.0f * mVelocity / mDeceleration);
        }

        boolean continueWhenFinished(long time) {
            switch (mState) {
                case SPLINE:
                    // Duration from start to null velocity
                    if (mDuration < mSplineDuration) {
                        // If the animation was clamped, we reached the edge
                        mStart = mFinal;
                        // TODO Better compute speed when edge was reached
                        mVelocity = (int) mCurrVelocity;
                        mDeceleration = getDeceleration(mVelocity);
                        mStartTime += mDuration;
                        onEdgeReached();
                    } else {
                        // Normal stop, no need to continue
                        return false;
                    }
                    break;
                case BALLISTIC:
                    mStartTime += mDuration;
                    startSpringback(mFinal, mStart, 0);
                    break;
                case CUBIC:
                    return false;
            }

            update(time);
            return true;
        }

        /*
         * Update the current position and velocity for current time. Returns
         * true if update has been done and false if animation duration has been
         * reached.
         */
        boolean update(long time) {
            final long currentTime = time - mStartTime;

            if (((mState == SPLINE) && (mSplineDuration <= 0)) ||
                    ((mState == CUBIC) && (mDuration <= 0))) {
                return false;
            }

            if (currentTime > mDuration) {
                return false;
            }

            double distance = 0.0;
            switch (mState) {
                case SPLINE: {
                    final float t = (float) currentTime / mSplineDuration;
                    final int index = (int) (NB_SAMPLES * t);
                    float distanceCoef = 1.f;
                    float velocityCoef = 0.f;
                    if (index < NB_SAMPLES) {
                        final float tInf = (float) index / NB_SAMPLES;
                        final float tSup = (float) (index + 1) / NB_SAMPLES;
                        final float dInf = SPLINE_POSITION[index];
                        final float dSup = SPLINE_POSITION[index + 1];
                        velocityCoef = (dSup - dInf) / (tSup - tInf);
                        distanceCoef = dInf + (t - tInf) * velocityCoef;
                    }

                    distance = distanceCoef * mSplineDistance;
                    mCurrVelocity = velocityCoef * mSplineDistance / mSplineDuration * 1000.0f;
                    break;
                }

                case BALLISTIC: {
                    final float t = currentTime / 1000.0f;
                    mCurrVelocity = mVelocity + mDeceleration * t;
                    distance = mVelocity * t + mDeceleration * t * t / 2.0f;
                    break;
                }

                case CUBIC: {
                    final float t = (float) (currentTime) / mDuration;
                    final float t2 = t * t;
                    final float sign = Math.signum(mVelocity);
                    distance = sign * mOver * (3.0f * t2 - 2.0f * t * t2);
                    mCurrVelocity = sign * mOver * 6.0f * (-t + t2);
                    break;
                }
            }

            mCurrentPosition = mStart + (int) Math.round(distance);

            return true;
        }
    }
}
