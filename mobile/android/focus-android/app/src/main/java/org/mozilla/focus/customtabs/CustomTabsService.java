/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.customtabs;

import android.app.Service;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.support.customtabs.ICustomTabsCallback;
import android.support.customtabs.ICustomTabsService;

import java.util.List;

public class CustomTabsService extends Service {
    @Override
    public IBinder onBind(Intent intent) {
        return new ICustomTabsService.Stub() {
            @Override
            public boolean warmup(long flags) throws RemoteException {
                return true;
            }

            @Override
            public boolean newSession(ICustomTabsCallback callback) throws RemoteException {
                return true;
            }

            @Override
            public boolean mayLaunchUrl(ICustomTabsCallback callback, Uri url, Bundle extras, List<Bundle> otherLikelyBundles) throws RemoteException {
                return true;
            }

            @Override
            public Bundle extraCommand(String s, Bundle bundle) throws RemoteException {
                return null;
            }

            @Override
            public boolean updateVisuals(ICustomTabsCallback callback, Bundle bundle) throws RemoteException {
                return false;
            }

            @Override
            public boolean requestPostMessageChannel(ICustomTabsCallback iCustomTabsCallback,
                                                     Uri uri) throws RemoteException {
                return false;
            }

            @Override
            public int postMessage(ICustomTabsCallback iCustomTabsCallback, String s,
                                   Bundle bundle) throws RemoteException {
                return 0;
            }

            @Override
            public boolean validateRelationship(ICustomTabsCallback iCustomTabsCallback, int i, Uri uri, Bundle bundle) throws RemoteException {
                return false;
            }
        };
    }
}
