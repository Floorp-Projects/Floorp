/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync.helpers;

import org.mozilla.gecko.sync.repositories.InvalidRequestException;
import org.mozilla.gecko.sync.repositories.domain.Record;

public class ExpectInvalidRequestFetchDelegate extends DefaultFetchDelegate {
  public static final String LOG_TAG = "ExpInvRequestFetchDel";

  @Override
  public void onFetchFailed(Exception ex, Record rec) {
    if (ex instanceof InvalidRequestException) {
      onDone();
    } else {
      performNotify("Expected InvalidRequestException but got ", ex);
    }
  }

  private void onDone() {
    performNotify();
  }
}
