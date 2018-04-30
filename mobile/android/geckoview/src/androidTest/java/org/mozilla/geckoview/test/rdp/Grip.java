/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview.test.rdp;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import org.json.JSONObject;

import java.util.AbstractList;
import java.util.AbstractMap;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Provide methods for interacting with grips, including unpacking grips into Java
 * objects.
 */
/* package */ final class Grip extends Actor {

    private static final class Cache extends HashMap<String, Object> {
    }

    private static final class LazyObject extends AbstractMap<String, Object> {
        private final Cache mCache;
        private final String mType;
        private Grip mGrip;
        private Map<String, Object> mRealObject;

        public LazyObject(final @NonNull Cache cache,
                          final @NonNull String type,
                          final @NonNull Grip grip) {
            mCache = cache;
            mType = type;
            mGrip = grip;

            cache.put(mGrip.name, this);
        }

        private Map<String, Object> ensureRealObject() {
            if (mRealObject == null) {
                mRealObject = mGrip.unpackAsObject(mCache);
                mGrip.release();
                mGrip = null;
            }
            return mRealObject;
        }

        @Override
        public boolean equals(final Object object) {
            if (object instanceof LazyObject) {
                final LazyObject other = (LazyObject) object;
                if (mGrip != null && other.mGrip != null) {
                    return mGrip.equals(other.mGrip);
                }
                return ensureRealObject().equals(other.ensureRealObject());
            }
            return ensureRealObject().equals(object);
        }

        @Override
        public String toString() {
            return "[" + mType + ']' + (mRealObject != null ? mRealObject : "");
        }

        @Override
        public Set<Entry<String, Object>> entrySet() {
            return ensureRealObject().entrySet();
        }

        @Override
        public boolean containsKey(final Object key) {
            return ensureRealObject().containsKey(key);
        }

        @Override
        public Object get(final Object key) {
            return ensureRealObject().get(key);
        }

        @Override
        public Set<String> keySet() {
            return ensureRealObject().keySet();
        }
    }

    private static final class LazyArray extends AbstractList<Object> {
        private final Cache mCache;
        private final String mType;
        private final int mLength;
        private Grip mGrip;
        private List<Object> mRealObject;

        public LazyArray(final @NonNull Cache cache,
                         final @NonNull String type,
                         final int length,
                         final @NonNull Grip grip) {
            mCache = cache;
            mType = type;
            mLength = length;
            mGrip = grip;

            cache.put(mGrip.name, this);
        }

        private List<Object> ensureRealObject() {
            if (mRealObject == null) {
                mRealObject = mGrip.unpackAsArray(mCache);
                mGrip.release();
                mGrip = null;
            }
            return mRealObject;
        }

        @Override
        public boolean equals(Object object) {
            if (object instanceof LazyArray) {
                final LazyArray other = (LazyArray) object;
                if (mGrip != null && other.mGrip != null) {
                    return mGrip.equals(other.mGrip);
                }
                return ensureRealObject().equals(other.ensureRealObject());
            }
            return ensureRealObject().equals(object);
        }

        @Override
        public String toString() {
            final String length = (mRealObject != null) ? ("(" + mRealObject.size() + ')') :
                                  (mLength >= 0)        ? ("(" + mLength + ')') : "";
            return "[" + mType + length + ']' + (mRealObject != null ? mRealObject : "");
        }

        @Override
        public Object get(int i) {
            return ensureRealObject().get(i);
        }

        @Override
        public int size() {
            return ensureRealObject().size();
        }
    }

    private static final class Function {
        @Override
        public String toString() {
            return "[Function]";
        }
    }

    /**
     * Unpack a received grip value into a Java object. The grip can be either a primitive
     * value, or a JSONObject that represents a live object on the server.
     *
     * @param connection Connection associated with this grip.
     * @param value Grip value received from the server.
     */
    public static Object unpack(final RDPConnection connection,
                                final Object value) {
        return unpackGrip(new Cache(), connection, value);
    }

    private static Object unpackGrip(final Cache cache,
                                     final RDPConnection connection,
                                     final Object value) {
        if (value == null || value instanceof String || value instanceof Boolean) {
            return value;
        } else if (value instanceof Number) {
            return ((Number) value).doubleValue();
        }

        final JSONObject obj = (JSONObject) value;
        switch (obj.optString("type")) {
            case "null":
            case "undefined":
                return null;
            case "Infinity":
                return Double.POSITIVE_INFINITY;
            case "-Infinity":
                return Double.NEGATIVE_INFINITY;
            case "NaN":
                return Double.NaN;
            case "-0":
                return -0.0;
            case "object":
                break;
            default:
                throw new IllegalArgumentException();
        }

        final String actor = obj.optString("actor", null);
        final Object cached = cache.get(actor);
        if (cached != null) {
            return cached;
        }

        final String cls = obj.optString("class", null);
        if ("Function".equals(cls)) {
            return new Function();
        }

        final JSONObject preview = obj.optJSONObject("preview");
        final boolean isArray;
        if ("Array".equals(cls)) {
            isArray = true;
        } else if (preview != null) {
            isArray = "ArrayLike".equals(preview.optString("kind"));
        } else {
            isArray = false;
        }

        final Grip grip = new Grip(connection, obj);
        final Object output;
        if (isArray) {
            final int length = (preview != null) ? preview.optInt("length", -1) : -1;
            output = new LazyArray(cache, cls, length, grip);
        } else {
            output = new LazyObject(cache, cls, grip);
        }
        return output;
    }

    private Grip(final RDPConnection connection, final JSONObject grip) {
        super(connection, grip);
    }

    @Override
    protected void release() {
        sendPacket("{\"type\":\"release\"}", null);
        super.release();
    }

    /* package */ List<Object> unpackAsArray(final @NonNull Cache cache) {
        final JSONObject reply = sendPacket("{\"type\":\"prototypeAndProperties\"}",
                                            "ownProperties");
        final JSONObject props = reply.optJSONObject("ownProperties");
        final JSONObject getterValues = reply.optJSONObject("safeGetterValues");

        JSONObject prop = props.optJSONObject("length");
        String valueKey = "value";
        if (prop == null) {
            prop = getterValues.optJSONObject("length");
            valueKey = "getterValue";
        }

        final int len = prop.optInt(valueKey);
        final Object[] output = new Object[len];
        for (int i = 0; i < len; i++) {
            prop = props.optJSONObject(String.valueOf(i));
            valueKey = "value";
            if (prop == null) {
                prop = getterValues.optJSONObject(String.valueOf(i));
                valueKey = "getterValue";
            }
            output[i] = unpackGrip(cache, connection, prop.opt(valueKey));
        }
        return Arrays.asList(output);
    }

    /* package */ Map<String, Object> unpackAsObject(final @NonNull Cache cache) {
        final JSONObject reply = sendPacket("{\"type\":\"prototypeAndProperties\"}",
                                            "ownProperties");
        final Map<String, Object> output = new HashMap<>();

        fillProperties(cache, output, reply.optJSONObject("ownProperties"), "value");
        fillProperties(cache, output, reply.optJSONObject("safeGetterValues"), "getterValue");
        return output;
    }

    private void fillProperties(final @NonNull Cache cache,
                                final @NonNull Map<String, Object> output,
                                final @Nullable JSONObject props,
                                final @NonNull String valueKey) {
        if (props == null) {
            return;
        }
        for (final Iterator<String> it = props.keys(); it.hasNext();) {
            final String key = it.next();
            final JSONObject prop = props.optJSONObject(key);
            final Object value = prop.opt(valueKey);
            output.put(key, unpackGrip(cache, connection, value));
        }
    }
}
