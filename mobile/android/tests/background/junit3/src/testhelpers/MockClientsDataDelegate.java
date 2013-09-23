/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.testhelpers;

import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.delegates.ClientsDataDelegate;

public class MockClientsDataDelegate implements ClientsDataDelegate {
  private String accountGUID;
  private String clientName;
  private int clientsCount;

  @Override
  public synchronized String getAccountGUID() {
    if (accountGUID == null) {
      accountGUID = Utils.generateGuid();
    }
    return accountGUID;
  }

  @Override
  public synchronized String getClientName() {
    if (clientName == null) {
      clientName = "Default Name";
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
  public boolean isLocalGUID(String guid) {
    return getAccountGUID().equals(guid);
  }
}