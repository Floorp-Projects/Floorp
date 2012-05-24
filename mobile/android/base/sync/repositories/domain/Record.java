/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.domain;

import java.io.UnsupportedEncodingException;

import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.ExtendedJSONObject;

/**
 * Record is the abstract base class for all entries that Sync processes:
 * bookmarks, passwords, history, and such.
 *
 * A Record can be initialized from or serialized to a CryptoRecord for
 * submission to an encrypted store.
 *
 * Records should be considered to be conventionally immutable: modifications
 * should be completed before the new record object escapes its constructing
 * scope. Note that this is a critically important part of equality. As Rich
 * Hickey notes:
 *
 *   … the only things you can really compare for equality are immutable things,
 *   because if you compare two things for equality that are mutable, and ever
 *   say true, and they're ever not the same thing, you are wrong. Or you will
 *   become wrong at some point in the future.
 *
 * Records have a layered definition of equality. Two records can be said to be
 * "equal" if:
 *
 * * They have the same GUID and collection. Two crypto/keys records are in some
 *   way "the same".
 *   This is `equalIdentifiers`.
 *
 * * Their most significant fields are the same. That is to say, they share a
 *   GUID, a collection, deletion, and domain-specific fields. Two copies of
 *   crypto/keys, neither deleted, with the same encrypted data but different
 *   modified times and sortIndex are in a stronger way "the same".
 *   This is `equalPayloads`.
 *
 * * Their most significant fields are the same, and their local fields (e.g.,
 *   the androidID to which we have decided that this record maps) are congruent.
 *   A record with the same androidID, or one whose androidID has not been set,
 *   can be considered "the same".
 *   This concept can be extended by Record subclasses. The key point is that
 *   reconciling should be applied to the contents of these records. For example,
 *   two history records with the same URI and GUID, but different visit arrays,
 *   can be said to be congruent.
 *   This is `congruentWith`.
 *
 * * They are strictly identical. Every field that is persisted, including
 *   lastModified and androidID, is equal.
 *   This is `equals`.
 *
 * Different parts of the codebase have use for different layers of this
 * comparison hierarchy. For instance, lastModified times change every time a
 * record is stored; a store followed by a retrieval will return a Record that
 * shares its most significant fields with the input, but has a later
 * lastModified time and might not yet have values set for others. Reconciling
 * will thus ignore the modification time of a record.
 *
 * @author rnewman
 *
 */
public abstract class Record {

  public String guid;
  public String collection;
  public long lastModified;
  public boolean deleted;
  public long androidID;
  /**
   * An integer indicating the relative importance of this item in the collection.
   * <p>
   * Default is 0.
   */
  public long sortIndex;
  /**
   * The number of seconds to keep this record. After that time this item will
   * no longer be returned in response to any request, and it may be pruned from
   * the database.
   * <p>
   * Negative values mean never forget this record.
   * <p>
   * Default is 1 year.
   */
  public long ttl;

  public Record(String guid, String collection, long lastModified, boolean deleted) {
    this.guid         = guid;
    this.collection   = collection;
    this.lastModified = lastModified;
    this.deleted      = deleted;
    this.sortIndex    = 0;
    this.ttl          = 365 * 24 * 60 * 60; // Seconds.
    this.androidID    = -1;
  }

  /**
   * Return true iff the input is a Record and has the same
   * collection and guid as this object.
   */
  public boolean equalIdentifiers(Object o) {
    if (o == null || !(o instanceof Record)) {
      return false;
    }

    Record other = (Record) o;
    if (this.guid == null) {
      if (other.guid != null) {
        return false;
      }
    } else {
      if (!this.guid.equals(other.guid)) {
        return false;
      }
    }
    if (this.collection == null) {
      if (other.collection != null) {
        return false;
      }
    } else {
      if (!this.collection.equals(other.collection)) {
        return false;
      }
    }
    return true;
  }

  /**
   * @param o
   *        The object to which this object should be compared.
   * @return
   *        true iff the input is a Record which is substantially the
   *        same as this object.
   */
  public boolean equalPayloads(Object o) {
    if (!this.equalIdentifiers(o)) {
      return false;
    }
    Record other = (Record) o;
    return this.deleted == other.deleted;
  }

  /**
   *
   *
   * @param o
   *        The object to which this object should be compared.
   * @return
   *        true iff the input is a Record which is substantially the
   *        same as this object, considering the ability and desire to
   *        reconcile the two objects if possible.
   */
  public boolean congruentWith(Object o) {
    if (!this.equalIdentifiers(o)) {
      return false;
    }
    Record other = (Record) o;
    return congruentAndroidIDs(other) &&
           (this.deleted == other.deleted);
  }

  public boolean congruentAndroidIDs(Record other) {
    // We treat -1 as "unset", and treat this as
    // congruent with any other value.
    if (this.androidID  != -1 &&
        other.androidID != -1 &&
        this.androidID  != other.androidID) {
      return false;
    }
    return true;
  }

  /**
   * Return true iff the input is both equal in terms of payload,
   * and also shares transient values such as timestamps.
   */
  @Override
  public boolean equals(Object o) {
    if (o == null || !(o instanceof Record)) {
      return false;
    }

    Record other = (Record) o;
    return equalTimestamps(other) &&
           equalSortIndices(other) &&
           equalAndroidIDs(other) &&
           equalPayloads(o);
  }

  public boolean equalAndroidIDs(Record other) {
    return this.androidID == other.androidID;
  }

  public boolean equalSortIndices(Record other) {
    return this.sortIndex == other.sortIndex;
  }

  public boolean equalTimestamps(Object o) {
    if (o == null || !(o instanceof Record)) {
      return false;
    }
    return ((Record) o).lastModified == this.lastModified;
  }

  protected abstract void populatePayload(ExtendedJSONObject payload);
  protected abstract void initFromPayload(ExtendedJSONObject payload);

  public void initFromEnvelope(CryptoRecord envelope) {
    ExtendedJSONObject p = envelope.payload;
    this.guid = envelope.guid;
    checkGUIDs(p);

    this.collection    = envelope.collection;
    this.lastModified  = envelope.lastModified;

    final Object del = p.get("deleted");
    if (del instanceof Boolean) {
      this.deleted = (Boolean) del;
    } else {
      this.initFromPayload(p);
    }

  }

  public CryptoRecord getEnvelope() {
    CryptoRecord rec = new CryptoRecord(this);
    ExtendedJSONObject payload = new ExtendedJSONObject();
    payload.put("id", this.guid);

    if (this.deleted) {
      payload.put("deleted", true);
    } else {
      populatePayload(payload);
    }
    rec.payload = payload;
    return rec;
  }

  public String toJSONString() {
    throw new RuntimeException("Cannot JSONify non-CryptoRecord Records.");
  }

  public byte[] toJSONBytes() {
    try {
      return this.toJSONString().getBytes("UTF-8");
    } catch (UnsupportedEncodingException e) {
      // Can't happen.
      return null;
    }
  }

  /**
   * Utility for safely populating an output CryptoRecord.
   *
   * @param rec
   * @param key
   * @param value
   */
  protected void putPayload(CryptoRecord rec, String key, String value) {
    if (value == null) {
      return;
    }
    rec.payload.put(key, value);
  }

  protected void putPayload(ExtendedJSONObject payload, String key, String value) {
    if (value == null) {
      return;
    }
    payload.put(key, value);
  }

  protected void checkGUIDs(ExtendedJSONObject payload) {
    String payloadGUID = (String) payload.get("id");
    if (this.guid == null ||
        payloadGUID == null) {
      String detailMessage = "Inconsistency: either envelope or payload GUID missing.";
      throw new IllegalStateException(detailMessage);
    }
    if (!this.guid.equals(payloadGUID)) {
      String detailMessage = "Inconsistency: record has envelope ID " + this.guid + ", payload ID " + payloadGUID;
      throw new IllegalStateException(detailMessage);
    }
  }

  /**
   * Oh for persistent data structures.
   *
   * @param guid
   * @param androidID
   * @return
   *        An identical copy of this record with the provided two values.
   */
  public abstract Record copyWithIDs(String guid, long androidID);
}
