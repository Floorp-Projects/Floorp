/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.push.autopush;

import android.text.TextUtils;

import org.mozilla.gecko.Locales;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.push.RegisterUserAgentResponse;
import org.mozilla.gecko.push.SubscribeChannelResponse;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.BaseResourceDelegate;
import org.mozilla.gecko.sync.net.BearerAuthHeaderProvider;
import org.mozilla.gecko.sync.net.Resource;
import org.mozilla.gecko.sync.net.SyncResponse;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.security.GeneralSecurityException;
import java.util.Locale;
import java.util.concurrent.Executor;

import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.HttpHeaders;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;


/**
 * Interact with the autopush endpoint HTTP API.
 * <p/>
 * The API is a Mozilla-proprietary interface, and not even specified to Mozilla's usual ad-hoc standards.
 * This client is written against a work-in-progress, un-deployed upstream commit.
 */
public class AutopushClient {
    protected static final String LOG_TAG = AutopushClient.class.getSimpleName();

    protected static final String ACCEPT_HEADER = "application/json;charset=utf-8";
    protected static final String TYPE = "gcm";

    protected static final String JSON_KEY_UAID = "uaid";
    protected static final String JSON_KEY_SECRET = "secret";
    protected static final String JSON_KEY_CHANNEL_ID = "channelID";
    protected static final String JSON_KEY_ENDPOINT = "endpoint";

    protected static final String[] REGISTER_USER_AGENT_RESPONSE_REQUIRED_STRING_FIELDS = new String[] { JSON_KEY_UAID, JSON_KEY_SECRET, JSON_KEY_CHANNEL_ID, JSON_KEY_ENDPOINT };
    protected static final String[] REGISTER_CHANNEL_RESPONSE_REQUIRED_STRING_FIELDS = new String[] { JSON_KEY_CHANNEL_ID, JSON_KEY_ENDPOINT };

    public static final String JSON_KEY_CODE = "code";
    public static final String JSON_KEY_ERRNO = "errno";
    public static final String JSON_KEY_ERROR = "error";
    public static final String JSON_KEY_MESSAGE = "message";

    protected static final String[] requiredErrorStringFields = { JSON_KEY_ERROR, JSON_KEY_MESSAGE };
    protected static final String[] requiredErrorLongFields = { JSON_KEY_CODE, JSON_KEY_ERRNO };

    /**
     * The server's URI.
     * <p>
     * We assume throughout that this ends with a trailing slash (and guarantee as
     * much in the constructor).
     */
    public final String serverURI;

    protected final Executor executor;

    public AutopushClient(String serverURI, Executor executor) {
        if (serverURI == null) {
            throw new IllegalArgumentException("Must provide a server URI.");
        }
        if (executor == null) {
            throw new IllegalArgumentException("Must provide a non-null executor.");
        }
        this.serverURI = serverURI.endsWith("/") ? serverURI : serverURI + "/";
        if (!this.serverURI.endsWith("/")) {
            throw new IllegalArgumentException("Constructed serverURI must end with a trailing slash: " + this.serverURI);
        }
        this.executor = executor;
    }

    /**
     * A legal autopush server URL includes a sender ID embedded into it.  Extract it.
     *
     * @return a non-null non-empty sender ID.
     * @throws AutopushClientException on failure.
     */
    public String getSenderIDFromServerURI() throws AutopushClientException {
        // Turn "https://updates-autopush-dev.stage.mozaws.net/v1/gcm/829133274407/" into "829133274407".
        final String[] parts = serverURI.split("/", -1); // The -1 keeps the trailing empty part.
        if (parts.length < 3) {
            throw new AutopushClientException("Could not get sender ID from autopush server URI: " + serverURI);
        }
        if (!TextUtils.isEmpty(parts[parts.length - 1])) {
            // We guarantee a trailing slash, so we should always have an empty part at the tail.
            throw new AutopushClientException("Could not get sender ID from autopush server URI: " + serverURI);
        }
        if (!TextUtils.equals("gcm", parts[parts.length - 3])) {
            // We should always have /gcm/senderID/.
            throw new AutopushClientException("Could not get sender ID from autopush server URI: " + serverURI);
        }
        final String senderID = parts[parts.length - 2];
        if (TextUtils.isEmpty(senderID)) {
            // Something is horribly wrong -- we have /gcm//.  Abort.
            throw new AutopushClientException("Could not get sender ID from autopush server URI: " + serverURI);
        }
        return senderID;
    }

    /**
     * Process a typed value extracted from a successful response (in an
     * endpoint-dependent way).
     */
    public interface RequestDelegate<T> {
        void handleError(Exception e);
        void handleFailure(AutopushClientException e);
        void handleSuccess(T result);
    }

    /**
     * Intepret a response from the autopush server.
     * <p>
     * Throw an appropriate exception on errors; otherwise, return the response's
     * status code.
     *
     * @return response's HTTP status code.
     * @throws AutopushClientException
     */
    public static int validateResponse(HttpResponse response) throws AutopushClientException {
        final int status = response.getStatusLine().getStatusCode();
        if (200 <= status && status <= 299) {
            return status;
        }
        long code;
        long errno;
        String error;
        String message;
        String info;
        ExtendedJSONObject body;
        try {
            body = new SyncStorageResponse(response).jsonObjectBody();
            // TODO: The service doesn't do the right thing yet :(
            // body.throwIfFieldsMissingOrMisTyped(requiredErrorStringFields, String.class);
            body.throwIfFieldsMissingOrMisTyped(requiredErrorLongFields, Long.class);
            // Would throw above if missing; the -1 defaults quiet NPE warnings.
            code = body.getLong(JSON_KEY_CODE, -1);
            errno = body.getLong(JSON_KEY_ERRNO, -1);
            error = body.getString(JSON_KEY_ERROR);
            message = body.getString(JSON_KEY_MESSAGE);
        } catch (Exception e) {
            throw new AutopushClientException.AutopushClientMalformedResponseException(response);
        }
        throw new AutopushClientException.AutopushClientRemoteException(response, code, errno, error, message, body);
    }

    protected <T> void invokeHandleError(final RequestDelegate<T> delegate, final Exception e) {
        executor.execute(new Runnable() {
            @Override
            public void run() {
                delegate.handleError(e);
            }
        });
    }

    protected <T> void post(BaseResource resource, final ExtendedJSONObject requestBody, final RequestDelegate<T> delegate) {
        try {
            if (requestBody == null) {
                resource.post((HttpEntity) null);
            } else {
                resource.post(requestBody);
            }
        } catch (Exception e) {
            invokeHandleError(delegate, e);
            return;
        }
    }

    /**
     * Translate resource callbacks into request callbacks invoked on the provided
     * executor.
     * <p>
     * Override <code>handleSuccess</code> to parse the body of the resource
     * request and call the request callback. <code>handleSuccess</code> is
     * invoked via the executor, so you don't need to delegate further.
     */
    protected abstract class ResourceDelegate<T> extends BaseResourceDelegate {
        protected abstract void handleSuccess(final int status, HttpResponse response, final ExtendedJSONObject body);

        protected final String secret;
        protected final RequestDelegate<T> delegate;

        /**
         * Create a delegate for an un-authenticated resource.
         */
        public ResourceDelegate(final Resource resource, final String secret, final RequestDelegate<T> delegate) {
            super(resource);
            this.delegate = delegate;
            this.secret = secret;
        }

        @Override
        public AuthHeaderProvider getAuthHeaderProvider() {
            if (secret != null) {
                return new BearerAuthHeaderProvider(secret);
            }
            return null;
        }

        @Override
        public String getUserAgent() {
            return FxAccountConstants.USER_AGENT;
        }

        @Override
        public void handleHttpResponse(HttpResponse response) {
            try {
                final int status = validateResponse(response);
                invokeHandleSuccess(status, response);
            } catch (AutopushClientException e) {
                invokeHandleFailure(e);
            }
        }

        protected void invokeHandleFailure(final AutopushClientException e) {
            executor.execute(new Runnable() {
                @Override
                public void run() {
                    delegate.handleFailure(e);
                }
            });
        }

        protected void invokeHandleSuccess(final int status, final HttpResponse response) {
            executor.execute(new Runnable() {
                @Override
                public void run() {
                    try {
                        ExtendedJSONObject body = new SyncResponse(response).jsonObjectBody();
                        ResourceDelegate.this.handleSuccess(status, response, body);
                    } catch (Exception e) {
                        delegate.handleError(e);
                    }
                }
            });
        }

        @Override
        public void handleHttpProtocolException(final ClientProtocolException e) {
            invokeHandleError(delegate, e);
        }

        @Override
        public void handleHttpIOException(IOException e) {
            invokeHandleError(delegate, e);
        }

        @Override
        public void handleTransportException(GeneralSecurityException e) {
            invokeHandleError(delegate, e);
        }

        @Override
        public void addHeaders(HttpRequestBase request, DefaultHttpClient client) {
            super.addHeaders(request, client);

            // The basics.
            final Locale locale = Locale.getDefault();
            request.addHeader(HttpHeaders.ACCEPT_LANGUAGE, Locales.getLanguageTag(locale));
            request.addHeader(HttpHeaders.ACCEPT, ACCEPT_HEADER);
        }
    }

    public void registerUserAgent(final String token, RequestDelegate<RegisterUserAgentResponse> delegate) {
        BaseResource resource;
        try {
            resource = new BaseResource(new URI(serverURI + "registration"));
        } catch (URISyntaxException e) {
            invokeHandleError(delegate, e);
            return;
        }

        resource.delegate = new ResourceDelegate<RegisterUserAgentResponse>(resource, null, delegate) {
            @Override
            public void handleSuccess(int status, HttpResponse response, ExtendedJSONObject body) {
                try {
                    body.throwIfFieldsMissingOrMisTyped(REGISTER_USER_AGENT_RESPONSE_REQUIRED_STRING_FIELDS, String.class);
                    final String uaid = body.getString(JSON_KEY_UAID);
                    final String secret = body.getString(JSON_KEY_SECRET);
                    delegate.handleSuccess(new RegisterUserAgentResponse(uaid, secret));
                    return;
                } catch (Exception e) {
                    delegate.handleError(e);
                    return;
                }
            }
        };

        final ExtendedJSONObject body = new ExtendedJSONObject();
        body.put("type", TYPE);
        body.put("token", token);

        resource.post(body);
    }

    public void reregisterUserAgent(final String uaid, final String secret, final String token, RequestDelegate<Void> delegate) {
        final BaseResource resource;
        try {
            resource = new BaseResource(new URI(serverURI + "registration/" + uaid));
        } catch (Exception e) {
            invokeHandleError(delegate, e);
            return;
        }

        resource.delegate = new ResourceDelegate<Void>(resource, secret, delegate) {
            @Override
            public void handleSuccess(int status, HttpResponse response, ExtendedJSONObject body) {
                try {
                    delegate.handleSuccess(null);
                    return;
                } catch (Exception e) {
                    delegate.handleError(e);
                    return;
                }
            }
        };

        final ExtendedJSONObject body = new ExtendedJSONObject();
        body.put("type", TYPE);
        body.put("token", token);

        resource.put(body);
    }


    public void subscribeChannel(final String uaid, final String secret, final String appServerKey, RequestDelegate<SubscribeChannelResponse> delegate) {
        final BaseResource resource;
        try {
            resource = new BaseResource(new URI(serverURI + "registration/" + uaid + "/subscription"));
        } catch (Exception e) {
            invokeHandleError(delegate, e);
            return;
        }

        resource.delegate = new ResourceDelegate<SubscribeChannelResponse>(resource, secret, delegate) {
            @Override
            public void handleSuccess(int status, HttpResponse response, ExtendedJSONObject body) {
                try {
                    body.throwIfFieldsMissingOrMisTyped(REGISTER_CHANNEL_RESPONSE_REQUIRED_STRING_FIELDS, String.class);
                    final String channelID = body.getString(JSON_KEY_CHANNEL_ID);
                    final String endpoint = body.getString(JSON_KEY_ENDPOINT);
                    delegate.handleSuccess(new SubscribeChannelResponse(channelID, endpoint));
                    return;
                } catch (Exception e) {
                    delegate.handleError(e);
                    return;
                }
            }
        };

        final ExtendedJSONObject body = new ExtendedJSONObject();
        body.put("key", appServerKey);
        resource.post(body);
    }

    public void unsubscribeChannel(final String uaid, final String secret, final String channelID, RequestDelegate<Void> delegate) {
        final BaseResource resource;
        try {
            resource = new BaseResource(new URI(serverURI + "registration/" + uaid + "/subscription/" + channelID));
        } catch (Exception e) {
            invokeHandleError(delegate, e);
            return;
        }

        resource.delegate = new ResourceDelegate<Void>(resource, secret, delegate) {
            @Override
            public void handleSuccess(int status, HttpResponse response, ExtendedJSONObject body) {
                delegate.handleSuccess(null);
            }
        };

        resource.delete();
    }

    public void unregisterUserAgent(final String uaid, final String secret, RequestDelegate<Void> delegate) {
        final BaseResource resource;
        try {
            resource = new BaseResource(new URI(serverURI + "registration/" + uaid));
        } catch (Exception e) {
            invokeHandleError(delegate, e);
            return;
        }

        resource.delegate = new ResourceDelegate<Void>(resource, secret, delegate) {
            @Override
            public void handleSuccess(int status, HttpResponse response, ExtendedJSONObject body) {
                delegate.handleSuccess(null);
            }
        };

        resource.delete();
    }
}
