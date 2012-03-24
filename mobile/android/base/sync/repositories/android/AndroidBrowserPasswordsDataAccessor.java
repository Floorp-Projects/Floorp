/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.repositories.domain.PasswordRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;

public class AndroidBrowserPasswordsDataAccessor extends AndroidBrowserRepositoryDataAccessor {

  public AndroidBrowserPasswordsDataAccessor(Context context) {
    super(context);
  }

  @Override
  protected ContentValues getContentValues(Record record) {
    PasswordRecord rec = (PasswordRecord) record;

    ContentValues cv = new ContentValues();
    cv.put(BrowserContract.Passwords.GUID,            rec.guid);
    cv.put(BrowserContract.Passwords.HOSTNAME,        rec.hostname);
    cv.put(BrowserContract.Passwords.HTTP_REALM,      rec.httpRealm);
    cv.put(BrowserContract.Passwords.FORM_SUBMIT_URL, rec.formSubmitURL);
    cv.put(BrowserContract.Passwords.USERNAME_FIELD,  rec.usernameField);
    cv.put(BrowserContract.Passwords.PASSWORD_FIELD,  rec.passwordField);
    
    // TODO Do encryption of username/password here. Bug 711636
    cv.put(BrowserContract.Passwords.ENC_TYPE,           rec.encType);
    cv.put(BrowserContract.Passwords.ENCRYPTED_USERNAME, rec.username);
    cv.put(BrowserContract.Passwords.ENCRYPTED_PASSWORD, rec.password);
    
    cv.put(BrowserContract.Passwords.TIMES_USED,     rec.timesUsed);
    cv.put(BrowserContract.Passwords.TIME_LAST_USED, rec.timeLastUsed);
    return cv;
  }

  @Override
  protected Uri getUri() {
    return BrowserContractHelpers.PASSWORDS_CONTENT_URI;
  }

  @Override
  protected String[] getAllColumns() {
    return BrowserContractHelpers.PasswordColumns;
  }
}
