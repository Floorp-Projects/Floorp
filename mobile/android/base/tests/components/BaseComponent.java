/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests.components;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.tests.UITestContext;

import com.jayway.android.robotium.solo.Solo;

import android.app.Activity;

/**
 * A base class for constructing components - an abstraction over small bits of Firefox
 * functionality. For example, the Toolbar or the about:home screen could be considered a
 * component. Components should not need to know about each others existences and should be
 * combined via helpers. Helpers can also handle a series of actions taken on one component
 * (e.g. clicking the toolbar, entering a url, and waiting for page load).
 */
public abstract class BaseComponent {
    private final UITestContext mTestContext;
    protected final Activity mActivity;
    protected final Solo mSolo;
    protected final Actions mActions;

    public BaseComponent(final UITestContext testContext) {
        mTestContext = testContext;
        mActivity = mTestContext.getActivity();
        mSolo = mTestContext.getSolo();
        mActions = mTestContext.getActions();
    }

    protected UITestContext getTestContext() {
        return mTestContext;
    }
}
