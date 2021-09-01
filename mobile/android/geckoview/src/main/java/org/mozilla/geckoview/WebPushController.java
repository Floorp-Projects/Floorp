/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import android.util.Log;

public class WebPushController {
    private static final String LOGTAG = "WebPushController";

    private WebPushDelegate mDelegate;
    private BundleEventListener mEventListener;

    /* package */ WebPushController() {
        mEventListener = new EventListener();
        EventDispatcher.getInstance().registerUiThreadListener(mEventListener,
                "GeckoView:PushSubscribe",
                "GeckoView:PushUnsubscribe",
                "GeckoView:PushGetSubscription");
    }

    /**
     * Sets the {@link WebPushDelegate} for this instance.
     *
     * @param delegate The {@link WebPushDelegate} instance.
     */
    @UiThread
    public void setDelegate(final @Nullable WebPushDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        mDelegate = delegate;
    }

    /**
     * Gets the {@link WebPushDelegate} for this instance.
     *
     * @return delegate The {@link WebPushDelegate} instance.
     */
    @UiThread
    @Nullable
    public WebPushDelegate getDelegate() {
        ThreadUtils.assertOnUiThread();
        return mDelegate;
    }

    /**
     * Send a push event for a given subscription.
     *
     * @param scope The Service Worker scope associated with this subscription.
     */
    @UiThread
    public void onPushEvent(final @NonNull String scope) {
        ThreadUtils.assertOnUiThread();
        onPushEvent(scope, null);
    }

    /**
     * Send a push event with a payload for a given subscription.
     * @param scope The Service Worker scope associated with this subscription.
     * @param data The unencrypted payload.
     */
    @UiThread
    public void onPushEvent(final @NonNull String scope, final @Nullable byte[] data) {
        ThreadUtils.assertOnUiThread();

        GeckoThread.waitForState(GeckoThread.State.JNI_READY).accept(val -> {
            final GeckoBundle msg = new GeckoBundle(2);
            msg.putString("scope", scope);
            msg.putString("data", Base64Utils.encode(data));
            EventDispatcher.getInstance().dispatch("GeckoView:PushEvent", msg);
        }, e -> Log.e(LOGTAG, "Unable to deliver Web Push message", e));
    }

    /**
     * Notify that a given subscription has changed. This is normally a signal to the content
     * that it needs to re-subscribe.
     *
     * @param scope The Service Worker scope associated with this subscription.
     */
    @UiThread
    public void onSubscriptionChanged(final @NonNull String scope) {
        ThreadUtils.assertOnUiThread();

        final GeckoBundle msg = new GeckoBundle(1);
        msg.putString("scope", scope);
        EventDispatcher.getInstance().dispatch("GeckoView:PushSubscriptionChanged", msg);
    }

    private class EventListener implements BundleEventListener {

        @Override
        public void handleMessage(final String event, final GeckoBundle message, final EventCallback callback) {
            if (mDelegate == null) {
                callback.sendError("Not allowed");
                return;
            }

            switch (event) {
                case "GeckoView:PushSubscribe": {
                    byte[] appServerKey = null;
                    if (message.containsKey("appServerKey")) {
                        appServerKey = Base64Utils.decode(message.getString("appServerKey"));
                    }

                    final GeckoResult<WebPushSubscription> result =
                            mDelegate.onSubscribe(message.getString("scope"), appServerKey);

                    if (result == null) {
                        callback.sendSuccess(null);
                        return;
                    }

                    result.accept(subscription -> callback.sendSuccess(subscription != null ? subscription.toBundle() : null),
                        error -> callback.sendSuccess(null));
                    break;
                }
                case "GeckoView:PushUnsubscribe": {
                    final GeckoResult<Void> result = mDelegate.onUnsubscribe(message.getString("scope"));
                    if (result == null) {
                        callback.sendSuccess(null);
                        return;
                    }

                    callback.resolveTo(result.map(val -> null));
                    break;
                }
                case "GeckoView:PushGetSubscription": {
                    final GeckoResult<WebPushSubscription> result = mDelegate.onGetSubscription(message.getString("scope"));
                    if (result == null) {
                        callback.sendSuccess(null);
                        return;
                    }

                    callback.resolveTo(result.map(subscription ->
                            subscription != null ? subscription.toBundle() : null));
                    break;
                }
            }
        }
    }
}
