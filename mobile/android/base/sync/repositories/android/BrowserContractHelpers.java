/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import android.net.Uri;

// This will exist implicitly when we're merged into Fennec, or explicitly
// due to our own imported copy of Fennec's BrowserContract.java.in.
import org.mozilla.gecko.db.BrowserContract;

public class BrowserContractHelpers extends BrowserContract {
  protected static Uri withSyncAndDeleted(Uri u) {
    return u.buildUpon()
            .appendQueryParameter(PARAM_IS_SYNC, "true")
            .appendQueryParameter(PARAM_SHOW_DELETED, "true")
            .build();
  }

  public static final Uri IMAGES_CONTENT_URI               = withSyncAndDeleted(Images.CONTENT_URI);
  public static final Uri BOOKMARKS_CONTENT_URI            = withSyncAndDeleted(Bookmarks.CONTENT_URI);
  public static final Uri BOOKMARKS_PARENTS_CONTENT_URI    = withSyncAndDeleted(Bookmarks.PARENTS_CONTENT_URI);
  public static final Uri BOOKMARKS_POSITIONS_CONTENT_URI  = withSyncAndDeleted(Bookmarks.POSITIONS_CONTENT_URI);
  public static final Uri HISTORY_CONTENT_URI              = withSyncAndDeleted(History.CONTENT_URI);
  public static final Uri SCHEMA_CONTENT_URI               = withSyncAndDeleted(Schema.CONTENT_URI);

  public static final Uri PASSWORDS_CONTENT_URI            = null;
  /*
  public static final Uri PASSWORDS_CONTENT_URI            = withSyncAndDeleted(Passwords.CONTENT_URI);
  public static final Uri FORM_HISTORY_CONTENT_URI         = withSyncAndDeleted(FormHistory.CONTENT_URI);
  public static final Uri DELETED_FORM_HISTORY_CONTENT_URI = withSyncAndDeleted(DeletedFormHistory.CONTENT_URI);
   */

  public static final String[] PasswordColumns = new String[] {
    CommonColumns._ID,
    SyncColumns.GUID,
    SyncColumns.DATE_CREATED,
    SyncColumns.DATE_MODIFIED,
    SyncColumns.IS_DELETED,
    Passwords.HOSTNAME,
    Passwords.HTTP_REALM,
    Passwords.FORM_SUBMIT_URL,
    Passwords.USERNAME_FIELD,
    Passwords.PASSWORD_FIELD,
    Passwords.ENCRYPTED_USERNAME,
    Passwords.ENCRYPTED_PASSWORD,
    Passwords.ENC_TYPE,
    Passwords.TIME_LAST_USED,
    Passwords.TIMES_USED
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
    Bookmarks.IS_FOLDER,
    Bookmarks.PARENT,
    Bookmarks.POSITION,
    Bookmarks.TAGS,
    Bookmarks.DESCRIPTION,
    Bookmarks.KEYWORD
  };
}
