/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc.catalog;

import android.support.annotation.IntDef;
import android.support.annotation.NonNull;
import android.support.annotation.StringDef;

import org.json.JSONException;
import org.json.JSONObject;

public class DownloadContent {
    private static final String KEY_ID = "id";
    private static final String KEY_LOCATION = "location";
    private static final String KEY_FILENAME = "filename";
    private static final String KEY_CHECKSUM = "checksum";
    private static final String KEY_DOWNLOAD_CHECKSUM = "download_checksum";
    private static final String KEY_LAST_MODIFIED = "last_modified";
    private static final String KEY_TYPE = "type";
    private static final String KEY_KIND = "kind";
    private static final String KEY_SIZE = "size";
    private static final String KEY_STATE = "state";

    @IntDef({STATE_NONE, STATE_SCHEDULED, STATE_DOWNLOADED, STATE_FAILED, STATE_IGNORED})
    public @interface State {}
    public static final int STATE_NONE = 0;
    public static final int STATE_SCHEDULED = 1;
    public static final int STATE_DOWNLOADED = 2;
    public static final int STATE_FAILED = 3; // Permanently failed for this version of the content
    public static final int STATE_IGNORED = 4;

    @StringDef({TYPE_ASSET_ARCHIVE})
    public @interface Type {}
    public static final String TYPE_ASSET_ARCHIVE = "asset-archive";

    @StringDef({KIND_FONT})
    public @interface Kind {}
    public static final String KIND_FONT = "font";

    private final String id;
    private final String location;
    private final String filename;
    private final String checksum;
    private final String downloadChecksum;
    private final long lastModified;
    private final String type;
    private final String kind;
    private final long size;
    private int state;

    private DownloadContent(@NonNull String id, @NonNull String location, @NonNull String filename,
                            @NonNull String checksum, @NonNull String downloadChecksum, @NonNull long lastModified,
                            @NonNull String type, @NonNull String kind, long size) {
        this.id = id;
        this.location = location;
        this.filename = filename;
        this.checksum = checksum;
        this.downloadChecksum = downloadChecksum;
        this.lastModified = lastModified;
        this.type = type;
        this.kind = kind;
        this.size = size;
        this.state = STATE_NONE;
    }

    public String getId() {
        return id;
    }

    /* package-private */ void setState(@State int state) {
        this.state = state;
    }

    @State
    public int getState() {
        return state;
    }

    @Kind
    public String getKind() {
        return kind;
    }

    @Type
    public String getType() {
        return type;
    }

    public String getLocation() {
        return location;
    }

    public long getLastModified() {
        return lastModified;
    }

    public String getFilename() {
        return filename;
    }

    public String getChecksum() {
        return checksum;
    }

    public String getDownloadChecksum() {
        return downloadChecksum;
    }

    public long getSize() {
        return size;
    }

    public boolean isFont() {
        return KIND_FONT.equals(kind);
    }

    public boolean isAssetArchive() {
        return TYPE_ASSET_ARCHIVE.equals(type);
    }

    public static DownloadContent fromJSON(JSONObject object) throws JSONException {
        return new Builder()
                .setId(object.getString(KEY_ID))
                .setLocation(object.getString(KEY_LOCATION))
                .setFilename(object.getString(KEY_FILENAME))
                .setChecksum(object.getString(KEY_CHECKSUM))
                .setDownloadChecksum(object.getString(KEY_DOWNLOAD_CHECKSUM))
                .setLastModified(object.getLong(KEY_LAST_MODIFIED))
                .setType(object.getString(KEY_TYPE))
                .setKind(object.getString(KEY_KIND))
                .setSize(object.getLong(KEY_SIZE))
                .setState(object.getInt(KEY_STATE))
                .build();
    }

    public JSONObject toJSON() throws JSONException {
        JSONObject object = new JSONObject();
        object.put(KEY_ID, id);
        object.put(KEY_LOCATION, location);
        object.put(KEY_FILENAME, filename);
        object.put(KEY_CHECKSUM, checksum);
        object.put(KEY_DOWNLOAD_CHECKSUM, downloadChecksum);
        object.put(KEY_LAST_MODIFIED, lastModified);
        object.put(KEY_TYPE, type);
        object.put(KEY_KIND, kind);
        object.put(KEY_SIZE, size);
        object.put(KEY_STATE, state);
        return object;
    }

    public String toString() {
        return String.format("[%s,%s] %s (%d bytes) %s", getType(), getKind(), getId(), getSize(), getChecksum());
    }

    public static class Builder {
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

        public DownloadContent build() {
            DownloadContent content = new DownloadContent(id, location, filename, checksum, downloadChecksum,
                                                          lastModified, type, kind, size);
            content.setState(state);
            return content;
        }

        public Builder setId(String id) {
            this.id = id;
            return this;
        }

        public Builder setLocation(String location) {
            this.location = location;
            return this;
        }

        public Builder setFilename(String filename) {
            this.filename = filename;
            return this;
        }

        public Builder setChecksum(String checksum) {
            this.checksum = checksum;
            return this;
        }

        public Builder setDownloadChecksum(String downloadChecksum) {
            this.downloadChecksum = downloadChecksum;
            return this;
        }

        public Builder setLastModified(long lastModified) {
            this.lastModified = lastModified;
            return this;
        }

        public Builder setType(String type) {
            this.type = type;
            return this;
        }

        public Builder setKind(String kind) {
            this.kind = kind;
            return this;
        }

        public Builder setSize(long size) {
            this.size = size;
            return this;
        }

        public Builder setState(int state) {
            this.state = state;
            return this;
        }
    }
}
