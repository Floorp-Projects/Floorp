/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.firstrun;

import android.content.Context;
import android.content.res.Resources;
import android.support.annotation.NonNull;

import org.mozilla.gecko.R;
import org.mozilla.gecko.util.OnboardingStringUtil;

public class LocalFirstRunPanelProvider implements FirstRunPanelConfigProviderStrategy {
    public PanelConfig getPanelConfig(@NonNull Context context, PanelConfig.TYPE type, final boolean useLocalValues) {
        final Resources resources = context.getResources();
        final OnboardingStringUtil onboardingStrings = OnboardingStringUtil.getInstance(context);

        if (onboardingStrings.areStringsLocalized()) {
            switch (type) {
                case WELCOME:
                    return new PanelConfig(type, useLocalValues, resources.getString(R.string.firstrun_panel_title_welcome),
                            resources.getString(R.string.newfirstrun_urlbar_message),
                            resources.getString(R.string.newfirstrun_urlbar_subtext),
                            R.drawable.firstrun_welcome2);
                case PRIVACY:
                case LAST_PRIVACY:
                    return new PanelConfig(type, useLocalValues, resources.getString(R.string.firstrun_panel_title_privacy),
                            FirstrunPanel.NO_MESSAGE,
                            resources.getString(R.string.newfirstrun_privacy_subtext),
                            R.drawable.firstrun_private2);
                case CUSTOMIZE:
                case LAST_CUSTOMIZE:
                    throw new IllegalArgumentException("Onboarding will not show the addons screen anymore");
                case SYNC:
                    return new PanelConfig(type, useLocalValues, resources.getString(R.string.firstrun_sync_title),
                            FirstrunPanel.NO_MESSAGE,
                            resources.getString(R.string.newfirstrun_sync_subtext),
                            R.drawable.firstrun_sync2);
                default:    // This will also be the case for "WELCOME"
                    return new PanelConfig(type, useLocalValues, resources.getString(R.string.firstrun_panel_title_welcome),
                            resources.getString(R.string.newfirstrun_urlbar_message),
                            resources.getString(R.string.newfirstrun_urlbar_subtext),
                            R.drawable.firstrun_welcome2);
            }

        // Show the previous Onboarding experience. Same old screens, imagery and strings.
        } else {
            switch (type) {
                case WELCOME:
                    return new PanelConfig(type, useLocalValues, resources.getString(R.string.firstrun_panel_title_welcome),
                            resources.getString(R.string.firstrun_urlbar_message),
                            resources.getString(R.string.firstrun_urlbar_subtext),
                            R.drawable.firstrun_welcome);
                case PRIVACY:
                case LAST_PRIVACY:
                    return new PanelConfig(type, useLocalValues, resources.getString(R.string.firstrun_panel_title_privacy),
                            resources.getString(R.string.firstrun_privacy_message),
                            resources.getString(R.string.firstrun_privacy_subtext),
                            R.drawable.firstrun_private);
                case CUSTOMIZE:
                case LAST_CUSTOMIZE:
                    return new PanelConfig(type, useLocalValues, resources.getString(R.string.firstrun_panel_title_customize),
                            resources.getString(R.string.firstrun_customize_message),
                            resources.getString(R.string.firstrun_customize_subtext),
                            R.drawable.firstrun_data);
                case SYNC:
                    return new PanelConfig(type, useLocalValues, resources.getString(R.string.firstrun_sync_title),
                            resources.getString(R.string.firstrun_sync_message),
                            resources.getString(R.string.firstrun_sync_subtext),
                            R.drawable.firstrun_sync);
                default:    // This will also be the case for "WELCOME"
                    return new PanelConfig(type, useLocalValues, resources.getString(R.string.firstrun_panel_title_welcome),
                            resources.getString(R.string.firstrun_urlbar_message),
                            resources.getString(R.string.firstrun_urlbar_subtext),
                            R.drawable.firstrun_welcome);
            }
        }
    }
}
