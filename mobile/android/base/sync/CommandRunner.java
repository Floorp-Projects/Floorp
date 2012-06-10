/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import java.util.List;

public abstract class CommandRunner {
  public final int argCount;

  public CommandRunner(int argCount) {
    this.argCount = argCount;
  }

  public abstract void executeCommand(List<String> args);

  public boolean argumentsAreValid(List<String> args) {
    return args != null &&
           args.size() == argCount;
  }
}
