/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.fxa.oauth;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.security.GeneralSecurityException;
import java.util.Locale;
import java.util.concurrent.Executor;

import org.mozilla.gecko.background.fxa.FxAccountClientException;
import org.mozilla.gecko.background.fxa.oauth.FxAccountAbstractClientException.FxAccountAbstractClientMalformedResponseException;
import org.mozilla.gecko.background.fxa.oauth.FxAccountAbstractClientException.FxAccountAbstractClientRemoteException;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.BaseResourceDelegate;
import org.mozilla.gecko.sync.net.Resource;
import org.mozilla.gecko.sync.net.SyncResponse;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.HttpHeaders;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;

public abstract class FxAccountAbstractClient {
  protected static final String LOG_TAG = FxAccountAbstractClient.class.getSimpleName();

  protected static final String ACCEPT_HEADER = "application/json;charset=utf-8";
  protected static final String AUTHORIZATION_RESPONSE_TYPE = "token";

  public static final String JSON_KEY_ERROR = "error";
  public static final String JSON_KEY_MESSAGE = "message";
  public static final String JSON_KEY_CODE = "code";
  public static final String JSON_KEY_ERRNO = "errno";

  protected static final String[] requiredErrorStringFields = { JSON_KEY_ERROR, JSON_KEY_MESSAGE };
  protected static final String[] requiredErrorLongFields = { JSON_KEY_CODE, JSON_KEY_ERRNO };

  /**
   * The server's URI.
   * <p>
   * We assume throughout that this ends with a trailing slash (and guarantee as
   * much in the constructor).
   */
  protected final String serverURI;

  protected final Executor executor;

  public FxAccountAbstractClient(String serverURI, Executor executor) {
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
   * Process a typed value extracted from a successful response (in an
   * endpoint-dependent way).
   */
  public interface RequestDelegate<T> {
    public void handleError(Exception e);
    public void handleFailure(FxAccountAbstractClientRemoteException e);
    public void handleSuccess(T result);
  }

  /**
   * Intepret a response from the auth server.
   * <p>
   * Throw an appropriate exception on errors; otherwise, return the response's
   * status code.
   *
   * @return response's HTTP status code.
   * @throws FxAccountClientException
   */
  public static int validateResponse(HttpResponse response) throws FxAccountAbstractClientRemoteException {
    final int status = response.getStatusLine().getStatusCode();
    if (status == 200) {
      return status;
    }
    int code;
    int errno;
    String error;
    String message;
    ExtendedJSONObject body;
    try {
      body = new SyncStorageResponse(response).jsonObjectBody();
      body.throwIfFieldsMissingOrMisTyped(requiredErrorStringFields, String.class);
      body.throwIfFieldsMissingOrMisTyped(requiredErrorLongFields, Long.class);
      code = body.getLong(JSON_KEY_CODE).intValue();
      errno = body.getLong(JSON_KEY_ERRNO).intValue();
      error = body.getString(JSON_KEY_ERROR);
      message = body.getString(JSON_KEY_MESSAGE);
    } catch (Exception e) {
      throw new FxAccountAbstractClientMalformedResponseException(response);
    }
    throw new FxAccountAbstractClientRemoteException(response, code, errno, error, message, body);
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
    } catch (UnsupportedEncodingException e) {
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

    protected final RequestDelegate<T> delegate;

    /**
     * Create a delegate for an un-authenticated resource.
     */
    public ResourceDelegate(final Resource resource, final RequestDelegate<T> delegate) {
      super(resource);
      this.delegate = delegate;
    }

    @Override
    public AuthHeaderProvider getAuthHeaderProvider() {
      return super.getAuthHeaderProvider();
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
      } catch (FxAccountAbstractClientRemoteException e) {
        invokeHandleFailure(e);
      }
    }

    protected void invokeHandleFailure(final FxAccountAbstractClientRemoteException e) {
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
      request.addHeader(HttpHeaders.ACCEPT_LANGUAGE, Utils.getLanguageTag(locale));
      request.addHeader(HttpHeaders.ACCEPT, ACCEPT_HEADER);
    }
  }
}
