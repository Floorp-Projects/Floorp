/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc.catalog;

import android.text.TextUtils;

import org.json.JSONException;
import org.json.JSONObject;

public class DownloadContentBuilder {
    private static final String LOCAL_KEY_ID = "id";
    private static final String LOCAL_KEY_LOCATION = "location";
    private static final String LOCAL_KEY_FILENAME = "filename";
    private static final String LOCAL_KEY_CHECKSUM = "checksum";
    private static final String LOCAL_KEY_DOWNLOAD_CHECKSUM = "download_checksum";
    private static final String LOCAL_KEY_LAST_MODIFIED = "last_modified";
    private static final String LOCAL_KEY_TYPE = "type";
    private static final String LOCAL_KEY_KIND = "kind";
    private static final String LOCAL_KEY_SIZE = "size";
    private static final String LOCAL_KEY_STATE = "state";
    private static final String LOCAL_KEY_FAILURES = "failures";
    private static final String LOCAL_KEY_LAST_FAILURE_TYPE = "last_failure_type";
    private static final String LOCAL_KEY_PATTERN_APP_ID = "pattern_app_id";
    private static final String LOCAL_KEY_PATTERN_ANDROID_API = "pattern_android_api";
    private static final String LOCAL_KEY_PATTERN_APP_VERSION = "pattern_app_version";

    private static final String KINTO_KEY_ID = "id";
    private static final String KINTO_KEY_ATTACHMENT = "attachment";
    private static final String KINTO_KEY_ORIGINAL = "original";
    private static final String KINTO_KEY_LOCATION = "location";
    private static final String KINTO_KEY_FILENAME = "filename";
    private static final String KINTO_KEY_HASH = "hash";
    private static final String KINTO_KEY_LAST_MODIFIED = "last_modified";
    private static final String KINTO_KEY_TYPE = "type";
    private static final String KINTO_KEY_KIND = "kind";
    private static final String KINTO_KEY_SIZE = "size";
    private static final String KINTO_KEY_MATCH = "match";
    private static final String KINTO_KEY_APP_ID = "appId";
    private static final String KINTO_KEY_ANDROID_API = "androidApi";
    private static final String KINTO_KEY_APP_VERSION = "appVersion";

    private String id;
    private String location;
    private String filename;
    private String checksum;
    private String downloadChecksum;
    private long lastModified;
    private String type;
    private String kind;
    private long size;
    private int state;
    private int failures;
    private int lastFailureType;
    private String appVersionPattern;
    private String androidApiPattern;
    private String appIdPattern;

    public static DownloadContentBuilder buildUpon(DownloadContent content) {
        DownloadContentBuilder builder = new DownloadContentBuilder();

        builder.id = content.getId();
        builder.location = content.getLocation();
        builder.filename = content.getFilename();
        builder.checksum = content.getChecksum();
        builder.downloadChecksum = content.getDownloadChecksum();
        builder.lastModified = content.getLastModified();
        builder.type = content.getType();
        builder.kind = content.getKind();
        builder.size = content.getSize();
        builder.state = content.getState();
        builder.failures = content.getFailures();
        builder.lastFailureType = content.getLastFailureType();

        return builder;
    }

    public static DownloadContent fromJSON(JSONObject object) throws JSONException {
        return new DownloadContentBuilder()
                .setId(object.getString(LOCAL_KEY_ID))
                .setLocation(object.getString(LOCAL_KEY_LOCATION))
                .setFilename(object.getString(LOCAL_KEY_FILENAME))
                .setChecksum(object.getString(LOCAL_KEY_CHECKSUM))
                .setDownloadChecksum(object.getString(LOCAL_KEY_DOWNLOAD_CHECKSUM))
                .setLastModified(object.getLong(LOCAL_KEY_LAST_MODIFIED))
                .setType(object.getString(LOCAL_KEY_TYPE))
                .setKind(object.getString(LOCAL_KEY_KIND))
                .setSize(object.getLong(LOCAL_KEY_SIZE))
                .setState(object.getInt(LOCAL_KEY_STATE))
                .setFailures(object.optInt(LOCAL_KEY_FAILURES), object.optInt(LOCAL_KEY_LAST_FAILURE_TYPE))
                .setAppVersionPattern(object.optString(LOCAL_KEY_PATTERN_APP_VERSION))
                .setAppIdPattern(object.optString(LOCAL_KEY_PATTERN_APP_ID))
                .setAndroidApiPattern(object.optString(LOCAL_KEY_PATTERN_ANDROID_API))
                .build();
    }

    public static JSONObject toJSON(DownloadContent content) throws JSONException {
        final JSONObject object = new JSONObject();
        object.put(LOCAL_KEY_ID, content.getId());
        object.put(LOCAL_KEY_LOCATION, content.getLocation());
        object.put(LOCAL_KEY_FILENAME, content.getFilename());
        object.put(LOCAL_KEY_CHECKSUM, content.getChecksum());
        object.put(LOCAL_KEY_DOWNLOAD_CHECKSUM, content.getDownloadChecksum());
        object.put(LOCAL_KEY_LAST_MODIFIED, content.getLastModified());
        object.put(LOCAL_KEY_TYPE, content.getType());
        object.put(LOCAL_KEY_KIND, content.getKind());
        object.put(LOCAL_KEY_SIZE, content.getSize());
        object.put(LOCAL_KEY_STATE, content.getState());
        object.put(LOCAL_KEY_PATTERN_APP_VERSION, content.getAppVersionPattern());
        object.put(LOCAL_KEY_PATTERN_APP_ID, content.getAppIdPattern());
        object.put(LOCAL_KEY_PATTERN_ANDROID_API, content.getAndroidApiPattern());

        final int failures = content.getFailures();
        if (failures > 0) {
            object.put(LOCAL_KEY_FAILURES, failures);
            object.put(LOCAL_KEY_LAST_FAILURE_TYPE, content.getLastFailureType());
        }

        return object;
    }

    public DownloadContent build() {
        DownloadContent content = new DownloadContent(id, location, filename, checksum,
                downloadChecksum, lastModified, type, kind, size, failures, lastFailureType,
                appVersionPattern, androidApiPattern, appIdPattern);
        content.setState(state);

        return content;
    }

    public DownloadContentBuilder setId(String id) {
        this.id = id;
        return this;
    }

    public DownloadContentBuilder setLocation(String location) {
        this.location = location;
        return this;
    }

    public DownloadContentBuilder setFilename(String filename) {
        this.filename = filename;
        return this;
    }

    public DownloadContentBuilder setChecksum(String checksum) {
        this.checksum = checksum;
        return this;
    }

    public DownloadContentBuilder setDownloadChecksum(String downloadChecksum) {
        this.downloadChecksum = downloadChecksum;
        return this;
    }

    public DownloadContentBuilder setLastModified(long lastModified) {
        this.lastModified = lastModified;
        return this;
    }

    public DownloadContentBuilder setType(String type) {
        this.type = type;
        return this;
    }

    public DownloadContentBuilder setKind(String kind) {
        this.kind = kind;
        return this;
    }

    public DownloadContentBuilder setSize(long size) {
        this.size = size;
        return this;
    }

    public DownloadContentBuilder setState(int state) {
        this.state = state;
        return this;
    }

    /* package-private */ DownloadContentBuilder setFailures(int failures, int lastFailureType) {
        this.failures = failures;
        this.lastFailureType = lastFailureType;

        return this;
    }

    public DownloadContentBuilder setAppVersionPattern(String appVersionPattern) {
        this.appVersionPattern = appVersionPattern;
        return this;
    }

    public DownloadContentBuilder setAndroidApiPattern(String androidApiPattern) {
        this.androidApiPattern = androidApiPattern;
        return this;
    }

    public DownloadContentBuilder setAppIdPattern(String appIdPattern) {
        this.appIdPattern = appIdPattern;
        return this;
    }

    public DownloadContentBuilder updateFromKinto(JSONObject object)  throws JSONException {
        final String objectId = object.getString(KINTO_KEY_ID);

        if (TextUtils.isEmpty(id)) {
            // New object without an id yet
            id = objectId;
        } else if (!id.equals(objectId)) {
            throw new JSONException(String.format("Record ids do not match: Expected=%s, Actual=%s", id, objectId));
        }

        setType(object.getString(KINTO_KEY_TYPE));
        setKind(object.getString(KINTO_KEY_KIND));
        setLastModified(object.getLong(KINTO_KEY_LAST_MODIFIED));

        JSONObject attachment = object.getJSONObject(KINTO_KEY_ATTACHMENT);
        JSONObject original = attachment.getJSONObject(KINTO_KEY_ORIGINAL);

        setFilename(original.getString(KINTO_KEY_FILENAME));
        setChecksum(original.getString(KINTO_KEY_HASH));
        setSize(original.getLong(KINTO_KEY_SIZE));

        setLocation(attachment.getString(KINTO_KEY_LOCATION));
        setDownloadChecksum(attachment.getString(KINTO_KEY_HASH));

        JSONObject match = object.optJSONObject(KINTO_KEY_MATCH);
        if (match != null) {
            setAndroidApiPattern(match.optString(KINTO_KEY_ANDROID_API));
            setAppIdPattern(match.optString(KINTO_KEY_APP_ID));
            setAppVersionPattern(match.optString(KINTO_KEY_APP_VERSION));
        }

        return this;
    }
}
