/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

import android.util.Log;

/* package */ abstract class GeckoSessionHandler<Delegate>
    implements BundleEventListener {

    private static final String LOGTAG = "GeckoSessionHandler";
    private static final boolean DEBUG = false;

    private final String mModuleName;
    private final String[] mEvents;
    private Delegate mDelegate;
    private boolean mRegisteredListeners;

    /* package */ GeckoSessionHandler(final String module,
                                      final GeckoSession session,
                                      final String[] events) {
        session.handlersCount++;

        mModuleName = module;
        mEvents = events;
    }

    public Delegate getDelegate() {
        return mDelegate;
    }

    public void setDelegate(final Delegate delegate, final GeckoSession session) {
        if (mDelegate == delegate) {
            return;
        }

        mDelegate = delegate;

        if (!mRegisteredListeners && delegate != null) {
            session.getEventDispatcher().registerUiThreadListener(this, mEvents);
            mRegisteredListeners = true;
        }

        // If session is not open, we will update module state during session opening.
        if (!session.isOpen()) {
            return;
        }

        final GeckoBundle msg = new GeckoBundle(2);
        msg.putString("module", mModuleName);
        msg.putBoolean("enabled", isEnabled());
        session.getEventDispatcher().dispatch("GeckoView:UpdateModuleState", msg);
    }

    public String getName() {
        return mModuleName;
    }

    public boolean isEnabled() {
        return mDelegate != null;
    }

    @Override
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        if (DEBUG) {
            Log.d(LOGTAG, mModuleName + " handleMessage: event = " + event);
        }

        if (mDelegate != null) {
            handleMessage(mDelegate, event, message, callback);
        } else if (callback != null) {
            callback.sendError("No delegate registered");
        }
    }

    protected abstract void handleMessage(final Delegate delegate,
                                          final String event,
                                          final GeckoBundle message,
                                          final EventCallback callback);
}
