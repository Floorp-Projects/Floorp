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
import android.os.Environment;
import android.provider.MediaStore;
import android.text.TextUtils;
import android.util.Log;

import java.io.File;
import java.util.HashMap;
import java.util.Map;

public final class WebActivityMapper {
    private static final String LOGTAG = "Gecko";

    private static final Map<String, WebActivityMapping> activityMap = new HashMap<String, WebActivityMapping>();
    static {
        activityMap.put("dial", new DialMapping());
        activityMap.put("open", new OpenMapping());
        activityMap.put("pick", new PickMapping());
        activityMap.put("send", new SendMapping());
        activityMap.put("view", new ViewMapping());
        activityMap.put("record", new RecordMapping());
    };

    private static abstract class WebActivityMapping {
        protected JSONObject mData;

        public void setData(JSONObject data) {
            mData = data;
        }

        // Cannot return null
        public abstract String getAction();

        public String getMime() throws JSONException {
            return null;
        }

        public String getUri() throws JSONException {
            return null;
        }

        public void putExtras(Intent intent) throws JSONException {}
    }

    /**
     * Provides useful defaults for mime type and uri.
     */
    private static abstract class BaseMapping extends WebActivityMapping {
        /**
         * If 'type' is present in data object, uses the value as the MIME type.
         */
        @Override
        public String getMime() throws JSONException {
            return mData.optString("type", null);
        }

        /**
         * If 'uri' or 'url' is present in data object, uses the respective value as the Uri.
         */
        @Override
        public String getUri() throws JSONException {
            // Will return uri or url if present.
            String uri = mData.optString("uri", null);
            return uri != null ? uri : mData.optString("url", null);
        }
    }

    public static Intent getIntentForWebActivity(JSONObject message) throws JSONException {
        final String name = message.getString("name").toLowerCase();
        final JSONObject data = message.getJSONObject("data");

        Log.w(LOGTAG, "Activity is: " + name);
        final WebActivityMapping mapping = activityMap.get(name);
        if (mapping == null) {
            Log.w(LOGTAG, "No mapping found!");
            return null;
        }

        mapping.setData(data);

        final Intent intent = new Intent(mapping.getAction());

        final String mime = mapping.getMime();
        if (!TextUtils.isEmpty(mime)) {
            intent.setType(mime);
        }

        final String uri = mapping.getUri();
        if (!TextUtils.isEmpty(uri)) {
            intent.setData(Uri.parse(uri));
        }

        mapping.putExtras(intent);

        return intent;
    }

    private static class DialMapping extends WebActivityMapping {
        @Override
        public String getAction() {
            return Intent.ACTION_DIAL;
        }

        @Override
        public String getUri() throws JSONException {
            return "tel:" + mData.getString("number");
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

        @Override
        public String getMime() throws JSONException {
            // bug 1007112 - pick action needs a mimetype to work
            String mime = mData.optString("type", null);
            return !TextUtils.isEmpty(mime) ? mime : "*/*";
        }
    }

    private static class SendMapping extends BaseMapping {
        @Override
        public String getAction() {
            return Intent.ACTION_SEND;
        }

        @Override
        public void putExtras(Intent intent) throws JSONException {
            optPutExtra("text", Intent.EXTRA_TEXT, intent);
            optPutExtra("html_text", Intent.EXTRA_HTML_TEXT, intent);
            optPutExtra("stream", Intent.EXTRA_STREAM, intent);
        }

        private void optPutExtra(String key, String extraName, Intent intent) {
            final String extraValue = mData.optString(key);
            if (!TextUtils.isEmpty(extraValue)) {
                intent.putExtra(extraName, extraValue);
            }
        }
    }

    private static class ViewMapping extends BaseMapping {
        @Override
        public String getAction() {
            return Intent.ACTION_VIEW;
        }

        @Override
        public String getMime() {
            // MozActivity adds a type 'url' here, we don't want to set the MIME to 'url'.
            String type = mData.optString("type", null);
            if ("url".equals(type) || "uri".equals(type)) {
                return null;
            } else {
                return type;
            }
        }
    }

    private static class RecordMapping extends WebActivityMapping {
        @Override
        public String getAction() {
            String type = mData.optString("type", null);
            if ("photos".equals(type)) {
                return "android.media.action.IMAGE_CAPTURE";
            } else if ("videos".equals(type)) {
                return "android.media.action.VIDEO_CAPTURE";
            }
            return null;
        }

        // Add an extra to specify where to save the picture/video.
        @Override
        public void putExtras(Intent intent) {
            final String action = getAction();

            final String dirType = action == "android.media.action.IMAGE_CAPTURE"
                ? Environment.DIRECTORY_PICTURES
                : Environment.DIRECTORY_MOVIES;

            final String ext = action == "android.media.action.IMAGE_CAPTURE"
                ? ".jpg"
                : ".mp4";

            File destDir = Environment.getExternalStoragePublicDirectory(dirType);

            try {
                File dest = File.createTempFile(
                    "capture", /* prefix */
                    ext,       /* suffix */
                    destDir    /* directory */
                );
                intent.putExtra(MediaStore.EXTRA_OUTPUT, Uri.fromFile(dest));
            } catch (Exception e) {
                Log.w(LOGTAG, "Failed to add extra for " + action + " : " + e);
            }
        }
    }
}
