/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.domain.PasswordRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.content.Context;
import android.database.Cursor;

public class AndroidBrowserPasswordsRepositorySession extends
    AndroidBrowserRepositorySession {

  public AndroidBrowserPasswordsRepositorySession(Repository repository, Context context) {
    super(repository);
    dbHelper = new AndroidBrowserPasswordsDataAccessor(context);
  }

  @Override
  protected Record recordFromMirrorCursor(Cursor cur) {
    return RepoUtils.passwordFromMirrorCursor(cur);
  }

  @Override
  protected String buildRecordString(Record record) {
    PasswordRecord rec = (PasswordRecord) record;
    return rec.hostname + rec.formSubmitURL + rec.httpRealm + rec.username;
  }

  @Override
  protected Record prepareRecord(Record record) {
    return record;
  }
}
