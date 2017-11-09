/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.testhelpers;

import org.mozilla.gecko.sync.CommandProcessor.Command;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.android.ClientsDatabaseAccessor;
import org.mozilla.gecko.sync.repositories.domain.ClientRecord;

import java.util.Collection;
import java.util.List;
import java.util.Map;

public class MockClientsDatabaseAccessor extends ClientsDatabaseAccessor {
  public boolean storedRecord = false;
  public boolean dbWiped = false;
  public boolean clientsTableWiped = false;
  public boolean closed = false;
  public boolean storedArrayList = false;
  public boolean storedCommand;

  @Override
  public void store(ClientRecord record) {
    storedRecord = true;
  }

  @Override
  public void store(Collection<ClientRecord> records) {
    storedArrayList = false;
  }

  @Override
  public void store(String accountGUID, Command command) throws NullCursorException {
    storedCommand = true;
  }

  @Override
  public ClientRecord fetchClient(String profileID) throws NullCursorException {
    return null;
  }

  @Override
  public Map<String, ClientRecord> fetchAllClients() throws NullCursorException {
    return null;
  }

  @Override
  public List<Command> fetchCommandsForClient(String accountGUID) throws NullCursorException {
    return null;
  }

  @Override
  public int clientsCount() {
    return 0;
  }

  @Override
  public void wipeDB() {
    dbWiped = true;
  }

  @Override
  public void wipeClientsTable() {
    clientsTableWiped = true;
  }

  @Override
  public void close() {
    closed = true;
  }

  public void resetVars() {
    storedRecord = dbWiped = clientsTableWiped = closed = storedArrayList = false;
  }
}