/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.testhelpers;

import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.repositories.domain.Record;

import java.util.Random;

public class MockRecord extends Record {
  private final int payloadByteCount;
  public MockRecord(String guid, String collection, long lastModified, boolean deleted) {
    super(guid, collection, lastModified, deleted);
    // Payload used to be "foo", so let's not stray too far.
    // Perhaps some tests "depend" on that payload size.
    payloadByteCount = 3;
  }

  public MockRecord(String guid, String collection, long lastModified, boolean deleted, int payloadByteCount) {
    super(guid, collection, lastModified, deleted);
    this.payloadByteCount = payloadByteCount;
  }

  @Override
  protected void populatePayload(ExtendedJSONObject payload) {
  }

  @Override
  protected void initFromPayload(ExtendedJSONObject payload) {
  }

  @Override
  public Record copyWithIDs(String guid, long androidID) {
    MockRecord r = new MockRecord(guid, this.collection, this.lastModified, this.deleted);
    r.androidID = androidID;
    return r;
  }

  @Override
  public String toJSONString() {
    // Build up a randomish payload string based on the length we were asked for.
    final Random random = new Random();
    final char[] payloadChars = new char[payloadByteCount];
    for (int i = 0; i < payloadByteCount; i++) {
      payloadChars[i] = (char) (random.nextInt(26) + 'a');
    }
    final String payloadString = new String(payloadChars);
    return "{\"id\":\"" + guid + "\", \"payload\": \"" + payloadString+ "\"}";
  }
}