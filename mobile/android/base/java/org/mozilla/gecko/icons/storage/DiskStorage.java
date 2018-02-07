/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons.storage;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.support.annotation.CheckResult;
import android.support.annotation.Nullable;
import android.util.Log;

import com.jakewharton.disklrucache.DiskLruCache;

import org.mozilla.gecko.background.nativecode.NativeCrypto;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.util.IOUtils;
import org.mozilla.gecko.util.StringUtils;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.security.MessageDigest;

/**
 * Least Recently Used (LRU) disk cache for icons and the mappings from page URLs to icon URLs.
 */
public class DiskStorage {
    private static final String LOGTAG = "Gecko/DiskStorage";

    /**
     * Maximum size (in bytes) of the cache. This cache is located in the cache directory of the
     * application and can be cleared by the user.
     */
    private static final int DISK_CACHE_SIZE = 50 * 1024 * 1024;

    /**
     * Version of the cache. Updating the version will invalidate all existing items.
     */
    private static final int CACHE_VERSION = 1;

    private static final String KEY_PREFIX_ICON = "icon:";
    private static final String KEY_PREFIX_MAPPING = "mapping:";

    private static DiskStorage instance;

    public static DiskStorage get(Context context) {
        if (instance == null) {
            instance = new DiskStorage(context);
        }

        return instance;
    }

    private Context context;
    private DiskLruCache cache;

    private DiskStorage(Context context) {
        this.context = context.getApplicationContext();
    }

    @CheckResult
    private synchronized DiskLruCache ensureCacheIsReady() throws IOException {
        if (cache == null || cache.isClosed()) {
            cache = DiskLruCache.open(
                    new File(context.getCacheDir(), "icons"),
                    CACHE_VERSION,
                    1,
                    DISK_CACHE_SIZE);
        }

        return cache;
    }

    /**
     * Store a mapping from page URL to icon URL in the cache.
     */
    public void putMapping(IconRequest request, String iconUrl) {
        putMapping(request.getPageUrl(), iconUrl);
    }

    /**
     * Store a mapping from page URL to icon URL in the cache.
     */
    public void putMapping(String pageUrl, String iconUrl) {
        DiskLruCache.Editor editor = null;

        try {
            final DiskLruCache cache = ensureCacheIsReady();

            final String key = createKey(KEY_PREFIX_MAPPING, pageUrl);
            if (key == null) {
                return;
            }

            editor = cache.edit(key);
            if (editor == null) {
                return;
            }

            editor.set(0, iconUrl);
            editor.commit();
        } catch (IOException e) {
            Log.w(LOGTAG, "IOException while accessing disk cache", e);

            abortSilently(editor);
        }
    }

    /**
     * Store an icon in the cache (uses the icon URL as key).
     */
    public void putIcon(IconResponse response) {
        putIcon(response.getUrl(), response.getBitmap());
    }

    /**
     * Store an icon in the cache (uses the icon URL as key).
     */
    public void putIcon(String iconUrl, Bitmap bitmap) {
        OutputStream outputStream = null;
        DiskLruCache.Editor editor = null;

        try {
            final DiskLruCache cache = ensureCacheIsReady();

            final String key = createKey(KEY_PREFIX_ICON, iconUrl);
            if (key == null) {
                return;
            }

            editor = cache.edit(key);
            if (editor == null) {
                return;
            }

            outputStream = editor.newOutputStream(0);
            boolean success = bitmap.compress(Bitmap.CompressFormat.PNG, 100 /* quality; ignored. PNG is lossless */, outputStream);

            outputStream.close();

            if (success) {
                editor.commit();
            } else {
                editor.abort();
            }
        } catch (IOException e) {
            Log.w(LOGTAG, "IOException while accessing disk cache", e);

            abortSilently(editor);
        } finally {
            IOUtils.safeStreamClose(outputStream);
        }
    }



    /**
     * Get an icon for the icon URL from the cache. Returns null if no icon is cached for this URL.
     */
    @Nullable
    public IconResponse getIcon(String iconUrl) {
        InputStream inputStream = null;

        try {
            final DiskLruCache cache = ensureCacheIsReady();

            final String key = createKey(KEY_PREFIX_ICON, iconUrl);
            if (key == null) {
                return null;
            }

            if (cache.isClosed()) {
                throw new RuntimeException("CLOSED");
            }

            final DiskLruCache.Snapshot snapshot = cache.get(key);
            if (snapshot == null) {
                return null;
            }

            inputStream = snapshot.getInputStream(0);

            final Bitmap bitmap = BitmapFactory.decodeStream(inputStream);
            if (bitmap == null) {
                return null;
            }

            return IconResponse.createFromDisk(bitmap, iconUrl);
        } catch (IOException e) {
            Log.w(LOGTAG, "IOException while accessing disk cache", e);
        } finally {
            IOUtils.safeStreamClose(inputStream);
        }

        return null;
    }

    /**
     * Get the icon URL for this page URL. Returns null if no mapping is in the cache.
     */
    @Nullable
    public String getMapping(String pageUrl) {
        try {
            final DiskLruCache cache = ensureCacheIsReady();

            final String key = createKey(KEY_PREFIX_MAPPING, pageUrl);
            if (key == null) {
                return null;
            }

            DiskLruCache.Snapshot snapshot = cache.get(key);
            if (snapshot == null) {
                return null;
            }

            return snapshot.getString(0);
        } catch (IOException e) {
            Log.w(LOGTAG, "IOException while accessing disk cache", e);
        }

        return null;
    }

    /**
     * Remove all entries from this cache.
     */
    public void evictAll() {
        try {
            final DiskLruCache cache = ensureCacheIsReady();

            cache.delete();

        } catch (IOException e) {
            Log.w(LOGTAG, "IOException while accessing disk cache", e);
        }
    }

    /**
     * Create a key for this URL using the given prefix.
     *
     * The disk cache requires valid file names to be used as key. Therefore we hash the created key
     * (SHA-256).
     */
    @Nullable
    private String createKey(String prefix, String url) {
        try {
            // We use our own crypto implementation to avoid the penalty of loading the java crypto
            // framework.
            byte[] ctx = NativeCrypto.sha256init();
            if (ctx == null) {
                return null;
            }

            byte[] data = prefix.getBytes(StringUtils.UTF_8);
            NativeCrypto.sha256update(ctx, data, data.length);

            data = url.getBytes(StringUtils.UTF_8);
            NativeCrypto.sha256update(ctx, data, data.length);
            return Utils.byte2Hex(NativeCrypto.sha256finalize(ctx));
        } catch (NoClassDefFoundError | ExceptionInInitializerError error) {
            // We could not load libmozglue.so. Let's use Java's MessageDigest as fallback. We do
            // this primarily for our unit tests that can't load native libraries. On an device
            // we will have a lot of other problems if we can't load libmozglue.so
            try {
                MessageDigest md = MessageDigest.getInstance("SHA-256");
                md.update(prefix.getBytes(StringUtils.UTF_8));
                md.update(url.getBytes(StringUtils.UTF_8));
                return Utils.byte2Hex(md.digest());
            } catch (Exception e) {
                // Just give up. And let everyone know.
                throw new RuntimeException(e);
            }
        }
    }

    private void abortSilently(DiskLruCache.Editor editor) {
        if (editor != null) {
            try {
                editor.abort();
            } catch (IOException e) {
                // Ignore
            }
        }
    }
}
