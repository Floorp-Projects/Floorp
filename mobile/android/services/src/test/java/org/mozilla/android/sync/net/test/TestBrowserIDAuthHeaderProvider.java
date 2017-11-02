/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.net.test;

import ch.boye.httpclientandroidlib.Header;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.net.BrowserIDAuthHeaderProvider;

import static org.junit.Assert.assertEquals;

@RunWith(TestRunner.class)
public class TestBrowserIDAuthHeaderProvider {
  @Test
  public void testHeader() {
    Header header = new BrowserIDAuthHeaderProvider("assertion").getAuthHeader(null, null, null);

    assertEquals("authorization", header.getName().toLowerCase());
    assertEquals("BrowserID assertion", header.getValue());
  }
}
