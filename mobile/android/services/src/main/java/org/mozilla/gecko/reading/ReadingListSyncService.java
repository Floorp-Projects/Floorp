/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reading;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

public class ReadingListSyncService extends Service {
  private static final Object syncAdapterLock = new Object();
  private static ReadingListSyncAdapter syncAdapter;

  @Override
  public void onCreate() {
    synchronized (syncAdapterLock) {
      if (syncAdapter == null) {
        syncAdapter = new ReadingListSyncAdapter(getApplicationContext(), true);
      }
    }
  }

  @Override
  public IBinder onBind(Intent intent) {
    return syncAdapter.getSyncAdapterBinder();
  }
}
