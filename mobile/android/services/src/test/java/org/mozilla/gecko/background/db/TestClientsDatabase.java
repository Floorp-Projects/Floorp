/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.db;

import android.content.Context;
import android.database.Cursor;

import org.json.simple.JSONArray;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.android.ClientsDatabase;
import org.mozilla.gecko.sync.repositories.android.RepoUtils;
import org.mozilla.gecko.sync.repositories.domain.ClientRecord;
import org.mozilla.gecko.sync.setup.Constants;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import java.util.ArrayList;

@RunWith(RobolectricTestRunner.class)
public class TestClientsDatabase {

    protected ClientsDatabase db;

    @Before
    public void setUp() {
        final Context context = RuntimeEnvironment.application;
        db = new ClientsDatabase(context);
        db.wipeDB();
    }

    @After
    public void tearDown() {
        db.close();
    }

    @Test
    public void testStoreAndFetch() {
        ClientRecord record = new ClientRecord();
        String profileConst = Constants.DEFAULT_PROFILE;
        db.store(profileConst, record);

        Cursor cur = null;
        try {
            // Test stored item gets fetched correctly.
            cur = db.fetchClientsCursor(record.guid, profileConst);
            Assert.assertTrue(cur.moveToFirst());
            Assert.assertEquals(1, cur.getCount());

            String guid = RepoUtils.getStringFromCursor(cur, ClientsDatabase.COL_ACCOUNT_GUID);
            String profileId = RepoUtils.getStringFromCursor(cur, ClientsDatabase.COL_PROFILE);
            String clientName = RepoUtils.getStringFromCursor(cur, ClientsDatabase.COL_NAME);
            String clientType = RepoUtils.getStringFromCursor(cur, ClientsDatabase.COL_TYPE);

            Assert.assertEquals(record.guid, guid);
            Assert.assertEquals(profileConst, profileId);
            Assert.assertEquals(record.name, clientName);
            Assert.assertEquals(record.type, clientType);
        } catch (NullCursorException e) {
            Assert.fail("Should not have NullCursorException");
        } finally {
            if (cur != null) {
                cur.close();
            }
        }
    }

    @Test
    public void testStoreAndFetchSpecificCommands() {
        String accountGUID = Utils.generateGuid();
        ArrayList<String> args = new ArrayList<>();
        args.add("URI of Page");
        args.add("Sender GUID");
        args.add("Title of Page");
        String jsonArgs = JSONArray.toJSONString(args);

        Cursor cur = null;
        try {
            db.store(accountGUID, "displayURI", jsonArgs, "flowID");

            // This row should not show up in the fetch.
            args.add("Another arg.");
            db.store(accountGUID, "displayURI", JSONArray.toJSONString(args), "flowID");

            // Test stored item gets fetched correctly.
            cur = db.fetchSpecificCommand(accountGUID, "displayURI", jsonArgs);
            Assert.assertTrue(cur.moveToFirst());
            Assert.assertEquals(1, cur.getCount());

            String guid = RepoUtils.getStringFromCursor(cur, ClientsDatabase.COL_ACCOUNT_GUID);
            String commandType = RepoUtils.getStringFromCursor(cur, ClientsDatabase.COL_COMMAND);
            String fetchedArgs = RepoUtils.getStringFromCursor(cur, ClientsDatabase.COL_ARGS);

            Assert.assertEquals(accountGUID, guid);
            Assert.assertEquals("displayURI", commandType);
            Assert.assertEquals(jsonArgs, fetchedArgs);
        } catch (NullCursorException e) {
            Assert.fail("Should not have NullCursorException");
        } finally {
            if (cur != null) {
                cur.close();
            }
        }
    }

    @Test
    public void testFetchCommandsForClient() {
        String accountGUID = Utils.generateGuid();
        ArrayList<String> args = new ArrayList<>();
        args.add("URI of Page");
        args.add("Sender GUID");
        args.add("Title of Page");
        String jsonArgs = JSONArray.toJSONString(args);

        Cursor cur = null;
        try {
            db.store(accountGUID, "displayURI", jsonArgs, "flowID");

            // This row should ALSO show up in the fetch.
            args.add("Another arg.");
            db.store(accountGUID, "displayURI", JSONArray.toJSONString(args), "flowID");

            // Test both stored items with the same GUID but different command are fetched.
            cur = db.fetchCommandsForClient(accountGUID);
            Assert.assertTrue(cur.moveToFirst());
            Assert.assertEquals(2, cur.getCount());
        } catch (NullCursorException e) {
            Assert.fail("Should not have NullCursorException");
        } finally {
            if (cur != null) {
                cur.close();
            }
        }
    }

    @Test
    @SuppressWarnings("resource")
    public void testDelete() {
        ClientRecord record1 = new ClientRecord();
        ClientRecord record2 = new ClientRecord();
        String profileConst = Constants.DEFAULT_PROFILE;

        db.store(profileConst, record1);
        db.store(profileConst, record2);

        Cursor cur = null;
        try {
            // Test record doesn't exist after delete.
            db.deleteClient(record1.guid, profileConst);
            cur = db.fetchClientsCursor(record1.guid, profileConst);
            Assert.assertFalse(cur.moveToFirst());
            Assert.assertEquals(0, cur.getCount());

            // Test record2 still there after deleting record1.
            cur = db.fetchClientsCursor(record2.guid, profileConst);
            Assert.assertTrue(cur.moveToFirst());
            Assert.assertEquals(1, cur.getCount());

            String guid = RepoUtils.getStringFromCursor(cur, ClientsDatabase.COL_ACCOUNT_GUID);
            String profileId = RepoUtils.getStringFromCursor(cur, ClientsDatabase.COL_PROFILE);
            String clientName = RepoUtils.getStringFromCursor(cur, ClientsDatabase.COL_NAME);
            String clientType = RepoUtils.getStringFromCursor(cur, ClientsDatabase.COL_TYPE);

            Assert.assertEquals(record2.guid, guid);
            Assert.assertEquals(profileConst, profileId);
            Assert.assertEquals(record2.name, clientName);
            Assert.assertEquals(record2.type, clientType);
        } catch (NullCursorException e) {
            Assert.fail("Should not have NullCursorException");
        } finally {
            if (cur != null) {
                cur.close();
            }
        }
    }

    @Test
    @SuppressWarnings("resource")
    public void testWipe() {
        ClientRecord record1 = new ClientRecord();
        ClientRecord record2 = new ClientRecord();
        String profileConst = Constants.DEFAULT_PROFILE;

        db.store(profileConst, record1);
        db.store(profileConst, record2);


        Cursor cur = null;
        try {
            // Test before wipe the records are there.
            cur = db.fetchClientsCursor(record2.guid, profileConst);
            Assert.assertTrue(cur.moveToFirst());
            Assert.assertEquals(1, cur.getCount());
            cur = db.fetchClientsCursor(record2.guid, profileConst);
            Assert.assertTrue(cur.moveToFirst());
            Assert.assertEquals(1, cur.getCount());

            // Test after wipe neither record exists.
            db.wipeClientsTable();
            cur = db.fetchClientsCursor(record2.guid, profileConst);
            Assert.assertFalse(cur.moveToFirst());
            Assert.assertEquals(0, cur.getCount());
            cur = db.fetchClientsCursor(record1.guid, profileConst);
            Assert.assertFalse(cur.moveToFirst());
            Assert.assertEquals(0, cur.getCount());
        } catch (NullCursorException e) {
            Assert.fail("Should not have NullCursorException");
        } finally {
            if (cur != null) {
                cur.close();
            }
        }
    }
}
