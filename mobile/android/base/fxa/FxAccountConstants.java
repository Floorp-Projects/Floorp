/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa;

import org.mozilla.gecko.AppConstants;

public class FxAccountConstants {
  public static final String GLOBAL_LOG_TAG = "FxAccounts";
  public static final String ACCOUNT_TYPE = AppConstants.MOZ_ANDROID_SHARED_FXACCOUNT_TYPE;

  // Must be a client ID allocated with "canGrant" privileges!
  public static final String OAUTH_CLIENT_ID_FENNEC = "3332a18d142636cb";

  public static final String DEFAULT_AUTH_SERVER_ENDPOINT = "https://api.accounts.firefox.com/v1";
  public static final String DEFAULT_TOKEN_SERVER_ENDPOINT = "https://token.services.mozilla.com/1.0/sync/1.5";
  public static final String DEFAULT_OAUTH_SERVER_ENDPOINT = "https://oauth.accounts.firefox.com/v1";

  public static final String STAGE_AUTH_SERVER_ENDPOINT = "https://stable.dev.lcip.org/auth/v1";
  public static final String STAGE_TOKEN_SERVER_ENDPOINT = "https://stable.dev.lcip.org/syncserver/token/1.0/sync/1.5";
  public static final String STAGE_OAUTH_SERVER_ENDPOINT = "https://oauth-stable.dev.lcip.org/v1";

  // You must be at least 13 years old, on the day of creation, to create a Firefox Account.
  public static final int MINIMUM_AGE_TO_CREATE_AN_ACCOUNT = 13;

  // You must wait 15 minutes after failing an age check before trying to create a different account.
  public static final long MINIMUM_TIME_TO_WAIT_AFTER_AGE_CHECK_FAILED_IN_MILLISECONDS = 15 * 60 * 1000;

  public static final String USER_AGENT = "Firefox-Android-FxAccounts/" + AppConstants.MOZ_APP_VERSION + " (" + AppConstants.MOZ_APP_DISPLAYNAME + ")";

  public static final String ACCOUNT_PICKLE_FILENAME = "fxa.account.json";

  /**
   * It's possible to migrate an Old Sync account to a new Firefox Account.
   * During migration, we pickle the Old Sync Account preferences to disk; this
   * is the name of that file. It's only written during migration and is simply
   * here as an escape hatch: if we see problems in the wild, we can produce a
   * hot-fix add-on that deletes the migrated Firefox Account and re-instates
   * this pickle file.
   * <p>
   * Must not contain path separators!
   */
  public static final String OLD_SYNC_ACCOUNT_PICKLE_FILENAME = "old.sync.account.json";

  /**
   * This action is broadcast when an Android Firefox Account is deleted.
   * This allows each installed Firefox to delete any Firefox Account pickle
   * file.
   * <p>
   * It is protected by signing-level permission PER_ACCOUNT_TYPE_PERMISSION and
   * can be received only by Firefox channels sharing the same Android Firefox
   * Account type.
   * <p>
   * See {@link org.mozilla.gecko.fxa.AndroidFxAccount#makeDeletedAccountIntent()}
   * for contents of the intent.
   *
   * See bug 790931 for additional information in the context of Sync.
   */
  public static final String ACCOUNT_DELETED_ACTION = AppConstants.MOZ_ANDROID_SHARED_FXACCOUNT_TYPE + ".accounts.ACCOUNT_DELETED_ACTION";

  /**
   * Version number of contents of SYNC_ACCOUNT_DELETED_ACTION intent.
   */
  public static final long ACCOUNT_DELETED_INTENT_VERSION = 1;

  public static final String ACCOUNT_DELETED_INTENT_VERSION_KEY = "account_deleted_intent_version";
  public static final String ACCOUNT_DELETED_INTENT_ACCOUNT_KEY = "account_deleted_intent_account";
  public static final String ACCOUNT_OAUTH_SERVICE_ENDPOINT_KEY = "account_oauth_service_endpoint";
  public static final String ACCOUNT_DELETED_INTENT_ACCOUNT_AUTH_TOKENS = "account_deleted_intent_auth_tokens";

  /**
   * This signing-level permission protects broadcast intents that should be
   * received only by Firefox channels sharing the same Android Firefox Account type.
   */
  public static final String PER_ACCOUNT_TYPE_PERMISSION = AppConstants.MOZ_ANDROID_SHARED_FXACCOUNT_TYPE + ".permission.PER_ACCOUNT_TYPE";

  /**
   * This action is broadcast when an Android Firefox Account's internal state
   * is changed.
   * <p>
   * It is protected by signing-level permission PER_ACCOUNT_TYPE_PERMISSION and
   * can be received only by Firefox versions sharing the same Android Firefox
   * Account type.
   */
  public static final String ACCOUNT_STATE_CHANGED_ACTION = AppConstants.MOZ_ANDROID_SHARED_FXACCOUNT_TYPE + ".accounts.ACCOUNT_STATE_CHANGED_ACTION";
}
