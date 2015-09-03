/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.repositories.test;

import java.net.URISyntaxException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

import org.junit.Assert;
import org.junit.Test;
import org.mozilla.android.sync.test.helpers.HTTPServerTestHelper;
import org.mozilla.android.sync.test.helpers.MockServer;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.sync.InfoCollections;
import org.mozilla.gecko.sync.JSONRecordFetcher;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;
import org.mozilla.gecko.sync.stage.SafeConstrainedServer11Repository;
import org.simpleframework.http.Request;
import org.simpleframework.http.Response;

public class TestSafeConstrainedServer11Repository {
  private static final int     TEST_PORT      = HTTPServerTestHelper.getTestPort();
  private static final String  TEST_SERVER    = "http://localhost:" + TEST_PORT + "/";
  private static final String  TEST_USERNAME  = "c6o7dvmr2c4ud2fyv6woz2u4zi22bcyd";
  private static final String  TEST_BASE_PATH = "/1.1/" + TEST_USERNAME + "/";

  public AuthHeaderProvider getAuthHeaderProvider() {
    return null;
  }

  protected final InfoCollections infoCollections = new InfoCollections();

  private class CountsMockServer extends MockServer {
    public final AtomicInteger count = new AtomicInteger(0);
    public final AtomicBoolean error = new AtomicBoolean(false);

    @Override
    public void handle(Request request, Response response) {
      final String path = request.getPath().getPath();
      if (error.get()) {
        super.handle(request, response, 503, "Unavailable.");
        return;
      }

      if (!path.startsWith(TEST_BASE_PATH)) {
        super.handle(request, response, 404, "Not Found");
        return;
      }

      if (path.endsWith("info/collections")) {
        super.handle(request, response, 200, "{\"rotary\": 123456789}");
        return;
      }

      if (path.endsWith("info/collection_counts")) {
        super.handle(request, response, 200, "{\"rotary\": " + count.get() + "}");
        return;
      }

      super.handle(request, response);
    }
  }

  /**
   * Ensure that a {@link SafeConstrainedServer11Repository} will advise
   * skipping if the reported collection counts are higher than its limit.
   */
  @Test
  public void testShouldSkip() throws URISyntaxException {
    final HTTPServerTestHelper data = new HTTPServerTestHelper();
    final CountsMockServer server = new CountsMockServer();
    data.startHTTPServer(server);

    try {
      String countsURL = TEST_SERVER + TEST_BASE_PATH + "info/collection_counts";
      JSONRecordFetcher countFetcher = new JSONRecordFetcher(countsURL, getAuthHeaderProvider());
      String sort = "sortindex";
      String collection = "rotary";

      final int TEST_LIMIT = 1000;
      final SafeConstrainedServer11Repository repo = new SafeConstrainedServer11Repository(
          collection, getCollectionURL(collection), null, infoCollections,
          TEST_LIMIT, sort, countFetcher);

      final AtomicBoolean shouldSkipLots = new AtomicBoolean(false);
      final AtomicBoolean shouldSkipFew = new AtomicBoolean(true);
      final AtomicBoolean shouldSkip503 = new AtomicBoolean (false);

      WaitHelper.getTestWaiter().performWait(2000, new Runnable() {
        @Override
        public void run() {
          repo.createSession(new RepositorySessionCreationDelegate() {
            @Override
            public void onSessionCreated(RepositorySession session) {
              // Try with too many items.
              server.count.set(TEST_LIMIT + 1);
              shouldSkipLots.set(session.shouldSkip());

              // … and few enough that we should sync.
              server.count.set(TEST_LIMIT - 1);
              shouldSkipFew.set(session.shouldSkip());

              // Now try with an error response. We advise skipping if we can't
              // fetch counts, because we'll try again later.
              server.error.set(true);
              shouldSkip503.set(session.shouldSkip());

              WaitHelper.getTestWaiter().performNotify();
            }

            @Override
            public void onSessionCreateFailed(Exception ex) {
              WaitHelper.getTestWaiter().performNotify(ex);
            }

            @Override
            public RepositorySessionCreationDelegate deferredCreationDelegate() {
              return this;
            }
          }, null);
        }
      });

      Assert.assertTrue(shouldSkipLots.get());
      Assert.assertFalse(shouldSkipFew.get());
      Assert.assertTrue(shouldSkip503.get());

    } finally {
      data.stopHTTPServer();
    }
  }

  protected String getCollectionURL(String collection) {
    return TEST_BASE_PATH + "/storage/" + collection;
  }
}
