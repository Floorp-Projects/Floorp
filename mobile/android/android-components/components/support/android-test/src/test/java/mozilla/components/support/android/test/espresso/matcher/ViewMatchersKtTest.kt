/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.android.test.espresso.matcher

import android.view.View
import androidx.test.espresso.matcher.ViewMatchers
import mozilla.components.support.android.test.helpers.assertEqualsMatchers
import org.hamcrest.CoreMatchers.not
import org.hamcrest.Matcher
import org.junit.Test

private val BOOLEAN_VIEW_MATCHER_TO_UNDERLYING_MATCHER: List<Pair<(Boolean) -> Matcher<View>, Matcher<View>>> = listOf(
    ::hasFocus to ViewMatchers.hasFocus(),
    ::isChecked to ViewMatchers.isChecked(),
    ::isDisplayed to ViewMatchers.isDisplayed(),
    ::isEnabled to ViewMatchers.isEnabled(),
    ::isSelected to ViewMatchers.isSelected(),
)

class ViewMatchersKtTest {

    @Test
    fun `WHEN checking the unmodified ViewMatcher THEN it equals the underlying ViewMatcher`() {
        BOOLEAN_VIEW_MATCHER_TO_UNDERLYING_MATCHER.forEach { (input, expected) ->
            assertEqualsMatchers(expected, input(true))
        }
    }

    @Test
    fun `WHEN checking the modified ViewMatcher tHEN it equals the inversion of the underlying ViewMatcher`() {
        BOOLEAN_VIEW_MATCHER_TO_UNDERLYING_MATCHER.forEach { (input, inversionOfExpected) ->
            assertEqualsMatchers(not(inversionOfExpected), input(false))
        }
    }
}
