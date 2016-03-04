/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.push;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

/**
 * Represent an autopush User Agent registration.
 * <p/>
 * Such a registration associates an endpoint, optional debug flag, some Google
 * Cloud Messaging data, and the returned uaid and secret.
 * <p/>
 * Each registration is associated to a single Gecko profile, although we don't
 * enforce that here.  This class is immutable, so it is by definition
 * thread-safe.
 */
public class PushRegistration {
    public final String autopushEndpoint;
    public final boolean debug;
    // TODO: fold (timestamp, {uaid, secret}) into this class.
    public final @NonNull Fetched uaid;
    public final String secret;

    protected final @NonNull Map<String, PushSubscription> subscriptions;

    public PushRegistration(String autopushEndpoint, boolean debug, @NonNull Fetched uaid, @Nullable String secret, @NonNull Map<String, PushSubscription> subscriptions) {
        this.autopushEndpoint = autopushEndpoint;
        this.debug = debug;
        this.uaid = uaid;
        this.secret = secret;
        this.subscriptions = subscriptions;
    }

    public PushRegistration(String autopushEndpoint, boolean debug, @NonNull Fetched uaid, @Nullable String secret) {
        this(autopushEndpoint, debug, uaid, secret, new HashMap<String, PushSubscription>());
    }

    public JSONObject toJSONObject() throws JSONException {
        final JSONObject subscriptions = new JSONObject();
        for (Map.Entry<String, PushSubscription> entry : this.subscriptions.entrySet()) {
            subscriptions.put(entry.getKey(), entry.getValue().toJSONObject());
        }

        final JSONObject jsonObject = new JSONObject();
        jsonObject.put("autopushEndpoint", autopushEndpoint);
        jsonObject.put("debug", debug);
        jsonObject.put("uaid", uaid.toJSONObject());
        jsonObject.put("secret", secret);
        jsonObject.put("subscriptions", subscriptions);
        return jsonObject;
    }

    public static PushRegistration fromJSONObject(@NonNull JSONObject registration) throws JSONException {
        final String endpoint = registration.optString("autopushEndpoint", null);
        final boolean debug = registration.getBoolean("debug");
        final Fetched uaid = Fetched.fromJSONObject(registration.getJSONObject("uaid"));
        final String secret = registration.optString("secret", null);

        final JSONObject subscriptionsObject = registration.getJSONObject("subscriptions");
        final Map<String, PushSubscription> subscriptions = new HashMap<>();
        final Iterator<String> it = subscriptionsObject.keys();
        while (it.hasNext()) {
            final String chid = it.next();
            subscriptions.put(chid, PushSubscription.fromJSONObject(subscriptionsObject.getJSONObject(chid)));
        }

        return new PushRegistration(endpoint, debug, uaid, secret, subscriptions);
    }

    @Override
    public boolean equals(Object o) {
        // Auto-generated.
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        PushRegistration that = (PushRegistration) o;

        if (autopushEndpoint != null ? !autopushEndpoint.equals(that.autopushEndpoint) : that.autopushEndpoint != null)
            return false;
        if (!uaid.equals(that.uaid)) return false;
        if (secret != null ? !secret.equals(that.secret) : that.secret != null) return false;
        if (subscriptions != null ? !subscriptions.equals(that.subscriptions) : that.subscriptions != null) return false;
        return (debug == that.debug);
    }

    @Override
    public int hashCode() {
        // Auto-generated.
        int result = autopushEndpoint != null ? autopushEndpoint.hashCode() : 0;
        result = 31 * result + (debug ? 1 : 0);
        result = 31 * result + uaid.hashCode();
        result = 31 * result + (secret != null ? secret.hashCode() : 0);
        result = 31 * result + (subscriptions != null ? subscriptions.hashCode() : 0);
        return result;
    }

    public PushRegistration withDebug(boolean debug) {
        return new PushRegistration(this.autopushEndpoint, debug, this.uaid, this.secret, this.subscriptions);
    }

    public PushRegistration withUserAgentID(String uaid, String secret, long nextNow) {
        return new PushRegistration(this.autopushEndpoint, this.debug, new Fetched(uaid, nextNow), secret, this.subscriptions);
    }

    public PushSubscription getSubscription(@NonNull String chid) {
        return subscriptions.get(chid);
    }

    public PushSubscription putSubscription(@NonNull String chid, @NonNull PushSubscription subscription) {
        return subscriptions.put(chid, subscription);
    }

    public PushSubscription removeSubscription(@NonNull String chid) {
        return subscriptions.remove(chid);
    }
}
