/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.json.simple.JSONArray;

import org.mozilla.gecko.sync.CommandProcessor.Command;
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

  public void store(String accountGUID, Command command) throws NullCursorException {
    db.store(accountGUID, command.commandType, command.args.toJSONString());
  }

  public ClientRecord fetchClient(String accountGUID) throws NullCursorException {
    final Cursor cur = db.fetchClientsCursor(accountGUID, getProfileId());
    try {
      if (!cur.moveToFirst()) {
        return null;
      }
      return recordFromCursor(cur);
    } finally {
      cur.close();
    }
  }

  public Map<String, ClientRecord> fetchAllClients() throws NullCursorException {
    final HashMap<String, ClientRecord> map = new HashMap<String, ClientRecord>();
    final Cursor cur = db.fetchAllClients();
    try {
      if (!cur.moveToFirst()) {
        return Collections.unmodifiableMap(map);
      }

      while (!cur.isAfterLast()) {
        ClientRecord clientRecord = recordFromCursor(cur);
        map.put(clientRecord.guid, clientRecord);
        cur.moveToNext();
      }
      return Collections.unmodifiableMap(map);
    } finally {
      cur.close();
    }
  }

  public List<Command> fetchAllCommands() throws NullCursorException {
    final List<Command> commands = new ArrayList<Command>();
    final Cursor cur = db.fetchAllCommands();
    try {
      if (!cur.moveToFirst()) {
        return Collections.unmodifiableList(commands);
      }

      while (!cur.isAfterLast()) {
        Command command = commandFromCursor(cur);
        commands.add(command);
        cur.moveToNext();
      }
      return Collections.unmodifiableList(commands);
    } finally {
      cur.close();
    }
  }

  public List<Command> fetchCommandsForClient(String accountGUID) throws NullCursorException {
    final List<Command> commands = new ArrayList<Command>();
    final Cursor cur = db.fetchCommandsForClient(accountGUID);
    try {
      if (!cur.moveToFirst()) {
        return Collections.unmodifiableList(commands);
      }

      while(!cur.isAfterLast()) {
        Command command = commandFromCursor(cur);
        commands.add(command);
        cur.moveToNext();
      }
      return Collections.unmodifiableList(commands);
    } finally {
      cur.close();
    }
  }

  protected static ClientRecord recordFromCursor(Cursor cur) {
    String accountGUID = RepoUtils.getStringFromCursor(cur, ClientsDatabase.COL_ACCOUNT_GUID);
    String clientName = RepoUtils.getStringFromCursor(cur, ClientsDatabase.COL_NAME);
    String clientType = RepoUtils.getStringFromCursor(cur, ClientsDatabase.COL_TYPE);
    ClientRecord record = new ClientRecord(accountGUID);
    record.name = clientName;
    record.type = clientType;
    return record;
  }

  protected static Command commandFromCursor(Cursor cur) {
    String commandType = RepoUtils.getStringFromCursor(cur, ClientsDatabase.COL_COMMAND);
    JSONArray commandArgs = RepoUtils.getJSONArrayFromCursor(cur, ClientsDatabase.COL_ARGS);
    return new Command(commandType, commandArgs);
  }

  public int clientsCount() {
    try {
      final Cursor cur = db.fetchAllClients();
      try {
        return cur.getCount();
      } finally {
        cur.close();
      }
    } catch (NullCursorException e) {
      return 0;
    }

  }

  private String getProfileId() {
    return Constants.DEFAULT_PROFILE;
  }

  public void wipeDB() {
    db.wipeDB();
  }

  public void wipeClientsTable() {
    db.wipeClientsTable();
  }

  public void wipeCommandsTable() {
    db.wipeCommandsTable();
  }

  public void close() {
    db.close();
  }
}
