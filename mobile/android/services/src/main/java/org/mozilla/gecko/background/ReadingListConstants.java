/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background;

import org.mozilla.gecko.AppConstants;

/**
 * This is in 'background' not 'reading' so that it's still usable even when the
 * Reading List feature is build-time disabled.
 */
public class ReadingListConstants {
  public static final String GLOBAL_LOG_TAG = "FxReadingList";
  public static final String USER_AGENT = "Firefox-Android-FxReader/" + AppConstants.MOZ_APP_VERSION + " (" + AppConstants.MOZ_APP_UA_NAME + ")";
  public static final String DEFAULT_DEV_ENDPOINT = "https://readinglist.dev.mozaws.net/v1/";
  public static final String DEFAULT_PROD_ENDPOINT = "https://readinglist.services.mozilla.com/v1/";

  public static final String OAUTH_SCOPE_READINGLIST = "readinglist";
  public static final String AUTH_TOKEN_TYPE = "oauth::" + OAUTH_SCOPE_READINGLIST;

  public static boolean DEBUG = false;
}
