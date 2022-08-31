/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import androidx.annotation.AnyThread;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.charset.Charset;
import org.mozilla.gecko.annotation.WrapForJNI;

/**
 * WebRequest represents an HTTP[S] request. The typical pattern is to create instances of this
 * class via {@link WebRequest.Builder}, and fetch responses via {@link
 * GeckoWebExecutor#fetch(WebRequest)}.
 */
@WrapForJNI
@AnyThread
public class WebRequest extends WebMessage {
  /** The HTTP method for the request. Defaults to "GET". */
  public final @NonNull String method;

  /** The body of the request. Must be a directly-allocated ByteBuffer. May be null. */
  public final @Nullable ByteBuffer body;

  /**
   * The cache mode for the request. See {@link #CACHE_MODE_DEFAULT}. These modes match those from
   * the DOM Fetch API.
   *
   * @see <a href="https://developer.mozilla.org/en-US/docs/Web/API/Request/cache">DOM Fetch API
   *     cache modes</a>
   */
  public final @CacheMode int cacheMode;

  /**
   * If true, do not use newer protocol features that might have interop problems on the Internet.
   * Intended only for use with critical infrastructure.
   */
  public final boolean beConservative;

  /** The value of the Referer header for this request. */
  public final @Nullable String referrer;

  @Retention(RetentionPolicy.SOURCE)
  @IntDef({
    CACHE_MODE_DEFAULT,
    CACHE_MODE_NO_STORE,
    CACHE_MODE_RELOAD,
    CACHE_MODE_NO_CACHE,
    CACHE_MODE_FORCE_CACHE,
    CACHE_MODE_ONLY_IF_CACHED
  })
  public @interface CacheMode {};

  /** Default cache mode. Normal caching rules apply. */
  public static final int CACHE_MODE_DEFAULT = 1;

  /**
   * The response will be fetched from the server without looking in the cache, and will not update
   * the cache with the downloaded response.
   */
  public static final int CACHE_MODE_NO_STORE = 2;

  /**
   * The response will be fetched from the server without looking in the cache. The cache will be
   * updated with the downloaded response.
   */
  public static final int CACHE_MODE_RELOAD = 3;

  /** Forces a conditional request to the server if there is a cache match. */
  public static final int CACHE_MODE_NO_CACHE = 4;

  /**
   * If a response is found in the cache, it will be returned, whether it's fresh or not. If there
   * is no match, a normal request will be made and the cache will be updated with the downloaded
   * response.
   */
  public static final int CACHE_MODE_FORCE_CACHE = 5;

  /**
   * If a response is found in the cache, it will be returned, whether it's fresh or not. If there
   * is no match from the cache, 504 Gateway Timeout will be returned.
   */
  public static final int CACHE_MODE_ONLY_IF_CACHED = 6;

  /* package */ static final int CACHE_MODE_FIRST = CACHE_MODE_DEFAULT;
  /* package */ static final int CACHE_MODE_LAST = CACHE_MODE_ONLY_IF_CACHED;

  /**
   * Constructs a WebRequest with the specified URI.
   *
   * @param uri A URI String, e.g. https://mozilla.org
   */
  public WebRequest(final @NonNull String uri) {
    this(new Builder(uri));
  }

  /** Constructs a new WebRequest from a {@link WebRequest.Builder}. */
  /* package */ WebRequest(final @NonNull Builder builder) {
    super(builder);
    method = builder.mMethod;
    cacheMode = builder.mCacheMode;
    referrer = builder.mReferrer;
    beConservative = builder.mBeConservative;

    if (builder.mBody != null) {
      body = builder.mBody.asReadOnlyBuffer();
    } else {
      body = null;
    }
  }

  /** Builder offers a convenient way for constructing {@link WebRequest} instances. */
  @AnyThread
  public static class Builder extends WebMessage.Builder {
    /* package */ String mMethod = "GET";
    /* package */ int mCacheMode = CACHE_MODE_DEFAULT;
    /* package */ String mReferrer;
    /* package */ boolean mBeConservative;

    /**
     * Construct a Builder instance with the specified URI.
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
     * Set the body.
     *
     * @param buffer A {@link ByteBuffer} with the data. Must be allocated directly via {@link
     *     ByteBuffer#allocateDirect(int)}.
     * @return This Builder instance.
     */
    public @NonNull Builder body(final @Nullable ByteBuffer buffer) {
      if (buffer != null && !buffer.isDirect()) {
        throw new IllegalArgumentException("body must be directly allocated");
      }
      mBody = buffer;
      return this;
    }

    /**
     * Set the body.
     *
     * @param bodyString A {@link String} with the data.
     * @return This Builder instance.
     */
    public @NonNull Builder body(final @Nullable String bodyString) {
      if (bodyString == null) {
        mBody = null;
        return this;
      }
      final CharBuffer chars = CharBuffer.wrap(bodyString);
      final ByteBuffer buffer = ByteBuffer.allocateDirect(bodyString.length());
      Charset.forName("UTF-8").newEncoder().encode(chars, buffer, true);

      mBody = buffer;
      return this;
    }

    /**
     * Set the HTTP method.
     *
     * @param method The HTTP method String.
     * @return This Builder instance.
     */
    public @NonNull Builder method(final @NonNull String method) {
      mMethod = method;
      return this;
    }

    /**
     * Set the cache mode.
     *
     * @param mode One of the {@link #CACHE_MODE_DEFAULT CACHE_*} flags.
     * @return This Builder instance.
     */
    public @NonNull Builder cacheMode(final @CacheMode int mode) {
      if (mode < CACHE_MODE_FIRST || mode > CACHE_MODE_LAST) {
        throw new IllegalArgumentException("Unknown cache mode");
      }
      mCacheMode = mode;
      return this;
    }

    /**
     * Set the HTTP Referer header.
     *
     * @param referrer A URI String
     * @return This Builder instance.
     */
    public @NonNull Builder referrer(final @Nullable String referrer) {
      mReferrer = referrer;
      return this;
    }

    /**
     * Set the beConservative property.
     *
     * @param beConservative If true, do not use newer protocol features that might have interop
     *     problems on the Internet. Intended only for use with critical infrastructure.
     * @return This Builder instance.
     */
    public @NonNull Builder beConservative(final boolean beConservative) {
      mBeConservative = beConservative;
      return this;
    }

    /**
     * @return A {@link WebRequest} constructed with the values from this Builder instance.
     */
    public @NonNull WebRequest build() {
      if (mUri == null) {
        throw new IllegalStateException("Must set URI");
      }
      return new WebRequest(this);
    }
  }
}
