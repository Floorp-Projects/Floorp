/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.mozglue.JNITarget;
import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSObject;

import org.json.JSONArray;
import org.json.JSONObject;
import org.json.JSONException;

import android.content.Context;
import android.support.v7.media.MediaControlIntent;
import android.support.v7.media.MediaRouteSelector;
import android.support.v7.media.MediaRouter;
import android.support.v7.media.MediaRouter.RouteInfo;
import android.util.Log;

import com.google.android.gms.cast.CastMediaControlIntent;

import java.util.HashMap;

/* Wraper for different MediaRouter types supproted by Android. i.e. Chromecast, Miracast, etc. */
interface GeckoMediaPlayer {
    public JSONObject toJSON();
    public void load(String title, String url, String type, EventCallback callback);
    public void play(EventCallback callback);
    public void pause(EventCallback callback);
    public void stop(EventCallback callback);
    public void start(EventCallback callback);
    public void end(EventCallback callback);
    public void mirror(EventCallback callback);
    public void message(String message, EventCallback callback);
}

/* Manages a list of GeckoMediaPlayers methods (i.e. Chromecast/Miracast). Routes messages
 * from Gecko to the correct caster based on the id of the display
 */
class MediaPlayerManager implements NativeEventListener,
                                    GeckoAppShell.AppStateListener {
    private static final String LOGTAG = "GeckoMediaPlayerManager";

    private static final boolean SHOW_DEBUG = false;
    // Simplified debugging interfaces
    private static void debug(String msg, Exception e) {
        if (SHOW_DEBUG) {
            Log.e(LOGTAG, msg, e);
        }
    }

    private static void debug(String msg) {
        if (SHOW_DEBUG) {
            Log.d(LOGTAG, msg);
        }
    }

    private final Context context;
    private final MediaRouter mediaRouter;
    private final HashMap<String, GeckoMediaPlayer> displays = new HashMap<String, GeckoMediaPlayer>();
    private static MediaPlayerManager instance;

    @JNITarget
    public static void init(Context context) {
        if (instance != null) {
            debug("MediaPlayerManager initialized twice");
            return;
        }

        instance = new MediaPlayerManager(context);
    }

    private MediaPlayerManager(Context context) {
        this.context = context;

        if (context instanceof GeckoApp) {
            GeckoApp app = (GeckoApp) context;
            app.addAppStateListener(this);
        }

        mediaRouter = MediaRouter.getInstance(context);
        EventDispatcher.getInstance().registerGeckoThreadListener(this,
                                                                  "MediaPlayer:Load",
                                                                  "MediaPlayer:Start",
                                                                  "MediaPlayer:Stop",
                                                                  "MediaPlayer:Play",
                                                                  "MediaPlayer:Pause",
                                                                  "MediaPlayer:Get",
                                                                  "MediaPlayer:End",
                                                                  "MediaPlayer:Mirror",
                                                                  "MediaPlayer:Message");
    }

    @JNITarget
    public static void onDestroy() {
        if (instance == null) {
            return;
        }

        EventDispatcher.getInstance().unregisterGeckoThreadListener(instance,
                                                                    "MediaPlayer:Load",
                                                                    "MediaPlayer:Start",
                                                                    "MediaPlayer:Stop",
                                                                    "MediaPlayer:Play",
                                                                    "MediaPlayer:Pause",
                                                                    "MediaPlayer:Get",
                                                                    "MediaPlayer:End",
                                                                    "MediaPlayer:Mirror",
                                                                    "MediaPlayer:Message");
        if (instance.context instanceof GeckoApp) {
            GeckoApp app = (GeckoApp) instance.context;
            app.removeAppStateListener(instance);
        }
    }

    // GeckoEventListener implementation
    @Override
    public void handleMessage(String event, final NativeJSObject message, final EventCallback callback) {
        debug(event);

        if ("MediaPlayer:Get".equals(event)) {
            final JSONObject result = new JSONObject();
            final JSONArray disps = new JSONArray();
            for (GeckoMediaPlayer disp : displays.values()) {
                try {
                    disps.put(disp.toJSON());
                } catch(Exception ex) {
                    // This may happen if the device isn't a real Chromecast,
                    // for example Firefly casting devices.
                    Log.e(LOGTAG, "Couldn't create JSON for display", ex);
                }
            }

            try {
                result.put("displays", disps);
            } catch(JSONException ex) {
                Log.i(LOGTAG, "Error sending displays", ex);
            }

            callback.sendSuccess(result);
            return;
        }

        final GeckoMediaPlayer display = displays.get(message.getString("id"));
        if (display == null) {
            Log.e(LOGTAG, "Couldn't find a display for this id: " + message.getString("id") + " for message: " + event);
            if (callback != null) {
                callback.sendError(null);
            }
            return;
        }

        if ("MediaPlayer:Play".equals(event)) {
            display.play(callback);
        } else if ("MediaPlayer:Start".equals(event)) {
            display.start(callback);
        } else if ("MediaPlayer:Stop".equals(event)) {
            display.stop(callback);
        } else if ("MediaPlayer:Pause".equals(event)) {
            display.pause(callback);
        } else if ("MediaPlayer:End".equals(event)) {
            display.end(callback);
        } else if ("MediaPlayer:Mirror".equals(event)) {
            display.mirror(callback);
        } else if ("MediaPlayer:Message".equals(event) && message.has("data")) {
            display.message(message.getString("data"), callback);
        } else if ("MediaPlayer:Load".equals(event)) {
            final String url = message.optString("source", "");
            final String type = message.optString("type", "video/mp4");
            final String title = message.optString("title", "");
            display.load(title, url, type, callback);
        }
    }

    private final MediaRouter.Callback callback =
        new MediaRouter.Callback() {
            @Override
            public void onRouteRemoved(MediaRouter router, RouteInfo route) {
                debug("onRouteRemoved: route=" + route);
                displays.remove(route.getId());
            }

            @SuppressWarnings("unused")
            public void onRouteSelected(MediaRouter router, int type, MediaRouter.RouteInfo route) {
            }

            // These methods aren't used by the support version Media Router
            @SuppressWarnings("unused")
            public void onRouteUnselected(MediaRouter router, int type, RouteInfo route) {
            }

            @Override
            public void onRoutePresentationDisplayChanged(MediaRouter router, RouteInfo route) {
            }

            @Override
            public void onRouteVolumeChanged(MediaRouter router, RouteInfo route) {
            }

            @Override
            public void onRouteAdded(MediaRouter router, MediaRouter.RouteInfo route) {
                debug("onRouteAdded: route=" + route);
                GeckoMediaPlayer display = getMediaPlayerForRoute(route);
                if (display != null) {
                    displays.put(route.getId(), display);
                }
            }

            @Override
            public void onRouteChanged(MediaRouter router, MediaRouter.RouteInfo route) {
                debug("onRouteChanged: route=" + route);
                GeckoMediaPlayer display = displays.get(route.getId());
                if (display != null) {
                    displays.put(route.getId(), display);
                }
            }
        };

    private GeckoMediaPlayer getMediaPlayerForRoute(MediaRouter.RouteInfo route) {
        try {
            if (route.supportsControlCategory(MediaControlIntent.CATEGORY_REMOTE_PLAYBACK)) {
                return new ChromeCast(context, route);
            }
        } catch(Exception ex) {
            debug("Error handling presentation", ex);
        }

        return null;
    }

    /* Implementing GeckoAppShell.AppStateListener */
    @Override
    public void onPause() {
        mediaRouter.removeCallback(callback);
    }

    @Override
    public void onResume() {
        MediaRouteSelector selectorBuilder = new MediaRouteSelector.Builder()
            .addControlCategory(MediaControlIntent.CATEGORY_LIVE_VIDEO)
            .addControlCategory(MediaControlIntent.CATEGORY_REMOTE_PLAYBACK)
            .addControlCategory(CastMediaControlIntent.categoryForCast(ChromeCast.MIRROR_RECIEVER_APP_ID))
            .build();
        mediaRouter.addCallback(selectorBuilder, callback, MediaRouter.CALLBACK_FLAG_REQUEST_DISCOVERY);
    }

    @Override
    public void onOrientationChanged() { }

}
