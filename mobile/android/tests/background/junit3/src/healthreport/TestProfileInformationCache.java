/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.healthreport;

import java.io.File;
import java.io.IOException;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.background.helpers.FakeProfileTestCase;

public class TestProfileInformationCache extends FakeProfileTestCase {

  public final void testInitState() throws IOException {
    MockProfileInformationCache cache = new MockProfileInformationCache(this.fakeProfileDirectory.getAbsolutePath());
    assertFalse(cache.isInitialized());
    assertFalse(cache.needsWrite());

    try {
      cache.isBlocklistEnabled();
      fail("Should throw fetching isBlocklistEnabled.");
    } catch (IllegalStateException e) {
      // Great!
    }

    cache.beginInitialization();
    assertFalse(cache.isInitialized());
    assertTrue(cache.needsWrite());

    try {
      cache.isBlocklistEnabled();
      fail("Should throw fetching isBlocklistEnabled.");
    } catch (IllegalStateException e) {
      // Great!
    }

    cache.completeInitialization();
    assertTrue(cache.isInitialized());
    assertFalse(cache.needsWrite());
  }

  public final MockProfileInformationCache makeCache(final String suffix) {
    File subdir = new File(this.fakeProfileDirectory.getAbsolutePath() + File.separator + suffix);
    subdir.mkdir();
    return new MockProfileInformationCache(subdir.getAbsolutePath());
  }

  public final void testPersisting() throws IOException {
    MockProfileInformationCache cache = makeCache("testPersisting");
    // We start with no file.
    assertFalse(cache.getFile().exists());

    // Partially populate. Note that this doesn't happen in live code, but
    // apparently we can end up with null add-ons JSON on disk, so this
    // reproduces that scenario.
    cache.beginInitialization();
    cache.setBlocklistEnabled(true);
    cache.setTelemetryEnabled(true);
    cache.setProfileCreationTime(1234L);
    cache.completeInitialization();

    assertTrue(cache.getFile().exists());

    // But reading this from disk won't work, because we were only partially
    // initialized. We want to start over.
    cache = makeCache("testPersisting");
    assertFalse(cache.isInitialized());
    assertFalse(cache.restoreUnlessInitialized());
    assertFalse(cache.isInitialized());

    // Now fully populate, and try again...
    cache.beginInitialization();
    cache.setBlocklistEnabled(true);
    cache.setTelemetryEnabled(true);
    cache.setProfileCreationTime(1234L);
    cache.setJSONForAddons(new JSONObject());
    cache.completeInitialization();
    assertTrue(cache.getFile().exists());

    // ... and this time we succeed.
    cache = makeCache("testPersisting");
    assertFalse(cache.isInitialized());
    assertTrue(cache.restoreUnlessInitialized());
    assertTrue(cache.isInitialized());
    assertTrue(cache.isBlocklistEnabled());
    assertTrue(cache.isTelemetryEnabled());
    assertEquals(1234L, cache.getProfileCreationTime());

    // Mutate.
    cache.beginInitialization();
    assertFalse(cache.isInitialized());
    cache.setBlocklistEnabled(false);
    cache.setProfileCreationTime(2345L);
    cache.completeInitialization();
    assertTrue(cache.isInitialized());

    cache = makeCache("testPersisting");
    assertFalse(cache.isInitialized());
    assertTrue(cache.restoreUnlessInitialized());

    assertTrue(cache.isInitialized());
    assertFalse(cache.isBlocklistEnabled());
    assertTrue(cache.isTelemetryEnabled());
    assertEquals(2345L, cache.getProfileCreationTime());
  }

  public final void testVersioning() throws JSONException, IOException {
    MockProfileInformationCache cache = makeCache("testVersioning");
    final int currentVersion = ProfileInformationCache.FORMAT_VERSION;
    final JSONObject json = cache.toJSON();
    assertEquals(currentVersion, json.getInt("version"));

    // Initialize enough that we can re-load it.
    cache.beginInitialization();
    cache.setJSONForAddons(new JSONObject());
    cache.completeInitialization();
    cache.writeJSON(json);
    assertTrue(cache.restoreUnlessInitialized());
    cache.beginInitialization();     // So that we'll need to read again.
    json.put("version", currentVersion + 1);
    cache.writeJSON(json);

    // We can't restore a future version.
    assertFalse(cache.restoreUnlessInitialized());
  }

  public void testRestoreInvalidJSON() throws Exception {
    final MockProfileInformationCache cache = makeCache("invalid");

    final JSONObject invalidJSON = new JSONObject();
    invalidJSON.put("blocklist", true);
    invalidJSON.put("telemetry", false);
    invalidJSON.put("profileCreated", 1234567L);
    cache.writeJSON(invalidJSON);
    assertFalse(cache.restoreUnlessInitialized());
  }

  private JSONObject getValidCacheJSON() throws Exception {
    final JSONObject json = new JSONObject();
    json.put("blocklist", true);
    json.put("telemetry", false);
    json.put("profileCreated", 1234567L);
    json.put("addons", new JSONObject());
    json.put("version", ProfileInformationCache.FORMAT_VERSION);
    return json;
  }

  public void testRestoreImplicitV1() throws Exception {
    assertTrue(ProfileInformationCache.FORMAT_VERSION > 1);

    final MockProfileInformationCache cache = makeCache("implicitV1");
    final JSONObject json = getValidCacheJSON();
    json.remove("version");
    cache.writeJSON(json);
    // Can't restore v1 (which is implicitly set) since it is not the current version.
    assertFalse(cache.restoreUnlessInitialized());
  }

  public void testRestoreOldVersion() throws Exception {
    final int oldVersion = 1;
    assertTrue(ProfileInformationCache.FORMAT_VERSION > oldVersion);

    final MockProfileInformationCache cache = makeCache("oldVersion");
    final JSONObject json = getValidCacheJSON();
    json.put("version", oldVersion);
    cache.writeJSON(json);
    assertFalse(cache.restoreUnlessInitialized());
  }

  public void testRestoreCurrentVersion() throws Exception {
    final MockProfileInformationCache cache = makeCache("currentVersion");
    final JSONObject json = getValidCacheJSON();
    cache.writeJSON(json);
    cache.beginInitialization();
    cache.setTelemetryEnabled(true);
    cache.completeInitialization();
    assertEquals(ProfileInformationCache.FORMAT_VERSION, cache.readJSON().getInt("version"));
  }
}
