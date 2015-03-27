/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reading;

import java.util.Collection;

public interface ReadingListSynchronizerDelegate {
  // Called on failure.
  void onUnableToSync(Exception e);

  // These are called sequentially, or not at all
  // if a failure occurs.
  void onDeletionsUploadComplete();
  void onStatusUploadComplete(Collection<String> uploaded, Collection<String> failed);
  void onNewItemUploadComplete(Collection<String> uploaded, Collection<String> failed);
  void onDownloadComplete();
  void onModifiedUploadComplete();

  // If no failure occurred, called at the end.
  void onComplete();
}
