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

    private Delegate mDelegate;
    private final boolean mAlwaysListen;
    private final String mModuleName;
    private final String[] mEvents;


    /* package */ GeckoSessionHandler(final String module,
                                      final GeckoSession session,
                                      final String[] events) {
        this(module, session, events, /* alwaysListen */ false);
    }

    /* package */ GeckoSessionHandler(final String module,
                                      final GeckoSession session,
                                      final String[] events,
                                      final boolean alwaysListen) {
        session.handlersCount++;

        mAlwaysListen = alwaysListen;
        mModuleName = module;
        mEvents = events;

        if (alwaysListen) {
            register(session);
        }
    }

    public Delegate getDelegate() {
        return mDelegate;
    }

    public void setDelegate(final Delegate delegate, final GeckoSession session) {
        if (mDelegate == delegate) {
            return;
        }

        final boolean unsettingOldDelegate = mDelegate != null &&
                                             delegate == null;
        final boolean settingNewDelegate = mDelegate == null &&
                                           delegate != null;

        if (!mAlwaysListen && unsettingOldDelegate) {
            unregister(session);
        }

        mDelegate = delegate;

        if (!mAlwaysListen && settingNewDelegate) {
            register(session);
        }
    }

    private void unregister(final GeckoSession session) {
        setSessionIsReady(session, /* ready */ false);
        session.getEventDispatcher().unregisterUiThreadListener(this, mEvents);
    }

    private void register(final GeckoSession session) {
        session.getEventDispatcher().registerUiThreadListener(this, mEvents);
        setSessionIsReady(session, /* ready */ true);
    }

    public void setSessionIsReady(final GeckoSession session, final boolean ready) {
        // If not enabled, we don't need Gecko to register/unregister.
        if (!mAlwaysListen && mDelegate == null) {
            return;
        }

        final GeckoBundle msg = new GeckoBundle(2);
        msg.putString("module", mModuleName);
        msg.putBoolean("enabled", ready);
        session.getEventDispatcher().dispatch("GeckoView:UpdateModuleState", msg);
    }

    @Override
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        if (DEBUG) {
            Log.d(LOGTAG, mModuleName + " handleMessage: event = " + event);
        }

        if (mDelegate != null) {
            handleMessage(mDelegate, event, message, callback);
        } else {
            callback.sendError("No delegate registered");
        }
    }

    protected abstract void handleMessage(final Delegate delegate,
                                          final String event,
                                          final GeckoBundle message,
                                          final EventCallback callback);
}
