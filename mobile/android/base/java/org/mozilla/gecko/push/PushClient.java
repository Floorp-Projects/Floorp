/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.push;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import org.mozilla.gecko.push.RegisterUserAgentResponse;
import org.mozilla.gecko.push.SubscribeChannelResponse;
import org.mozilla.gecko.push.autopush.AutopushClient;
import org.mozilla.gecko.push.autopush.AutopushClientException;
import org.mozilla.gecko.sync.Utils;

import java.util.concurrent.Executor;

/**
 * This class bridges the autopush client, which is written in callback style, with the Fennec
 * push implementation, which is written in a linear style.  It handles returning results and
 * re-throwing exceptions passed as messages.
 * <p/>
 * TODO: fold this into the autopush client directly.
 */
public class PushClient {
    public static class LocalException extends Exception {
        private static final long serialVersionUID = 2387554736L;

        public LocalException(Throwable throwable) {
            super(throwable);
        }
    }

    private final AutopushClient autopushClient;

    public PushClient(String serverURI) {
        this.autopushClient = new AutopushClient(serverURI, Utils.newSynchronousExecutor());
    }

    /**
     * Each instance is <b>single-use</b>!  Exactly one delegate method should be invoked once,
     * but we take care to handle multiple invocations (favoring the earliest), just to be safe.
     */
    protected static class Delegate<T> implements AutopushClient.RequestDelegate<T> {
        Object result; // Oh, for an algebraic data type when you need one!

        @SuppressWarnings("unchecked")
        public T responseOrThrow() throws LocalException, AutopushClientException {
            if (result instanceof LocalException) {
                throw (LocalException) result;
            }
            if (result instanceof AutopushClientException) {
                throw (AutopushClientException) result;
            }
            return (T) result;
        }

        @Override
        public void handleError(Exception e) {
            if (result == null) {
                result = new LocalException(e);
            }
        }

        @Override
        public void handleFailure(AutopushClientException e) {
            if (result == null) {
                result = e;
            }
        }

        @Override
        public void handleSuccess(T response) {
            if (result == null) {
                result = response;
            }
        }
    }

    public RegisterUserAgentResponse registerUserAgent(@NonNull String token) throws LocalException, AutopushClientException {
        final Delegate<RegisterUserAgentResponse> delegate = new Delegate<>();
        autopushClient.registerUserAgent(token, delegate);
        return delegate.responseOrThrow();
    }

    public void reregisterUserAgent(@NonNull String uaid, @NonNull String secret, @NonNull String token) throws LocalException, AutopushClientException {
        final Delegate<Void> delegate = new Delegate<>();
        autopushClient.reregisterUserAgent(uaid, secret, token, delegate);
        delegate.responseOrThrow(); // For side-effects only.
    }

    public void unregisterUserAgent(@NonNull String uaid, @NonNull String secret) throws LocalException, AutopushClientException {
        final Delegate<Void> delegate = new Delegate<>();
        autopushClient.unregisterUserAgent(uaid, secret, delegate);
        delegate.responseOrThrow(); // For side-effects only.
    }

    public SubscribeChannelResponse subscribeChannel(@NonNull String uaid, @NonNull String secret, @Nullable String appServerKey) throws LocalException, AutopushClientException {
        final Delegate<SubscribeChannelResponse> delegate = new Delegate<>();
        autopushClient.subscribeChannel(uaid, secret, appServerKey, delegate);
        return delegate.responseOrThrow();
    }

    public void unsubscribeChannel(@NonNull String uaid, @NonNull String secret, @NonNull String chid) throws LocalException, AutopushClientException {
        final Delegate<Void> delegate = new Delegate<>();
        autopushClient.unsubscribeChannel(uaid, secret, chid, delegate);
        delegate.responseOrThrow(); // For side-effects only.
    }
}
