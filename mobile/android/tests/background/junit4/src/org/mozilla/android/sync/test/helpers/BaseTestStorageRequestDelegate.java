/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.test.helpers;

import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.BasicAuthHeaderProvider;
import org.mozilla.gecko.sync.net.SyncStorageRequestDelegate;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

import java.io.IOException;

import static org.junit.Assert.fail;

public class BaseTestStorageRequestDelegate implements
    SyncStorageRequestDelegate {

  protected final AuthHeaderProvider authHeaderProvider;

  public BaseTestStorageRequestDelegate(AuthHeaderProvider authHeaderProvider) {
    this.authHeaderProvider = authHeaderProvider;
  }

  public BaseTestStorageRequestDelegate(String username, String password) {
    this(new BasicAuthHeaderProvider(username, password));
  }

  @Override
  public AuthHeaderProvider getAuthHeaderProvider() {
    return authHeaderProvider;
  }

  @Override
  public String ifUnmodifiedSince() {
    return null;
  }

  @Override
  public void handleRequestSuccess(SyncStorageResponse response) {
    BaseResource.consumeEntity(response);
    fail("Should not be called.");
  }

  @Override
  public void handleRequestFailure(SyncStorageResponse response) {
    System.out.println("Response: " + response.httpResponse().getStatusLine().getStatusCode());
    BaseResource.consumeEntity(response);
    fail("Should not be called.");
  }

  @Override
  public void handleRequestError(Exception e) {
    if (e instanceof IOException) {
      System.out.println("WARNING: TEST FAILURE IGNORED!");
      // Assume that this is because Jenkins doesn't have network access.
      return;
    }
    fail("Should not error.");
  }
}
