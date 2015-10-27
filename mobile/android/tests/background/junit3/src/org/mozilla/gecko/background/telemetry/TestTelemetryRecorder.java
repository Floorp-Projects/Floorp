/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.telemetry;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.UnsupportedEncodingException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

import junit.framework.Assert;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.background.datareporting.TelemetryRecorder;
import org.mozilla.gecko.background.helpers.FakeProfileTestCase;

import android.util.Base64;

public class TestTelemetryRecorder extends FakeProfileTestCase {
  private TelemetryRecorder telemetryRecorder;
  private File TelemetryControllerDir;
  private File cacheDir;

  private static final String DEST_FILENAME = "dest-filename";
  private final String TEST_PAYLOAD1 = "{\"ver\": 1, \"measurements\":" +
		                                   "{ \"uptime\": 24982 }, \"data\": {";
  private final String TEST_PAYLOAD2 = "\"key1\": \"value1\", \"key2\": \"value2\" } }";

  @Override
  protected void setUp() throws Exception {
    super.setUp();
    TelemetryControllerDir = new File(fakeProfileDirectory, "telemetry-ping");
    if (!TelemetryControllerDir.mkdir()) {
      fail("Could not create directory for telemetry pings.");
    }
    cacheDir = new File(fakeProfileDirectory, "fakeCacheDir");
    if (!cacheDir.mkdir()) {
      fail("Could not create directory for fake cacheDir");
    }
  }

  public void testConstructorWithoutParentDir() {
    File fileNotDirectory = new File(TelemetryControllerDir, "testFile");
    try {
      fileNotDirectory.createNewFile();
    } catch (IOException e) {
      fail("Failed to create new file.");
    }
    try {
      telemetryRecorder = new TelemetryRecorder(fileNotDirectory, cacheDir, "filename");
      fail("Should have thrown");
    } catch (Exception e) {
      Assert.assertTrue(e instanceof IllegalArgumentException);
    }
    fileNotDirectory.delete();
  }

  /**
   * Check that the real file is not created.
   */
  public void testStartPingFile() {
    File destFile = new File(DEST_FILENAME);
    if (destFile.exists()) {
      destFile.delete();
    }
    telemetryRecorder = new TelemetryRecorder(TelemetryControllerDir, cacheDir, DEST_FILENAME);
    try {
      telemetryRecorder.startPingFile();
    } catch (Exception e) {
      fail("Error starting ping file: " + e.getMessage());
    }
    Assert.assertFalse(destFile.exists());
  }

  /**
   * Check that caller-specified file contains the expected contents and
   * verify the checksum.
   */
  public void testFinishedPingFile() {
    telemetryRecorder = new TelemetryRecorder(TelemetryControllerDir, cacheDir, DEST_FILENAME);
    String charset = telemetryRecorder.getCharset();
    try {
      telemetryRecorder.startPingFile();
      telemetryRecorder.appendPayload(TEST_PAYLOAD1);
      telemetryRecorder.appendPayload(TEST_PAYLOAD2);
      telemetryRecorder.finishPingFile();
    } catch (Exception e) {
      fail("Error writing payload: " + e);
    }

    File destFile = new File(TelemetryControllerDir, DEST_FILENAME);
    Assert.assertTrue(destFile.exists());

    StringBuilder sb = new StringBuilder();
    FileInputStream fis = null;
    InputStreamReader isr = null;
    try {
      fis = new FileInputStream(destFile);
      isr = new InputStreamReader(fis, charset);;
      // Test data is short, and we don't want extra characters in the string.
      char[] charBuf = new char[1];

      // Read contents into StringBuilder.
      while (isr.read(charBuf) != -1) {
        sb.append(charBuf);
      }
    } catch (FileNotFoundException e) {
      fail("Unable to find file: " + e);
    } catch (UnsupportedEncodingException e) {
      fail("Failing with UnsupportedEncodingException: " + e);
    } catch (IOException e) {
      fail("Failing with IOException: " + e);
    } finally {
      try {
        fis.close();
        isr.close();
      } catch (IOException e) {
        // Do nothing.
      }
    }
    String fileContents = sb.toString();

    JSONObject fileJSON = null;
    try {
      fileJSON = new JSONObject(fileContents);
    } catch (JSONException e) {
      fail("Failing with bad JSON: " + e);
    }

    // Check format.
    Assert.assertTrue(fileJSON.has("slug"));
    Assert.assertTrue(fileJSON.has("payload"));
    Assert.assertTrue(fileJSON.has("checksum"));

    // Check header.
    String pingSlug;
    try {
      pingSlug = fileJSON.getString("slug");
      Assert.assertEquals(DEST_FILENAME, pingSlug);
    } catch (JSONException e) {
      fail("JSONException attempting to fetch slug: " + e);
    }

    // Calculate and check the checksum.
    try {
      MessageDigest checksumDigest = MessageDigest.getInstance("SHA-256");
      String payload = fileJSON.getString("payload");
      checksumDigest.update(payload.getBytes(charset));
      String calculatedChecksum = Base64.encodeToString(checksumDigest.digest(), Base64.NO_WRAP);

      String payloadChecksum = fileJSON.getString("checksum");
      Assert.assertEquals(payloadChecksum, calculatedChecksum);
    } catch (JSONException e) {
      fail("Failed to fetch JSON value: " + e);
    } catch (NoSuchAlgorithmException e) {
      fail("No such MessageDigest algorithm: " + e);
    } catch (UnsupportedEncodingException e) {
      fail("Unsupported encoding: " + e);
    }
  }
}
