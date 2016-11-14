/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.annotation.WrapForJNI;

import android.support.v4.util.SimpleArrayMap;

import java.lang.reflect.Array;
import java.util.Set;

/**
 * A lighter-weight version of Bundle that adds support for type coercion (e.g.
 * int to double) in order to better cooperate with JS objects.
 */
@RobocopTarget
public final class GeckoBundle {
    private static final String LOGTAG = "GeckoBundle";
    private static final boolean DEBUG = false;

    @WrapForJNI(calledFrom = "gecko")
    private static final boolean[] EMPTY_BOOLEAN_ARRAY = new boolean[0];
    private static final int[] EMPTY_INT_ARRAY = new int[0];
    private static final double[] EMPTY_DOUBLE_ARRAY = new double[0];
    private static final String[] EMPTY_STRING_ARRAY = new String[0];
    private static final GeckoBundle[] EMPTY_BUNDLE_ARRAY = new GeckoBundle[0];

    @WrapForJNI(calledFrom = "gecko")
    private static Object box(boolean b) { return b; }
    @WrapForJNI(calledFrom = "gecko")
    private static Object box(int i) { return i; }
    @WrapForJNI(calledFrom = "gecko")
    private static Object box(double d) { return d; }
    @WrapForJNI(calledFrom = "gecko")
    private static boolean unboxBoolean(Boolean b) { return b; }
    @WrapForJNI(calledFrom = "gecko")
    private static int unboxInteger(Integer i) { return i; }
    @WrapForJNI(calledFrom = "gecko")
    private static double unboxDouble(Double d) { return d; }

    private SimpleArrayMap<String, Object> mMap;

    /**
     * Construct an empty GeckoBundle.
     */
    public GeckoBundle() {
        mMap = new SimpleArrayMap<>();
    }

    /**
     * Construct an empty GeckoBundle with specific capacity.
     *
     * @param capacity Initial capacity.
     */
    public GeckoBundle(final int capacity) {
        mMap = new SimpleArrayMap<>(capacity);
    }

    /**
     * Construct a copy of another GeckoBundle.
     *
     * @param bundle GeckoBundle to copy from.
     */
    public GeckoBundle(final GeckoBundle bundle) {
        mMap = new SimpleArrayMap<>(bundle.mMap);
    }

    @WrapForJNI(calledFrom = "gecko")
    private GeckoBundle(final String[] keys, final Object[] values) {
        final int len = keys.length;
        mMap = new SimpleArrayMap<>(len);
        for (int i = 0; i < len; i++) {
            mMap.put(keys[i], values[i]);
        }
    }

    /**
     * Clear all mappings.
     */
    public void clear() {
        mMap.clear();
    }

    /**
     * Returns whether a mapping exists.
     *
     * @param key Key to look for.
     * @return True if the specified key exists.
     */
    public boolean containsKey(final String key) {
        return mMap.containsKey(key) && mMap.get(key) != null;
    }

    /**
     * Returns the value associated with a mapping as an Object.
     *
     * @param key Key to look for.
     * @return Mapping value or null if the mapping does not exist.
     */
    public Object get(final String key) {
        return mMap.get(key);
    }

    /**
     * Returns the value associated with a boolean mapping, or defaultValue if the mapping
     * does not exist.
     *
     * @param key Key to look for.
     * @param defaultValue Value to return if mapping does not exist.
     * @return Boolean value
     */
    public boolean getBoolean(final String key, final boolean defaultValue) {
        final Object value = mMap.get(key);
        return value == null ? defaultValue : (Boolean) value;
    }

    /**
     * Returns the value associated with a boolean mapping, or false if the mapping does
     * not exist.
     *
     * @param key Key to look for.
     * @return Boolean value
     */
    public boolean getBoolean(final String key) {
        return getBoolean(key, false);
    }

    /**
     * Returns the value associated with a boolean array mapping, or null if the mapping
     * does not exist.
     *
     * @param key Key to look for.
     * @return Boolean array value
     */
    public boolean[] getBooleanArray(final String key) {
        final Object value = mMap.get(key);
        return Array.getLength(value) == 0 ? EMPTY_BOOLEAN_ARRAY : (boolean[]) value;
    }

    /**
     * Returns the value associated with a double mapping, or defaultValue if the mapping
     * does not exist.
     *
     * @param key Key to look for.
     * @param defaultValue Value to return if mapping does not exist.
     * @return Double value
     */
    public double getDouble(final String key, final double defaultValue) {
        final Object value = mMap.get(key);
        return value == null ? defaultValue : ((Number) value).doubleValue();
    }

    /**
     * Returns the value associated with a double mapping, or 0.0 if the mapping does not
     * exist.
     *
     * @param key Key to look for.
     * @return Double value
     */
    public double getDouble(final String key) {
        return getDouble(key, 0.0);
    }

    private double[] getDoubleArray(final int[] array) {
        final int len = array.length;
        final double[] ret = new double[len];
        for (int i = 0; i < len; i++) {
            ret[i] = (double) array[i];
        }
        return ret;
    }

    /**
     * Returns the value associated with a double array mapping, or null if the mapping
     * does not exist.
     *
     * @param key Key to look for.
     * @return Double array value
     */
    public double[] getDoubleArray(final String key) {
        final Object value = mMap.get(key);
        return Array.getLength(value) == 0 ? EMPTY_DOUBLE_ARRAY :
               value instanceof int[] ? getDoubleArray((int[]) value) : (double[]) value;
    }

    /**
     * Returns the value associated with an int mapping, or defaultValue if the mapping
     * does not exist.
     *
     * @param key Key to look for.
     * @param defaultValue Value to return if mapping does not exist.
     * @return Int value
     */
    public int getInt(final String key, final int defaultValue) {
        final Object value = mMap.get(key);
        return value == null ? defaultValue : ((Number) value).intValue();
    }

    /**
     * Returns the value associated with an int mapping, or 0 if the mapping does not
     * exist.
     *
     * @param key Key to look for.
     * @return Int value
     */
    public int getInt(final String key) {
        return getInt(key, 0);
    }

    private int[] getIntArray(final double[] array) {
        final int len = array.length;
        final int[] ret = new int[len];
        for (int i = 0; i < len; i++) {
            ret[i] = (int) array[i];
        }
        return ret;
    }

    /**
     * Returns the value associated with an int array mapping, or null if the mapping does
     * not exist.
     *
     * @param key Key to look for.
     * @return Int array value
     */
    public int[] getIntArray(final String key) {
        final Object value = mMap.get(key);
        return Array.getLength(value) == 0 ? EMPTY_INT_ARRAY :
               value instanceof double[] ? getIntArray((double[]) value) : (int[]) value;
    }

    /**
     * Returns the value associated with a String mapping, or defaultValue if the mapping
     * does not exist.
     *
     * @param key Key to look for.
     * @param defaultValue Value to return if mapping does not exist.
     * @return String value
     */
    public String getString(final String key, final String defaultValue) {
        final Object value = mMap.get(key);
        return value == null ? defaultValue : (String) value;
    }

    /**
     * Returns the value associated with a String mapping, or null if the mapping does not
     * exist.
     *
     * @param key Key to look for.
     * @return String value
     */
    public String getString(final String key) {
        return getString(key, null);
    }

    // The only case where we convert String[] to/from GeckoBundle[] is if every element
    // is null.
    private int getNullArrayLength(final Object array) {
        final int len = Array.getLength(array);
        for (int i = 0; i < len; i++) {
            if (Array.get(array, i) != null) {
                throw new ClassCastException("Cannot cast array type");
            }
        }
        return len;
    }

    /**
     * Returns the value associated with a String array mapping, or null if the mapping
     * does not exist.
     *
     * @param key Key to look for.
     * @return String array value
     */
    public String[] getStringArray(final String key) {
        final Object value = mMap.get(key);
        return Array.getLength(value) == 0 ? EMPTY_STRING_ARRAY :
               !(value instanceof String[]) ? new String[getNullArrayLength(value)] :
                                              (String[]) value;
    }

    /**
     * Returns the value associated with a GeckoBundle mapping, or null if the mapping
     * does not exist.
     *
     * @param key Key to look for.
     * @return GeckoBundle value
     */
    public GeckoBundle getBundle(final String key) {
        return (GeckoBundle) mMap.get(key);
    }

    /**
     * Returns the value associated with a GeckoBundle array mapping, or null if the
     * mapping does not exist.
     *
     * @param key Key to look for.
     * @return GeckoBundle array value
     */
    public GeckoBundle[] getBundleArray(final String key) {
        final Object value = mMap.get(key);
        return Array.getLength(value) == 0 ? EMPTY_BUNDLE_ARRAY :
               !(value instanceof GeckoBundle[]) ? new GeckoBundle[getNullArrayLength(value)] :
                                                   (GeckoBundle[]) value;
    }

    /**
     * Returns whether this GeckoBundle has no mappings.
     *
     * @return True if no mapping exists.
     */
    public boolean isEmpty() {
        return mMap.isEmpty();
    }

    /**
     * Returns an array of all mapped keys.
     *
     * @return String array containing all mapped keys.
     */
    @WrapForJNI(calledFrom = "gecko")
    public String[] keys() {
        final int len = mMap.size();
        final String[] ret = new String[len];
        for (int i = 0; i < len; i++) {
            ret[i] = mMap.keyAt(i);
        }
        return ret;
    }

    @WrapForJNI(calledFrom = "gecko")
    private Object[] values() {
        final int len = mMap.size();
        final Object[] ret = new Object[len];
        for (int i = 0; i < len; i++) {
            ret[i] = mMap.valueAt(i);
        }
        return ret;
    }

    /**
     * @hide
     * XXX: temporary helper for converting Bundle to GeckoBundle.
     */
    public void put(final String key, final Object value) {
        mMap.put(key, value);
    }

    /**
     * Map a key to a boolean value.
     *
     * @param key Key to map.
     * @param value Value to map to.
     */
    public void putBoolean(final String key, final boolean value) {
        mMap.put(key, value);
    }

    /**
     * Map a key to a boolean array value.
     *
     * @param key Key to map.
     * @param value Value to map to.
     */
    public void putBooleanArray(final String key, final boolean[] value) {
        mMap.put(key, value);
    }

    /**
     * Map a key to a double value.
     *
     * @param key Key to map.
     * @param value Value to map to.
     */
    public void putDouble(final String key, final double value) {
        mMap.put(key, value);
    }

    /**
     * Map a key to a double array value.
     *
     * @param key Key to map.
     * @param value Value to map to.
     */
    public void putDoubleArray(final String key, final double[] value) {
        mMap.put(key, value);
    }

    /**
     * Map a key to an int value.
     *
     * @param key Key to map.
     * @param value Value to map to.
     */
    public void putInt(final String key, final int value) {
        mMap.put(key, value);
    }

    /**
     * Map a key to an int array value.
     *
     * @param key Key to map.
     * @param value Value to map to.
     */
    public void putIntArray(final String key, final int[] value) {
        mMap.put(key, value);
    }

    /**
     * Map a key to a String value.
     *
     * @param key Key to map.
     * @param value Value to map to.
     */
    public void putString(final String key, final String value) {
        mMap.put(key, value);
    }

    /**
     * Map a key to a String array value.
     *
     * @param key Key to map.
     * @param value Value to map to.
     */
    public void putStringArray(final String key, final String[] value) {
        mMap.put(key, value);
    }

    /**
     * Map a key to a GeckoBundle value.
     *
     * @param key Key to map.
     * @param value Value to map to.
     */
    public void putBundle(final String key, final GeckoBundle value) {
        mMap.put(key, value);
    }

    /**
     * Map a key to a GeckoBundle array value.
     *
     * @param key Key to map.
     * @param value Value to map to.
     */
    public void putBundleArray(final String key, final GeckoBundle[] value) {
        mMap.put(key, value);
    }

    /**
     * Remove a mapping.
     *
     * @param key Key to remove.
     */
    public void remove(final String key) {
        mMap.remove(key);
    }

    /**
     * Returns number of mappings in this GeckoBundle.
     *
     * @return Number of mappings.
     */
    public int size() {
        return mMap.size();
    }
}
