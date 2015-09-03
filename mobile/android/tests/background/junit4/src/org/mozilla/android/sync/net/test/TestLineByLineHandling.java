/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.net.test;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.io.IOException;
import java.io.PrintStream;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;

import org.junit.Test;
import org.mozilla.android.sync.test.helpers.HTTPServerTestHelper;
import org.mozilla.android.sync.test.helpers.MockServer;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.SyncStorageCollectionRequest;
import org.mozilla.gecko.sync.net.SyncStorageCollectionRequestDelegate;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.simpleframework.http.Request;
import org.simpleframework.http.Response;

public class TestLineByLineHandling {
  private static final int     TEST_PORT   = HTTPServerTestHelper.getTestPort();
  private static final String  TEST_SERVER = "http://localhost:" + TEST_PORT;
  private static final String  LOG_TAG     = "TestLineByLineHandling";
  static String                STORAGE_URL = TEST_SERVER + "/1.1/c6o7dvmr2c4ud2fyv6woz2u4zi22bcyd/storage/lines";
  private HTTPServerTestHelper data        = new HTTPServerTestHelper();

  public ArrayList<String>     lines       = new ArrayList<String>();

  public class LineByLineMockServer extends MockServer {
    public void handle(Request request, Response response) {
      try {
        System.out.println("Handling line-by-line request...");
        PrintStream bodyStream = this.handleBasicHeaders(request, response, 200, "application/newlines");

        bodyStream.print("First line.\n");
        bodyStream.print("Second line.\n");
        bodyStream.print("Third line.\n");
        bodyStream.print("Fourth line.\n");
        bodyStream.close();
      } catch (IOException e) {
        System.err.println("Oops.");
      }
    }
  }

  public class BaseLineByLineDelegate extends
      SyncStorageCollectionRequestDelegate {

    @Override
    public void handleRequestProgress(String progress) {
      lines.add(progress);
    }

    @Override
    public AuthHeaderProvider getAuthHeaderProvider() {
      return null;
    }

    @Override
    public String ifUnmodifiedSince() {
      return null;
    }

    @Override
    public void handleRequestSuccess(SyncStorageResponse res) {
      Logger.info(LOG_TAG, "Request success.");
      assertTrue(res.wasSuccessful());
      assertTrue(res.httpResponse().containsHeader("X-Weave-Timestamp"));

      assertEquals(lines.size(), 4);
      assertEquals(lines.get(0), "First line.");
      assertEquals(lines.get(1), "Second line.");
      assertEquals(lines.get(2), "Third line.");
      assertEquals(lines.get(3), "Fourth line.");
      data.stopHTTPServer();
    }

    @Override
    public void handleRequestFailure(SyncStorageResponse response) {
      Logger.info(LOG_TAG, "Got request failure: " + response);
      BaseResource.consumeEntity(response);
      fail("Should not be called.");
    }

    @Override
    public void handleRequestError(Exception ex) {
      Logger.error(LOG_TAG, "Got request error: ", ex);
      fail("Should not be called.");
    }
  }

  @Test
  public void testLineByLine() throws URISyntaxException {
    BaseResource.rewriteLocalhost = false;

    data.startHTTPServer(new LineByLineMockServer());
    Logger.info(LOG_TAG, "Server started.");
    SyncStorageCollectionRequest r = new SyncStorageCollectionRequest(new URI(STORAGE_URL));
    SyncStorageCollectionRequestDelegate delegate = new BaseLineByLineDelegate();
    r.delegate = delegate;
    r.get();
    // Server is stopped in the callback.
  }
}
