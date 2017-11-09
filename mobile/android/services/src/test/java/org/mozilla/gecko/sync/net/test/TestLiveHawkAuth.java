/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.net.test;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.entity.StringEntity;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;
import ch.boye.httpclientandroidlib.message.BasicHeader;
import ch.boye.httpclientandroidlib.protocol.BasicHttpContext;
import org.junit.Assert;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.BaseResourceDelegate;
import org.mozilla.gecko.sync.net.HawkAuthHeaderProvider;
import org.mozilla.gecko.sync.net.Resource;
import org.mozilla.gecko.sync.net.SyncResponse;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.security.GeneralSecurityException;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;


public class TestLiveHawkAuth {
  /**
   * Hawk comes with an example/usage.js server. Modify it to serve indefinitely,
   * un-comment the following line, and verify that the port and credentials
   * have not changed; then the following test should pass.
   */
  // @org.junit.Test
  public void testHawkUsage() throws Exception {
    // Id and credentials are hard-coded in example/usage.js.
    final String id = "dh37fgj492je";
    final byte[] key = "werxhqb98rpaxn39848xrunpaw3489ruxnpa98w4rxn".getBytes("UTF-8");
    final BaseResource resource = new BaseResource("http://localhost:8000/", false);

    // Basic GET.
    resource.delegate = new TestBaseResourceDelegate(resource, new HawkAuthHeaderProvider(id, key, false, 0L));
    WaitHelper.getTestWaiter().performWait(new Runnable() {
      @Override
      public void run() {
        resource.get();
      }
    });

    // PUT with payload verification.
    resource.delegate = new TestBaseResourceDelegate(resource, new HawkAuthHeaderProvider(id, key, true, 0L));
    WaitHelper.getTestWaiter().performWait(new Runnable() {
      @Override
      public void run() {
        try {
          resource.put(new StringEntity("Thank you for flying Hawk"));
        } catch (UnsupportedEncodingException e) {
          WaitHelper.getTestWaiter().performNotify(e);
        }
      }
    });

    // PUT with a large (32k or so) body and payload verification.
    final StringBuilder sb = new StringBuilder();
    for (int i = 0; i < 16000; i++) {
      sb.append(Integer.valueOf(i % 100).toString());
    }
    resource.delegate = new TestBaseResourceDelegate(resource, new HawkAuthHeaderProvider(id, key, true, 0L));
    WaitHelper.getTestWaiter().performWait(new Runnable() {
      @Override
      public void run() {
        try {
          resource.put(new StringEntity(sb.toString()));
        } catch (UnsupportedEncodingException e) {
          WaitHelper.getTestWaiter().performNotify(e);
        }
      }
    });

    // PUT without payload verification.
    resource.delegate = new TestBaseResourceDelegate(resource, new HawkAuthHeaderProvider(id, key, false, 0L));
    WaitHelper.getTestWaiter().performWait(new Runnable() {
      @Override
      public void run() {
        try {
          resource.put(new StringEntity("Thank you for flying Hawk"));
        } catch (UnsupportedEncodingException e) {
          WaitHelper.getTestWaiter().performNotify(e);
        }
      }
    });

    // PUT with *bad* payload verification.
    HawkAuthHeaderProvider provider = new HawkAuthHeaderProvider(id, key, true, 0L) {
      @Override
      public Header getAuthHeader(HttpRequestBase request, BasicHttpContext context, DefaultHttpClient client) throws GeneralSecurityException {
        Header header = super.getAuthHeader(request, context, client);
        // Here's a cheap way of breaking the hash.
        String newValue = header.getValue().replaceAll("hash=\"....", "hash=\"XXXX");
        return new BasicHeader(header.getName(), newValue);
      }
    };

    resource.delegate = new TestBaseResourceDelegate(resource, provider);
    try {
      WaitHelper.getTestWaiter().performWait(new Runnable() {
        @Override
        public void run() {
          try {
            resource.put(new StringEntity("Thank you for flying Hawk"));
          } catch (UnsupportedEncodingException e) {
            WaitHelper.getTestWaiter().performNotify(e);
          }
        }
      });
      fail("Expected assertion after 401 response.");
    } catch (WaitHelper.InnerError e) {
      assertTrue(e.innerError instanceof AssertionError);
      assertEquals("expected:<200> but was:<401>", ((AssertionError) e.innerError).getMessage());
    }
  }

  protected final class TestBaseResourceDelegate extends BaseResourceDelegate {
    protected final HawkAuthHeaderProvider provider;

    protected TestBaseResourceDelegate(Resource resource, HawkAuthHeaderProvider provider) throws UnsupportedEncodingException {
      super(resource);
      this.provider = provider;
    }

    @Override
    public AuthHeaderProvider getAuthHeaderProvider() {
      return provider;
    }

    @Override
    public int connectionTimeout() {
      return 1000;
    }

    @Override
    public int socketTimeout() {
      return 1000;
    }

    @Override
    public void handleHttpResponse(HttpResponse response) {
      SyncResponse res = new SyncResponse(response);
      try {
        Assert.assertEquals(200, res.getStatusCode());
        WaitHelper.getTestWaiter().performNotify();
      } catch (Throwable e) {
        WaitHelper.getTestWaiter().performNotify(e);
      }
    }

    @Override
    public void handleTransportException(GeneralSecurityException e) {
      WaitHelper.getTestWaiter().performNotify(e);
    }

    @Override
    public void handleHttpProtocolException(ClientProtocolException e) {
      WaitHelper.getTestWaiter().performNotify(e);
    }

    @Override
    public void handleHttpIOException(IOException e) {
      WaitHelper.getTestWaiter().performNotify(e);
    }

    @Override
    public String getUserAgent() {
      return null;
    }
  }
}
