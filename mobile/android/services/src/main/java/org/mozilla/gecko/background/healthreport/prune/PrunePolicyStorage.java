/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport.prune;

/**
 * Abstracts over the Storage instance behind the PrunePolicy.
 */
public interface PrunePolicyStorage {
  public void pruneEvents(final int count);
  public void pruneEnvironments(final int count);

  public int deleteDataBefore(final long time);

  public void cleanup();

  public int getEventCount();
  public int getEnvironmentCount();

  /**
   * Release the resources owned by this helper. MUST be called before this helper is garbage
   * collected.
   */
  public void close();
}
