/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.mozglue.RobocopTarget;

/**
 * Holds data definitions for our UI Telemetry implementation.
 *
 * Note that enum values of "_TEST*" are reserved for testing and
 * should not be changed without changing the associated tests.
 *
 * See mobile/android/base/docs/index.rst for a full dictionary.
 */
@RobocopTarget
public interface TelemetryContract {

    /**
     * Holds event names. Intended for use with
     * Telemetry.sendUIEvent() as the "action" parameter.
     *
     * Please keep this list sorted.
     */
    public enum Event {
        // Generic action, usually for tracking menu and toolbar actions.
        ACTION("action.1"),

        // Cancel a state, action, etc.
        CANCEL("cancel.1"),

        // Start casting a video.
        // Note: Only used in JavaScript for now, but here for completeness.
        CAST("cast.1"),

        // Editing an item.
        EDIT("edit.1"),

        // Launching (opening) an external application.
        // Note: Only used in JavaScript for now, but here for completeness.
        LAUNCH("launch.1"),

        // Loading a URL.
        LOAD_URL("loadurl.1"),

        LOCALE_BROWSER_RESET("locale.browser.reset.1"),
        LOCALE_BROWSER_SELECTED("locale.browser.selected.1"),
        LOCALE_BROWSER_UNSELECTED("locale.browser.unselected.1"),

        // Hide a built-in home panel.
        PANEL_HIDE("panel.hide.1"),

        // Move a home panel up or down.
        PANEL_MOVE("panel.move.1"),

        // Remove a custom home panel.
        PANEL_REMOVE("panel.remove.1"),

        // Set default home panel.
        PANEL_SET_DEFAULT("panel.setdefault.1"),

        // Show a hidden built-in home panel.
        PANEL_SHOW("panel.show.1"),

        // Pinning an item.
        PIN("pin.1"),

        // Outcome of data policy notification: can be true or false.
        POLICY_NOTIFICATION_SUCCESS("policynotification.success.1"),

        // Sanitizing private data.
        SANITIZE("sanitize.1"),

        // Saving a resource (reader, bookmark, etc) for viewing later.
        SAVE("save.1"),

        // Perform a search -- currently used when starting a search in the search activity.
        SEARCH("search.1"),

        // Remove a search engine.
        SEARCH_REMOVE("search.remove.1"),

        // Restore default search engines.
        SEARCH_RESTORE_DEFAULTS("search.restoredefaults.1"),

        // Set default search engine.
        SEARCH_SET_DEFAULT("search.setdefault.1"),

        // Sharing content.
        SHARE("share.1"),

        // Show a UI element.
        SHOW("show.1"),

        // Undoing a user action.
        // Note: Only used in JavaScript for now, but here for completeness.
        UNDO("undo.1"),

        // Unpinning an item.
        UNPIN("unpin.1"),

        // Stop holding a resource (reader, bookmark, etc) for viewing later.
        UNSAVE("unsave.1"),

        // VALUES BELOW THIS LINE ARE EXCLUSIVE TO TESTING.
        _TEST1("_test_event_1.1"),
        _TEST2("_test_event_2.1"),
        _TEST3("_test_event_3.1"),
        _TEST4("_test_event_4.1"),
        ;

        private final String string;

        Event(final String string) {
            this.string = string;
        }

        @Override
        public String toString() {
            return string;
        }
    }

    /**
     * Holds event methods. Intended for use in
     * Telemetry.sendUIEvent() as the "method" parameter.
     *
     * Please keep this list sorted.
     */
    public enum Method {
        // Action triggered from the action bar (including the toolbar).
        ACTIONBAR("actionbar"),

        // Action triggered by hitting the Android back button.
        BACK("back"),

        // Action triggered from a button.
        BUTTON("button"),

        // Action taken from a content page -- for example, a search results web page.
        CONTENT("content"),

        // Action occurred via a context menu.
        CONTEXT_MENU("contextmenu"),

        // Action triggered from a dialog.
        DIALOG("dialog"),

        // Action triggered from a view grid item, like a thumbnail.
        GRID_ITEM("griditem"),

        // Action occurred via an intent.
        INTENT("intent"),

        // Action occurred via a homescreen launcher.
        HOMESCREEN("homescreen"),

        // Action triggered from a list.
        LIST("list"),

        // Action triggered from a view list item, like a row of a list.
        LIST_ITEM("listitem"),

        // Action occurred via the main menu.
        MENU("menu"),

        // No method is specified.
        NONE(null),

        // Action triggered from a notification in the Android notification bar.
        NOTIFICATION("notification"),

        // Action triggered from a pageaction in the URLBar.
        // Note: Only used in JavaScript for now, but here for completeness.
        PAGEACTION("pageaction"),

        // Action triggered from a settings screen.
        SETTINGS("settings"),

        // Actions triggered from the share overlay.
        SHARE_OVERLAY("shareoverlay"),

        // Action triggered from a suggestion provided to the user.
        SUGGESTION("suggestion"),

        // Action triggered from a SuperToast.
        // Note: Only used in JavaScript for now, but here for completeness.
        TOAST("toast"),

        // Action triggerred by pressing a SearchWidget button
        WIDGET("widget"),

        // VALUES BELOW THIS LINE ARE EXCLUSIVE TO TESTING.
        _TEST1("_test_method_1"),
        _TEST2("_test_method_2"),
        ;

        private final String string;

        Method(final String string) {
            this.string = string;
        }

        @Override
        public String toString() {
            return string;
        }
    }

    /**
     * Holds session names. Intended for use with
     * Telemetry.startUISession() as the "sessionName" parameter.
     *
     * Please keep this list sorted.
     */
    public enum Session {
        // Awesomescreen (including frecency search) is active.
        AWESOMESCREEN("awesomescreen.1"),

        // Started the very first time we believe the application has been launched.
        FIRSTRUN("firstrun.1"),

        // Awesomescreen frecency search is active.
        FRECENCY("frecency.1"),

        // Started when a user enters a given home panel.
        // Session name is dynamic, encoded as "homepanel.1:<panel_id>"
        HOME_PANEL("homepanel.1"),

        // Started when a Reader viewer becomes active in the foreground.
        // Note: Only used in JavaScript for now, but here for completeness.
        READER("reader.1"),

        // Started when the search activity launches.
        SEARCH_ACTIVITY("searchactivity.1"),

        // Settings activity is active.
        SETTINGS("settings.1"),

        // VALUES BELOW THIS LINE ARE EXCLUSIVE TO TESTING.
        _TEST_STARTED_TWICE("_test_session_started_twice.1"),
        _TEST_STOPPED_TWICE("_test_session_stopped_twice.1"),
        ;

        private final String string;

        Session(final String string) {
            this.string = string;
        }

        @Override
        public String toString() {
            return string;
        }
    }

    /**
     * Holds reasons for stopping a session. Intended for use in
     * Telemetry.stopUISession() as the "reason" parameter.
     *
     * Please keep this list sorted.
     */
    public enum Reason {
        // Changes were committed.
        COMMIT("commit"),

        // No reason is specified.
        NONE(null),

        // VALUES BELOW THIS LINE ARE EXCLUSIVE TO TESTING.
        _TEST1("_test_reason_1"),
        _TEST2("_test_reason_2"),
        _TEST_IGNORED("_test_reason_ignored"),
        ;

        private final String string;

        Reason(final String string) {
            this.string = string;
        }

        @Override
        public String toString() {
            return string;
        }
    }
}
