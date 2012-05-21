/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.delegates;

import java.util.concurrent.ExecutorService;

public interface RepositorySessionWipeDelegate {
  public void onWipeFailed(Exception ex);
  public void onWipeSucceeded();
  public RepositorySessionWipeDelegate deferredWipeDelegate(ExecutorService executor);
}
