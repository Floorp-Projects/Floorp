/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.firstrun;

import android.content.Context;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.support.annotation.NonNull;

import org.mozilla.gecko.mma.MmaDelegate;

import java.util.LinkedList;
import java.util.List;

class FirstrunPagerConfig {
    static final String LOGTAG = "FirstrunPagerConfig";

    static final String KEY_IMAGE = "panelImage";
    static final String KEY_MESSAGE = "panelMessage";
    static final String KEY_SUBTEXT = "panelDescription";

    static List<FirstrunPanelConfig> getDefault(Context context, final boolean useLocalValues) {
        final List<FirstrunPanelConfig> panels = new LinkedList<>();
        panels.add(FirstrunPanelConfig.getConfiguredPanel(context, PanelConfig.TYPE.WELCOME, useLocalValues));
        panels.add(FirstrunPanelConfig.getConfiguredPanel(context, PanelConfig.TYPE.PRIVACY, useLocalValues));
        panels.add(FirstrunPanelConfig.getConfiguredPanel(context, PanelConfig.TYPE.CUSTOMIZE, useLocalValues));
        panels.add(FirstrunPanelConfig.getConfiguredPanel(context, PanelConfig.TYPE.SYNC, useLocalValues));

        return panels;
    }

    static List<FirstrunPanelConfig> forFxAUser(Context context, final boolean useLocalValues) {
        final List<FirstrunPanelConfig> panels = new LinkedList<>();
        panels.add(FirstrunPanelConfig.getConfiguredPanel(context, PanelConfig.TYPE.WELCOME, useLocalValues));
        panels.add(FirstrunPanelConfig.getConfiguredPanel(context, PanelConfig.TYPE.PRIVACY, useLocalValues));
        panels.add(FirstrunPanelConfig.getConfiguredPanel(context, PanelConfig.TYPE.LAST_CUSTOMIZE, useLocalValues));

        return panels;
    }

    static List<FirstrunPanelConfig> getRestricted(Context context) {
        final List<FirstrunPanelConfig> panels = new LinkedList<>();
        panels.add(new FirstrunPanelConfig(RestrictedWelcomePanel.class.getName(),
                context.getString(RestrictedWelcomePanel.TITLE_RES)));
        return panels;
    }

    static class FirstrunPanelConfig {
        private String classname;
        private String title;
        private Bundle args;

        FirstrunPanelConfig(String resource, String title) {
            this(resource, title, null, null, null, true);
        }

        private FirstrunPanelConfig(String classname, String title, Bitmap image, String message,
                                    String subtext, boolean isCustom) {
            this.classname = classname;
            this.title = title;

            if (!isCustom) {
                args = new Bundle();
                args.putParcelable(KEY_IMAGE, image);
                args.putString(KEY_MESSAGE, message);
                args.putString(KEY_SUBTEXT, subtext);
            }
        }

        static FirstrunPanelConfig getConfiguredPanel(@NonNull Context context,
                                                      PanelConfig.TYPE wantedPanelConfig,
                                                      final boolean useLocalValues) {
            PanelConfig panelConfig;
            if (useLocalValues) {
                panelConfig = new LocalFirstRunPanelProvider().getPanelConfig(context, wantedPanelConfig, useLocalValues);
            } else {
                panelConfig = new RemoteFirstRunPanelConfig().getPanelConfig(context, wantedPanelConfig, useLocalValues);
            }
            return new FirstrunPanelConfig(panelConfig.getClassName(), panelConfig.getTitle(),
                    panelConfig.getImage(), panelConfig.getMessage(), panelConfig.getText(), false);
        }


        String getClassname() {
            return classname;
        }

        String getTitle() {
            return title;
        }

        Bundle getArgs() {
            return args;
        }
    }
}
