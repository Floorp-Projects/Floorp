/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories;

import java.util.HashSet;
import java.util.Iterator;

import org.mozilla.gecko.sync.repositories.domain.Record;

public class HashSetStoreTracker implements StoreTracker {

  // Guarded by `this`.
  // Used to store GUIDs that were not locally modified but
  // have been modified by a call to `store`, and thus
  // should not be returned by a subsequent fetch.
  private HashSet<String> guids;

  public HashSetStoreTracker() {
    guids = new HashSet<String>();
  }

  @Override
  public String toString() {
    return "#<Tracker: " + guids.size() + " guids tracked.>";
  }

  @Override
  public synchronized boolean trackRecordForExclusion(String guid) {
    return (guid != null) && guids.add(guid);
  }

  @Override
  public synchronized boolean isTrackedForExclusion(String guid) {
    return (guid != null) && guids.contains(guid);
  }

  @Override
  public synchronized boolean untrackStoredForExclusion(String guid) {
    return (guid != null) && guids.remove(guid);
  }

  @Override
  public RecordFilter getFilter() {
    if (guids.size() == 0) {
      return null;
    }
    return new RecordFilter() {
      @Override
      public boolean excludeRecord(Record r) {
        return isTrackedForExclusion(r.guid);
      }
    };
  }

  @Override
  public Iterator<String> recordsTrackedForExclusion() {
    return this.guids.iterator();
  }
}
