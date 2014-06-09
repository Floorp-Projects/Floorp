/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.testhelpers;

import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.delegates.ClientsDataDelegate;

public class MockClientsDataDelegate implements ClientsDataDelegate {
  private String accountGUID;
  private String clientName;
  private int clientsCount;
  private long clientDataTimestamp = 0;

  @Override
  public synchronized String getAccountGUID() {
    if (accountGUID == null) {
      accountGUID = Utils.generateGuid();
    }
    return accountGUID;
  }

  @Override
  public synchronized String getDefaultClientName() {
    return "Default client";
  }

  @Override
  public synchronized void setClientName(String clientName, long now) {
    this.clientName = clientName;
    this.clientDataTimestamp = now;
  }

  @Override
  public synchronized String getClientName() {
    if (clientName == null) {
      setClientName(getDefaultClientName(), System.currentTimeMillis());
    }
    return clientName;
  }

  @Override
  public synchronized void setClientsCount(int clientsCount) {
    this.clientsCount = clientsCount;
  }

  @Override
  public synchronized int getClientsCount() {
    return clientsCount;
  }

  @Override
  public synchronized boolean isLocalGUID(String guid) {
    return getAccountGUID().equals(guid);
  }

  @Override
  public synchronized long getLastModifiedTimestamp() {
    return clientDataTimestamp;
  }
}
