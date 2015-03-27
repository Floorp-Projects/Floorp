/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.json.JSONArray;
import org.json.JSONObject;

import org.mozilla.gecko.widget.DoorHanger.Type;

public class DoorhangerConfig {

    public static class Link {
        public final String label;
        public final String url;
        public final String delimiter;

        private Link(String label, String url, String delimiter) {
            this.label = label;
            this.url = url;
            this.delimiter = delimiter;
        }
    }

    private final int tabId;
    private final String id;
    private DoorHanger.Type type;
    private String message;
    private JSONObject options;
    private Link link;
    private JSONArray buttons;

    public DoorhangerConfig() {
        // XXX: This should only be used by SiteIdentityPopup doorhangers which
        // don't need tab or id references, until bug 1141904 unifies doorhangers.
        this(-1, null);
    }

    public DoorhangerConfig(int tabId, String id) {
        this.tabId = tabId;
        this.id = id;
    }

    public int getTabId() {
        return tabId;
    }

    public String getId() {
        return id;
    }

    public void setType(Type type) {
        this.type = type;
    }

    public Type getType() {
        return type;
    }

    public void setMessage(String message) {
        this.message = message;
    }

    public String getMessage() {
        return message;
    }

    public void setOptions(JSONObject options) {
        this.options = options;
    }

    public JSONObject getOptions() {
        return options;
    }

    public void setButtons(JSONArray buttons) {
        this.buttons = buttons;
    }

    public JSONArray getButtons() {
        return buttons;
    }

    public void setLink(String label, String url, String delimiter) {
        this.link = new Link(label, url, delimiter);
    }

    public Link getLink() {
        return link;
    }
}
