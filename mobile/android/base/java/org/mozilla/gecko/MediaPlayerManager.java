/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v7.media.MediaControlIntent;
import android.support.v7.media.MediaRouteSelector;
import android.support.v7.media.MediaRouter;
import android.support.v7.media.MediaRouter.RouteInfo;
import android.util.Log;

import com.google.android.gms.cast.CastMediaControlIntent;

import org.json.JSONObject;
import org.mozilla.gecko.annotation.JNITarget;
import org.mozilla.gecko.annotation.ReflectionTarget;
import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSObject;

import java.util.HashMap;
import java.util.Map;

/**
 * Manages a list of GeckoMediaPlayers methods (i.e. Chromecast/Miracast). Routes messages
 * from Gecko to the correct caster based on the id of the display
 */
public class MediaPlayerManager extends Fragment implements NativeEventListener {
    /**
     * Create a new instance of DetailsFragment, initialized to
     * show the text at 'index'.
     */
    @ReflectionTarget
    public static MediaPlayerManager newInstance() {
        if (Versions.feature17Plus) {
            return new PresentationMediaPlayerManager();
        } else {
            return new MediaPlayerManager();
        }
    }

    private static final String LOGTAG = "GeckoMediaPlayerManager";

    @ReflectionTarget
    public static final String MEDIA_PLAYER_TAG = "MPManagerFragment";

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

    protected MediaRouter mediaRouter = null;
    protected final Map<String, GeckoMediaPlayer> displays = new HashMap<String, GeckoMediaPlayer>();

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EventDispatcher.getInstance().registerGeckoThreadListener(this,
                "MediaPlayer:Load",
                "MediaPlayer:Start",
                "MediaPlayer:Stop",
                "MediaPlayer:Play",
                "MediaPlayer:Pause",
                "MediaPlayer:End",
                "MediaPlayer:Mirror",
                "MediaPlayer:Message");
    }

    @Override
    @JNITarget
    public void onDestroy() {
        super.onDestroy();
        EventDispatcher.getInstance().unregisterGeckoThreadListener(this,
                                                                    "MediaPlayer:Load",
                                                                    "MediaPlayer:Start",
                                                                    "MediaPlayer:Stop",
                                                                    "MediaPlayer:Play",
                                                                    "MediaPlayer:Pause",
                                                                    "MediaPlayer:End",
                                                                    "MediaPlayer:Mirror",
                                                                    "MediaPlayer:Message");
    }

    // GeckoEventListener implementation
    @Override
    public void handleMessage(String event, final NativeJSObject message, final EventCallback callback) {
        debug(event);

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
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent(
                        "MediaPlayer:Removed", route.getId()));
                updatePresentation();
            }

            @SuppressWarnings("unused")
            public void onRouteSelected(MediaRouter router, int type, MediaRouter.RouteInfo route) {
                updatePresentation();
            }

            // These methods aren't used by the support version Media Router
            @SuppressWarnings("unused")
            public void onRouteUnselected(MediaRouter router, int type, RouteInfo route) {
                updatePresentation();
            }

            @Override
            public void onRoutePresentationDisplayChanged(MediaRouter router, RouteInfo route) {
                updatePresentation();
            }

            @Override
            public void onRouteVolumeChanged(MediaRouter router, RouteInfo route) {
            }

            @Override
            public void onRouteAdded(MediaRouter router, MediaRouter.RouteInfo route) {
                debug("onRouteAdded: route=" + route);
                final GeckoMediaPlayer display = getMediaPlayerForRoute(route);
                saveAndNotifyOfDisplay("MediaPlayer:Added", route, display);
                updatePresentation();
            }

            @Override
            public void onRouteChanged(MediaRouter router, MediaRouter.RouteInfo route) {
                debug("onRouteChanged: route=" + route);
                final GeckoMediaPlayer display = displays.get(route.getId());
                saveAndNotifyOfDisplay("MediaPlayer:Changed", route, display);
                updatePresentation();
            }

            private void saveAndNotifyOfDisplay(final String eventName,
                    MediaRouter.RouteInfo route, final GeckoMediaPlayer display) {
                if (display == null) {
                    return;
                }

                final JSONObject json = display.toJSON();
                if (json == null) {
                    return;
                }

                displays.put(route.getId(), display);
                final GeckoEvent event = GeckoEvent.createBroadcastEvent(eventName, json.toString());
                GeckoAppShell.sendEventToGecko(event);
            }
        };

    private GeckoMediaPlayer getMediaPlayerForRoute(MediaRouter.RouteInfo route) {
        try {
            if (route.supportsControlCategory(MediaControlIntent.CATEGORY_REMOTE_PLAYBACK)) {
                return new ChromeCast(getActivity(), route);
            }
        } catch(Exception ex) {
            debug("Error handling presentation", ex);
        }

        return null;
    }

    @Override
    public void onPause() {
        super.onPause();
        mediaRouter.removeCallback(callback);
        mediaRouter = null;
    }

    @Override
    public void onResume() {
        super.onResume();

        // The mediaRouter shouldn't exist here, but this is a nice safety check.
        if (mediaRouter != null) {
            return;
        }

        mediaRouter = MediaRouter.getInstance(getActivity());
        final MediaRouteSelector selectorBuilder = new MediaRouteSelector.Builder()
            .addControlCategory(MediaControlIntent.CATEGORY_LIVE_VIDEO)
            .addControlCategory(MediaControlIntent.CATEGORY_REMOTE_PLAYBACK)
            .addControlCategory(CastMediaControlIntent.categoryForCast(ChromeCast.MIRROR_RECEIVER_APP_ID))
            .build();
        mediaRouter.addCallback(selectorBuilder, callback, MediaRouter.CALLBACK_FLAG_REQUEST_DISCOVERY);
    }

    protected void updatePresentation() { /* Overridden in sub-classes. */ }
}
