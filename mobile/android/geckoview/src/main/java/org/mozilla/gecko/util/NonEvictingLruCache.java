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
    private final ConcurrentHashMap<K, V> mPermanent = new ConcurrentHashMap<K, V>();
    private final LruCache<K, V> mEvitable;

    public NonEvictingLruCache(final int evictableSize) {
        mEvitable = new LruCache<K, V>(evictableSize);
    }

    public V get(final K key) {
        V val = mPermanent.get(key);
        if (val == null) {
            return mEvitable.get(key);
        }
        return val;
    }

    public void putWithoutEviction(final K key, final V value) {
        mPermanent.put(key, value);
    }

    public void put(final K key, final V value) {
        mEvitable.put(key, value);
    }

    public void evictAll() {
        mEvitable.evictAll();
    }
}
