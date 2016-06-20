/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc.catalog;

import android.support.annotation.IntDef;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.StringDef;

public class DownloadContent {
    @IntDef({STATE_NONE, STATE_SCHEDULED, STATE_DOWNLOADED, STATE_FAILED, STATE_UPDATED, STATE_DELETED})
    public @interface State {}
    public static final int STATE_NONE = 0;
    public static final int STATE_SCHEDULED = 1;
    public static final int STATE_DOWNLOADED = 2;
    public static final int STATE_FAILED = 3; // Permanently failed for this version of the content
    public static final int STATE_UPDATED = 4;
    public static final int STATE_DELETED = 5;

    @StringDef({TYPE_ASSET_ARCHIVE})
    public @interface Type {}
    public static final String TYPE_ASSET_ARCHIVE = "asset-archive";

    @StringDef({KIND_FONT, KIND_HYPHENATION_DICTIONARY})
    public @interface Kind {}
    public static final String KIND_FONT = "font";
    public static final String KIND_HYPHENATION_DICTIONARY = "hyphenation";

    private final String id;
    private final String location;
    private final String filename;
    private final String checksum;
    private final String downloadChecksum;
    private final long lastModified;
    private final String type;
    private final String kind;
    private final long size;
    private final String appVersionPattern;
    private final String androidApiPattern;
    private final String appIdPattern;
    private int state;
    private int failures;
    private int lastFailureType;

    /* package-private */ DownloadContent(@NonNull String id, @NonNull String location, @NonNull String filename,
                            @NonNull String checksum, @NonNull String downloadChecksum, @NonNull long lastModified,
                            @NonNull String type, @NonNull String kind, long size, int failures, int lastFailureType,
                            @Nullable String appVersionPattern, @Nullable String androidApiPattern, @Nullable String appIdPattern) {
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
        this.failures = failures;
        this.lastFailureType = lastFailureType;
        this.appVersionPattern = appVersionPattern;
        this.androidApiPattern = androidApiPattern;
        this.appIdPattern = appIdPattern;
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

    public boolean isStateIn(@State int... states) {
        for (int state : states) {
            if (this.state == state) {
                return true;
            }
        }

        return false;
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

    public boolean isHyphenationDictionary() {
        return KIND_HYPHENATION_DICTIONARY.equals(kind);
    }

    /**
     *Checks whether the content to be downloaded is a known content.
     *Currently it checks whether the type is "Asset Archive" and is of kind
     *"Font" or "Hyphenation Dictionary".
     */
    public boolean isKnownContent() {
        return ((isFont() || isHyphenationDictionary()) && isAssetArchive());
    }

    public boolean isAssetArchive() {
        return TYPE_ASSET_ARCHIVE.equals(type);
    }

    /* package-private */ int getFailures() {
        return failures;
    }

    /* package-private */ int getLastFailureType() {
        return lastFailureType;
    }

    /* package-private */ void rememberFailure(int failureType) {
        if (lastFailureType != failureType) {
            lastFailureType = failureType;
            failures = 1;
        } else {
            failures++;
        }
    }

    /* package-private */ void resetFailures() {
        failures = 0;
        lastFailureType = 0;
    }

    public String getAppVersionPattern() {
        return appVersionPattern;
    }

    public String getAndroidApiPattern() {
        return androidApiPattern;
    }

    public String getAppIdPattern() {
        return appIdPattern;
    }

    public DownloadContentBuilder buildUpon() {
        return DownloadContentBuilder.buildUpon(this);
    }


    public String toString() {
        return String.format("[%s,%s] %s (%d bytes) %s", getType(), getKind(), getId(), getSize(), getChecksum());
    }
}
