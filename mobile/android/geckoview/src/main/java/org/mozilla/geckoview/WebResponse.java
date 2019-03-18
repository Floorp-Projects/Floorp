/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import org.mozilla.gecko.annotation.WrapForJNI;

import android.support.annotation.AnyThread;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import java.io.InputStream;

/**
 * WebResponse represents an HTTP[S] response. It is normally created
 * by {@link GeckoWebExecutor#fetch(WebRequest)}.
 */
@WrapForJNI
@AnyThread
public class WebResponse extends WebMessage {
    /**
     * The HTTP status code for the response, e.g. 200.
     */
    public final int statusCode;

    /**
     * A boolean indicating whether or not this response is
     * the result of a redirection.
     */
    public final boolean redirected;

    /**
     * An {@link InputStream} containing the response body, if available.
     */
    public final @Nullable InputStream body;

    protected WebResponse(final @NonNull Builder builder) {
        super(builder);
        this.statusCode = builder.mStatusCode;
        this.redirected = builder.mRedirected;
        this.body = builder.mBody;
    }

    /**
     * Builder offers a convenient way to create WebResponse instances.
     */
    @WrapForJNI
    @AnyThread
    public static class Builder extends WebMessage.Builder {
        /* package */ int mStatusCode;
        /* package */ boolean mRedirected;
        /* package */ InputStream mBody;

        /**
         * Constructs a new Builder instance with the specified URI.
         *
         * @param uri A URI String.
         */
        public Builder(final @NonNull String uri) {
            super(uri);
        }

        @Override
        public @NonNull Builder uri(final @NonNull String uri) {
            super.uri(uri);
            return this;
        }

        @Override
        public @NonNull Builder header(final @NonNull String key, final @NonNull String value) {
            super.header(key, value);
            return this;
        }

        @Override
        public @NonNull Builder addHeader(final @NonNull String key, final @NonNull String value) {
            super.addHeader(key, value);
            return this;
        }

        /**
         * Sets the {@link InputStream} containing the body of this response.
         *
         * @param stream An {@link InputStream} with the body of the response.
         * @return This Builder instance.
         */
        public @NonNull Builder body(final @NonNull InputStream stream) {
            mBody = stream;
            return this;
        }

        /**
         * Set the HTTP status code, e.g. 200.
         *
         * @param code A int representing the HTTP status code.
         * @return This Builder instance.
         */
        public @NonNull Builder statusCode(final int code) {
            mStatusCode = code;
            return this;
        }

        /**
         * Set whether or not this response was the result of a redirect.
         *
         * @param redirected A boolean representing whether or not the request was redirected.
         * @return This Builder instance.
         */
        public @NonNull Builder redirected(final boolean redirected) {
            mRedirected = redirected;
            return this;
        }

        /**
         * @return A {@link WebResponse} constructed with the values from this Builder instance.
         */
        public @NonNull WebResponse build() {
            return new WebResponse(this);
        }
    }
}
