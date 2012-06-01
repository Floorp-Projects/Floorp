/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.net;

import java.io.BufferedReader;
import java.io.IOException;
import java.lang.ref.WeakReference;
import java.net.URI;
import java.net.URISyntaxException;
import java.security.KeyManagementException;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;

import javax.net.ssl.SSLContext;

import org.mozilla.gecko.sync.Logger;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.HttpVersion;
import ch.boye.httpclientandroidlib.auth.Credentials;
import ch.boye.httpclientandroidlib.auth.UsernamePasswordCredentials;
import ch.boye.httpclientandroidlib.client.AuthCache;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;
import ch.boye.httpclientandroidlib.client.methods.HttpDelete;
import ch.boye.httpclientandroidlib.client.methods.HttpGet;
import ch.boye.httpclientandroidlib.client.methods.HttpPost;
import ch.boye.httpclientandroidlib.client.methods.HttpPut;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.client.methods.HttpUriRequest;
import ch.boye.httpclientandroidlib.client.protocol.ClientContext;
import ch.boye.httpclientandroidlib.conn.ClientConnectionManager;
import ch.boye.httpclientandroidlib.conn.scheme.PlainSocketFactory;
import ch.boye.httpclientandroidlib.conn.scheme.Scheme;
import ch.boye.httpclientandroidlib.conn.scheme.SchemeRegistry;
import ch.boye.httpclientandroidlib.conn.ssl.SSLSocketFactory;
import ch.boye.httpclientandroidlib.impl.auth.BasicScheme;
import ch.boye.httpclientandroidlib.impl.client.BasicAuthCache;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;
import ch.boye.httpclientandroidlib.impl.conn.tsccm.ThreadSafeClientConnManager;
import ch.boye.httpclientandroidlib.params.HttpConnectionParams;
import ch.boye.httpclientandroidlib.params.HttpParams;
import ch.boye.httpclientandroidlib.params.HttpProtocolParams;
import ch.boye.httpclientandroidlib.protocol.BasicHttpContext;
import ch.boye.httpclientandroidlib.protocol.HttpContext;
import ch.boye.httpclientandroidlib.util.EntityUtils;

/**
 * Provide simple HTTP access to a Sync server or similar.
 * Implements Basic Auth by asking its delegate for credentials.
 * Communicates with a ResourceDelegate to asynchronously return responses and errors.
 * Exposes simple get/post/put/delete methods.
 */
public class BaseResource implements Resource {
  private static final String ANDROID_LOOPBACK_IP = "10.0.2.2";

  private static final int MAX_TOTAL_CONNECTIONS     = 20;
  private static final int MAX_CONNECTIONS_PER_ROUTE = 10;

  public static boolean rewriteLocalhost = true;

  private static final String LOG_TAG = "BaseResource";

  protected URI uri;
  protected BasicHttpContext context;
  protected DefaultHttpClient client;
  public    ResourceDelegate delegate;
  protected HttpRequestBase request;
  public String charset = "utf-8";

  protected static WeakReference<HttpResponseObserver> httpResponseObserver = null;

  public BaseResource(String uri) throws URISyntaxException {
    this(uri, rewriteLocalhost);
  }

  public BaseResource(URI uri) {
    this(uri, rewriteLocalhost);
  }

  public BaseResource(String uri, boolean rewrite) throws URISyntaxException {
    this(new URI(uri), rewrite);
  }

  public BaseResource(URI uri, boolean rewrite) {
    if (rewrite && uri.getHost().equals("localhost")) {
      // Rewrite localhost URIs to refer to the special Android emulator loopback passthrough interface.
      Logger.debug(LOG_TAG, "Rewriting " + uri + " to point to " + ANDROID_LOOPBACK_IP + ".");
      try {
        this.uri = new URI(uri.getScheme(), uri.getUserInfo(), ANDROID_LOOPBACK_IP, uri.getPort(), uri.getPath(), uri.getQuery(), uri.getFragment());
      } catch (URISyntaxException e) {
        Logger.error(LOG_TAG, "Got error rewriting URI for Android emulator.", e);
      }
    } else {
      this.uri = uri;
    }
  }

  public static synchronized HttpResponseObserver getHttpResponseObserver() {
    if (httpResponseObserver == null) {
      return null;
    }
    return httpResponseObserver.get();
  }

  public static synchronized void setHttpResponseObserver(HttpResponseObserver newHttpResponseObserver) {
    if (httpResponseObserver != null) {
      httpResponseObserver.clear();
    }
    httpResponseObserver = new WeakReference<HttpResponseObserver>(newHttpResponseObserver);
  }

  public URI getURI() {
    return this.uri;
  }

  /**
   * This shuts up HttpClient, which will otherwise debug log about there
   * being no auth cache in the context.
   */
  private static void addAuthCacheToContext(HttpUriRequest request, HttpContext context) {
    AuthCache authCache = new BasicAuthCache();                // Not thread safe.
    context.setAttribute(ClientContext.AUTH_CACHE, authCache);
  }

  /**
   * Apply the provided credentials string to the provided request.
   * @param credentials a string, "user:pass".
   */
  private static void applyCredentials(String credentials, HttpUriRequest request, HttpContext context) {
    Credentials creds = new UsernamePasswordCredentials(credentials);
    Header header = BasicScheme.authenticate(creds, "US-ASCII", false);
    request.addHeader(header);
    Logger.trace(LOG_TAG, "Adding Basic Auth header.");
  }

  /**
   * Invoke this after delegate and request have been set.
   * @throws NoSuchAlgorithmException
   * @throws KeyManagementException
   */
  private void prepareClient() throws KeyManagementException, NoSuchAlgorithmException {
    context = new BasicHttpContext();

    // We could reuse these client instances, except that we mess around
    // with their parameters… so we'd need a pool of some kind.
    client = new DefaultHttpClient(getConnectionManager());

    // TODO: Eventually we should use Apache HttpAsyncClient. It's not out of alpha yet.
    // Until then, we synchronously make the request, then invoke our delegate's callback.
    String credentials = delegate.getCredentials();
    if (credentials != null) {
      BaseResource.applyCredentials(credentials, request, context);
    }

    addAuthCacheToContext(request, context);

    HttpParams params = client.getParams();
    HttpConnectionParams.setConnectionTimeout(params, delegate.connectionTimeout());
    HttpConnectionParams.setSoTimeout(params, delegate.socketTimeout());
    HttpConnectionParams.setStaleCheckingEnabled(params, false);
    HttpProtocolParams.setContentCharset(params, charset);
    HttpProtocolParams.setVersion(params, HttpVersion.HTTP_1_1);
    delegate.addHeaders(request, client);
  }

  private static Object connManagerMonitor = new Object();
  private static ClientConnectionManager connManager;

  /**
   * This method exists for test code.
   */
  public static ClientConnectionManager enablePlainHTTPConnectionManager() {
    synchronized (connManagerMonitor) {
      ThreadSafeClientConnManager cm = new ThreadSafeClientConnManager();
      connManager = cm;
      return cm;
    }
  }

  // Call within a synchronized block on connManagerMonitor.
  private static ClientConnectionManager enableTLSConnectionManager() throws KeyManagementException, NoSuchAlgorithmException  {
    SSLContext sslContext = SSLContext.getInstance("TLS");
    sslContext.init(null, null, new SecureRandom());
    SSLSocketFactory sf = new TLSSocketFactory(sslContext);
    SchemeRegistry schemeRegistry = new SchemeRegistry();
    schemeRegistry.register(new Scheme("https", 443, sf));
    schemeRegistry.register(new Scheme("http", 80, new PlainSocketFactory()));
    ThreadSafeClientConnManager cm = new ThreadSafeClientConnManager(schemeRegistry);

    cm.setMaxTotal(MAX_TOTAL_CONNECTIONS);
    cm.setDefaultMaxPerRoute(MAX_CONNECTIONS_PER_ROUTE);
    connManager = cm;
    return cm;
  }

  public static ClientConnectionManager getConnectionManager() throws KeyManagementException, NoSuchAlgorithmException
                                                         {
    // TODO: shutdown.
    synchronized (connManagerMonitor) {
      if (connManager != null) {
        return connManager;
      }
      return enableTLSConnectionManager();
    }
  }

  /**
   * Do some cleanup, so we don't need the stale connection check.
   */
  public static void closeExpiredConnections() {
    ClientConnectionManager connectionManager;
    synchronized (connManagerMonitor) {
      connectionManager = connManager;
    }
    if (connectionManager == null) {
      return;
    }
    Logger.trace(LOG_TAG, "Closing expired connections.");
    connectionManager.closeExpiredConnections();
  }

  public static void shutdownConnectionManager() {
    ClientConnectionManager connectionManager;
    synchronized (connManagerMonitor) {
      connectionManager = connManager;
      connManager = null;
    }
    if (connectionManager == null) {
      return;
    }
    Logger.debug(LOG_TAG, "Shutting down connection manager.");
    connectionManager.shutdown();
  }

  private void execute() {
    try {
      HttpResponse response = client.execute(request, context);
      Logger.debug(LOG_TAG, "Response: " + response.getStatusLine().toString());
      HttpResponseObserver observer = getHttpResponseObserver();
      if (observer != null) {
        observer.observeHttpResponse(response);
      }
      delegate.handleHttpResponse(response);
    } catch (ClientProtocolException e) {
      delegate.handleHttpProtocolException(e);
    } catch (IOException e) {
      delegate.handleHttpIOException(e);
    }
  }

  private void go(HttpRequestBase request) {
   if (delegate == null) {
      throw new IllegalArgumentException("No delegate provided.");
    }
    this.request = request;
    try {
      this.prepareClient();
    } catch (KeyManagementException e) {
      Logger.error(LOG_TAG, "Couldn't prepare client.", e);
      delegate.handleTransportException(e);
    } catch (NoSuchAlgorithmException e) {
      Logger.error(LOG_TAG, "Couldn't prepare client.", e);
      delegate.handleTransportException(e);
    }
    this.execute();
  }

  @Override
  public void get() {
    Logger.debug(LOG_TAG, "HTTP GET " + this.uri.toASCIIString());
    this.go(new HttpGet(this.uri));
  }

  @Override
  public void delete() {
    Logger.debug(LOG_TAG, "HTTP DELETE " + this.uri.toASCIIString());
    this.go(new HttpDelete(this.uri));
  }

  @Override
  public void post(HttpEntity body) {
    Logger.debug(LOG_TAG, "HTTP POST " + this.uri.toASCIIString());
    HttpPost request = new HttpPost(this.uri);
    request.setEntity(body);
    this.go(request);
  }

  @Override
  public void put(HttpEntity body) {
    Logger.debug(LOG_TAG, "HTTP PUT " + this.uri.toASCIIString());
    HttpPut request = new HttpPut(this.uri);
    request.setEntity(body);
    this.go(request);
  }

  /**
   * Best-effort attempt to ensure that the entity has been fully consumed and
   * that the underlying stream has been closed.
   *
   * This releases the connection back to the connection pool.
   *
   * @param entity The HttpEntity to be consumed.
   */
  public static void consumeEntity(HttpEntity entity) {
    try {
      EntityUtils.consume(entity);
    } catch (IOException e) {
      // Doesn't matter.
    }
  }

  /**
   * Best-effort attempt to ensure that the entity corresponding to the given
   * HTTP response has been fully consumed and that the underlying stream has
   * been closed.
   *
   * This releases the connection back to the connection pool.
   *
   * @param response
   *          The HttpResponse to be consumed.
   */
  public static void consumeEntity(HttpResponse response) {
    if (response == null) {
      return;
    }
    try {
      EntityUtils.consume(response.getEntity());
    } catch (IOException e) {
    }
  }

  /**
   * Best-effort attempt to ensure that the entity corresponding to the given
   * Sync storage response has been fully consumed and that the underlying
   * stream has been closed.
   *
   * This releases the connection back to the connection pool.
   *
   * @param response
   *          The SyncStorageResponse to be consumed.
   */
  public static void consumeEntity(SyncStorageResponse response) {
    if (response.httpResponse() == null) {
      return;
    }
    consumeEntity(response.httpResponse());
  }

  /**
   * Best-effort attempt to ensure that the reader has been fully consumed, so
   * that the underlying stream will be closed.
   *
   * This should allow the connection to be released back to the connection pool.
   *
   * @param reader The BufferedReader to be consumed.
   */
  public static void consumeReader(BufferedReader reader) {
    try {
      reader.close();
    } catch (IOException e) {
      // Do nothing.
    }
  }
}
