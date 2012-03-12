/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
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
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.IntSize;
import org.mozilla.gecko.gfx.RectUtils;
import org.mozilla.gecko.gfx.ViewportMetrics;
import android.os.*;
import android.app.*;
import android.view.*;
import android.content.*;
import android.graphics.*;
import android.widget.*;
import android.hardware.*;
import android.location.*;
import android.util.FloatMath;
import android.util.DisplayMetrics;
import android.graphics.PointF;
import android.text.format.Time;
import android.os.SystemClock;
import java.lang.System;

import android.util.Log;

/* We're not allowed to hold on to most events given to us
 * so we save the parts of the events we want to use in GeckoEvent.
 * Fields have different meanings depending on the event type.
 */

public class GeckoEvent {
    private static final String LOGTAG = "GeckoEvent";

    private static final int INVALID = -1;
    private static final int NATIVE_POKE = 0;
    private static final int KEY_EVENT = 1;
    private static final int MOTION_EVENT = 2;
    private static final int ORIENTATION_EVENT = 3;
    private static final int ACCELERATION_EVENT = 4;
    private static final int LOCATION_EVENT = 5;
    private static final int IME_EVENT = 6;
    private static final int DRAW = 7;
    private static final int SIZE_CHANGED = 8;
    private static final int ACTIVITY_STOPPING = 9;
    private static final int ACTIVITY_PAUSING = 10;
    private static final int ACTIVITY_SHUTDOWN = 11;
    private static final int LOAD_URI = 12;
    private static final int SURFACE_CREATED = 13;
    private static final int SURFACE_DESTROYED = 14;
    private static final int GECKO_EVENT_SYNC = 15;
    private static final int ACTIVITY_START = 17;
    private static final int BROADCAST = 19;
    private static final int VIEWPORT = 20;
    private static final int VISITED = 21;
    private static final int NETWORK_CHANGED = 22;
    private static final int PROXIMITY_EVENT = 23;
    private static final int ACTIVITY_RESUMING = 24;
    private static final int SCREENSHOT = 25;

    public static final int IME_COMPOSITION_END = 0;
    public static final int IME_COMPOSITION_BEGIN = 1;
    public static final int IME_SET_TEXT = 2;
    public static final int IME_GET_TEXT = 3;
    public static final int IME_DELETE_TEXT = 4;
    public static final int IME_SET_SELECTION = 5;
    public static final int IME_GET_SELECTION = 6;
    public static final int IME_ADD_RANGE = 7;

    public static final int IME_RANGE_CARETPOSITION = 1;
    public static final int IME_RANGE_RAWINPUT = 2;
    public static final int IME_RANGE_SELECTEDRAWTEXT = 3;
    public static final int IME_RANGE_CONVERTEDTEXT = 4;
    public static final int IME_RANGE_SELECTEDCONVERTEDTEXT = 5;

    public static final int IME_RANGE_UNDERLINE = 1;
    public static final int IME_RANGE_FORECOLOR = 2;
    public static final int IME_RANGE_BACKCOLOR = 4;

    final public int mType;
    public int mAction;
    public long mTime;
    public Point[] mPoints;
    public int[] mPointIndicies;
    public int mPointerIndex; // index of the point that has changed
    public float[] mOrientations;
    public float[] mPressures;
    public Point[] mPointRadii;
    public Rect mRect;
    public double mX, mY, mZ;
    public double mAlpha, mBeta, mGamma;
    public double mDistance;

    public int mMetaState, mFlags;
    public int mKeyCode, mUnicodeChar;
    public int mOffset, mCount;
    public String mCharacters, mCharactersExtra;
    public int mRangeType, mRangeStyles;
    public int mRangeForeColor, mRangeBackColor;
    public Location mLocation;
    public Address  mAddress;

    public double mBandwidth;
    public boolean mCanBeMetered;

    public int mNativeWindow;

    private GeckoEvent(int evType) {
        mType = evType;
    }

    public static GeckoEvent createPauseEvent(int activityDepth) {
        GeckoEvent event = new GeckoEvent(ACTIVITY_PAUSING);
        event.mFlags = activityDepth > 0 ? 1 : 0;
        return event;
    }

    public static GeckoEvent createResumeEvent(int activityDepth) {
        GeckoEvent event = new GeckoEvent(ACTIVITY_RESUMING);
        event.mFlags = activityDepth > 0 ? 1 : 0;
        return event;
    }

    public static GeckoEvent createStoppingEvent(int activityDepth) {
        GeckoEvent event = new GeckoEvent(ACTIVITY_STOPPING);
        event.mFlags = activityDepth > 0 ? 1 : 0;
        return event;
    }

    public static GeckoEvent createStartEvent(int activityDepth) {
        GeckoEvent event = new GeckoEvent(ACTIVITY_START);
        event.mFlags = activityDepth > 0 ? 1 : 0;
        return event;
    }

    public static GeckoEvent createShutdownEvent() {
        return new GeckoEvent(ACTIVITY_SHUTDOWN);
    }

    public static GeckoEvent createSyncEvent() {
        return new GeckoEvent(GECKO_EVENT_SYNC);
    }

    public static GeckoEvent createKeyEvent(KeyEvent k) {
        GeckoEvent event = new GeckoEvent(KEY_EVENT);
        event.initKeyEvent(k);
        return event;
    }

    private void initKeyEvent(KeyEvent k) {
        mAction = k.getAction();
        mTime = k.getEventTime();
        mMetaState = k.getMetaState();
        mFlags = k.getFlags();
        mKeyCode = k.getKeyCode();
        mUnicodeChar = k.getUnicodeChar();
        mCharacters = k.getCharacters();
    }

    public static GeckoEvent createMotionEvent(MotionEvent m) {
        GeckoEvent event = new GeckoEvent(MOTION_EVENT);
        event.initMotionEvent(m);
        return event;
    }

    private void initMotionEvent(MotionEvent m) {
        mAction = m.getAction();
        mTime = (System.currentTimeMillis() - SystemClock.elapsedRealtime()) + m.getEventTime();
        mMetaState = m.getMetaState();

        switch (mAction & MotionEvent.ACTION_MASK) {
            case MotionEvent.ACTION_CANCEL:
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
            case MotionEvent.ACTION_POINTER_DOWN:
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_MOVE: {
                mCount = m.getPointerCount();
                mPoints = new Point[mCount];
                mPointIndicies = new int[mCount];
                mOrientations = new float[mCount];
                mPressures = new float[mCount];
                mPointRadii = new Point[mCount];
                mPointerIndex = (mAction & MotionEvent.ACTION_POINTER_INDEX_MASK) >> MotionEvent.ACTION_POINTER_INDEX_SHIFT;
                for (int i = 0; i < mCount; i++) {
                    addMotionPoint(i, i, m);
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
                mPointRadii = new Point[mCount];
            }
        }
    }

    public void addMotionPoint(int index, int eventIndex, MotionEvent event) {
        try {
            PointF geckoPoint = new PointF(event.getX(eventIndex), event.getY(eventIndex));
            geckoPoint = GeckoApp.mAppContext.getLayerController().convertViewPointToLayerPoint(geckoPoint);
    
            mPoints[index] = new Point(Math.round(geckoPoint.x), Math.round(geckoPoint.y));
            mPointIndicies[index] = event.getPointerId(eventIndex);
            // getToolMajor, getToolMinor and getOrientation are API Level 9 features
            if (Build.VERSION.SDK_INT >= 9) {
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
                    mPointRadii[index] = new Point((int)event.getToolMajor(eventIndex)/2,
                                                   (int)event.getToolMinor(eventIndex)/2);
                } else {
                    mPointRadii[index] = new Point((int)event.getToolMinor(eventIndex)/2,
                                                   (int)event.getToolMajor(eventIndex)/2);
                }
            } else {
                float size = event.getSize(eventIndex);
                DisplayMetrics displaymetrics = new DisplayMetrics();
                GeckoApp.mAppContext.getWindowManager().getDefaultDisplay().getMetrics(displaymetrics);
                size = size*Math.min(displaymetrics.heightPixels, displaymetrics.widthPixels);
                mPointRadii[index] = new Point((int)size,(int)size);
                mOrientations[index] = 0;
            }
            mPressures[index] = event.getPressure(eventIndex);
        } catch(Exception ex) {
            Log.e(LOGTAG, "Error creating motion point " + index, ex);
            mPointRadii[index] = new Point(0, 0);
            mPoints[index] = new Point(0, 0);
        }
    }

    public static GeckoEvent createSensorEvent(SensorEvent s) {
        GeckoEvent event = null;
        int sensor_type = s.sensor.getType();
 
        switch(sensor_type) {
        case Sensor.TYPE_ACCELEROMETER:
            event = new GeckoEvent(ACCELERATION_EVENT);
            event.mX = s.values[0];
            event.mY = s.values[1];
            event.mZ = s.values[2];
            break;
            
        case Sensor.TYPE_ORIENTATION:
            event = new GeckoEvent(ORIENTATION_EVENT);
            event.mAlpha = -s.values[0];
            event.mBeta = -s.values[1];
            event.mGamma = -s.values[2];
            break;

        case Sensor.TYPE_PROXIMITY:
            event = new GeckoEvent(PROXIMITY_EVENT);
            event.mDistance = s.values[0];
            break;
        }
        return event;
    }

    public static GeckoEvent createLocationEvent(Location l, Address a) {
        GeckoEvent event = new GeckoEvent(LOCATION_EVENT);
        event.mLocation = l;
        event.mAddress  = a;
        return event;
    }

    public static GeckoEvent createIMEEvent(int imeAction, int offset, int count) {
        GeckoEvent event = new GeckoEvent(IME_EVENT);
        event.mAction = imeAction;
        event.mOffset = offset;
        event.mCount = count;
        return event;
    }

    private void InitIMERange(int action, int offset, int count,
                              int rangeType, int rangeStyles,
                              int rangeForeColor, int rangeBackColor) {
        mAction = action;
        mOffset = offset;
        mCount = count;
        mRangeType = rangeType;
        mRangeStyles = rangeStyles;
        mRangeForeColor = rangeForeColor;
        mRangeBackColor = rangeBackColor;
        return;
    }
    
    public static GeckoEvent createIMERangeEvent(int offset, int count,
                                                 int rangeType, int rangeStyles,
                                                 int rangeForeColor, int rangeBackColor,
                                                 String text) {
        GeckoEvent event = new GeckoEvent(IME_EVENT);
        event.InitIMERange(IME_SET_TEXT, offset, count, rangeType, rangeStyles,
                           rangeForeColor, rangeBackColor);
        event.mCharacters = text;
        return event;
    }

    public static GeckoEvent createIMERangeEvent(int offset, int count,
                                                 int rangeType, int rangeStyles,
                                                 int rangeForeColor, int rangeBackColor) {
        GeckoEvent event = new GeckoEvent(IME_EVENT);
        event.InitIMERange(IME_SET_TEXT, offset, count, rangeType, rangeStyles,
                           rangeForeColor, rangeBackColor);
        return event;
    }

    public static GeckoEvent createDrawEvent(Rect rect) {
        GeckoEvent event = new GeckoEvent(DRAW);
        event.mRect = rect;
        return event;
    }

    public static GeckoEvent createSizeChangedEvent(int w, int h, int screenw, int screenh) {
        GeckoEvent event = new GeckoEvent(SIZE_CHANGED);
        event.mPoints = new Point[2];
        event.mPoints[0] = new Point(w, h);
        event.mPoints[1] = new Point(screenw, screenh);
        return event;
    }

    public static GeckoEvent createBroadcastEvent(String subject, String data) {
        GeckoEvent event = new GeckoEvent(BROADCAST);
        event.mCharacters = subject;
        event.mCharactersExtra = data;
        return event;
    }

    public static GeckoEvent createViewportEvent(ViewportMetrics viewport, RectF displayPort) {
        GeckoEvent event = new GeckoEvent(VIEWPORT);
        event.mCharacters = "Viewport:Change";
        PointF origin = viewport.getOrigin();
        StringBuffer sb = new StringBuffer(256);
        sb.append("{ \"x\" : ").append(origin.x)
          .append(", \"y\" : ").append(origin.y)
          .append(", \"zoom\" : ").append(viewport.getZoomFactor())
          .append(", \"displayPort\" :").append(RectUtils.toJSON(displayPort))
          .append('}');
        event.mCharactersExtra = sb.toString();
        return event;
    }

    public static GeckoEvent createLoadEvent(String uri) {
        GeckoEvent event = new GeckoEvent(LOAD_URI);
        event.mCharacters = uri;
        return event;
    }

    public static GeckoEvent createVisitedEvent(String data) {
        GeckoEvent event = new GeckoEvent(VISITED);
        event.mCharacters = data;
        return event;
    }

    public static GeckoEvent createNetworkEvent(double bandwidth, boolean canBeMetered) {
        GeckoEvent event = new GeckoEvent(NETWORK_CHANGED);
        event.mBandwidth = bandwidth;
        event.mCanBeMetered = canBeMetered;
        return event;
    }

    public static GeckoEvent createScreenshotEvent(int tabId, int sw, int sh, int dw, int dh) {
        GeckoEvent event = new GeckoEvent(SCREENSHOT);
        event.mPoints = new Point[2];
        event.mPoints[0] = new Point(sw, sh);
        event.mPoints[1] = new Point(dw, dh);
        event.mMetaState = tabId;
        return event;
    }
}
