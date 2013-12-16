/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.sync.delegates.ClientsDataDelegate;

import android.content.SharedPreferences;

/**
 * A <code>ClientsDataDelegate</code> implementation that persists to a
 * <code>SharedPreferences</code> instance.
 */
public class SharedPreferencesClientsDataDelegate implements ClientsDataDelegate {
  protected final SharedPreferences sharedPreferences;

  public SharedPreferencesClientsDataDelegate(SharedPreferences sharedPreferences) {
    this.sharedPreferences = sharedPreferences;
  }

  @Override
  public synchronized String getAccountGUID() {
    String accountGUID = sharedPreferences.getString(SyncConfiguration.PREF_ACCOUNT_GUID, null);
    if (accountGUID == null) {
      accountGUID = Utils.generateGuid();
      sharedPreferences.edit().putString(SyncConfiguration.PREF_ACCOUNT_GUID, accountGUID).commit();
    }
    return accountGUID;
  }

  @Override
  public synchronized String getClientName() {
    String clientName = sharedPreferences.getString(SyncConfiguration.PREF_CLIENT_NAME, null);
    if (clientName == null) {
      clientName = GlobalConstants.MOZ_APP_DISPLAYNAME + " on " + android.os.Build.MODEL;
      sharedPreferences.edit().putString(SyncConfiguration.PREF_CLIENT_NAME, clientName).commit();
    }
    return clientName;
  }

  @Override
  public synchronized void setClientsCount(int clientsCount) {
    sharedPreferences.edit().putLong(SyncConfiguration.PREF_NUM_CLIENTS, (long) clientsCount).commit();
  }

  @Override
  public boolean isLocalGUID(String guid) {
    return getAccountGUID().equals(guid);
  }

  @Override
  public synchronized int getClientsCount() {
    return (int) sharedPreferences.getLong(SyncConfiguration.PREF_NUM_CLIENTS, 0);
  }
}
