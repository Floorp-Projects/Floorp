/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.support.annotation.AnyThread;
import android.support.annotation.IntDef;
import android.support.annotation.NonNull;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import java.net.InetAddress;
import java.nio.ByteBuffer;
import java.util.List;

import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.annotation.WrapForJNI;

/**
 * GeckoWebExecutor is responsible for fetching a {@link WebRequest} and delivering
 * a {@link WebResponse} to the caller via {@link #fetch(WebRequest)}. Example:
 * <pre>
 *     final GeckoWebExecutor executor = new GeckoWebExecutor();
 *
 *     final GeckoResult&lt;WebResponse&gt; result = executor.fetch(
 *             new WebRequest.Builder("https://example.org/json")
 *             .header("Accept", "application/json")
 *             .build());
 *
 *     result.then(response -&gt; {
 *         // Do something with response
 *     });
 * </pre>
 */
@AnyThread
public class GeckoWebExecutor {
    // We don't use this right now because we access GeckoThread directly, but
    // it's future-proofing for a world where we allow multiple GeckoRuntimes.
    private final GeckoRuntime mRuntime;

    @WrapForJNI(dispatchTo = "gecko", stubName = "Fetch")
    private static native void nativeFetch(WebRequest request, int flags, GeckoResult<WebResponse> result);

    @WrapForJNI(dispatchTo = "gecko", stubName = "Resolve")
    private static native void nativeResolve(String host, GeckoResult<InetAddress[]> result);

    @WrapForJNI(calledFrom = "gecko", exceptionMode = "nsresult")
    private static ByteBuffer createByteBuffer(final int capacity) {
        return ByteBuffer.allocateDirect(capacity);
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({FETCH_FLAGS_NONE, FETCH_FLAGS_ANONYMOUS, FETCH_FLAGS_NO_REDIRECTS, FETCH_FLAGS_STREAM_FAILURE_TEST})
    /* package */ @interface FetchFlags {};

    /**
     * No special treatment.
     */
    public static final int FETCH_FLAGS_NONE = 0;

    /**
     * Don't send cookies or other user data along with the request.
     */
    @WrapForJNI
    public static final int FETCH_FLAGS_ANONYMOUS = 1;

    /**
     * Don't automatically follow redirects.
     */
    @WrapForJNI
    public static final int FETCH_FLAGS_NO_REDIRECTS = 1 << 1;

    /**
     * This flag causes a read error in the {@link WebResponse} body. Useful for testing.
     */
    @WrapForJNI
    public static final int FETCH_FLAGS_STREAM_FAILURE_TEST = 1 << 10;

    /**
     * Create a new GeckoWebExecutor instance.
     *
     * @param runtime A GeckoRuntime instance
     */
    public GeckoWebExecutor(final @NonNull GeckoRuntime runtime) {
        mRuntime = runtime;
    }

    /**
     * Send the given {@link WebRequest}.
     *
     * @param request A {@link WebRequest} instance
     * @return A {@link GeckoResult} which will be completed with a {@link WebResponse}. If the
     *         request fails to complete, the {@link GeckoResult} will be completed exceptionally
     *         with a {@link WebRequestError}.
     * @throws IllegalArgumentException if request is null or otherwise unusable.
     */
    public @NonNull GeckoResult<WebResponse> fetch(final @NonNull WebRequest request) {
        return fetch(request, FETCH_FLAGS_NONE);
    }

    /**
     * Send the given {@link WebRequest} with specified flags.
     *
     * @param request A {@link WebRequest} instance
     * @param flags The specified flags. One or more of the {@link #FETCH_FLAGS_NONE FETCH_*} flags.
     * @return A {@link GeckoResult} which will be completed with a {@link WebResponse}. If the
     *         request fails to complete, the {@link GeckoResult} will be completed exceptionally
     *         with a {@link WebRequestError}.
     * @throws IllegalArgumentException if request is null or otherwise unusable.
     */
    public @NonNull GeckoResult<WebResponse> fetch(final @NonNull WebRequest request,
                                                   final @FetchFlags int flags) {
        if (request.body != null && !request.body.isDirect()) {
            throw new IllegalArgumentException("Request body must be a direct ByteBuffer");
        }

        if (request.cacheMode < WebRequest.CACHE_MODE_FIRST ||
            request.cacheMode > WebRequest.CACHE_MODE_LAST) {
            throw new IllegalArgumentException("Unknown cache mode");
        }

        // We don't need to fully validate the URI here, just a sanity check
        if (!request.uri.toLowerCase().matches("(http|blob).*")) {
            throw new IllegalArgumentException("Unsupported URI scheme");
        }

        final GeckoResult<WebResponse> result = new GeckoResult<>();

        if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
            nativeFetch(request, flags, result);
        } else {
            GeckoThread.queueNativeCallUntil(GeckoThread.State.PROFILE_READY, this,
                    "nativeFetch", WebRequest.class, request, flags,
                    GeckoResult.class, result);
        }

        return result;
    }

    /**
     * Resolves the specified host name.
     *
     * @param host An Internet host name, e.g. mozilla.org.
     * @return A {@link GeckoResult} which will be fulfilled with a {@link List}
     *         of {@link InetAddress}. In case of failure, the {@link GeckoResult}
     *         will be completed exceptionally with a {@link java.net.UnknownHostException}.
     */
    public @NonNull GeckoResult<InetAddress[]> resolve(final @NonNull String host) {
        final GeckoResult<InetAddress[]> result = new GeckoResult<>();

        if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
            nativeResolve(host, result);
        } else {
            GeckoThread.queueNativeCallUntil(GeckoThread.State.PROFILE_READY, this,
                    "nativeResolve", String.class, host,
                    GeckoResult.class, result);
        }
        return result;
    }

    /**
     * This causes a speculative connection to be made to the host
     * in the specified URI. This is useful if an app thinks it may
     * be making a request to that host in the near future. If no request
     * is made, the connection will be cleaned up after an unspecified
     * amount of time.
     *
     * @param uri A URI String.
     */
    public void speculativeConnect(final @NonNull String uri) {
        GeckoThread.speculativeConnect(uri);
    }
}
