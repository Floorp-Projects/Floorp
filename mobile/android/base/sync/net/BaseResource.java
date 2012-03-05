/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Android Sync Client.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Richard Newman <rnewman@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko.sync.net;

import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.security.KeyManagementException;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;

import javax.net.ssl.SSLContext;

import android.util.Log;
import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.HttpVersion;
import ch.boye.httpclientandroidlib.auth.Credentials;
import ch.boye.httpclientandroidlib.auth.UsernamePasswordCredentials;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;
import ch.boye.httpclientandroidlib.client.methods.HttpDelete;
import ch.boye.httpclientandroidlib.client.methods.HttpGet;
import ch.boye.httpclientandroidlib.client.methods.HttpPost;
import ch.boye.httpclientandroidlib.client.methods.HttpPut;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.client.methods.HttpUriRequest;
import ch.boye.httpclientandroidlib.conn.ClientConnectionManager;
import ch.boye.httpclientandroidlib.conn.scheme.PlainSocketFactory;
import ch.boye.httpclientandroidlib.conn.scheme.Scheme;
import ch.boye.httpclientandroidlib.conn.scheme.SchemeRegistry;
import ch.boye.httpclientandroidlib.conn.ssl.SSLSocketFactory;
import ch.boye.httpclientandroidlib.impl.auth.BasicScheme;
import ch.boye.httpclientandroidlib.impl.client.AbstractHttpClient;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;
import ch.boye.httpclientandroidlib.impl.conn.tsccm.ThreadSafeClientConnManager;
import ch.boye.httpclientandroidlib.params.HttpConnectionParams;
import ch.boye.httpclientandroidlib.params.HttpParams;
import ch.boye.httpclientandroidlib.params.HttpProtocolParams;
import ch.boye.httpclientandroidlib.protocol.BasicHttpContext;
import ch.boye.httpclientandroidlib.protocol.HttpContext;

/**
 * Provide simple HTTP access to a Sync server or similar.
 * Implements Basic Auth by asking its delegate for credentials.
 * Communicates with a ResourceDelegate to asynchronously return responses and errors.
 * Exposes simple get/post/put/delete methods.
 */
public class BaseResource implements Resource {
  public static boolean rewriteLocalhost = true;

  private static final String LOG_TAG = "BaseResource";
  protected URI uri;
  protected BasicHttpContext context;
  protected DefaultHttpClient client;
  public    ResourceDelegate delegate;
  protected HttpRequestBase request;
  public String charset = "utf-8";

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
      Log.d(LOG_TAG, "Rewriting " + uri + " to point to 10.0.2.2.");
      try {
        this.uri = new URI(uri.getScheme(), uri.getUserInfo(), "10.0.2.2", uri.getPort(), uri.getPath(), uri.getQuery(), uri.getFragment());
      } catch (URISyntaxException e) {
        Log.e(LOG_TAG, "Got error rewriting URI for Android emulator.", e);
      }
    } else {
      this.uri = uri;
    }
  }

  public URI getURI() {
    return this.uri;
  }

  /**
   * Apply the provided credentials string to the provided request.
   * @param credentials
   *        A string, "user:pass".
   * @param client
   * @param request
   * @param context
   */
  private static void applyCredentials(String credentials, AbstractHttpClient client, HttpUriRequest request, HttpContext context) {
    Credentials creds = new UsernamePasswordCredentials(credentials);
    Header header = BasicScheme.authenticate(creds, "US-ASCII", false);
    request.addHeader(header);
    Log.d(LOG_TAG, "Adding auth header " + header);
  }

  /**
   * Invoke this after delegate and request have been set.
   * @throws NoSuchAlgorithmException
   * @throws KeyManagementException
   */
  private void prepareClient() throws KeyManagementException, NoSuchAlgorithmException {
    ClientConnectionManager connectionManager = getConnectionManager();
    context = new BasicHttpContext();
    client = new DefaultHttpClient(connectionManager);

    // TODO: Eventually we should use Apache HttpAsyncClient. It's not out of alpha yet.
    // Until then, we synchronously make the request, then invoke our delegate's callback.
    String credentials = delegate.getCredentials();
    if (credentials != null) {
      BaseResource.applyCredentials(credentials, client, request, context);
    }

    HttpParams params = client.getParams();
    HttpConnectionParams.setConnectionTimeout(params, delegate.connectionTimeout());
    HttpConnectionParams.setSoTimeout(params, delegate.socketTimeout());
    HttpProtocolParams.setContentCharset(params, charset);
    HttpProtocolParams.setVersion(params, HttpVersion.HTTP_1_1);
    delegate.addHeaders(request, client);
  }

  private static Object connManagerMonitor = new Object();
  private static ClientConnectionManager connManager;

  /**
   * This method exists for test code.
   * @return
   */
  public static ClientConnectionManager enablePlainHTTPConnectionManager() {
    synchronized (connManagerMonitor) {
      ThreadSafeClientConnManager cm = new ThreadSafeClientConnManager();
      connManager = cm;
      return cm;
    }
  }

  private static ClientConnectionManager enableTLSConnectionManager() throws KeyManagementException, NoSuchAlgorithmException  {
    SSLContext sslContext = SSLContext.getInstance("TLS");
    sslContext.init(null, null, new SecureRandom());
    SSLSocketFactory sf = new TLSSocketFactory(sslContext);
    SchemeRegistry schemeRegistry = new SchemeRegistry();
    schemeRegistry.register(new Scheme("https", 443, sf));
    schemeRegistry.register(new Scheme("http", 80, new PlainSocketFactory()));
    ThreadSafeClientConnManager cm = new ThreadSafeClientConnManager(schemeRegistry);
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

  private void execute() {
    try {
      HttpResponse response = client.execute(request, context);
      Log.i(LOG_TAG, "Response: " + response.getStatusLine().toString());
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
      Log.e(LOG_TAG, "Couldn't prepare client.", e);
      delegate.handleTransportException(e);
    } catch (NoSuchAlgorithmException e) {
      Log.e(LOG_TAG, "Couldn't prepare client.", e);
      delegate.handleTransportException(e);
    }
    this.execute();
  }

  @Override
  public void get() {
    Log.i(LOG_TAG, "HTTP GET " + this.uri.toASCIIString());
    this.go(new HttpGet(this.uri));
  }

  @Override
  public void delete() {
    Log.i(LOG_TAG, "HTTP DELETE " + this.uri.toASCIIString());
    this.go(new HttpDelete(this.uri));
  }

  @Override
  public void post(HttpEntity body) {
    Log.i(LOG_TAG, "HTTP POST " + this.uri.toASCIIString());
    HttpPost request = new HttpPost(this.uri);
    request.setEntity(body);
    this.go(request);
  }

  @Override
  public void put(HttpEntity body) {
    Log.i(LOG_TAG, "HTTP PUT " + this.uri.toASCIIString());
    HttpPut request = new HttpPut(this.uri);
    request.setEntity(body);
    this.go(request);
  }
}
