/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reading;

/**
 * Grab one of these, then you can add records to it by parsing
 * server responses. Finishing it will flush those changes (e.g.,
 * via UPDATE) to the DB.
 */
public interface ReadingListChangeAccumulator {
  void addDeletion(String guid);
  void addDeletion(ClientReadingListRecord record);

  // addChangedRecord is also used to apply the server's reconciliation results after upload.
  void addChangedRecord(ClientReadingListRecord record);
  void addDownloadedRecord(ServerReadingListRecord down);
  void finish() throws Exception;
}
