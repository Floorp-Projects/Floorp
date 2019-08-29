/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;

public interface WebPushDelegate {
    /**
     * Creates a push subscription for the given service worker scope. A scope
     * uniquely identifies a service worker. `appServerKey` optionally
     * creates a restricted subscription.
     *
     * Applications will likely want to persist the returned {@link WebPushSubscription} in order
     * to support {@link #onGetSubscription(String)}.
     *
     * @param scope The Service Worker scope.
     * @param appServerKey An optional application server key.
     * @return A {@link GeckoResult} which resolves to a {@link WebPushSubscription}
     *
     * @see <a href="http://w3c.github.io/push-api/#dom-pushmanager-subscribe">subscribe()</a>
     * @see <a href="http://w3c.github.io/push-api/#dom-pushsubscriptionoptionsinit-applicationserverkey">Application server key</a>
     */
    @UiThread
    default @Nullable GeckoResult<WebPushSubscription> onSubscribe(@NonNull String scope,
                                                                   @Nullable byte[] appServerKey) {
        return null;
    }

    /**
     * Retrieves a subscription for the given service worker scope.
     *
     * @param scope The scope for the requested {@link WebPushSubscription}.
     * @return A {@link GeckoResult} which resolves to a {@link WebPushSubscription}
     *
     * @see <a href="http://w3c.github.io/push-api/#dom-pushmanager-getsubscription">getSubscription()</a>
     */
    @UiThread
    default @Nullable GeckoResult<WebPushSubscription> onGetSubscription(@NonNull String scope) {
        return null;
    }

    /**
     * Removes a push subscription. If this fails, apps should resolve the
     * returned {@link GeckoResult} with an exception.
     *
     * @param scope The Service Worker scope for the subscription.
     * @return A {@link GeckoResult}, which if non-exceptional indicates successfully unsubscribing.
     *
     * @see <a href="http://w3c.github.io/push-api/#dom-pushsubscription-unsubscribe">unsubscribe()</a>
     */
    @UiThread
    default @Nullable GeckoResult<Void> onUnsubscribe(@NonNull String scope) {
        return null;
    }
}
