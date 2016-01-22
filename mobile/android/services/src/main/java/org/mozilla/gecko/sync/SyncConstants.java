/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import org.mozilla.gecko.AppConstants;

public class SyncConstants {
  public static final String GLOBAL_LOG_TAG = "FxSync";
  public static final String SYNC_MAJOR_VERSION  = "1";
  public static final String SYNC_MINOR_VERSION  = "0";
  public static final String SYNC_VERSION_STRING = SYNC_MAJOR_VERSION + "." +
                                                   AppConstants.MOZ_APP_VERSION + "." +
                                                   SYNC_MINOR_VERSION;

  public static final String USER_AGENT = "Firefox AndroidSync " +
                                          SYNC_VERSION_STRING + " (" +
                                          AppConstants.MOZ_APP_UA_NAME + ")";
}
