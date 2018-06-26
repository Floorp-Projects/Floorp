/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mdns;

import android.content.Context;

/**
 * This class is the bridge between XPCOM mDNS module and NsdManager.
 *
 * @See nsIDNSServiceDiscovery.idl
 */
public abstract class MulticastDNSManager {
    protected static final boolean DEBUG = false;
    protected static final String LOGTAG = "GeckoMDNSManager";
    private static MulticastDNSManager instance = null;

    public static MulticastDNSManager getInstance(final Context context) {
        if (instance == null) {
            instance = new DummyMulticastDNSManager();
        }
        return instance;
    }

    public abstract void init();
    public abstract void tearDown();
}
