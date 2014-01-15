/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.db;

import org.mozilla.gecko.background.sync.helpers.HistoryHelpers;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.android.AndroidBrowserHistoryDataExtender;
import org.mozilla.gecko.sync.repositories.android.ClientsDatabase;
import org.mozilla.gecko.sync.repositories.android.ClientsDatabaseAccessor;
import org.mozilla.gecko.sync.repositories.domain.ClientRecord;
import org.mozilla.gecko.sync.repositories.domain.HistoryRecord;
import org.mozilla.gecko.sync.setup.Constants;

import android.database.Cursor;
import android.test.AndroidTestCase;

public class TestCachedSQLiteOpenHelper extends AndroidTestCase {

  protected ClientsDatabase clientsDB;
  protected AndroidBrowserHistoryDataExtender extender;

  public void setUp() {
    clientsDB = new ClientsDatabase(mContext);
    extender = new AndroidBrowserHistoryDataExtender(mContext);
  }

  public void tearDown() {
    clientsDB.close();
    extender.close();
  }

  public void testUnclosedDatabasesDontInteract() throws NullCursorException {
    // clientsDB gracefully does its thing and closes.
    clientsDB.wipeClientsTable();
    ClientRecord record = new ClientRecord();
    String profileConst = Constants.DEFAULT_PROFILE;
    clientsDB.store(profileConst, record);
    clientsDB.close();

    // extender does its thing but still hasn't closed.
    HistoryRecord h = HistoryHelpers.createHistory1();
    extender.store(h.guid, h.visits);

    // Ensure items in the clientsDB are still accessible nonetheless.
    Cursor cur = null;
    try {
      cur = clientsDB.fetchAllClients();
      assertTrue(cur.moveToFirst());
      assertEquals(1, cur.getCount());
    } finally {
      if (cur != null) {
        cur.close();
      }
    }
  }
}
