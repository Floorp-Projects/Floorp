/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons.storage;

import android.graphics.Bitmap;
import android.support.annotation.Nullable;
import android.util.Log;
import android.util.LruCache;

import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;

/**
 * Least Recently Used (LRU) memory cache for icons and the mappings from page URLs to icon URLs.
 */
public class MemoryStorage {
    /**
     * Maximum number of items in the cache for mapping page URLs to icon URLs.
     */
    private static final int MAPPING_CACHE_SIZE = 500;

    private static MemoryStorage instance;

    public static synchronized MemoryStorage get() {
        if (instance == null) {
            instance = new MemoryStorage();
        }

        return instance;
    }

    /**
     * Class representing an cached icon. We store the original bitmap and the color in cache only.
     */
    private static class CacheEntry {
        private final Bitmap bitmap;
        private final int color;

        private CacheEntry(Bitmap bitmap, int color) {
            this.bitmap = bitmap;
            this.color = color;
        }
    }

    private final LruCache<String, CacheEntry> iconCache; // Guarded by 'this'
    private final LruCache<String, String> mappingCache; // Guarded by 'this'

    private MemoryStorage() {
        iconCache = new LruCache<String, CacheEntry>(calculateCacheSize()) {
            @Override
            protected int sizeOf(String key, CacheEntry value) {
                return value.bitmap.getByteCount() / 1024;
            }
        };

        mappingCache = new LruCache<>(MAPPING_CACHE_SIZE);
    }

    private int calculateCacheSize() {
        // Use a maximum of 1/8 of the available memory for storing cached icons.
        int maxMemory = (int) (Runtime.getRuntime().maxMemory() / 1024);
        return maxMemory / 8;
    }

    /**
     * Store a mapping from page URL to icon URL in the cache.
     */
    public synchronized void putMapping(IconRequest request, String iconUrl) {
        mappingCache.put(request.getPageUrl(), iconUrl);
    }

    /**
     * Get the icon URL for this page URL. Returns null if no mapping is in the cache.
     */
    @Nullable
    public synchronized String getMapping(String pageUrl) {
        return mappingCache.get(pageUrl);
    }

    /**
     * Store an icon in the cache (uses the icon URL as key).
     */
    public synchronized void putIcon(String url, IconResponse response) {
        final CacheEntry entry = new CacheEntry(response.getBitmap(), response.getColor());

        iconCache.put(url, entry);
    }

    /**
     * Get an icon for the icon URL from the cache. Returns null if no icon is cached for this URL.
     */
    @Nullable
    public synchronized IconResponse getIcon(String iconUrl) {
        final CacheEntry entry = iconCache.get(iconUrl);
        if (entry == null) {
            return null;
        }

        return IconResponse.createFromMemory(entry.bitmap, iconUrl, entry.color);
    }

    /**
     * Remove all entries from this cache.
     */
    public synchronized void evictAll() {
        iconCache.evictAll();
        mappingCache.evictAll();
    }
}
