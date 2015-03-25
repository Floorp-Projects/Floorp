/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reading;

import org.mozilla.gecko.AppConstants;

public class ReadingListConstants {
  public static final String GLOBAL_LOG_TAG = "FxReadingList";
  public static final String USER_AGENT = "Firefox-Android-FxReader/" + AppConstants.MOZ_APP_VERSION + " (" + AppConstants.MOZ_APP_DISPLAYNAME + ")";
  public static final String DEFAULT_DEV_ENDPOINT = "https://readinglist.dev.mozaws.net/v1/";
  public static final String DEFAULT_PROD_ENDPOINT = "https://readinglist.services.mozilla.com/v1/";

  public static final String OAUTH_ENDPOINT_PROD = "https://oauth.accounts.firefox.com/v1";

  public static boolean DEBUG = false;
}
