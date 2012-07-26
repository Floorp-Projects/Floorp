/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup;

import android.content.Intent;

public class Constants {
  // Constants for Firefox Sync SyncAdapter Accounts.
  public static final String OPTION_SYNCKEY       = "option.synckey";
  public static final String OPTION_USERNAME      = "option.username";
  public static final String AUTHTOKEN_TYPE_PLAIN = "auth.plain";
  public static final String OPTION_SERVER        = "option.serverUrl";
  public static final String ACCOUNT_GUID         = "account.guid";
  public static final String CLIENT_NAME          = "account.clientName";
  public static final String NUM_CLIENTS          = "account.numClients";
  public static final String DATA_ENABLE_ON_UPGRADE = "data.enableOnUpgrade";

  /**
   * Name of file to pickle current account preferences to each sync.
   * <p>
   * Must not contain path separators!
   */
  public static final String ACCOUNT_PICKLE_FILENAME = "sync.account.json";

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

  // Constants for Activities.
  public static final String INTENT_EXTRA_IS_SETUP        = "isSetup";
  public static final String INTENT_EXTRA_IS_PAIR         = "isPair";
  public static final String INTENT_EXTRA_IS_ACCOUNTERROR = "isAccountError";

  public static final int FLAG_ACTIVITY_REORDER_TO_FRONT_NO_ANIMATION =
    Intent.FLAG_ACTIVITY_REORDER_TO_FRONT |
    Intent.FLAG_ACTIVITY_NO_ANIMATION;

  // Constants for Account Authentication.
  public static final String AUTH_NODE_DEFAULT    = "https://auth.services.mozilla.com/";
  public static final String AUTH_NODE_PATHNAME   = "user/";
  public static final String AUTH_NODE_VERSION    = "1.0/";
  public static final String AUTH_NODE_SUFFIX     = "node/weave";
  public static final String AUTH_SERVER_VERSION  = "1.1/";
  public static final String AUTH_SERVER_SUFFIX   = "info/collections/";

  // Account Authentication Errors.
  public static final String AUTH_ERROR_NOUSER    = "auth.error.badcredentials";

  // Links for J-PAKE setup help pages.
  public static final String LINK_FIND_CODE       = "https://support.mozilla.org/kb/find-code-to-add-device-to-firefox-sync";
  public static final String LINK_FIND_ADD_DEVICE = "https://support.mozilla.org/kb/add-a-device-to-firefox-sync";

  // Constants for JSON payload.
  public static final String JSON_KEY_PAYLOAD    = "payload";
  public static final String JSON_KEY_CIPHERTEXT = "ciphertext";
  public static final String JSON_KEY_HMAC       = "hmac";
  public static final String JSON_KEY_IV         = "IV";
  public static final String JSON_KEY_TYPE       = "type";
  public static final String JSON_KEY_VERSION    = "version";
  public static final String JSON_KEY_ETAG       = "etag";

  public static final String JSON_KEY_ACCOUNT    = "account";
  public static final String JSON_KEY_PASSWORD   = "password";
  public static final String JSON_KEY_SYNCKEY    = "synckey";
  public static final String JSON_KEY_SERVER     = "serverURL";
  public static final String JSON_KEY_CLUSTER    = "clusterURL";
  public static final String JSON_KEY_CLIENT_NAME = "clientName";
  public static final String JSON_KEY_CLIENT_GUID = "clientGUID";
  public static final String JSON_KEY_SYNC_AUTOMATICALLY = "syncAutomatically";
  public static final String JSON_KEY_TIMESTAMP  = "timestamp";

  public static final String CRYPTO_KEY_GR1 = "gr1";
  public static final String CRYPTO_KEY_GR2 = "gr2";

  public static final String ZKP_KEY_GX1    = "gx1";
  public static final String ZKP_KEY_GX2    = "gx2";

  public static final String ZKP_KEY_ZKP_X1 = "zkp_x1";
  public static final String ZKP_KEY_ZKP_X2 = "zkp_x2";
  public static final String ZKP_KEY_B      = "b";
  public static final String ZKP_KEY_GR     = "gr";
  public static final String ZKP_KEY_ID     = "id";

  public static final String ZKP_KEY_A      = "A";
  public static final String ZKP_KEY_ZKP_A  = "zkp_A";

  // J-PAKE errors.
  public static final String JPAKE_ERROR_CHANNEL          = "jpake.error.channel";
  public static final String JPAKE_ERROR_NETWORK          = "jpake.error.network";
  public static final String JPAKE_ERROR_SERVER           = "jpake.error.server";
  public static final String JPAKE_ERROR_TIMEOUT          = "jpake.error.timeout";
  public static final String JPAKE_ERROR_INTERNAL         = "jpake.error.internal";
  public static final String JPAKE_ERROR_INVALID          = "jpake.error.invalid";
  public static final String JPAKE_ERROR_NODATA           = "jpake.error.nodata";
  public static final String JPAKE_ERROR_KEYMISMATCH      = "jpake.error.keymismatch";
  public static final String JPAKE_ERROR_WRONGMESSAGE     = "jpake.error.wrongmessage";
  public static final String JPAKE_ERROR_USERABORT        = "jpake.error.userabort";
  public static final String JPAKE_ERROR_DELAYUNSUPPORTED = "jpake.error.delayunsupported";
}
