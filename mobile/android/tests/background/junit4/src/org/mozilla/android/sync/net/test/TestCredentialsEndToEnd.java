/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.net.test;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.net.BasicAuthHeaderProvider;

import java.io.IOException;
import java.io.UnsupportedEncodingException;

import ch.boye.httpclientandroidlib.Header;

import static org.junit.Assert.assertEquals;

/**
 * Test the transfer of a UTF-8 string from desktop, and ensure that it results in the
 * correct hashed Basic Auth header.
 */
@RunWith(TestRunner.class)
public class TestCredentialsEndToEnd {

  public static final String REAL_PASSWORD         = "pïgéons1";
  public static final String USERNAME              = "utvm3mk6hnngiir2sp4jsxf2uvoycrv6";
  public static final String DESKTOP_PASSWORD_JSON = "{\"password\":\"pÃ¯gÃ©ons1\"}";
  public static final String BTOA_PASSWORD         = "cMOvZ8Opb25zMQ==";
  public static final int    DESKTOP_ASSERTED_SIZE = 10;
  public static final String DESKTOP_BASIC_AUTH    = "Basic dXR2bTNtazZobm5naWlyMnNwNGpzeGYydXZveWNydjY6cMOvZ8Opb25zMQ==";

  private String getCreds(String password) {
    Header authenticate = new BasicAuthHeaderProvider(USERNAME, password).getAuthHeader(null, null, null);
    return authenticate.getValue();
  }

  @SuppressWarnings("static-method")
  @Test
  public void testUTF8() throws UnsupportedEncodingException {
    final String in  = "pÃ¯gÃ©ons1";
    final String out = "pïgéons1";
    assertEquals(out, Utils.decodeUTF8(in));
  }

  @Test
  public void testAuthHeaderFromPassword() throws NonObjectJSONException, IOException {
    final ExtendedJSONObject parsed = new ExtendedJSONObject(DESKTOP_PASSWORD_JSON);

    final String password = parsed.getString("password");
    final String decoded = Utils.decodeUTF8(password);

    final byte[] expectedBytes = Utils.decodeBase64(BTOA_PASSWORD);
    final String expected = new String(expectedBytes, "UTF-8");

    assertEquals(DESKTOP_ASSERTED_SIZE, password.length());
    assertEquals(expected, decoded);

    System.out.println("Retrieved password: " + password);
    System.out.println("Expected password:  " + expected);
    System.out.println("Rescued password:   " + decoded);

    assertEquals(getCreds(expected), getCreds(decoded));
    assertEquals(getCreds(decoded), DESKTOP_BASIC_AUTH);
  }

  // Note that we do *not* have a test for the J-PAKE setup process
  // (SetupSyncActivity) that actually stores credentials and requires
  // decodeUTF8. This will have to suffice.
}
