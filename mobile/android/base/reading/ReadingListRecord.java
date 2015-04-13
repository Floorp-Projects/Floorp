/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reading;

import org.mozilla.gecko.sync.ExtendedJSONObject;

/**
 * This models the wire protocol format, not database contents.
 */
public abstract class ReadingListRecord {
  public static class ServerMetadata {
    public final String guid;             // Null if not yet uploaded successfully.
    public final long lastModified;       // A server timestamp.

    public ServerMetadata(String guid, long lastModified) {
      this.guid = guid;
      this.lastModified = lastModified;
    }

    /**
     * From server record.
     */
    public ServerMetadata(ExtendedJSONObject obj) {
      this(obj.getString("id"), obj.containsKey("last_modified") ? obj.getLong("last_modified") : -1L);
    }
  }

  public final ServerMetadata serverMetadata;

  public String getGUID() {
    if (serverMetadata == null) {
      return null;
    }

    return serverMetadata.guid;
  }

  public long getServerLastModified() {
    if (serverMetadata == null) {
      return -1L;
    }

    return serverMetadata.lastModified;
  }

  protected ReadingListRecord(final ServerMetadata serverMetadata) {
    this.serverMetadata = serverMetadata;
  }

  public abstract String getURL();
  public abstract String getTitle();
  public abstract String getAddedBy();
}
