/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

import org.mozilla.gecko.db.BrowserContract;

import android.net.Uri;

public class BrowserContractHelpers extends BrowserContract {

  protected static Uri withSyncAndDeleted(Uri u) {
    return u.buildUpon()
            .appendQueryParameter(PARAM_IS_SYNC, "true")
            .appendQueryParameter(PARAM_SHOW_DELETED, "true")
            .build();
  }
  protected static Uri withSync(Uri u) {
    return u.buildUpon()
        .appendQueryParameter(PARAM_IS_SYNC, "true")
        .build();
  }

  public static final Uri IMAGES_CONTENT_URI               = withSyncAndDeleted(Images.CONTENT_URI);
  public static final Uri BOOKMARKS_CONTENT_URI            = withSyncAndDeleted(Bookmarks.CONTENT_URI);
  public static final Uri BOOKMARKS_PARENTS_CONTENT_URI    = withSyncAndDeleted(Bookmarks.PARENTS_CONTENT_URI);
  public static final Uri BOOKMARKS_POSITIONS_CONTENT_URI  = withSyncAndDeleted(Bookmarks.POSITIONS_CONTENT_URI);
  public static final Uri HISTORY_CONTENT_URI              = withSyncAndDeleted(History.CONTENT_URI);
  public static final Uri SCHEMA_CONTENT_URI               = withSyncAndDeleted(Schema.CONTENT_URI);
  public static final Uri PASSWORDS_CONTENT_URI            = withSyncAndDeleted(Passwords.CONTENT_URI);
  public static final Uri DELETED_PASSWORDS_CONTENT_URI    = withSyncAndDeleted(DeletedPasswords.CONTENT_URI);
  public static final Uri FORM_HISTORY_CONTENT_URI         = withSync(FormHistory.CONTENT_URI);
  public static final Uri DELETED_FORM_HISTORY_CONTENT_URI = withSync(DeletedFormHistory.CONTENT_URI);

  public static final String[] PasswordColumns = new String[] {
    Passwords.ID,
    Passwords.HOSTNAME,
    Passwords.HTTP_REALM,
    Passwords.FORM_SUBMIT_URL,
    Passwords.USERNAME_FIELD,
    Passwords.PASSWORD_FIELD,
    Passwords.ENCRYPTED_USERNAME,
    Passwords.ENCRYPTED_PASSWORD,
    Passwords.ENC_TYPE,
    Passwords.TIME_CREATED,
    Passwords.TIME_LAST_USED,
    Passwords.TIME_PASSWORD_CHANGED,
    Passwords.TIMES_USED,
    Passwords.GUID
  };

  public static final String[] HistoryColumns = new String[] {
    CommonColumns._ID,
    SyncColumns.GUID,
    SyncColumns.DATE_CREATED,
    SyncColumns.DATE_MODIFIED,
    SyncColumns.IS_DELETED,
    History.TITLE,
    History.URL,
    History.DATE_LAST_VISITED,
    History.VISITS
  };

  public static final String[] BookmarkColumns = new String[] {
    CommonColumns._ID,
    SyncColumns.GUID,
    SyncColumns.DATE_CREATED,
    SyncColumns.DATE_MODIFIED,
    SyncColumns.IS_DELETED,
    Bookmarks.TITLE,
    Bookmarks.URL,
    Bookmarks.TYPE,
    Bookmarks.PARENT,
    Bookmarks.POSITION,
    Bookmarks.TAGS,
    Bookmarks.DESCRIPTION,
    Bookmarks.KEYWORD
  };

  public static final String[] FormHistoryColumns = new String[] {
    FormHistory.ID,
    FormHistory.GUID,
    FormHistory.FIELD_NAME,
    FormHistory.VALUE,
    FormHistory.TIMES_USED,
    FormHistory.FIRST_USED,
    FormHistory.LAST_USED
  };

  public static final String[] DeletedColumns = new String[] {
    BrowserContract.DeletedColumns.ID,
    BrowserContract.DeletedColumns.GUID,
    BrowserContract.DeletedColumns.TIME_DELETED
  };

  // Mapping from Sync types to Fennec types.
  public static final String[] BOOKMARK_TYPE_CODE_TO_STRING = {
    // Observe omissions: "microsummary", "item".
    "folder", "bookmark", "separator", "livemark", "query"
  };
  private static final int MAX_BOOKMARK_TYPE_CODE = BOOKMARK_TYPE_CODE_TO_STRING.length - 1;
  public static final Map<String, Integer> BOOKMARK_TYPE_STRING_TO_CODE;
  static {
    HashMap<String, Integer> t = new HashMap<String, Integer>();
    t.put("folder",    Bookmarks.TYPE_FOLDER);
    t.put("bookmark",  Bookmarks.TYPE_BOOKMARK);
    t.put("separator", Bookmarks.TYPE_SEPARATOR);
    t.put("livemark",  Bookmarks.TYPE_LIVEMARK);
    t.put("query",     Bookmarks.TYPE_QUERY);
    BOOKMARK_TYPE_STRING_TO_CODE = Collections.unmodifiableMap(t);
  }

  /**
   * Convert a database bookmark type code into the Sync string equivalent.
   *
   * @param code one of the <code>Bookmarks.TYPE_*</code> enumerations.
   * @return the string equivalent, or null if not found.
   */
  public static String typeStringForCode(int code) {
    if (0 <= code && code <= MAX_BOOKMARK_TYPE_CODE) {
      return BOOKMARK_TYPE_CODE_TO_STRING[code];
    }
    return null;
  }

  /**
   * Convert a Sync type string into a Fennec type code.
   *
   * @param type a type string, such as "livemark".
   * @return the type code, or -1 if not found.
   */
  public static int typeCodeForString(String type) {
    Integer found = BOOKMARK_TYPE_STRING_TO_CODE.get(type);
    if (found == null) {
      return -1;
    }
    return found.intValue();
  }

  public static boolean isSupportedType(String type) {
    return BOOKMARK_TYPE_STRING_TO_CODE.containsKey(type);
  }
}
