/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

/**
 * Holds data definitions for our UI Telemetry implementation.
 *
 * See mobile/android/base/docs/index.rst for a full dictionary.
 */
public interface TelemetryContract {

    /**
     * Holds event names. Intended for use with
     * Telemetry.sendUIEvent() as the "action" parameter.
     *
     * Please keep this list sorted.
     */
    public interface Event {
        // Generic action, usually for tracking menu and toolbar actions.
        public static final String ACTION = "action.1";

        // Cancel a state, action, etc.
        public static final String CANCEL = "cancel.1";

        // Editing an item.
        public static final String EDIT = "edit.1";

        // Launching (opening) an external application.
        // Note: Only used in JavaScript for now, but here for completeness.
        public static final String LAUNCH = "launch.1";

        // Loading a URL.
        public static final String LOAD_URL = "loadurl.1";

        public static final String LOCALE_BROWSER_RESET = "locale.browser.reset.1";
        public static final String LOCALE_BROWSER_SELECTED = "locale.browser.selected.1";
        public static final String LOCALE_BROWSER_UNSELECTED = "locale.browser.unselected.1";

        // Set default panel.
        public static final String PANEL_SET_DEFAULT = "setdefault.1";

        // Pinning an item.
        public static final String PIN = "pin.1";

        // Outcome of data policy notification: can be true or false.
        public static final String POLICY_NOTIFICATION_SUCCESS = "policynotification.success.1:";

        // Sanitizing private data.
        public static final String SANITIZE = "sanitize.1";

        // Saving a resource (reader, bookmark, etc) for viewing later.
        // Note: Only used in JavaScript for now, but here for completeness.
        public static final String SAVE = "save.1";

        // Sharing content.
        public static final String SHARE = "share.1";

        // Unpinning an item.
        public static final String UNPIN = "unpin.1";

        // Stop holding a resource (reader, bookmark, etc) for viewing later.
        // Note: Only used in JavaScript for now, but here for completeness.
        public static final String UNSAVE = "unsave.1";
    }

    /**
     * Holds event methods. Intended for use in
     * Telemetry.sendUIEvent() as the "method" parameter.
     *
     * Please keep this list sorted.
     */
    public interface Method {
        // Action triggered from the action bar (including the toolbar).
        public static final String ACTIONBAR = "actionbar";

        // Action triggered by hitting the Android back button.
        public static final String BACK = "back";

        // Action triggered from a button.
        public static final String BUTTON = "button";

        // Action occurred via a context menu.
        public static final String CONTEXT_MENU = "contextmenu";

        // Action triggered from a dialog.
        public static final String DIALOG = "dialog";

        // Action triggered from a view grid item, like a thumbnail.
        public static final String GRID_ITEM = "griditem";

        // Action occurred via an intent.
        public static final String INTENT = "intent";

        // Action triggered from a list.
        public static final String LIST = "list";

        // Action triggered from a view list item, like a row of a list.
        public static final String LIST_ITEM = "listitem";

        // Action occurred via the main menu.
        public static final String MENU = "menu";

        // Action triggered from a pageaction in the URLBar.
        // Note: Only used in JavaScript for now, but here for completeness.
        public static final String PAGEACTION = "pageaction";

        // Action triggered from a suggestion provided to the user.
        public static final String SUGGESTION = "suggestion";
    }

    /**
     * Holds session names. Intended for use with
     * Telemetry.startUISession() as the "sessionName" parameter.
     *
     * Please keep this list sorted.
     */
    public interface Session {
        // Awesomescreen (including frecency search) is active.
        public static final String AWESOMESCREEN = "awesomescreen.1";

        // Started the very first time we believe the application has been launched.
        public static final String FIRSTRUN = "firstrun.1";

        // Awesomescreen frecency search is active.
        public static final String FRECENCY = "frecency.1";

        // Started when a user enters about:home.
        public static final String HOME = "home.1";

        // Started when a user enters a given home panel.
        // Session name is dynamic, encoded as "homepanel.1:<panel_id>"
        public static final String HOME_PANEL = "homepanel.1:";

        // Started when a Reader viewer becomes active in the foreground.
        // Note: Only used in JavaScript for now, but here for completeness.
        public static final String READER = "reader.1";
    }

    /**
     * Holds reasons for stopping a session. Intended for use in
     * Telemetry.stopUISession() as the "reason" parameter.
     *
     * Please keep this list sorted.
     */
    public interface Reason {
        // Changes were committed.
        public static final String COMMIT = "commit";
    }
}
