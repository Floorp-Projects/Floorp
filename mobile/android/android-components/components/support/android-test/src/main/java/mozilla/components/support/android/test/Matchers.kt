/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.android.test

import org.hamcrest.CoreMatchers.not
import org.hamcrest.Matcher

/**
 * A collection of non-domain specific [Matcher]s.
 */
object Matchers {

    /**
     * Conditionally applies the [not] matcher based on the given argument: inverts the matcher when
     * [useUnmodifiedMatcher] is false, otherwise returns the matcher unmodified.
     *
     * This allows developers to write code more generically by using a boolean argument: e.g. assertIsShown(Boolean)
     * rather than two methods, assertIsShown() and assertIsNotShown().
     */
    fun <T> maybeInvertMatcher(matcher: Matcher<T>, useUnmodifiedMatcher: Boolean): Matcher<T> = when {
        useUnmodifiedMatcher -> matcher
        else -> not(matcher)
    }
}
