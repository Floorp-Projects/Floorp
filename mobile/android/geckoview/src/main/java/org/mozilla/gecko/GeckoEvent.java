/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.nio.ByteBuffer;
import java.util.concurrent.ArrayBlockingQueue;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.gfx.DisplayPortMetrics;
import org.mozilla.gecko.gfx.ImmutableViewportMetrics;

import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;
import android.os.SystemClock;
import android.util.Log;
import android.util.SparseArray;
import android.view.KeyEvent;
import android.view.MotionEvent;
import org.mozilla.gecko.annotation.JNITarget;
import org.mozilla.gecko.annotation.RobocopTarget;

/**
 * We're not allowed to hold on to most events given to us
 * so we save the parts of the events we want to use in GeckoEvent.
 * Fields have different meanings depending on the event type.
 */
@JNITarget
public class GeckoEvent {
    private static final String LOGTAG = "GeckoEvent";

    private static final int EVENT_FACTORY_SIZE = 5;

    // Maybe we're probably better to just make mType non final, and just store GeckoEvents in here...
    private static final SparseArray<ArrayBlockingQueue<GeckoEvent>> mEvents = new SparseArray<ArrayBlockingQueue<GeckoEvent>>();

    public static GeckoEvent get(NativeGeckoEvent type) {
        synchronized (mEvents) {
            ArrayBlockingQueue<GeckoEvent> events = mEvents.get(type.value);
            if (events != null && events.size() > 0) {
                return events.poll();
            }
        }

        return new GeckoEvent(type);
    }

    public void recycle() {
        synchronized (mEvents) {
            ArrayBlockingQueue<GeckoEvent> events = mEvents.get(mType);
            if (events == null) {
                events = new ArrayBlockingQueue<GeckoEvent>(EVENT_FACTORY_SIZE);
                mEvents.put(mType, events);
            }

            events.offer(this);
        }
    }

    // Make sure to keep these values in sync with the enum in
    // AndroidGeckoEvent in widget/android/AndroidJavaWrappers.h
    @JNITarget
    public enum NativeGeckoEvent {
        MOTION_EVENT(2),
        VIEWPORT(20),
        NATIVE_GESTURE_EVENT(31),
        CALL_OBSERVER(33),
        REMOVE_OBSERVER(34),
        LONG_PRESS(47);

        public final int value;

        private NativeGeckoEvent(int value) {
            this.value = value;
        }
    }

    public static final int ACTION_MAGNIFY_START = 11;
    public static final int ACTION_MAGNIFY = 12;
    public static final int ACTION_MAGNIFY_END = 13;

    private final int mType;
    private int mAction;
    private long mTime;
    private Point[] mPoints;
    private int[] mPointIndicies;
    private int mPointerIndex; // index of the point that has changed
    private float[] mOrientations;
    private float[] mPressures;
    private int[] mToolTypes;
    private Point[] mPointRadii;
    private double mX;

    private int mMetaState;
    private int mCount;
    private String mCharacters;
    private String mCharactersExtra;
    private String mData;

    private GeckoEvent(NativeGeckoEvent event) {
        mType = event.value;
    }

    public static GeckoEvent createNativeGestureEvent(int action, PointF pt, double size) {
        try {
            GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.NATIVE_GESTURE_EVENT);
            event.mAction = action;
            event.mCount = 1;
            event.mPoints = new Point[1];

            PointF geckoPoint = new PointF(pt.x, pt.y);
            geckoPoint = GeckoAppShell.getLayerView().convertViewPointToLayerPoint(geckoPoint);

            if (geckoPoint == null) {
                // This could happen if Gecko isn't ready yet.
                return null;
            }

            event.mPoints[0] = new Point((int)Math.floor(geckoPoint.x), (int)Math.floor(geckoPoint.y));

            event.mX = size;
            event.mTime = System.currentTimeMillis();
            return event;
        } catch (Exception e) {
            // This can happen if Gecko isn't ready yet
            return null;
        }
    }

    /**
     * Creates a GeckoEvent that contains the data from the MotionEvent.
     * The keepInViewCoordinates parameter can be set to false to convert from the Java
     * coordinate system (device pixels relative to the LayerView) to a coordinate system
     * relative to gecko's coordinate system (CSS pixels relative to gecko scroll position).
     */
    public static GeckoEvent createMotionEvent(MotionEvent m, boolean keepInViewCoordinates) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.MOTION_EVENT);
        event.initMotionEvent(m, keepInViewCoordinates);
        return event;
    }

    /**
     * Creates a GeckoEvent that contains the data from the LongPressEvent, to be
     * dispatched in CSS pixels relative to gecko's scroll position.
     */
    public static GeckoEvent createLongPressEvent(MotionEvent m) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.LONG_PRESS);
        event.initMotionEvent(m, false);
        return event;
    }

    private void initMotionEvent(MotionEvent m, boolean keepInViewCoordinates) {
        mAction = m.getActionMasked();
        mTime = (System.currentTimeMillis() - SystemClock.elapsedRealtime()) + m.getEventTime();
        mMetaState = m.getMetaState();

        switch (mAction) {
            case MotionEvent.ACTION_CANCEL:
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
            case MotionEvent.ACTION_POINTER_DOWN:
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_MOVE:
            case MotionEvent.ACTION_HOVER_ENTER:
            case MotionEvent.ACTION_HOVER_MOVE:
            case MotionEvent.ACTION_HOVER_EXIT: {
                mCount = m.getPointerCount();
                mPoints = new Point[mCount];
                mPointIndicies = new int[mCount];
                mOrientations = new float[mCount];
                mPressures = new float[mCount];
                mToolTypes = new int[mCount];
                mPointRadii = new Point[mCount];
                mPointerIndex = m.getActionIndex();
                for (int i = 0; i < mCount; i++) {
                    addMotionPoint(i, i, m, keepInViewCoordinates);
                }
                break;
            }
            default: {
                mCount = 0;
                mPointerIndex = -1;
                mPoints = new Point[mCount];
                mPointIndicies = new int[mCount];
                mOrientations = new float[mCount];
                mPressures = new float[mCount];
                mToolTypes = new int[mCount];
                mPointRadii = new Point[mCount];
            }
        }
    }

    private void addMotionPoint(int index, int eventIndex, MotionEvent event, boolean keepInViewCoordinates) {
        try {
            PointF geckoPoint = new PointF(event.getX(eventIndex), event.getY(eventIndex));
            if (!keepInViewCoordinates) {
                geckoPoint = GeckoAppShell.getLayerView().convertViewPointToLayerPoint(geckoPoint);
            }

            mPoints[index] = new Point((int)Math.floor(geckoPoint.x), (int)Math.floor(geckoPoint.y));
            mPointIndicies[index] = event.getPointerId(eventIndex);

            double radians = event.getOrientation(eventIndex);
            mOrientations[index] = (float) Math.toDegrees(radians);
            // w3c touchevents spec does not allow orientations == 90
            // this shifts it to -90, which will be shifted to zero below
            if (mOrientations[index] == 90)
                mOrientations[index] = -90;

            // w3c touchevent radius are given by an orientation between 0 and 90
            // the radius is found by removing the orientation and measuring the x and y
            // radius of the resulting ellipse
            // for android orientations >= 0 and < 90, the major axis should correspond to
            // just reporting the y radius as the major one, and x as minor
            // however, for a radius < 0, we have to shift the orientation by adding 90, and
            // reverse which radius is major and minor
            if (mOrientations[index] < 0) {
                mOrientations[index] += 90;
                mPointRadii[index] = new Point((int) event.getToolMajor(eventIndex) / 2,
                                               (int) event.getToolMinor(eventIndex) / 2);
            } else {
                mPointRadii[index] = new Point((int) event.getToolMinor(eventIndex) / 2,
                                               (int) event.getToolMajor(eventIndex) / 2);
            }

            if (!keepInViewCoordinates) {
                // If we are converting to gecko CSS pixels, then we should adjust the
                // radii as well
                float zoom = GeckoAppShell.getLayerView().getViewportMetrics().zoomFactor;
                mPointRadii[index].x /= zoom;
                mPointRadii[index].y /= zoom;
            }
            mPressures[index] = event.getPressure(eventIndex);
            if (Versions.feature14Plus) {
                mToolTypes[index] = event.getToolType(index);
            }
        } catch (Exception ex) {
            Log.e(LOGTAG, "Error creating motion point " + index, ex);
            mPointRadii[index] = new Point(0, 0);
            mPoints[index] = new Point(0, 0);
        }
    }

    public static GeckoEvent createViewportEvent(ImmutableViewportMetrics metrics, DisplayPortMetrics displayPort) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.VIEWPORT);
        event.mCharacters = "Viewport:Change";
        StringBuilder sb = new StringBuilder(256);
        sb.append("{ \"x\" : ").append(metrics.viewportRectLeft)
          .append(", \"y\" : ").append(metrics.viewportRectTop)
          .append(", \"zoom\" : ").append(metrics.zoomFactor)
          .append(", \"displayPort\" :").append(displayPort.toJSON())
          .append('}');
        event.mCharactersExtra = sb.toString();
        return event;
    }

    public static GeckoEvent createCallObserverEvent(String observerKey, String topic, String data) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.CALL_OBSERVER);
        event.mCharacters = observerKey;
        event.mCharactersExtra = topic;
        event.mData = data;
        return event;
    }

    public static GeckoEvent createRemoveObserverEvent(String observerKey) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.REMOVE_OBSERVER);
        event.mCharacters = observerKey;
        return event;
    }
}
