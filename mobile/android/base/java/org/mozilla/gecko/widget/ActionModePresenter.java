/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import android.support.v7.view.ActionMode;

/* Presenters handle the actual showing/hiding of the action mode UI in the app. Its their
 * responsibility to create an action mode, and assign it Callbacks*/

public interface ActionModePresenter {
    /* Called when an action mode should be shown */
    void startActionMode(final ActionMode.Callback callback);

    /* Called when whatever action mode is showing should be hidden */
    void endActionMode();
}
