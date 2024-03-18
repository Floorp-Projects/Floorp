/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.android.test.espresso

import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions
import mozilla.components.support.android.test.espresso.matcher.hasFocus as hasFocusFun
import mozilla.components.support.android.test.espresso.matcher.isChecked as isCheckedFun
import mozilla.components.support.android.test.espresso.matcher.isDisplayed as isDisplayedFun
import mozilla.components.support.android.test.espresso.matcher.isEnabled as isEnabledFun
import mozilla.components.support.android.test.espresso.matcher.isSelected as isSelectedFun

/**
 * Shorthand to [ViewActions.click] the View.
 */
fun ViewInteraction.click(): ViewInteraction = this.perform(ViewActions.click())!!

/**
 * Asserts the View has focus or does not have focus based on the Boolean argument.
 */
fun ViewInteraction.assertHasFocus(hasFocus: Boolean): ViewInteraction {
    return this.check(ViewAssertions.matches(hasFocusFun(hasFocus)))
}

/**
 * Asserts the View is checked or is not checked based on the Boolean argument.
 */
fun ViewInteraction.assertIsChecked(isChecked: Boolean): ViewInteraction {
    return this.check(ViewAssertions.matches(isCheckedFun(isChecked)))!!
}

/**
 * Asserts the View is displayed or is not displayed based on the Boolean argument.
 */
fun ViewInteraction.assertIsDisplayed(isDisplayed: Boolean): ViewInteraction {
    return this.check(ViewAssertions.matches(isDisplayedFun(isDisplayed)))!!
}

/**
 * Asserts the View is enabled or is not enabled based on the Boolean argument.
 */
fun ViewInteraction.assertIsEnabled(isEnabled: Boolean): ViewInteraction {
    return this.check(ViewAssertions.matches(isEnabledFun(isEnabled)))!!
}

/**
 * Asserts the View is selected or is not selected based on the Boolean argument.
 */
fun ViewInteraction.assertIsSelected(isSelected: Boolean): ViewInteraction {
    return this.check(ViewAssertions.matches(isSelectedFun(isSelected)))!!
}
