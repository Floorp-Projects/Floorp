/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.fxa.profile;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.concurrent.Executor;

import org.mozilla.gecko.background.fxa.oauth.FxAccountAbstractClient;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.BearerAuthHeaderProvider;

import ch.boye.httpclientandroidlib.HttpResponse;


/**
 * Talk to an fxa-profile-server to get profile information like name, age, gender, and avatar image.
 * <p>
 * This client was written against the API documented at <a href="https://github.com/mozilla/fxa-profile-server/blob/0c065619f5a2e867f813a343b4c67da3fe2c82a4/docs/API.md">https://github.com/mozilla/fxa-profile-server/blob/0c065619f5a2e867f813a343b4c67da3fe2c82a4/docs/API.md</a>.
 */
public class FxAccountProfileClient10 extends FxAccountAbstractClient {
  public FxAccountProfileClient10(String serverURI, Executor executor) {
    super(serverURI, executor);
  }

  public void profile(final String token, RequestDelegate<ExtendedJSONObject> delegate) {
    BaseResource resource;
    try {
      resource = new BaseResource(new URI(serverURI + "profile"));
    } catch (URISyntaxException e) {
      invokeHandleError(delegate, e);
      return;
    }

    resource.delegate = new ResourceDelegate<ExtendedJSONObject>(resource, delegate) {
      @Override
      public AuthHeaderProvider getAuthHeaderProvider() {
        return new BearerAuthHeaderProvider(token);
      }

      @Override
      public void handleSuccess(int status, HttpResponse response, ExtendedJSONObject body) {
        try {
          delegate.handleSuccess(body);
          return;
        } catch (Exception e) {
          delegate.handleError(e);
          return;
        }
      }
    };

    resource.get();
  }
}
