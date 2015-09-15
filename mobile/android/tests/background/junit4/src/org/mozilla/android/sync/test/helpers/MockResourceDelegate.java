/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.test.helpers;

import static org.junit.Assert.assertEquals;

import java.io.IOException;
import java.security.GeneralSecurityException;

import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.BasicAuthHeaderProvider;
import org.mozilla.gecko.sync.net.ResourceDelegate;

import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;

public class MockResourceDelegate implements ResourceDelegate {
  public WaitHelper waitHelper = null;
  public static String USER_PASS    = "john:password";
  public static String EXPECT_BASIC = "Basic am9objpwYXNzd29yZA==";

  public boolean handledHttpResponse = false;
  public HttpResponse httpResponse = null;

  public MockResourceDelegate(WaitHelper waitHelper) {
    this.waitHelper = waitHelper;
  }

  public MockResourceDelegate() {
    this.waitHelper = WaitHelper.getTestWaiter();
  }

  @Override
  public String getUserAgent() {
    return null;
  }

  @Override
  public void addHeaders(HttpRequestBase request, DefaultHttpClient client) {
  }

  @Override
  public int connectionTimeout() {
    return 0;
  }

  @Override
  public int socketTimeout() {
    return 0;
  }

  @Override
  public AuthHeaderProvider getAuthHeaderProvider() {
    return new BasicAuthHeaderProvider(USER_PASS);
  }

  @Override
  public void handleHttpProtocolException(ClientProtocolException e) {
    waitHelper.performNotify(e);
  }

  @Override
  public void handleHttpIOException(IOException e) {
    waitHelper.performNotify(e);
  }

  @Override
  public void handleTransportException(GeneralSecurityException e) {
    waitHelper.performNotify(e);
  }

  @Override
  public void handleHttpResponse(HttpResponse response) {
    handledHttpResponse = true;
    httpResponse = response;

    assertEquals(response.getStatusLine().getStatusCode(), 200);
    BaseResource.consumeEntity(response);
    waitHelper.performNotify();
  }
}
