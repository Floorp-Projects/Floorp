/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Android Sync Client.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jason Voll <jvoll@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko.sync.repositories.android;

import android.provider.BaseColumns;

public final class PasswordColumns implements BaseColumns {

  /*
   * IMPORTANT NOTE
   * This file takes the column names from mobile/android/base/GeckoPassword.java
   * and is included here to avoid creating a compile-time dependency on Fennec.
   */

  public static final String _ID = "id";
  public static final String HOSTNAME = "hostname";
  public static final String HTTP_REALM = "httpRealm";
  public static final String FORM_SUBMIT_URL = "formSubmitURL";
  public static final String USERNAME_FIELD = "usernameField";
  public static final String PASSWORD_FIELD = "passwordField";
  public static final String ENCRYPTED_USERNAME = "encryptedUsername";
  public static final String ENCRYPTED_PASSWORD = "encryptedPassword";
  public static final String GUID = "guid";
  public static final String ENC_TYPE = "encType";
  public static final String TIME_CREATED = "timeCreated";
  public static final String TIME_LAST_USED = "timeLastUsed";
  public static final String TIME_PASSWORD_CHANGED = "timePasswordChanged";
  public static final String TIMES_USED = "timesUsed";
}
