/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.Assert;
import org.mozilla.gecko.Driver;
import org.mozilla.gecko.tests.components.BaseComponent;

import com.jayway.android.robotium.solo.Solo;

import android.app.Activity;
import android.app.Instrumentation;

/**
 * Interface to the global information about a UITest environment.
 */
public interface UITestContext {

    public static enum ComponentType {
        ABOUTHOME,
        APPMENU,
        GECKOVIEW,
        TOOLBAR
    }

    public Activity getActivity();
    public Solo getSolo();
    public Assert getAsserter();
    public Driver getDriver();
    public Actions getActions();
    public Instrumentation getInstrumentation();

    public void dumpLog(final String logtag, final String message);
    public void dumpLog(final String logtag, final String message, final Throwable t);

    /**
     * Returns the absolute version of the given URL using the host's hostname.
     */
    public String getAbsoluteHostnameUrl(final String url);

    /**
     * Returns the absolute version of the given URL using the host's IP address.
     */
    public String getAbsoluteIpUrl(final String url);

    public BaseComponent getComponent(final ComponentType type);
}
