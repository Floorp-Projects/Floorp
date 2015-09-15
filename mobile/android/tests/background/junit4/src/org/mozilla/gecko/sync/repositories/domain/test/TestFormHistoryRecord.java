/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.repositories.domain.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.domain.FormHistoryRecord;
import org.robolectric.RobolectricGradleTestRunner;

@RunWith(RobolectricGradleTestRunner.class)
public class TestFormHistoryRecord {
  public static FormHistoryRecord withIdFieldNameAndValue(long id, String fieldName, String value) {
    FormHistoryRecord fr = new FormHistoryRecord();
    fr.androidID = id;
    fr.fieldName = fieldName;
    fr.fieldValue = value;

    return fr;
  }

  @Test
  public void testCollection() {
    FormHistoryRecord fr = new FormHistoryRecord();
    assertEquals("forms", fr.collection);
  }

  @Test
  public void testGetPayload() {
    FormHistoryRecord fr = withIdFieldNameAndValue(0, "username", "aUsername");
    CryptoRecord rec = fr.getEnvelope();
    assertEquals("username",  rec.payload.get("name"));
    assertEquals("aUsername", rec.payload.get("value"));
  }

  @Test
  public void testCopyWithIDs() {
    FormHistoryRecord fr = withIdFieldNameAndValue(0, "username", "aUsername");
    String guid = Utils.generateGuid();
    FormHistoryRecord fr2 = (FormHistoryRecord)fr.copyWithIDs(guid, 9999);
    assertEquals(guid, fr2.guid);
    assertEquals(9999, fr2.androidID);
    assertEquals(fr.fieldName, fr2.fieldName);
    assertEquals(fr.fieldValue, fr2.fieldValue);
  }

  @Test
  public void testEquals() {
    FormHistoryRecord fr1a = withIdFieldNameAndValue(0, "username1", "Alice");
    FormHistoryRecord fr1b = withIdFieldNameAndValue(0, "username1", "Bob");
    FormHistoryRecord fr2a = withIdFieldNameAndValue(0, "username2", "Alice");
    FormHistoryRecord fr2b = withIdFieldNameAndValue(0, "username2", "Bob");

    assertFalse(fr1a.equals(fr1b));
    assertFalse(fr1a.equals(fr2a));
    assertFalse(fr1a.equals(fr2b));
    assertFalse(fr1b.equals(fr2a));
    assertFalse(fr1b.equals(fr2b));
    assertFalse(fr2a.equals(fr2b));

    assertFalse(fr1a.equals(withIdFieldNameAndValue(fr1a.androidID, fr1a.fieldName, fr1b.fieldValue)));
    assertFalse(fr1a.equals(fr1a.copyWithIDs(fr2a.guid, 9999)));
    assertTrue(fr1a.equals(fr1a));
  }

  @Test
  public void testEqualsForDeleted() {
    FormHistoryRecord fr1 = withIdFieldNameAndValue(0, "username1", "Alice");
    FormHistoryRecord fr2 = (FormHistoryRecord)fr1.copyWithIDs(fr1.guid, fr1.androidID);
    assertTrue(fr1.equals(fr2));
    fr1.deleted = true;
    assertFalse(fr1.equals(fr2));
    fr2.deleted = true;
    assertTrue(fr1.equals(fr2));
    FormHistoryRecord fr3 = (FormHistoryRecord)fr2.copyWithIDs(Utils.generateGuid(), 9999);
    assertFalse(fr2.equals(fr3));
  }

  @Test
  public void testTTL() {
    FormHistoryRecord fr = withIdFieldNameAndValue(0, "username", "aUsername");
    assertEquals(FormHistoryRecord.FORMS_TTL, fr.ttl);
    CryptoRecord rec = fr.getEnvelope();
    assertEquals(FormHistoryRecord.FORMS_TTL, rec.ttl);
  }
}
