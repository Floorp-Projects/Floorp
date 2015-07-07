/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.json.JSONObject;

import org.mozilla.gecko.widget.DoorHanger.Type;

public class DoorhangerConfig {

    public static class Link {
        public final String label;
        public final String url;

        private Link(String label, String url) {
            this.label = label;
            this.url = url;
        }
    }

    public static class ButtonConfig {
        public final String label;
        public final int callback;

        public ButtonConfig(String label, int callback) {
            this.label = label;
            this.callback = callback;
        }
    }
    private static final String LOGTAG = "DoorhangerConfig";

    private final int tabId;
    private final String id;
    private final DoorHanger.OnButtonClickListener buttonClickListener;
    private final DoorHanger.Type type;
    private String message;
    private JSONObject options;
    private Link link;
    private ButtonConfig positiveButtonConfig;
    private ButtonConfig negativeButtonConfig;

    public DoorhangerConfig(Type type, DoorHanger.OnButtonClickListener listener) {
        // XXX: This should only be used by SiteIdentityPopup doorhangers which
        // don't need tab or id references, until bug 1141904 unifies doorhangers.

        this(-1, null, type, listener);
    }

    public DoorhangerConfig(int tabId, String id, DoorHanger.Type type, DoorHanger.OnButtonClickListener buttonClickListener) {
        this.tabId = tabId;
        this.id = id;
        this.type = type;
        this.buttonClickListener = buttonClickListener;
    }

    public int getTabId() {
        return tabId;
    }

    public String getId() {
        return id;
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

    public void setButton(String label, int callbackId, boolean isPositive) {
        final ButtonConfig buttonConfig = new ButtonConfig(label, callbackId);
        if (isPositive) {
            positiveButtonConfig = buttonConfig;
        } else {
            negativeButtonConfig = buttonConfig;
        }
    }

    public ButtonConfig getPositiveButtonConfig() {
        return positiveButtonConfig;
    }

    public  ButtonConfig getNegativeButtonConfig() {
        return negativeButtonConfig;
    }

    public DoorHanger.OnButtonClickListener getButtonClickListener() {
        return this.buttonClickListener;
    }

    public void setLink(String label, String url) {
        this.link = new Link(label, url);
    }

    public Link getLink() {
        return link;
    }
}
