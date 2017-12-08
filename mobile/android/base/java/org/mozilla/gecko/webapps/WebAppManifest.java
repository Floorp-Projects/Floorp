/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.webapps;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;

import android.annotation.TargetApi;
import android.graphics.Bitmap;
import android.net.Uri;
import android.text.TextUtils;
import android.util.Log;

import org.json.JSONObject;
import org.json.JSONException;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.icons.decoders.FaviconDecoder;
import org.mozilla.gecko.icons.decoders.LoadFaviconResult;
import org.mozilla.gecko.util.ColorUtil;
import org.mozilla.gecko.util.FileUtils;

public class WebAppManifest {
    private static final String LOGTAG = "WebAppManifest";

    private Uri mManifestUri;

    private Integer mThemeColor;
    private String mName;
    private Bitmap mIcon;
    private Uri mScope;
    private Uri mStartUri;
    private String mDisplayMode;
    private String mOrientation;

    public static WebAppManifest fromFile(final String url, final String path) throws IOException, JSONException, IllegalArgumentException {
        if (url == null || TextUtils.isEmpty(url)) {
            throw new IllegalArgumentException("Must pass a non-empty manifest URL");
        }

        if (path == null || TextUtils.isEmpty(path)) {
            throw new IllegalArgumentException("Must pass a non-empty manifest path");
        }

        final Uri manifestUri = Uri.parse(url);
        if (manifestUri == null) {
            throw new IllegalArgumentException("Must pass a valid manifest URL");
        }

        final File manifestFile = new File(path);

        // Gecko adds some add some additional data, such as cached_icon, in
        // the toplevel object. The actual webapp manifest is in the "manifest" field.
        final JSONObject manifest = FileUtils.readJSONObjectFromFile(manifestFile);
        final JSONObject manifestField = manifest.getJSONObject("manifest");

        return new WebAppManifest(manifestUri, manifest, manifestField);
    }

    public static WebAppManifest fromString(final String url, final String input) {
        try {
            final Uri manifestUri = Uri.parse(url);
            final JSONObject manifest = new JSONObject(input);
            final JSONObject manifestField = manifest.getJSONObject("manifest");

            return new WebAppManifest(manifestUri, manifest, manifestField);
        } catch (Exception e) {
            Log.e(LOGTAG, "Failed to read webapp manifest", e);
            return null;
        }
    }

    private WebAppManifest(final Uri uri, final JSONObject manifest, final JSONObject manifestField) {
        mManifestUri = uri;
        readManifest(manifest, manifestField);
    }

    public Integer getThemeColor() {
        return mThemeColor;
    }

    public String getName() {
        return mName;
    }

    public Bitmap getIcon() {
        return mIcon;
    }

    public Uri getScope() {
        return mScope;
    }

    public Uri getStartUri() {
        return mStartUri;
    }

    public String getDisplayMode() {
        return mDisplayMode;
    }

    public String getOrientation() {
        return mOrientation;
    }

    private void readManifest(final JSONObject manifest, final JSONObject manifestField) {
        mThemeColor = readThemeColor(manifestField);
        mName = readName(manifestField);
        mIcon = readIcon(manifest);
        mStartUri = readStartUrl(manifestField);
        mScope = readScope(manifestField);

        mDisplayMode = manifestField.optString("display", null);
        mOrientation = manifestField.optString("orientation", null);
    }

    private Integer readThemeColor(final JSONObject manifest) {
        final String colorStr = manifest.optString("theme_color", null);
        if (colorStr != null) {
            return ColorUtil.parseStringColor(colorStr);
        }
        return null;
    }

    private Uri buildRelativeUrl(final Uri base, final Uri relative) {
        Uri.Builder builder = new Uri.Builder()
            .scheme(base.getScheme())
            .authority(base.getAuthority());

        try {
            // This is pretty gross, but seems to be the easiest way to get
            // a normalized path without java.nio.Path[s], which is
            // only in SDK 26.
            File file = new File(base.getPath() + "/" + relative.getPath());
            builder.path(file.getCanonicalPath());
        } catch (java.io.IOException e) {
            return null;
        }

        return builder.query(relative.getQuery())
            .fragment(relative.getFragment())
            .build();
    }

    private Uri readStartUrl(final JSONObject manifest) {
        Uri startUrl = Uri.parse(manifest.optString("start_url", "/"));
        if (startUrl.isRelative()) {
            startUrl = buildRelativeUrl(stripLastPathSegment(mManifestUri), startUrl);
        }

        return startUrl;
    }

    private String readName(final JSONObject manifest) {
        String name = manifest.optString("name", null);
        if (name == null) {
            name = manifest.optString("short_name", null);
        }
        if (name == null) {
            name = manifest.optString("start_url", null);
        }
        return name;
    }

    private Bitmap readIcon(final JSONObject manifest) {
        final String iconStr = manifest.optString("cached_icon", null);
        if (iconStr == null) {
            return null;
        }
        final LoadFaviconResult loadIconResult = FaviconDecoder
            .decodeDataURI(GeckoAppShell.getApplicationContext(), iconStr);
        if (loadIconResult == null) {
            return null;
        }
        return loadIconResult.getBestBitmap(GeckoAppShell.getPreferredIconSize());
    }

    private static Uri stripPath(final Uri uri) {
        return new Uri.Builder()
            .scheme(uri.getScheme())
            .authority(uri.getAuthority())
            .build();
    }

    private static Uri stripLastPathSegment(final Uri uri) {
        final Uri.Builder builder = new Uri.Builder()
            .scheme(uri.getScheme())
            .authority(uri.getAuthority());

        final List<String> segments = uri.getPathSegments();
        for (int i = 0; i < (segments.size() - 1); i++) {
            builder.appendPath(segments.get(i));
        }

        return builder.build();
    }

    private Uri readScope(final JSONObject manifest) {
        final Uri defaultScope = stripPath(mStartUri);
        final String scopeStr = manifest.optString("scope", null);
        if (scopeStr == null) {
            return defaultScope;
        }

        Uri scope = Uri.parse(scopeStr);
        if (scope == null) {
            return defaultScope;
        }

        if (scope.isRelative()) {
            scope = buildRelativeUrl(stripLastPathSegment(mManifestUri), scope);
        }

        return scope;
    }

    public boolean isInScope(final Uri uri) {
        if (mScope == null) {
            return true;
        }

        if (!uri.getScheme().equals(mScope.getScheme())) {
            return false;
        }

        if (!uri.getHost().equals(mScope.getHost())) {
            return false;
        }

        if (!uri.getPath().startsWith(mScope.getPath())) {
            return false;
        }

        return true;
    }
}
