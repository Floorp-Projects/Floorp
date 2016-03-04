/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.push;

import android.support.annotation.NonNull;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * Represent an autopush Channel subscription.
 * <p/>
 * Such a subscription associates a user agent and autopush data with a channel
 * ID, a WebPush endpoint, and some service-specific data.
 * <p/>
 * Cloud Messaging data, and the returned uaid and secret.
 * <p/>
 * Each registration is associated to a single Gecko profile, although we don't
 * enforce that here.  This class is immutable, so it is by definition
 * thread-safe.
 */
public class PushSubscription {
    public final @NonNull String chid;
    public final @NonNull String profileName;
    public final @NonNull String webpushEndpoint;
    public final @NonNull String service;
    public final JSONObject serviceData;

    public PushSubscription(@NonNull String chid, @NonNull String profileName, @NonNull String webpushEndpoint, @NonNull String service, JSONObject serviceData) {
        this.chid = chid;
        this.profileName = profileName;
        this.webpushEndpoint = webpushEndpoint;
        this.service = service;
        this.serviceData = serviceData;
    }

    public JSONObject toJSONObject() throws JSONException {
        final JSONObject jsonObject = new JSONObject();
        jsonObject.put("chid", chid);
        jsonObject.put("profileName", profileName);
        jsonObject.put("webpushEndpoint", webpushEndpoint);
        jsonObject.put("service", service);
        jsonObject.put("serviceData", serviceData);
        return jsonObject;
    }

    public static PushSubscription fromJSONObject(@NonNull JSONObject subscription) throws JSONException {
        final String chid = subscription.getString("chid");
        final String profileName = subscription.getString("profileName");
        final String webpushEndpoint = subscription.getString("webpushEndpoint");
        final String service = subscription.getString("service");
        final JSONObject serviceData = subscription.optJSONObject("serviceData");
        return new PushSubscription(chid, profileName, webpushEndpoint, service, serviceData);
    }

    @Override
    public boolean equals(Object o) {
        // Auto-generated.
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        PushSubscription that = (PushSubscription) o;

        if (!chid.equals(that.chid)) return false;
        if (!profileName.equals(that.profileName)) return false;
        if (!webpushEndpoint.equals(that.webpushEndpoint)) return false;
        return service.equals(that.service);
    }

    @Override
    public int hashCode() {
        // Auto-generated.
        int result = profileName.hashCode();
        result = 31 * result + chid.hashCode();
        result = 31 * result + webpushEndpoint.hashCode();
        result = 31 * result + service.hashCode();
        return result;
    }
}
