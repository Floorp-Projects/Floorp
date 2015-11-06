/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.testhelpers;

public class MockServerSyncStage extends BaseMockServerSyncStage {
  @Override
  public void execute() {
    session.advance();
  }
}
