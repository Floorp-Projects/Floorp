/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.TelemetryContract.Event;
import org.mozilla.gecko.TelemetryContract.Method;
import org.mozilla.gecko.TelemetryContract.Reason;
import org.mozilla.gecko.TelemetryContract.Session;
import org.mozilla.gecko.mma.MmaDelegate;

import android.util.Log;

import static org.mozilla.gecko.mma.MmaDelegate.INTERACT_WITH_SEARCH_URL_AREA;
import static org.mozilla.gecko.mma.MmaDelegate.OPENED_BOOKMARK;
import static org.mozilla.gecko.mma.MmaDelegate.SAVED_BOOKMARK;
import static org.mozilla.gecko.mma.MmaDelegate.SAVED_LOGIN_AND_PASSWORD;
import static org.mozilla.gecko.mma.MmaDelegate.SCREENSHOT;


@RobocopTarget
public class Telemetry {
    private static final String LOGTAG = "Telemetry";

    public static void addToHistogram(String name, int value) {
        TelemetryUtils.addToHistogram(name, value);
    }

    public static void addToKeyedHistogram(String name, String key, int value) {
        TelemetryUtils.addToKeyedHistogram(name, key, value);
    }

    public static void startUISession(final Session session, final String sessionNameSuffix) {
        TelemetryUtils.startUISession(session, sessionNameSuffix);
    }

    public static void startUISession(final Session session) {
        startUISession(session, null);
    }

    public static void stopUISession(final Session session, final String sessionNameSuffix,
            final Reason reason) {
        TelemetryUtils.stopUISession(session, sessionNameSuffix, reason);
    }

    public static void stopUISession(final Session session, final Reason reason) {
        stopUISession(session, null, reason);
    }

    public static void stopUISession(final Session session, final String sessionNameSuffix) {
        stopUISession(session, sessionNameSuffix, Reason.NONE);
    }

    public static void stopUISession(final Session session) {
        stopUISession(session, null, Reason.NONE);
    }

    public static void sendUIEvent(final Event event, final Method method, final long timestamp,
            final String extras) {
        sendUIEvent(event.toString(), method, timestamp, extras);
    }

    public static void sendUIEvent(final Event event, final Method method, final long timestamp) {
        sendUIEvent(event, method, timestamp, null);
    }

    public static void sendUIEvent(final Event event, final Method method, final String extras) {
        sendUIEvent(event, method, TelemetryUtils.realtime(), extras);
    }

    public static void sendUIEvent(final Event event, final Method method) {
        sendUIEvent(event, method, TelemetryUtils.realtime(), null);
    }

    public static void sendUIEvent(final Event event) {
        sendUIEvent(event, Method.NONE, TelemetryUtils.realtime(), null);
    }

    public static void sendUIEvent(final Event event, final boolean eventStatus) {
        final String eventName = event + ":" + eventStatus;
        sendUIEvent(eventName, Method.NONE, TelemetryUtils.realtime(), null);
    }

    private static void sendUIEvent(final String eventName, final Method method,
            final long timestamp, final String extras) {
        TelemetryUtils.sendUIEvent(eventName, method, timestamp, extras);
        mappingMmaTracking(eventName, method, extras);
    }

    private static void mappingMmaTracking(String eventName, Method method, String extras) {
        if (eventName == null || method == null || extras == null) {
            return;
        }
        if (eventName.equalsIgnoreCase(Event.SAVE.toString()) && method == Method.MENU && extras.equals("bookmark")) {
            MmaDelegate.track(SAVED_BOOKMARK);
        } else if (eventName.equalsIgnoreCase(Event.LOAD_URL.toString()) && method == Method.LIST_ITEM && extras.equals("bookmarks")) {
            MmaDelegate.track(OPENED_BOOKMARK);
        } else if (eventName.equalsIgnoreCase(Event.SHOW.toString()) && method == Method.ACTIONBAR && extras.equals("urlbar-url")) {
            MmaDelegate.track(INTERACT_WITH_SEARCH_URL_AREA);
        } else if (eventName.equalsIgnoreCase(Event.SHARE.toString()) && method == Method.BUTTON && extras.equals("screenshot")) {
            MmaDelegate.track(SCREENSHOT);
        } else if (eventName.equalsIgnoreCase(Event.ACTION.toString()) && method == Method.DOORHANGER && extras.equals("login-positive")) {
            MmaDelegate.track(SAVED_LOGIN_AND_PASSWORD);
        }
    }
}
