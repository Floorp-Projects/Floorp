/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync;

import org.json.simple.JSONArray;
import org.mozilla.gecko.background.helpers.AndroidSyncTestCase;
import org.mozilla.gecko.background.testhelpers.DefaultGlobalSessionCallback;
import org.mozilla.gecko.background.testhelpers.MockClientsDataDelegate;
import org.mozilla.gecko.background.testhelpers.MockSharedPreferences;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.delegates.ClientsDataDelegate;
import org.mozilla.gecko.sync.delegates.GlobalSessionCallback;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.BasicAuthHeaderProvider;
import org.mozilla.gecko.sync.repositories.android.ClientsDatabaseAccessor;
import org.mozilla.gecko.sync.repositories.domain.ClientRecord;
import org.mozilla.gecko.sync.stage.SyncClientsEngineStage;

import android.content.Context;
import android.content.SharedPreferences;

public class TestClientsStage extends AndroidSyncTestCase {
  private static final String TEST_USERNAME    = "johndoe";
  private static final String TEST_PASSWORD    = "password";
  private static final String TEST_SYNC_KEY    = "abcdeabcdeabcdeabcdeabcdea";

  public void setUp() {
    ClientsDatabaseAccessor db = new ClientsDatabaseAccessor(getApplicationContext());
    db.wipeDB();
    db.close();
  }

  public void testWipeClearsClients() throws Exception {

    // Wiping clients is equivalent to a reset and dropping all local stored client records.
    // Resetting is defined as being the same as for other engines -- discard local
    // and remote timestamps, tracked failed records, and tracked records to fetch.

    final Context context = getApplicationContext();
    final ClientsDatabaseAccessor dataAccessor = new ClientsDatabaseAccessor(context);
    final GlobalSessionCallback callback = new DefaultGlobalSessionCallback();
    final ClientsDataDelegate delegate = new MockClientsDataDelegate();

    final KeyBundle keyBundle = new KeyBundle(TEST_USERNAME, TEST_SYNC_KEY);
    final AuthHeaderProvider authHeaderProvider = new BasicAuthHeaderProvider(TEST_USERNAME, TEST_PASSWORD);
    final SharedPreferences prefs = new MockSharedPreferences();
    final SyncConfiguration config = new SyncConfiguration(TEST_USERNAME, authHeaderProvider, prefs);
    config.syncKeyBundle = keyBundle;
    GlobalSession session = new GlobalSession(config, callback, context, null, delegate, callback);

    SyncClientsEngineStage stage = new SyncClientsEngineStage() {

      @Override
      public synchronized ClientsDatabaseAccessor getClientsDatabaseAccessor() {
        if (db == null) {
          db = dataAccessor;
        }
        return db;
      }
    };

    String guid = "clientabcdef";
    long lastModified = System.currentTimeMillis();
    ClientRecord record = new ClientRecord(guid, "clients", lastModified , false);
    record.name = "John's Phone";
    record.type = "mobile";
    record.commands = new JSONArray();

    dataAccessor.store(record);
    assertEquals(1, dataAccessor.clientsCount());

    stage.wipeLocal(session);

    try {
      assertEquals(0, dataAccessor.clientsCount());
      assertEquals(0L, session.config.getPersistedServerClientRecordTimestamp());
      assertEquals(0, session.getClientsDelegate().getClientsCount());
    } finally {
      dataAccessor.close();
    }
  }
}
