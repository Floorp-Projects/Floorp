/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import org.mozilla.gecko.annotation.WrapForJNI;

import android.support.annotation.AnyThread;
import android.support.annotation.NonNull;
import android.support.v4.util.ArrayMap;

import java.nio.ByteBuffer;

import java.util.Collections;
import java.util.Map;

/**
 * This is an abstract base class for HTTP request and response types.
 */
@WrapForJNI
@AnyThread
public abstract class WebMessage {

    /**
     * The URI for the request or response.
     */
    public final @NonNull String uri;

    /**
     * An unmodifiable Map of headers. Defaults to an empty instance.
     */
    public final @NonNull Map<String, String> headers;

    protected WebMessage(final @NonNull Builder builder) {
        uri = builder.mUri;
        headers = Collections.unmodifiableMap(builder.mHeaders);
    }

    // This is only used via JNI.
    private String[] getHeaderKeys() {
        String[] keys = new String[headers.size()];
        headers.keySet().toArray(keys);
        return keys;
    }

    // This is only used via JNI.
    private String[] getHeaderValues() {
        String[] values = new String[headers.size()];
        headers.values().toArray(values);
        return values;
    }

    /**
     * This is a Builder used by subclasses of {@link WebMessage}.
     */
    @AnyThread
    public static abstract class Builder {
        /* package */ String mUri;
        /* package */ Map<String, String> mHeaders = new ArrayMap<>();
        /* package */ ByteBuffer mBody;

        /**
         * Construct a Builder instance with the specified URI.
         *
         * @param uri A URI String.
         */
        /* package */ Builder(final @NonNull String uri) {
            uri(uri);
        }

        /**
         * Set the URI
         *
         * @param uri A URI String
         * @return This Builder instance.
         */
        public @NonNull Builder uri(final @NonNull String uri) {
            mUri = uri;
            return this;
        }

        /**
         * Set a HTTP header. This may be called multiple times for additional headers. If an
         * existing header of the same name exists, it will be replaced by this value.
         *
         * @param key The key for the HTTP header, e.g. "Content-Type".
         * @param value The value for the HTTP header, e.g. "application/json".
         * @return This Builder instance.
         */
        public @NonNull Builder header(final @NonNull String key, final @NonNull String value) {
            mHeaders.put(key, value);
            return this;
        }

        /**
         * Add a HTTP header. This may be called multiple times for additional headers. If an
         * existing header of the same name exists, the values will be merged.
         *
         * @param key The key for the HTTP header, e.g. "Content-Type".
         * @param value The value for the HTTP header, e.g. "application/json".
         * @return This Builder instance.
         */
        public @NonNull Builder addHeader(final @NonNull String key, final @NonNull String value) {
            final String existingValue = mHeaders.get(key);
            if (existingValue != null) {
                final StringBuilder builder = new StringBuilder(existingValue);
                builder.append(", ");
                builder.append(value);
                mHeaders.put(key, builder.toString());
            } else {
                mHeaders.put(key, value);
            }

            return this;
        }
    }
}

