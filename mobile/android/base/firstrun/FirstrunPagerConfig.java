/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.firstrun;

import java.util.LinkedList;
import java.util.List;

public class FirstrunPagerConfig {
    public static List<FirstrunPanel> getDefault() {
        final List<FirstrunPanel> panels = new LinkedList<>();
        panels.add(new FirstrunPanel(WelcomePanel.class.getName(), WelcomePanel.TITLE_RES));
        return panels;
    }

    public static class FirstrunPanel {
        private String classname;
        private int titleRes;

        public FirstrunPanel(String resource, int titleRes) {
            this.classname= resource;
            this.titleRes = titleRes;
        }

        public String getClassname() {
            return this.classname;
        }

        public int getTitleRes() {
            return this.titleRes;
        }

    }
}
