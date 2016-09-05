/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons;

import android.graphics.Bitmap;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.text.TextUtils;

/**
 * Response object containing a successful loaded icon and meta data.
 */
public class IconResponse {
    /**
     * Create a response for a plain bitmap.
     */
    public static IconResponse create(@NonNull Bitmap bitmap) {
        return new IconResponse(bitmap);
    }

    /**
     * Create a response for a bitmap that has been loaded from the network by requesting a specific URL.
     */
    public static IconResponse createFromNetwork(@NonNull Bitmap bitmap, @NonNull String url) {
        final IconResponse response = new IconResponse(bitmap);
        response.url = url;
        response.fromNetwork = true;
        return response;
    }

    /**
     * Create a response for a generated bitmap with a dominant color.
     */
    public static IconResponse createGenerated(@NonNull Bitmap bitmap, int color) {
        final IconResponse response = new IconResponse(bitmap);
        response.color = color;
        response.generated = true;
        return response;
    }

    /**
     * Create a response for a bitmap that has been loaded from the memory cache.
     */
    public static IconResponse createFromMemory(@NonNull Bitmap bitmap, @NonNull String url, int color) {
        final IconResponse response = new IconResponse(bitmap);
        response.url = url;
        response.color = color;
        response.fromMemory = true;
        return response;
    }

    /**
     * Create a response for a bitmap that has been loaded from the disk cache.
     */
    public static IconResponse createFromDisk(@NonNull Bitmap bitmap, @NonNull String url) {
        final IconResponse response = new IconResponse(bitmap);
        response.url = url;
        response.fromDisk = true;
        return response;
    }

    private Bitmap bitmap;
    private int color;
    private boolean fromNetwork;
    private boolean fromMemory;
    private boolean fromDisk;
    private boolean generated;
    private String url;

    private IconResponse(Bitmap bitmap) {
        if (bitmap == null) {
            throw new NullPointerException("Bitmap is null");
        }

        this.bitmap = bitmap;
        this.color = 0;
        this.url = null;
        this.fromNetwork = false;
        this.fromMemory = false;
        this.fromDisk = false;
        this.generated = false;
    }

    /**
     * Get the icon bitmap. This method will always return a bitmap.
     */
    @NonNull
    public Bitmap getBitmap() {
        return bitmap;
    }

    /**
     * Get the dominant color of the icon. Will return 0 if no color could be extracted.
     */
    public int getColor() {
        return color;
    }

    /**
     * Does this response contain a dominant color?
     */
    public boolean hasColor() {
        return color != 0;
    }

    /**
     * Has this icon been loaded from the network?
     */
    public boolean isFromNetwork() {
        return fromNetwork;
    }

    /**
     * Has this icon been generated?
     */
    public boolean isGenerated() {
        return generated;
    }

    /**
     * Has this icon been loaded from memory (cache)?
     */
    public boolean isFromMemory() {
        return fromMemory;
    }

    /**
     * Has this icon been loaded from disk (cache)?
     */
    public boolean isFromDisk() {
        return fromDisk;
    }

    /**
     * Get the URL this icon has been loaded from.
     */
    @Nullable
    public String getUrl() {
        return url;
    }

    /**
     * Does this response contain an URL from which the icon has been loaded?
     */
    public boolean hasUrl() {
        return !TextUtils.isEmpty(url);
    }

    /**
     * Update the color of this response. This method is called by processors updating meta data
     * after the icon has been loaded.
     */
    public void updateColor(int color) {
        this.color = color;
    }

    /**
     * Update the bitmap of this response. This method is called by processors that modify the
     * loaded icon.
     */
    public void updateBitmap(Bitmap bitmap) {
        this.bitmap = bitmap;
    }
}
