/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.domain.ClientRecord;
import org.mozilla.gecko.sync.setup.Constants;

import android.content.Context;
import android.database.Cursor;

public class ClientsDatabaseAccessor {

  public static final String LOG_TAG = "ClientsDatabaseAccessor";

  private ClientsDatabase db;

  // Need this so we can properly stub out the class for testing.
  public ClientsDatabaseAccessor() {}

  public ClientsDatabaseAccessor(Context context) {
    db = new ClientsDatabase(context);
  }

  public void store(ClientRecord record) {
    db.store(getProfileId(), record);
  }

  public void store(Collection<ClientRecord> records) {
    for (ClientRecord record : records) {
      this.store(record);
    }
  }

  public ClientRecord fetch(String accountGUID) throws NullCursorException {
    Cursor cur = null;
    try {
      cur = db.fetch(accountGUID, getProfileId());

      if (cur == null || !cur.moveToFirst()) {
        return null;
      }
      return recordFromCursor(cur);
    } finally {
      if (cur != null) {
        cur.close();
      }
    }
  }

  public Map<String, ClientRecord> fetchAll() throws NullCursorException {
    HashMap<String, ClientRecord> map = new HashMap<String, ClientRecord>();
    Cursor cur = null;
    try {
      cur = db.fetchAll();
      if (cur == null || !cur.moveToFirst()) {
        return Collections.unmodifiableMap(map);
      }
      while (!cur.isAfterLast()) {
        ClientRecord clientRecord = recordFromCursor(cur);
        map.put(clientRecord.guid, clientRecord);
        cur.moveToNext();
      }

      return Collections.unmodifiableMap(map);
    } finally {
      if (cur != null) {
        cur.close();
      }
    }
  }

  protected ClientRecord recordFromCursor(Cursor cur) {
    String accountGUID = RepoUtils.getStringFromCursor(cur, ClientsDatabase.COL_ACCOUNT_GUID);
    String clientName = RepoUtils.getStringFromCursor(cur, ClientsDatabase.COL_NAME);
    String clientType = RepoUtils.getStringFromCursor(cur, ClientsDatabase.COL_TYPE);
    ClientRecord record = new ClientRecord(accountGUID);
    record.name = clientName;
    record.type = clientType;
    return record;
  }

  public int clientsCount() {
    Cursor cur;
    try {
      cur = db.fetchAll();
    } catch (NullCursorException e) {
      return 0;
    }
    try {
      return cur.getCount();
    } finally {
      cur.close();
    }
  }

  private String getProfileId() {
    return Constants.PROFILE_ID;
  }

  public void wipe() {
    db.wipe();
  }

  public void close() {
    db.close();
  }
}
