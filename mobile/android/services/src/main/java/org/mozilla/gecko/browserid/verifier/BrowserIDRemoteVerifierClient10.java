/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.browserid.verifier;

import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.Arrays;
import java.util.List;

import org.mozilla.gecko.sync.net.BaseResource;

import ch.boye.httpclientandroidlib.NameValuePair;
import ch.boye.httpclientandroidlib.client.entity.UrlEncodedFormEntity;
import ch.boye.httpclientandroidlib.message.BasicNameValuePair;

/**
 * The verifier protocol changed: version 1 posts form-encoded data; version 2
 * posts JSON data.
 */
public class BrowserIDRemoteVerifierClient10 extends AbstractBrowserIDRemoteVerifierClient {
  public static final String LOG_TAG = BrowserIDRemoteVerifierClient10.class.getSimpleName();

  public static final String DEFAULT_VERIFIER_URL = "https://verifier.login.persona.org/verify";

  public BrowserIDRemoteVerifierClient10() throws URISyntaxException {
    super(new URI(DEFAULT_VERIFIER_URL));
  }

  public BrowserIDRemoteVerifierClient10(URI verifierUri) {
    super(verifierUri);
  }

  @Override
  public void verify(String audience, String assertion, final BrowserIDVerifierDelegate delegate) {
    if (audience == null) {
      throw new IllegalArgumentException("audience cannot be null.");
    }
    if (assertion == null) {
      throw new IllegalArgumentException("assertion cannot be null.");
    }
    if (delegate == null) {
      throw new IllegalArgumentException("delegate cannot be null.");
    }

    BaseResource r = new BaseResource(verifierUri);

    r.delegate = new RemoteVerifierResourceDelegate(r, delegate);

    List<NameValuePair> nvps = Arrays.asList(new NameValuePair[] {
        new BasicNameValuePair("audience", audience),
        new BasicNameValuePair("assertion", assertion) });

    try {
      r.post(new UrlEncodedFormEntity(nvps, "UTF-8"));
    } catch (UnsupportedEncodingException e) {
      delegate.handleError(e);
    }
  }
}
