/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa;

import ch.boye.httpclientandroidlib.impl.cookie.DateUtils;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.fxa.SkewHandler;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.net.BaseResource;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

@RunWith(TestRunner.class)
public class TestSkewHandler {
  public TestSkewHandler() {
  }

  @Test
  public void testSkewUpdating() throws Throwable {
    SkewHandler h = new SkewHandler("foo.com");
    assertEquals(0L, h.getSkewInSeconds());
    assertEquals(0L, h.getSkewInMillis());

    long server = 1390101197865L;
    long local = server - 4500L;
    h.updateSkewFromServerMillis(server, local);
    assertEquals(4500L, h.getSkewInMillis());
    assertEquals(4L, h.getSkewInSeconds());

    local = server;
    h.updateSkewFromServerMillis(server, local);
    assertEquals(0L, h.getSkewInMillis());
    assertEquals(0L, h.getSkewInSeconds());

    local = server + 500L;
    h.updateSkewFromServerMillis(server, local);
    assertEquals(-500L, h.getSkewInMillis());
    assertEquals(0L, h.getSkewInSeconds());

    String date = "Sat, 18 Jan 2014 19:16:52 PST";
    long dateInMillis = 1390101412000L;              // Obviously this can differ somewhat due to precision.
    long parsed = DateUtils.parseDate(date).getTime();
    assertEquals(parsed, dateInMillis);

    h.updateSkewFromHTTPDateString(date, dateInMillis);
    assertEquals(0L, h.getSkewInMillis());
    assertEquals(0L, h.getSkewInSeconds());

    h.updateSkewFromHTTPDateString(date, dateInMillis + 1100L);
    assertEquals(-1100L, h.getSkewInMillis());
    assertEquals(Math.round(-1100L / 1000L), h.getSkewInSeconds());
  }

  @Test
  public void testSkewSingleton() throws Exception {
    SkewHandler h1 = SkewHandler.getSkewHandlerFromEndpointString("http://foo.com/bar");
    SkewHandler h2 = SkewHandler.getSkewHandlerForHostname("foo.com");
    SkewHandler h3 = SkewHandler.getSkewHandlerForResource(new BaseResource("http://foo.com/baz"));
    assertTrue(h1 == h2);
    assertTrue(h1 == h3);

    SkewHandler.getSkewHandlerForHostname("foo.com").updateSkewFromServerMillis(1390101412000L, 1390001412000L);
    final long actual = SkewHandler.getSkewHandlerForHostname("foo.com").getSkewInMillis();
    assertEquals(100000000L, actual);
  }
}
