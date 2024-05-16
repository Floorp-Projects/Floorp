/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.util;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.util.Arrays;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

@RunWith(RobolectricTestRunner.class)
public class WebAuthnUtilsTest {
  @Test
  public void requestJSONForMakeCredentail() throws Exception {
    final GeckoBundle credentialBundle = new GeckoBundle(7);

    final GeckoBundle rpBundle = new GeckoBundle(2);
    rpBundle.putString("id", "example.com");
    rpBundle.putString("name", "Example");
    credentialBundle.putBundle("rp", rpBundle);

    final GeckoBundle userBundle = new GeckoBundle(2);
    userBundle.putString("name", "Foo");
    userBundle.putString("displayName", "Foo");
    credentialBundle.putBundle("user", userBundle);

    credentialBundle.putString("origin", "example.com");
    credentialBundle.putDouble("timeout", 5000.0);
    credentialBundle.putString("attestation", "none");

    final GeckoBundle authenticatorSelection = new GeckoBundle(3);
    authenticatorSelection.putString("userVerification", "preferred");
    authenticatorSelection.putString("authenticatorAttachment", "platform");
    authenticatorSelection.putString("residentKey", "required");

    final byte[] userId = new byte[] {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7};
    final byte[] challenge =
        new byte[] {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};
    final int[] algs = new int[] {-7};
    final WebAuthnUtils.WebAuthnPublicCredential[] excludeList =
        new WebAuthnUtils.WebAuthnPublicCredential[0];

    final JSONObject request =
        WebAuthnUtils.getJSONObjectForMakeCredential(
            credentialBundle, userId, challenge, algs, excludeList, authenticatorSelection);

    final JSONObject expected =
        new JSONObject(
            "{"
                + "\"attestation\":\"none\","
                + "\"challenge\":\"AAECAwQFBgcICQoLDA0ODw\","
                + "\"authenticatorSelection\":{"
                + "\"userVerification\":\"preferred\",\"requireResidentKey\":true,"
                + "\"residentKey\":\"required\",\"authenticatorAttachment\":\"platform\"},"
                + "\"user\":{\"displayName\":\"Foo\",\"name\":\"Foo\",\"id\":\"AAECAwQFBgc\"},"
                + "\"rp\":{\"name\":\"Example\",\"id\":\"example.com\"},"
                + "\"excludeCredentials\":[],"
                + "\"timeout\":5000,"
                + "\"pubKeyCredParams\":[{\"type\":\"public-key\",\"alg\":-7}],"
                + "\"extensions\":{\"credProps\":true}"
                + "}");

    // No method to compare JSONObject. Use GeckoBundle instead.
    assertEquals(
        "request JSON for MakeCredential should be matched",
        GeckoBundle.fromJSONObject(expected),
        GeckoBundle.fromJSONObject(request));
  }

  @Test
  public void requestJSONForGetAssertion() throws Exception {
    final GeckoBundle assertionBundle = new GeckoBundle(3);
    assertionBundle.putDouble("timeout", 5000.0);
    assertionBundle.putString("rpId", "example.com");
    assertionBundle.putString("userVerification", "preferred");
    final GeckoBundle extensionsBundle = new GeckoBundle(0);

    final byte[] challenge =
        new byte[] {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};
    final WebAuthnUtils.WebAuthnPublicCredential[] allowList =
        new WebAuthnUtils.WebAuthnPublicCredential[0];

    final JSONObject response =
        WebAuthnUtils.getJSONObjectForGetAssertion(
            challenge, allowList, assertionBundle, extensionsBundle);

    final JSONObject expected =
        new JSONObject(
            "{"
                + "\"allowCredentials\":[],"
                + "\"challenge\":\"AAECAwQFBgcICQoLDA0ODw\","
                + "\"timeout\":5000,"
                + "\"rpId\":\"example.com\","
                + "\"userVerification\":\"preferred\""
                + "}");

    // No method to compare JSONObject. Use GeckoBundle instead.
    assertEquals(
        "request JSON for GetAssertion should be matched",
        GeckoBundle.fromJSONObject(expected),
        GeckoBundle.fromJSONObject(response));
  }

  @Test
  public void responseJSONForMakeCredential() throws Exception {
    // attestationObject isn't valid format, but this unit test is that parsing JSON and building
    // parameters.
    final String responseJSON =
        "{"
            + "\"id\": \"AAECAwQFBgcICQoLDA0ODw\" ,"
            + "\"rawId\": \"AAECAwQFBgcICQoLDA0ODw\" ,"
            + "\"type\": \"public-key\" ,"
            + "\"authenticatorAttachment\": \"platform\", "
            + "\"response\": {\"attestationObject\": \"AQIDBAUGBwgJCgsMDQ4PEA\", \"transports\": [ \"internal\" ]}"
            + "}";
    final WebAuthnUtils.MakeCredentialResponse response =
        WebAuthnUtils.getMakeCredentialResponse(responseJSON);

    final byte[] rawId =
        new byte[] {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};
    final byte[] attestationObject =
        new byte[] {
          0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10
        };
    assertTrue("rawId should be matched", Arrays.equals(response.keyHandle, rawId));
    assertTrue(
        "attestationObject should be matched",
        Arrays.equals(response.attestationObject, attestationObject));
    assertEquals("No clientDataJson in response", response.clientDataJson, null);
  }

  @Test(expected = JSONException.class)
  public void invalidMakeCredential() throws Exception {
    final String responseJSON =
        "{"
            + "\"type\": \"public-key\" ,"
            + "\"authenticatorAttachment\": \"platform\", "
            + "\"response\": {\"attestationObject\": \"AQIDBAUGBwgJCgsMDQ4PEA\", \"transports\": [ \"internal\" ]}"
            + "}";
    final WebAuthnUtils.MakeCredentialResponse response =
        WebAuthnUtils.getMakeCredentialResponse(responseJSON);
    assertTrue("Not reached", false);
  }

  @Test
  public void responseJSONForGetAssertion() throws Exception {
    // authenticatorData and signature aren't valid format, but this unit test is that parsing JSON
    // and building parameters.
    final String responseJSON =
        "{"
            + "\"id\": \"AAECAwQFBgcICQoLDA0ODw\" ,"
            + "\"rawId\": \"AAECAwQFBgcICQoLDA0ODw\" ,"
            + "\"authenticatorAttachment\": \"platform\", "
            + "\"response\": {\"authenticatorData\": \"AQIDBAUGBwgJCgsMDQ4PEA\", \"signature\": \"AgMEBQYHCAkKCwwNDg8QEQ\"}"
            + "}";
    final WebAuthnUtils.GetAssertionResponse response =
        WebAuthnUtils.getGetAssertionResponse(responseJSON);

    final byte[] rawId =
        new byte[] {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};
    final byte[] authData =
        new byte[] {
          0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10
        };
    final byte[] signature =
        new byte[] {
          0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11
        };
    assertTrue("rawId should be matched", Arrays.equals(response.keyHandle, rawId));
    assertTrue("authenticatorData should be matched", Arrays.equals(response.authData, authData));
    assertTrue("signature should be matched", Arrays.equals(response.signature, signature));
    assertEquals(
        "authenticatorAttachment should be matched", response.authenticatorAttachment, "platform");
    assertEquals("No clientDataJson in response", response.clientDataJson, null);
  }

  @Test(expected = JSONException.class)
  public void invalidGetAssertion() throws Exception {
    final String responseJSON =
        "{"
            + "\"authenticatorAttachment\": \"platform\", "
            + "\"response\": {\"authenticatorData\": \"AQIDBAUGBwgJCgsMDQ4PEA\", \"signature\": \"AgMEBQYHCAkKCwwNDg8QEQ\"}"
            + "}";
    final WebAuthnUtils.GetAssertionResponse response =
        WebAuthnUtils.getGetAssertionResponse(responseJSON);
    assertTrue("Not reached", false);
  }
}
