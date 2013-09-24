/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync;

import java.util.Arrays;

import org.mozilla.gecko.background.helpers.AndroidSyncTestCase;
import org.mozilla.gecko.sync.setup.activities.WebURLFinder;

/**
 * These tests are on device because the WebKit APIs are stubs on desktop.
 */
public class TestWebURLFinder extends AndroidSyncTestCase {
  public String find(String string) {
    return new WebURLFinder(string).bestWebURL();
  }

  public String find(String[] strings) {
    return new WebURLFinder(Arrays.asList(strings)).bestWebURL();
  }

  public void testNoEmail() {
    assertNull(find("test@test.com"));
  }

  public void testSchemeFirst() {
    assertEquals("http://scheme.com", find("test.com http://scheme.com"));
  }

  public void testFullURL() {
    assertEquals("http://scheme.com:8080/inner#anchor&arg=1", find("test.com http://scheme.com:8080/inner#anchor&arg=1"));
  }

  public void testNoScheme() {
    assertEquals("noscheme.com", find("noscheme.com"));
  }

  public void testNoBadScheme() {
    assertNull(find("file:///test javascript:///test.js"));
  }

  public void testStrings() {
    assertEquals("http://test.com", find(new String[] { "http://test.com", "noscheme.com" }));
    assertEquals("http://test.com", find(new String[] { "noschemefirst.com", "http://test.com" }));
    assertEquals("http://test.com/inner#test", find(new String[] { "noschemefirst.com", "http://test.com/inner#test", "http://second.org/fark" }));
    assertEquals("http://test.com", find(new String[] { "javascript:///test.js", "http://test.com" }));
  }
}
