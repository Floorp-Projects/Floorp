/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.db;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Map;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.CommandHelpers;
import org.mozilla.gecko.sync.CommandProcessor.Command;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.android.ClientsDatabaseAccessor;
import org.mozilla.gecko.sync.repositories.domain.ClientRecord;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import android.content.Context;

@RunWith(RobolectricTestRunner.class)
public class TestClientsDatabaseAccessor {

    ClientsDatabaseAccessor db;

    @Before
    public void setUp() {
        final Context context = RuntimeEnvironment.application;
        db = new ClientsDatabaseAccessor(context);
        db.wipeDB();
    }

    @After
    public void tearDown() {
        db.close();
    }

    public void testStoreArrayListAndFetch() throws NullCursorException {
        ArrayList<ClientRecord> list = new ArrayList<>();
        ClientRecord record1 = new ClientRecord(Utils.generateGuid());
        ClientRecord record2 = new ClientRecord(Utils.generateGuid());
        ClientRecord record3 = new ClientRecord(Utils.generateGuid());

        list.add(record1);
        list.add(record2);
        db.store(list);

        ClientRecord r1 = db.fetchClient(record1.guid);
        ClientRecord r2 = db.fetchClient(record2.guid);
        ClientRecord r3 = db.fetchClient(record3.guid);

        Assert.assertNotNull(r1);
        Assert.assertNotNull(r2);
        Assert.assertNull(r3);
        Assert.assertTrue(record1.equals(r1));
        Assert.assertTrue(record2.equals(r2));
    }

    @Test
    public void testStoreAndFetchCommandsForClient() {
        String accountGUID1 = Utils.generateGuid();
        String accountGUID2 = Utils.generateGuid();

        Command command1 = CommandHelpers.getCommand1();
        Command command2 = CommandHelpers.getCommand2();
        Command command3 = CommandHelpers.getCommand3();

        try {
            db.store(accountGUID1, command1);
            db.store(accountGUID1, command2);
            db.store(accountGUID2, command3);

            List<Command> commands = db.fetchCommandsForClient(accountGUID1);
            Assert.assertEquals(2, commands.size());
            Assert.assertEquals(1, commands.get(0).args.size());
            Assert.assertEquals(1, commands.get(1).args.size());
        } catch (NullCursorException e) {
            Assert.fail("Should not have NullCursorException");
        }
    }

    @Test
    public void testNumClients() {
        final int COUNT = 5;
        ArrayList<ClientRecord> list = new ArrayList<>();
        for (int i = 0; i < 5; i++) {
            list.add(new ClientRecord());
        }
        db.store(list);
        Assert.assertEquals(COUNT, db.clientsCount());
    }

    @Test
    public void testFetchAll() throws NullCursorException {
        ArrayList<ClientRecord> list = new ArrayList<>();
        ClientRecord record1 = new ClientRecord(Utils.generateGuid());
        ClientRecord record2 = new ClientRecord(Utils.generateGuid());

        list.add(record1);
        list.add(record2);

        boolean thrown = false;
        try {
            Map<String, ClientRecord> records = db.fetchAllClients();

            Assert.assertNotNull(records);
            Assert.assertEquals(0, records.size());

            db.store(list);
            records = db.fetchAllClients();
            Assert.assertNotNull(records);
            Assert.assertEquals(2, records.size());
            Assert.assertTrue(record1.equals(records.get(record1.guid)));
            Assert.assertTrue(record2.equals(records.get(record2.guid)));

            // put() should throw an exception since records is immutable.
            records.put(null, null);
        } catch (UnsupportedOperationException e) {
            thrown = true;
        }
        Assert.assertTrue(thrown);
    }

    @Test
    public void testFetchNonStaleClients() throws NullCursorException {
        String goodRecord1 = Utils.generateGuid();
        ClientRecord record1 = new ClientRecord(goodRecord1);
        record1.fxaDeviceId = "fxa1";
        ClientRecord record2 = new ClientRecord(Utils.generateGuid());
        record2.fxaDeviceId = "fxa2";
        String goodRecord2 = Utils.generateGuid();
        ClientRecord record3 = new ClientRecord(goodRecord2);
        record3.fxaDeviceId = "fxa4";

        ArrayList<ClientRecord> list = new ArrayList<>();
        list.add(record1);
        list.add(record2);
        list.add(record3);
        db.store(list);

        Assert.assertTrue(db.hasNonStaleClients(new String[]{"fxa1", "fxa-unknown"}));
        Assert.assertFalse(db.hasNonStaleClients(new String[]{}));

        String noFxADeviceId = Utils.generateGuid();
        ClientRecord record4 = new ClientRecord(noFxADeviceId);
        record4.fxaDeviceId = null;
        list.clear();
        list.add(record4);
        db.store(list);

        Assert.assertTrue(db.hasNonStaleClients(new String[]{}));

        Collection<ClientRecord> filtered = db.fetchNonStaleClients(new String[]{"fxa1", "fxa4", "fxa-unknown"});
        ClientRecord[] filteredArr = filtered.toArray(new ClientRecord[0]);
        Assert.assertEquals(3, filteredArr.length);
        Assert.assertEquals(filteredArr[0].guid, goodRecord1);
        Assert.assertEquals(filteredArr[1].guid, goodRecord2);
        Assert.assertEquals(filteredArr[2].guid, noFxADeviceId);
    }
}
