/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.state

/**
 * Represents the source/origin of a session to describe how and why it was created.
 */
enum class SourceState {
    /**
     * Created to handle an ACTION_SEND (share) intent
     */
    ACTION_SEND,

    /**
     * Created to handle an ACTION_VIEW intent
     */
    ACTION_VIEW,

    /**
     * Created to handle a CustomTabs intent
     */
    CUSTOM_TAB,

    /**
     * User interacted with the home screen
     */
    HOME_SCREEN,

    /**
     * User interacted with a menu
     */
    MENU,

    /**
     * User opened a new tab
     */
    NEW_TAB,

    /**
     * Default value and for testing purposes
     */
    NONE,

    /**
     * Default value and for testing purposes
     */
    TEXT_SELECTION,

    /**
     * User entered a URL or search term
     */
    USER_ENTERED
}
