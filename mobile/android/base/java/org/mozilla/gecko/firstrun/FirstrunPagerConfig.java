/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.firstrun;

import android.content.Context;
import android.os.Bundle;
import android.util.Log;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.Experiments;

import java.util.LinkedList;
import java.util.List;

public class FirstrunPagerConfig {
    public static final String LOGTAG = "FirstrunPagerConfig";

    public static final String KEY_IMAGE = "imageRes";
    public static final String KEY_TEXT = "textRes";
    public static final String KEY_SUBTEXT = "subtextRes";

   public static List<FirstrunPanelConfig> getDefault(Context context) {
        final List<FirstrunPanelConfig> panels = new LinkedList<>();
       panels.add(SimplePanelConfigs.welcomePanelConfig);
       panels.add(SimplePanelConfigs.privatePanelConfig);
       panels.add(SimplePanelConfigs.customizePanelConfig);
       panels.add(SimplePanelConfigs.syncPanelConfig);

        return panels;
    }

    public static List<FirstrunPanelConfig> forFxAUser(Context context) {
        final List<FirstrunPanelConfig> panels = new LinkedList<>();
        panels.add(SimplePanelConfigs.welcomePanelConfig);
        panels.add(SimplePanelConfigs.privatePanelConfig);
        panels.add(SimplePanelConfigs.customizeLastPanelConfig);

        return panels;
    }

    public static List<FirstrunPanelConfig> getRestricted() {
        final List<FirstrunPanelConfig> panels = new LinkedList<>();
        panels.add(new FirstrunPanelConfig(RestrictedWelcomePanel.class.getName(), RestrictedWelcomePanel.TITLE_RES));
        return panels;
    }

    public static class FirstrunPanelConfig {

        private String classname;
        private int titleRes;
        private Bundle args;

        public FirstrunPanelConfig(String resource, int titleRes) {
            this(resource, titleRes, -1, -1, -1, true);
        }

        public FirstrunPanelConfig(String classname, int titleRes, int imageRes, int textRes, int subtextRes) {
            this(classname, titleRes, imageRes, textRes, subtextRes, false);
        }

        private FirstrunPanelConfig(String classname, int titleRes, int imageRes, int textRes, int subtextRes, boolean isCustom) {
            this.classname = classname;
            this.titleRes = titleRes;

            if (!isCustom) {
                this.args = new Bundle();
                this.args.putInt(KEY_IMAGE, imageRes);
                this.args.putInt(KEY_TEXT, textRes);
                this.args.putInt(KEY_SUBTEXT, subtextRes);
            }
        }

        public String getClassname() {
            return this.classname;
        }

        public int getTitleRes() {
            return this.titleRes;
        }

        public Bundle getArgs() {
            return args;
        }
    }

    private static class SimplePanelConfigs {
        public static final FirstrunPanelConfig welcomePanelConfig = new FirstrunPanelConfig(FirstrunPanel.class.getName(), R.string.firstrun_panel_title_welcome, R.drawable.firstrun_welcome, R.string.firstrun_urlbar_message, R.string.firstrun_urlbar_subtext);
        public static final FirstrunPanelConfig privatePanelConfig = new FirstrunPanelConfig(FirstrunPanel.class.getName(), R.string.firstrun_panel_title_privacy, R.drawable.firstrun_private, R.string.firstrun_privacy_message, R.string.firstrun_privacy_subtext);
        public static final FirstrunPanelConfig customizePanelConfig = new FirstrunPanelConfig(FirstrunPanel.class.getName(), R.string.firstrun_panel_title_customize, R.drawable.firstrun_data, R.string.firstrun_customize_message, R.string.firstrun_customize_subtext);
        public static final FirstrunPanelConfig customizeLastPanelConfig = new FirstrunPanelConfig(LastPanel.class.getName(), R.string.firstrun_panel_title_customize, R.drawable.firstrun_data, R.string.firstrun_customize_message, R.string.firstrun_customize_subtext);

        public static final FirstrunPanelConfig syncPanelConfig = new FirstrunPanelConfig(SyncPanel.class.getName(), R.string.firstrun_sync_title, R.drawable.firstrun_sync, R.string.firstrun_sync_message, R.string.firstrun_sync_subtext);

    }
}
