/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reading;

public class ClientMetadata {
  public final long id;                 // A client numeric ID. We don't always have a GUID.
  public final long lastModified;       // A client timestamp.
  public final boolean isDeleted;
  public final boolean isArchived;

  public ClientMetadata(final long id, final long lastModified, final boolean isDeleted, final boolean isArchived) {
    this.id = id;
    this.lastModified = lastModified;
    this.isDeleted = isDeleted;
    this.isArchived = isArchived;
  }
}
