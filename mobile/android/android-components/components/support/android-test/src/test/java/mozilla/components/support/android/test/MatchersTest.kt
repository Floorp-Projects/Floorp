/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.android.test

import mozilla.components.support.android.test.Matchers.maybeInvertMatcher
import mozilla.components.support.android.test.helpers.assertEqualsMatchers
import org.hamcrest.CoreMatchers.not
import org.hamcrest.Matchers
import org.junit.Test

class MatchersTest {

    @Test
    fun `WHEN maybeInvertMatcher with unmodifiedMatcher THEN an equivalent matcher is returned`() {
        val expected = Matchers.contains(4)
        assertEqualsMatchers(expected, maybeInvertMatcher(expected, useUnmodifiedMatcher = true))
    }

    @Test
    fun `WHEN maybeInvertMatcher with a modified matcher THEN the inverted matcher is returned`() {
        val input = Matchers.contains(4)
        val expected = not(input)
        assertEqualsMatchers(expected, maybeInvertMatcher(input, useUnmodifiedMatcher = false))
    }
}
