/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

public class MetaGlobalException extends SyncException {
  private static final long serialVersionUID = -6182315615113508925L;

  public static class MetaGlobalMalformedSyncIDException extends MetaGlobalException {
    private static final long serialVersionUID = 1L;
  }

  public static class MetaGlobalMalformedVersionException extends MetaGlobalException {
    private static final long serialVersionUID = 1L;
  }

  public static class MetaGlobalOutdatedVersionException extends MetaGlobalException {
    private static final long serialVersionUID = 1L;
  }

  public static class MetaGlobalStaleClientVersionException extends MetaGlobalException {
    private static final long serialVersionUID = 1L;
    public final int serverVersion;
    public MetaGlobalStaleClientVersionException(final int version) {
      this.serverVersion = version;
    }
  }

  public static class MetaGlobalStaleClientSyncIDException extends MetaGlobalException {
    private static final long serialVersionUID = 1L;
    public final String serverSyncID;
    public MetaGlobalStaleClientSyncIDException(final String syncID) {
      this.serverSyncID = syncID;
    }
  }
}
