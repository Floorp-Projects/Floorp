/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream;

import android.content.Context;
import android.preference.SwitchPreference;
import android.util.AttributeSet;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.util.ThreadUtils;

/**
 * A custom switch preference that is used while we allow users to opt-out from using Activity Stream.
 */
public class ActivityStreamPreference extends SwitchPreference {
    @SuppressWarnings("unused") // Used from XML
    public ActivityStreamPreference(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
        init(context);
    }

    @SuppressWarnings("unused") // Used from XML
    public ActivityStreamPreference(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init(context);
    }

    @SuppressWarnings("unused") // Used from XML
    public ActivityStreamPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(context);
    }

    @SuppressWarnings("unused") // Used from XML
    public ActivityStreamPreference(Context context) {
        super(context);
        init(context);
    }

    private void init(Context context) {
        // The SwitchPreference shouldn't do any persistence itself. We want to avoid that a value
        // is written that is not set by the user but set from an experiment.
        setPersistent(false);

        setChecked(ActivityStream.isEnabled(context));
    }

    @Override
    public boolean isPersistent() {
        // Just be absolutely sure that no one re-sets this value since calling init().
        return false;
    }

    @Override
    protected void onClick() {
        super.onClick();

        ActivityStream.setUserEnabled(getContext(), isChecked());

        // We require a restart for this change to take effect. This is not nice, but this setting
        // is not something we want to ship outside of Nightly anyways.
        ThreadUtils.postDelayedToUiThread(new Runnable() {
            @Override
            public void run() {
                GeckoAppShell.scheduleRestart();
            }
        }, 1000);
    }
}
