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
        panels.add(new FirstrunPanel(WelcomePanel.class.getName(), "Welcome"));
        return panels;
    }

    public static class FirstrunPanel {
        private String resource;
        private String title;

        public FirstrunPanel(String resource, String title) {
            this.resource = resource;
            this.title = title;
        }

        public String getResource() {
            return this.resource;
        }

        public String getTitle() {
            return this.title;
        }

    }
}
