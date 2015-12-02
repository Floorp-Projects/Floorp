/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.delegates;

import java.util.concurrent.ExecutorService;

import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.RepositorySessionBundle;

public interface RepositorySessionFinishDelegate {
  public void onFinishFailed(Exception ex);
  public void onFinishSucceeded(RepositorySession session, RepositorySessionBundle bundle);
  public RepositorySessionFinishDelegate deferredFinishDelegate(ExecutorService executor);
}
