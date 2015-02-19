/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.browserid.verifier;

import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URISyntaxException;

import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.net.BaseResource;

/**
 * The verifier protocol changed: version 1 posts form-encoded data; version 2
 * posts JSON data.
 */
public class BrowserIDRemoteVerifierClient20 extends AbstractBrowserIDRemoteVerifierClient {
  public static final String LOG_TAG = BrowserIDRemoteVerifierClient20.class.getSimpleName();

  public static final String DEFAULT_VERIFIER_URL = "https://verifier.accounts.firefox.com/v2";

  protected static final String JSON_KEY_ASSERTION = "assertion";
  protected static final String JSON_KEY_AUDIENCE = "audience";

  public BrowserIDRemoteVerifierClient20() throws URISyntaxException {
    super(new URI(DEFAULT_VERIFIER_URL));
  }

  public BrowserIDRemoteVerifierClient20(URI verifierUri) {
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

    final ExtendedJSONObject requestBody = new ExtendedJSONObject();
    requestBody.put(JSON_KEY_AUDIENCE, audience);
    requestBody.put(JSON_KEY_ASSERTION, assertion);

    try {
      r.post(requestBody);
    } catch (UnsupportedEncodingException e) {
      delegate.handleError(e);
    }
  }
}
