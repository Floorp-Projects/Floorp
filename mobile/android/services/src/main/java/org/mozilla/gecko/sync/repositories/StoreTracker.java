/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories;

import java.util.Iterator;

/**
 * Our hacky version of transactional semantics. The goal is to prevent
 * the following situation:
 *
 * * AAA is not modified locally.
 * * A modified AAA is downloaded during the storing phase. Its local
 *   timestamp is advanced.
 * * The direction of syncing changes, and AAA is now uploaded to the server.
 *
 * The following situation should still be supported:
 *
 * * AAA is not modified locally.
 * * A modified AAA is downloaded and merged with the local AAA.
 * * The merged AAA is uploaded to the server.
 *
 * As should:
 *
 * * AAA is modified locally.
 * * A modified AAA is downloaded, and discarded or merged.
 * * The current version of AAA is uploaded to the server.
 *
 * We achieve this by tracking GUIDs during the storing phase. If we
 * apply a record such that the local copy is substantially the same
 * as the record we just downloaded, we add it to a list of records
 * to avoid uploading. The definition of "substantially the same"
 * depends on the particular repository. The only consideration is "do we
 * want to upload this record in this sync?".
 *
 * Note that items are removed from this list when a fetch that
 * considers them for upload completes successfully. The entire list
 * is discarded when the session is completed.
 *
 * This interface exposes methods to:
 *
 * * During a store, recording that a record has been stored, and should
 *   thus not be returned in subsequent fetches;
 * * During a fetch, checking whether a record should be returned.
 *
 * In the future this might also grow self-persistence.
 *
 * See also RepositorySession.trackRecord.
 *
 * @author rnewman
 *
 */
public interface StoreTracker {

  /**
   * @param guid
   *        The GUID of the item to track.
   * @return
   *        Whether the GUID was a newly tracked value.
   */
  public boolean trackRecordForExclusion(String guid);

  /**
   * @param guid
   *        The GUID of the item to check.
   * @return
   *        true if the item is already tracked.
   */
  public boolean isTrackedForExclusion(String guid);

  /**
  *
  * @param guid
  * @return true if the specified GUID was removed from the tracked set.
  */
  public boolean untrackStoredForExclusion(String guid);

  public RecordFilter getFilter();

  public Iterator<String> recordsTrackedForExclusion();
}
