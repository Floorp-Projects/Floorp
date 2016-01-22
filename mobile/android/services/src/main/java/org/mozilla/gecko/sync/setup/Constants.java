/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup;

public class Constants {
  public static final String DEFAULT_PROFILE = "default";

  /**
   * Key in sync extras bundle specifying stages to sync this sync session.
   * <p>
   * Corresponding value should be a String JSON-encoding an object, the keys of
   * which are the stage names to sync. For example:
   * <code>"{ \"stageToSync\": 0 }"</code>.
   */
  public static final String EXTRAS_KEY_STAGES_TO_SYNC = "sync";

  /**
   * Key in sync extras bundle specifying stages to skip this sync session.
   * <p>
   * Corresponding value should be a String JSON-encoding an object, the keys of
   * which are the stage names to skip. For example:
   * <code>"{ \"stageToSkip\": 0 }"</code>.
   */
  public static final String EXTRAS_KEY_STAGES_TO_SKIP = "skip";

  public static final String JSON_KEY_ACCOUNT    = "account";
}
