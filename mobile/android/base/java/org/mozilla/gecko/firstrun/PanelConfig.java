/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.firstrun;

/**
 * Onboarding screens configuration object.
 */
public class PanelConfig {
    public enum TYPE {
        WELCOME, PRIVACY, LAST_PRIVACY, CUSTOMIZE, LAST_CUSTOMIZE, SYNC
    }

    private final TYPE type;
    private final boolean useLocalValues;
    private final String title;
    private final String message;
    private final String text;
    private final int image;

    public PanelConfig(TYPE type, boolean useLocalValues, String title, String message, String text, int image) {
        this.type = type;
        this.useLocalValues = useLocalValues;
        this.title = title;
        this.message = message;
        this.text = text;
        this.image = image;
    }

    public String getClassName() {
        switch (type) {
            case WELCOME:
            case PRIVACY:
            case CUSTOMIZE:
                return FirstrunPanel.class.getName();
            case LAST_PRIVACY:
            case LAST_CUSTOMIZE:
                return LastPanel.class.getName();
            case SYNC:
                return SyncPanel.class.getName();
            default:    // Return the default Panel, same as for "WELCOME"
                return FirstrunPanel.class.getName();
        }
    }

    public TYPE getType() {
        return type;
    }

    public boolean isUsingLocalValues() {
        return useLocalValues;
    }

    public String getTitle() {
        return title;
    }

    public String getMessage() {
        return message;
    }

    public String getText() {
        return text;
    }

    public int getImage() {
        return image;
    }
}
