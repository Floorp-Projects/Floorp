/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.jpake.stage;

import org.mozilla.gecko.sync.jpake.JPakeClient;

public abstract class JPakeStage {
  protected final String LOG_TAG = "SyncJPakeStage";
  public abstract void execute(JPakeClient jClient);
}
