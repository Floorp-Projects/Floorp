/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.content.Intent;
import android.net.Uri;
import android.text.TextUtils;

import java.util.HashMap;
import java.util.Map;

public final class WebActivityMapper {
    private static final Map<String, WebActivityMapping> activityMap = new HashMap<String, WebActivityMapping>();
    static {
        activityMap.put("dial", new DialMapping());
        activityMap.put("open", new OpenMapping());
        activityMap.put("pick", new PickMapping());
        activityMap.put("send", new SendMapping());
        activityMap.put("view", new ViewMapping());
    };

    private static abstract class WebActivityMapping {
        // Cannot return null
        public abstract String getAction();

        public String getMime(JSONObject data) throws JSONException {
            return null;
        }

        public String getUri(JSONObject data) throws JSONException {
            return null;
        }

        public void putExtras(JSONObject data, Intent intent) throws JSONException {}
    }

    /**
     * Provides useful defaults for mime type and uri.
     */
    private static abstract class BaseMapping extends WebActivityMapping {
        /**
         * If 'type' is present in data object, uses the value as the MIME type.
         */
        public String getMime(JSONObject data) throws JSONException {
            return data.optString("type", null);
        }

        /**
         * If 'uri' or 'url' is present in data object, uses the respecitve value as the Uri.
         */
        public String getUri(JSONObject data) throws JSONException {
            // Will return uri or url if present.
            String uri = data.optString("uri", null);
            return uri != null ? uri : data.optString("url", null);
        }
    }

    public static Intent getIntentForWebActivity(JSONObject message) throws JSONException {
        final String name = message.getString("name").toLowerCase();
        final JSONObject data = message.getJSONObject("data");

        final WebActivityMapping mapping = activityMap.get(name);
        final Intent intent = new Intent(mapping.getAction());

        final String mime = mapping.getMime(data);
        if (!TextUtils.isEmpty(mime)) {
            intent.setType(mime);
        }

        final String uri = mapping.getUri(data);
        if (!TextUtils.isEmpty(uri)) {
            intent.setData(Uri.parse(uri));
        }

        mapping.putExtras(data, intent);

        return intent;
    }

    private static class DialMapping extends WebActivityMapping {
        @Override
        public String getAction() {
            return Intent.ACTION_DIAL;
        }

        @Override
        public String getUri(JSONObject data) throws JSONException {
            return "tel:" + data.getString("number");
        }
    }

    private static class OpenMapping extends BaseMapping {
        @Override
        public String getAction() {
            return Intent.ACTION_VIEW;
        }
    }

    private static class PickMapping extends BaseMapping {
        @Override
        public String getAction() {
            return Intent.ACTION_GET_CONTENT;
        }
    }

    private static class SendMapping extends BaseMapping {
        @Override
        public String getAction() {
            return Intent.ACTION_SEND;
        }

        @Override
        public void putExtras(JSONObject data, Intent intent) throws JSONException {
            optPutExtra("text", Intent.EXTRA_TEXT, data, intent);
            optPutExtra("html_text", Intent.EXTRA_HTML_TEXT, data, intent);
            optPutExtra("stream", Intent.EXTRA_STREAM, data, intent);
        }
    }

    private static class ViewMapping extends BaseMapping {
        @Override
        public String getAction() {
            return Intent.ACTION_VIEW;
        }

        @Override
        public String getMime(JSONObject data) {
            // MozActivity adds a type 'url' here, we don't want to set the MIME to 'url'.
            String type = data.optString("type", null);
            if ("url".equals(type) || "uri".equals(type)) {
                return null;
            } else {
                return type;
            }
        }
    }

    private static void optPutExtra(String key, String extraName, JSONObject data, Intent intent) {
        final String extraValue = data.optString(key);
        if (!TextUtils.isEmpty(extraValue)) {
            intent.putExtra(extraName, extraValue);
        }
    }
}
