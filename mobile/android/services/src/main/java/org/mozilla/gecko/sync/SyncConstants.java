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

  public static final String ACCOUNTTYPE_SYNC = AppConstants.MOZ_ANDROID_SHARED_ACCOUNT_TYPE;

  /**
   * Bug 790931: this action is broadcast when an Android Sync Account is
   * deleted.  This allows each installed Firefox to delete any Sync Account
   * pickle file and to (try to) wipe its client record from the Sync server.
   * <p>
   * It is protected by signing-level permission PER_ACCOUNT_TYPE_PERMISSION and
   * can be received only by Firefox versions sharing the same Android Sync
   * Account type.
   * <p>
   * See {@link org.mozilla.gecko.sync.setup.SyncAccounts#makeSyncAccountDeletedIntent(android.content.Context, android.accounts.AccountManager, android.accounts.Account)}
   * for contents of the intent.
   */
  public static final String SYNC_ACCOUNT_DELETED_ACTION = AppConstants.MOZ_ANDROID_SHARED_ACCOUNT_TYPE + ".accounts.SYNC_ACCOUNT_DELETED_ACTION";

  /**
   * Bug 790931: version number of contents of SYNC_ACCOUNT_DELETED_ACTION
   * intent.
   * <p>
   * See {@link org.mozilla.gecko.sync.setup.SyncAccounts#makeSyncAccountDeletedIntent(android.content.Context, android.accounts.AccountManager, android.accounts.Account)}
   * for contents of the intent.
   */
  public static final long SYNC_ACCOUNT_DELETED_INTENT_VERSION = 1;

  /**
   * Bug 790931: this signing-level permission protects broadcast intents that
   * should be received only by Firefox versions sharing the same Android Sync
   * Account type.
   */
  public static final String PER_ACCOUNT_TYPE_PERMISSION = AppConstants.MOZ_ANDROID_SHARED_ACCOUNT_TYPE + ".permission.PER_ACCOUNT_TYPE";

  public static final String DEFAULT_AUTH_SERVER = "https://auth.services.mozilla.com/";

  // Used for BackoffHandler storage for Sync 1.1's SyncAdapter.
  public static final String BACKOFF_PREF_SUFFIX_11 = "sync";
}
