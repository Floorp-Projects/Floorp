/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.healthreport;

import java.io.File;
import java.io.IOException;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.helpers.FakeProfileTestCase;

public class TestEnvironmentBuilder extends FakeProfileTestCase {
  public static void testIgnoringAddons() throws JSONException {
    Environment env = new Environment() {
      @Override
      public int register() {
        return 0;
      }
    };

    JSONObject addons = new JSONObject();
    JSONObject foo = new JSONObject();
    foo.put("a", 1);
    foo.put("b", "c");
    addons.put("foo", foo);
    JSONObject ignore = new JSONObject();
    ignore.put("ignore", true);
    addons.put("ig", ignore);

    env.setJSONForAddons(addons);

    JSONObject kept = env.getNonIgnoredAddons();
    assertTrue(kept.has("foo"));
    assertFalse(kept.has("ig"));
    JSONObject fooCopy = kept.getJSONObject("foo");
    assertSame(foo, fooCopy);
  }

  public void testSanity() throws IOException {
    File subdir = new File(this.fakeProfileDirectory.getAbsolutePath() +
                           File.separator + "testPersisting");
    subdir.mkdir();
    long now = System.currentTimeMillis();
    int expectedDays = (int) (now / GlobalConstants.MILLISECONDS_PER_DAY);

    MockProfileInformationCache cache = new MockProfileInformationCache(subdir.getAbsolutePath());
    assertFalse(cache.getFile().exists());
    cache.beginInitialization();
    cache.setBlocklistEnabled(true);
    cache.setTelemetryEnabled(false);
    cache.setProfileCreationTime(now);
    cache.completeInitialization();
    assertTrue(cache.getFile().exists());

    final AndroidConfigurationProvider configProvider = new AndroidConfigurationProvider(context);
    Environment environment = EnvironmentBuilder.getCurrentEnvironment(cache, configProvider);
    assertEquals(AppConstants.MOZ_APP_BUILDID, environment.appBuildID);
    assertEquals("Android", environment.os);
    assertTrue(100 < environment.memoryMB); // Seems like a sane lower bound...
    assertTrue(environment.cpuCount >= 1);
    assertEquals(1, environment.isBlocklistEnabled);
    assertEquals(0, environment.isTelemetryEnabled);
    assertEquals(expectedDays, environment.profileCreation);
    assertEquals(EnvironmentBuilder.getCurrentEnvironment(cache, configProvider).getHash(),
                 environment.getHash());

    // v3 sanity.
    assertEquals(configProvider.hasHardwareKeyboard(), environment.hasHardwareKeyboard);
    assertEquals(configProvider.getScreenXInMM(), environment.screenXInMM);
    assertTrue(1 < environment.screenXInMM);
    assertTrue(2000 > environment.screenXInMM);

    cache.beginInitialization();
    cache.setBlocklistEnabled(false);
    cache.completeInitialization();

    assertFalse(EnvironmentBuilder.getCurrentEnvironment(cache, configProvider).getHash()
                                  .equals(environment.getHash()));
  }
}
