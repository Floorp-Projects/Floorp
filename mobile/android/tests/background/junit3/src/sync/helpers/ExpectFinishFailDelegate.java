/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync.helpers;

import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;

public class ExpectFinishFailDelegate extends DefaultFinishDelegate {
  @Override
  public void onFinishFailed(Exception ex) {
    if (!(ex instanceof InvalidSessionTransitionException)) {
      performNotify("Expected InvalidSessionTransititionException but got ", ex);
    }
  }
}
