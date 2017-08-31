/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync.helpers;

public class ExpectStoreCompletedDelegate extends DefaultStoreDelegate {

  @Override
  public void onRecordStoreSucceeded(String guid) {
    // That's fine.
  }

  @Override
  public void onStoreCompleted() {
    performNotify();
  }
}
