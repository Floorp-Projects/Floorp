/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.facts

/**
 * A user or system action that causes [Fact] instances to be emitted.
 */
enum class Action {
    /**
     * The user has clicked on something.
     */
    CLICK,

    /**
     * The user has toggled something.
     *
     * Other than a click this action is performed on items that have a distinct number of states. For example clicking
     * a checkbox or switch widget could emit a [Fact] with a [TOGGLE] action instead of a [CLICK] action.
     */
    TOGGLE,

    /**
     * The user has committed an input (e.g. entered text into an input field and then pressed enter).
     */
    COMMIT,

    /**
     * The user has started playing something.
     */
    PLAY,

    /**
     * The user has paused something.
     */
    PAUSE,

    /**
     * The user has stopped something.
     */
    STOP,

    /**
     * The user has resumed something.
     */
    RESUME,

    /**
     * The user has confirmed something.
     */
    CONFIRM,

    /**
     * The user has cancelled something.
     */
    CANCEL,

    /**
     * The user has retried something.
     */
    TRY_AGAIN,

    /**
     * The user has opened something.
     */
    OPEN,

    /**
     * A generic interaction that can be caused by a previous action (e.g. the user clicks on a button which causes a
     * [Fact] with [CLICK] action to be emitted. This click may causes something to load which emits a follow-up a
     * [Fact] with [INTERACTION] action.
     */
    INTERACTION,

    /**
     * An implementation detail event exposed to understand the system for diagnostic purposes.
     * For example, for each action the app may add profiler markers to better understand
     * the app visually inside profiles.
     */
    IMPLEMENTATION_DETAIL,

    /**
     * An action triggered by the Android system.
     */
    SYSTEM,

    /**
     * Something is getting displayed.
     */
    DISPLAY,

    /**
     * The user selected something from a list of options.
     */
    SELECT,
}
