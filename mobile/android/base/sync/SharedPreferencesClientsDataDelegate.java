/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.sync.delegates.ClientsDataDelegate;
import org.mozilla.gecko.R;

import android.content.Context;
import android.content.SharedPreferences;

/**
 * A <code>ClientsDataDelegate</code> implementation that persists to a
 * <code>SharedPreferences</code> instance.
 */
public class SharedPreferencesClientsDataDelegate implements ClientsDataDelegate {
  protected final SharedPreferences sharedPreferences;
  protected final Context context;

  public SharedPreferencesClientsDataDelegate(SharedPreferences sharedPreferences, Context context) {
    this.sharedPreferences = sharedPreferences;
    this.context = context;
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

  /**
   * Set client name.
   *
   * @param clientName to change to.
   */
  @Override
  public synchronized void setClientName(String clientName, long now) {
    sharedPreferences
      .edit()
      .putString(SyncConfiguration.PREF_CLIENT_NAME, clientName)
      .putLong(SyncConfiguration.PREF_CLIENT_DATA_TIMESTAMP, now)
      .commit();
  }

  @Override
  public String getDefaultClientName() {
    String name = GlobalConstants.MOZ_APP_DISPLAYNAME;
    if (name.contains("Aurora")) {
        name = "Aurora";
    } else if (name.contains("Beta")) {
        name = "Beta";
    } else if (name.contains("Nightly")) {
        name = "Nightly";
    }
    return context.getResources().getString(R.string.sync_default_client_name, name, android.os.Build.MODEL);
  }

  @Override
  public synchronized String getClientName() {
    String clientName = sharedPreferences.getString(SyncConfiguration.PREF_CLIENT_NAME, null);
    if (clientName == null) {
      clientName = getDefaultClientName();
      long now = System.currentTimeMillis();
      setClientName(clientName, now);
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

  @Override
  public long getLastModifiedTimestamp() {
    return sharedPreferences.getLong(SyncConfiguration.PREF_CLIENT_DATA_TIMESTAMP, 0);
  }
}
