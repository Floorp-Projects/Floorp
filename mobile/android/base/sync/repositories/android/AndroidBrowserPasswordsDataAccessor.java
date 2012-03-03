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
 * Jason Voll <jvoll@mozilla.com>
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
    cv.put(PasswordColumns.GUID,            rec.guid);
    cv.put(PasswordColumns.HOSTNAME,        rec.hostname);
    cv.put(PasswordColumns.HTTP_REALM,      rec.httpRealm);
    cv.put(PasswordColumns.FORM_SUBMIT_URL, rec.formSubmitURL);
    cv.put(PasswordColumns.USERNAME_FIELD,  rec.usernameField);
    cv.put(PasswordColumns.PASSWORD_FIELD,  rec.passwordField);
    
    // TODO Do encryption of username/password here. Bug 711636
    cv.put(PasswordColumns.ENC_TYPE,           rec.encType);
    cv.put(PasswordColumns.ENCRYPTED_USERNAME, rec.username);
    cv.put(PasswordColumns.ENCRYPTED_PASSWORD, rec.password);
    
    cv.put(PasswordColumns.TIMES_USED,     rec.timesUsed);
    cv.put(PasswordColumns.TIME_LAST_USED, rec.timeLastUsed);
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
