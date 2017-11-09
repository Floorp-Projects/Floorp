/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.testhelpers;

import org.mozilla.gecko.sync.stage.AbstractNonRepositorySyncStage;

public class MockAbstractNonRepositorySyncStage extends AbstractNonRepositorySyncStage {
  @Override
  public void execute() {
    session.advance();
  }
}
