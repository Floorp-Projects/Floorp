/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.support.annotation.Nullable;

import java.util.Map;

/** Utils for {@link java.util.Map} and friends. */
public class MapUtils {
    private MapUtils() {}

    // impl via https://developer.android.com/reference/java/util/Map.html#putIfAbsent(K,%20V)
    // note: this method is available in API 24.
    @Nullable
    public static <K, V> V putIfAbsent(final Map<K, V> map, final K key, final V value) {
        V v = map.get(key);
        if (v == null) {
            v = map.put(key, value);
        }

        return v;
    }
}
