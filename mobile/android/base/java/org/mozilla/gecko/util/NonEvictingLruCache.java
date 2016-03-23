/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.util.LruCache;

import java.util.concurrent.ConcurrentHashMap;

/**
 * An LruCache that also supports a set of items that will never be evicted.
 *
 * Alas, LruCache is final, so we compose rather than inherit.
 */
public class NonEvictingLruCache<K, V> {
    private final ConcurrentHashMap<K, V> permanent = new ConcurrentHashMap<K, V>();
    private final LruCache<K, V> evictable;

    public NonEvictingLruCache(final int evictableSize) {
        evictable = new LruCache<K, V>(evictableSize);
    }

    public V get(K key) {
        V val = permanent.get(key);
        if (val == null) {
            return evictable.get(key);
        }
        return val;
    }

    public void putWithoutEviction(K key, V value) {
        permanent.put(key, value);
    }

    public void put(K key, V value) {
        evictable.put(key, value);
    }

    public void evictAll() {
        evictable.evictAll();
    }
}
