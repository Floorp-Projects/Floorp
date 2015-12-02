/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import android.content.SyncResult;

/**
 * There was a problem with the Sync account's credentials: bad username,
 * missing password, malformed sync key, etc.
 */
public abstract class CredentialException extends SyncException {
  private static final long serialVersionUID = 833010553314100538L;

  public CredentialException() {
    super();
  }

  public CredentialException(final Throwable e) {
    super(e);
  }

  @Override
  public void updateStats(GlobalSession globalSession, SyncResult syncResult) {
    syncResult.stats.numAuthExceptions += 1;
  }

  /**
   * No credentials at all.
   */
  public static class MissingAllCredentialsException extends CredentialException {
    private static final long serialVersionUID = 3763937096217604611L;

    public MissingAllCredentialsException() {
      super();
    }

    public MissingAllCredentialsException(final Throwable e) {
      super(e);
    }
  }

  /**
   * Some credential is missing.
   */
  public static class MissingCredentialException extends CredentialException {
    private static final long serialVersionUID = -7543031216547596248L;

    public final String missingCredential;

    public MissingCredentialException(final String missingCredential) {
      this.missingCredential = missingCredential;
    }
  }
}
