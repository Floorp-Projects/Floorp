/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.repositories.domain;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.Utils;
import org.robolectric.RobolectricGradleTestRunner;

@RunWith(RobolectricGradleTestRunner.class)
public class TestClientRecord {

  @Test
  public void testEnsureDefaults() {
    // Ensure defaults.
    ClientRecord record = new ClientRecord();
    assertEquals(ClientRecord.COLLECTION_NAME, record.collection);
    assertEquals(0, record.lastModified);
    assertEquals(false, record.deleted);
    assertEquals("Default Name", record.name);
    assertEquals(ClientRecord.CLIENT_TYPE, record.type);
    assertTrue(null == record.os);
    assertTrue(null == record.device);
    assertTrue(null == record.application);
    assertTrue(null == record.appPackage);
    assertTrue(null == record.formfactor);
  }

  @Test
  public void testGetPayload() {
    // Test ClientRecord.getPayload().
    ClientRecord record = new ClientRecord();
    CryptoRecord cryptoRecord = record.getEnvelope();
    assertEquals(record.guid, cryptoRecord.payload.get("id"));
    assertEquals(null, cryptoRecord.payload.get("collection"));
    assertEquals(null, cryptoRecord.payload.get("lastModified"));
    assertEquals(null, cryptoRecord.payload.get("deleted"));
    assertEquals(null, cryptoRecord.payload.get("version"));
    assertEquals(record.name, cryptoRecord.payload.get("name"));
    assertEquals(record.type, cryptoRecord.payload.get("type"));
  }

  @Test
  public void testInitFromPayload() {
    // Test ClientRecord.initFromPayload() in ClientRecordFactory.
    ClientRecord record1 = new ClientRecord();
    CryptoRecord cryptoRecord = record1.getEnvelope();
    ClientRecordFactory factory = new ClientRecordFactory();
    ClientRecord record2 = (ClientRecord) factory.createRecord(cryptoRecord);
    assertEquals(cryptoRecord.payload.get("id"), record2.guid);
    assertEquals(ClientRecord.COLLECTION_NAME, record2.collection);
    assertEquals(0, record2.lastModified);
    assertEquals(false, record2.deleted);
    assertEquals(cryptoRecord.payload.get("name"), record2.name);
    assertEquals(cryptoRecord.payload.get("type"), record2.type);
  }

  @Test
  public void testCopyWithIDs() {
    // Test ClientRecord.copyWithIDs.
    ClientRecord record1 = new ClientRecord();
    record1.version = "20";
    String newGUID = Utils.generateGuid();
    ClientRecord record2 = (ClientRecord) record1.copyWithIDs(newGUID, 0);
    assertEquals(newGUID, record2.guid);
    assertEquals(0, record2.androidID);
    assertEquals(record1.collection, record2.collection);
    assertEquals(record1.lastModified, record2.lastModified);
    assertEquals(record1.deleted, record2.deleted);
    assertEquals(record1.name, record2.name);
    assertEquals(record1.type, record2.type);
    assertEquals(record1.version, record2.version);
  }

  @Test
  public void testEquals() {
    // Test ClientRecord.equals().
    ClientRecord record1 = new ClientRecord();
    ClientRecord record2 = new ClientRecord();
    record2.guid = record1.guid;
    record2.version = "20";
    record1.version = null;

    ClientRecord record3 = new ClientRecord(Utils.generateGuid());
    record3.name = "New Name";

    ClientRecord record4 = new ClientRecord(Utils.generateGuid());
    record4.name = ClientRecord.DEFAULT_CLIENT_NAME;
    record4.type = "desktop";

    assertTrue(record2.equals(record1));
    assertFalse(record3.equals(record1));
    assertFalse(record3.equals(record2));
    assertFalse(record4.equals(record1));
    assertFalse(record4.equals(record2));
  }
}
