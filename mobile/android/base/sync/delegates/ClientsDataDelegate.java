/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.delegates;

public interface ClientsDataDelegate {
  public String getAccountGUID();
  public String getDefaultClientName();
  public void setClientName(String clientName, long now);
  public String getClientName();
  public void setClientsCount(int clientsCount);
  public int getClientsCount();
  public boolean isLocalGUID(String guid);

  /**
   * The last time the client's data was modified in a way that should be
   * reflected remotely.
   * <p>
   * Changing the client's name should be reflected remotely, while changing the
   * clients count should not (since that data is only used to inform local
   * policy.)
   *
   * @return timestamp in milliseconds.
   */
  public long getLastModifiedTimestamp();
}
