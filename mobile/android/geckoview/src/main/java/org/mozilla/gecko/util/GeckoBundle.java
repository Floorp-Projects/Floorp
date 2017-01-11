/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.AppConstants;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.os.Bundle;
import android.os.Parcelable;
import android.support.v4.util.SimpleArrayMap;

import java.lang.reflect.Array;
import java.util.Arrays;
import java.util.Collection;
import java.util.Iterator;

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
        return value == null ? null :
                Array.getLength(value) == 0 ? EMPTY_BOOLEAN_ARRAY : (boolean[]) value;
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
        return value == null ? null : Array.getLength(value) == 0 ? EMPTY_DOUBLE_ARRAY :
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
        return value == null ? null : Array.getLength(value) == 0 ? EMPTY_INT_ARRAY :
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
        return value == null ? null : Array.getLength(value) == 0 ? EMPTY_STRING_ARRAY :
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
        return value == null ? null : Array.getLength(value) == 0 ? EMPTY_BUNDLE_ARRAY :
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
     * Map a key to a boolean array value.
     *
     * @param key Key to map.
     * @param value Value to map to.
     */
    public void putBooleanArray(final String key, final Boolean[] value) {
        if (value == null) {
            mMap.put(key, null);
            return;
        }
        final boolean[] array = new boolean[value.length];
        for (int i = 0; i < value.length; i++) {
            array[i] = value[i];
        }
        mMap.put(key, array);
    }

    /**
     * Map a key to a boolean array value.
     *
     * @param key Key to map.
     * @param value Value to map to.
     */
    public void putBooleanArray(final String key, final Collection<Boolean> value) {
        if (value == null) {
            mMap.put(key, null);
            return;
        }
        final boolean[] array = new boolean[value.size()];
        int i = 0;
        for (final Boolean element : value) {
            array[i++] = element;
        }
        mMap.put(key, array);
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
     * Map a key to a double array value.
     *
     * @param key Key to map.
     * @param value Value to map to.
     */
    public void putDoubleArray(final String key, final Double[] value) {
        if (value == null) {
            mMap.put(key, null);
            return;
        }
        final double[] array = new double[value.length];
        for (int i = 0; i < value.length; i++) {
            array[i] = value[i];
        }
        mMap.put(key, array);
    }

    /**
     * Map a key to a double array value.
     *
     * @param key Key to map.
     * @param value Value to map to.
     */
    public void putDoubleArray(final String key, final Collection<Double> value) {
        if (value == null) {
            mMap.put(key, null);
            return;
        }
        final double[] array = new double[value.size()];
        int i = 0;
        for (final Double element : value) {
            array[i++] = element;
        }
        mMap.put(key, array);
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
     * Map a key to a int array value.
     *
     * @param key Key to map.
     * @param value Value to map to.
     */
    public void putIntArray(final String key, final Integer[] value) {
        if (value == null) {
            mMap.put(key, null);
            return;
        }
        final int[] array = new int[value.length];
        for (int i = 0; i < value.length; i++) {
            array[i] = value[i];
        }
        mMap.put(key, array);
    }

    /**
     * Map a key to a int array value.
     *
     * @param key Key to map.
     * @param value Value to map to.
     */
    public void putIntArray(final String key, final Collection<Integer> value) {
        if (value == null) {
            mMap.put(key, null);
            return;
        }
        final int[] array = new int[value.size()];
        int i = 0;
        for (final Integer element : value) {
            array[i++] = element;
        }
        mMap.put(key, array);
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
     * Map a key to a String array value.
     *
     * @param key Key to map.
     * @param value Value to map to.
     */
    public void putStringArray(final String key, final Collection<String> value) {
        if (value == null) {
            mMap.put(key, null);
            return;
        }
        final String[] array = new String[value.size()];
        int i = 0;
        for (final String element : value) {
            array[i++] = element;
        }
        mMap.put(key, array);
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
     * Map a key to a GeckoBundle array value.
     *
     * @param key Key to map.
     * @param value Value to map to.
     */
    public void putBundleArray(final String key, final Collection<GeckoBundle> value) {
        if (value == null) {
            mMap.put(key, null);
            return;
        }
        final GeckoBundle[] array = new GeckoBundle[value.size()];
        int i = 0;
        for (final GeckoBundle element : value) {
            array[i++] = element;
        }
        mMap.put(key, array);
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

    @Override // Object
    public boolean equals(Object other) {
        if (!(other instanceof GeckoBundle)) {
            return false;
        }

        // Support library's SimpleArrayMap.equals is buggy, so roll our own version.
        final SimpleArrayMap<String, Object> otherMap = ((GeckoBundle) other).mMap;
        if (mMap == otherMap) {
            return true;
        }
        if (mMap.size() != otherMap.size()) {
            return false;
        }

        for (int i = 0; i < mMap.size(); i++) {
            final String thisKey = mMap.keyAt(i);
            final int otherKey = otherMap.indexOfKey(thisKey);
            if (otherKey < 0) {
                return false;
            }
            final Object thisValue = mMap.valueAt(i);
            final Object otherValue = otherMap.valueAt(otherKey);
            if (thisValue == otherValue) {
                continue;
            }
            if (thisValue == null || otherValue == null || !thisValue.equals(otherValue)) {
                return false;
            }
        }
        return true;
    }

    @Override // Object
    public int hashCode() {
        return mMap.hashCode();
    }

    @Override // Object
    public String toString() {
        return mMap.toString();
    }

    public JSONObject toJSONObject() throws JSONException {
        final JSONObject out = new JSONObject();
        for (int i = 0; i < mMap.size(); i++) {
            final Object value = mMap.valueAt(i);
            final Object jsonValue;

            if (value instanceof GeckoBundle) {
                jsonValue = ((GeckoBundle) value).toJSONObject();
            } else if (value instanceof GeckoBundle[]) {
                final GeckoBundle[] array = (GeckoBundle[]) value;
                final JSONArray jsonArray = new JSONArray();
                for (int j = 0; j < array.length; j++) {
                    jsonArray.put(array[j] == null ? JSONObject.NULL : array[j].toJSONObject());
                }
                jsonValue = jsonArray;
            } else if (AppConstants.Versions.feature19Plus) {
                final Object wrapped = JSONObject.wrap(value);
                jsonValue = wrapped != null ? wrapped : value.toString();
            } else if (value == null) {
                jsonValue = JSONObject.NULL;
            } else if (value.getClass().isArray()) {
                final JSONArray jsonArray = new JSONArray();
                for (int j = 0; j < Array.getLength(value); j++) {
                    jsonArray.put(Array.get(value, j));
                }
                jsonValue = jsonArray;
            } else {
                jsonValue = value;
            }
            out.put(mMap.keyAt(i), jsonValue);
        }
        return out;
    }

    public Bundle toBundle() {
        final Bundle out = new Bundle(mMap.size());
        for (int i = 0; i < mMap.size(); i++) {
            final String key = mMap.keyAt(i);
            final Object val = mMap.valueAt(i);

            if (val == null) {
                out.putString(key, null);
            } else if (val instanceof GeckoBundle) {
                out.putBundle(key, ((GeckoBundle) val).toBundle());
            } else if (val instanceof GeckoBundle[]) {
                final GeckoBundle[] array = (GeckoBundle[]) val;
                final Parcelable[] parcelables = new Parcelable[array.length];
                for (int j = 0; j < array.length; j++) {
                    if (array[j] != null) {
                        parcelables[j] = array[j].toBundle();
                    }
                }
                out.putParcelableArray(key, parcelables);
            } else if (val instanceof Boolean) {
                out.putBoolean(key, (Boolean) val);
            } else if (val instanceof boolean[]) {
                out.putBooleanArray(key, (boolean[]) val);
            } else if (val instanceof Byte || val instanceof Short || val instanceof Integer) {
                out.putInt(key, ((Number) val).intValue());
            } else if (val instanceof int[]) {
                out.putIntArray(key, (int[]) val);
            } else if (val instanceof Float || val instanceof Double || val instanceof Long) {
                out.putDouble(key, ((Number) val).doubleValue());
            } else if (val instanceof double[]) {
                out.putDoubleArray(key, (double[]) val);
            } else if (val instanceof CharSequence || val instanceof Character) {
                out.putString(key, val.toString());
            } else if (val instanceof String[]) {
                out.putStringArray(key, (String[]) val);
            } else {
                throw new UnsupportedOperationException();
            }
        }
        return out;
    }

    public static GeckoBundle fromBundle(final Bundle bundle) {
        if (bundle == null) {
            return null;
        }

        final String[] keys = new String[bundle.size()];
        final Object[] values = new Object[bundle.size()];
        int i = 0;

        for (final String key : bundle.keySet()) {
            final Object value = bundle.get(key);
            keys[i] = key;

            if (value instanceof Bundle || value == null) {
                values[i] = fromBundle((Bundle) value);
            } else if (value instanceof Parcelable[]) {
                final Parcelable[] array = (Parcelable[]) value;
                final GeckoBundle[] out = new GeckoBundle[array.length];
                for (int j = 0; j < array.length; j++) {
                    out[j] = fromBundle((Bundle) array[j]);
                }
                values[i] = out;
            } else if (value instanceof Boolean || value instanceof Integer ||
                    value instanceof Double || value instanceof String ||
                    value instanceof boolean[] || value instanceof int[] ||
                    value instanceof double[] || value instanceof String[]) {
                values[i] = value;
            } else if (value instanceof Byte || value instanceof Short) {
                values[i] = ((Number) value).intValue();
            } else if (value instanceof Float || value instanceof Long) {
                values[i] = ((Number) value).doubleValue();
            } else if (value instanceof CharSequence || value instanceof Character) {
                values[i] = value.toString();
            } else {
                throw new UnsupportedOperationException();
            }

            i++;
        }
        return new GeckoBundle(keys, values);
    }

    private static Object fromJSONValue(Object value) throws JSONException {
        if (value instanceof JSONObject || value == JSONObject.NULL) {
            return fromJSONObject((JSONObject) value);
        }
        if (value instanceof JSONArray) {
            final JSONArray array = (JSONArray) value;
            final int len = array.length();
            if (len == 0) {
                return EMPTY_BOOLEAN_ARRAY;
            }
            Object out = null;
            for (int i = 0; i < len; i++) {
                final Object element = fromJSONValue(array.opt(i));
                if (element == null) {
                    continue;
                }
                if (out == null) {
                    Class<?> type = element.getClass();
                    if (type == Boolean.class) {
                        type = boolean.class;
                    } else if (type == Integer.class) {
                        type = int.class;
                    } else if (type == Double.class) {
                        type = double.class;
                    }
                    out = Array.newInstance(type, len);
                }
                Array.set(out, i, element);
            }
            if (out == null) {
                // Treat all-null arrays as String arrays.
                return new String[len];
            }
            return out;
        }
        if (value instanceof Boolean || value instanceof Integer ||
                value instanceof Double || value instanceof String) {
            return value;
        }
        if (value instanceof Byte || value instanceof Short) {
            return ((Number) value).intValue();
        }
        if (value instanceof Float || value instanceof Long) {
            return ((Number) value).doubleValue();
        }
        return value != null ? value.toString() : null;
    }

    public static GeckoBundle fromJSONObject(final JSONObject obj) throws JSONException {
        if (obj == null || obj == JSONObject.NULL) {
            return null;
        }

        final String[] keys = new String[obj.length()];
        final Object[] values = new Object[obj.length()];

        final Iterator<String> iter = obj.keys();
        for (int i = 0; iter.hasNext(); i++) {
            final String key = iter.next();
            keys[i] = key;
            values[i] = fromJSONValue(obj.opt(key));
        }
        return new GeckoBundle(keys, values);
    }
}
