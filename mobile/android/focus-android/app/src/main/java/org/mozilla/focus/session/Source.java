/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session;

/**
 * The source of a sessio: How was this session created? E.g. did we receive an Intent or did the
 * user start this session?
 */
public enum Source {
    /**
     * Intent.ACTION_VIEW
     */
    VIEW,

    /**
     * Intent.ACTION_SEND
     */
    SHARE,

    /**
     * Via text selection action ("Search privately")
     */
    TEXT_SELECTION,

    /**
     * Via a home screen shortcut.
     */
    HOME_SCREEN,

    /**
     * The user entered a URL (or search ter,s)
     */
    USER_ENTERED,

    /**
     * Custom tab from a third-party application.
     */
    CUSTOM_TAB,

    /**
     * Open as a new tab from the (context( menu.
     */
    MENU,

    /**
     * Only used internally if we need to temporarily create a session object with no specific source.
     */
    NONE
}
