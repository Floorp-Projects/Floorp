/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.delegates;

public interface WipeServerDelegate {
  public void onWiped(long timestamp);
  public void onWipeFailed(Exception e);
}
