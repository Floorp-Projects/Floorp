/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions.matches

fun ViewInteraction.click(): ViewInteraction = this.perform(ViewActions.click())!!

fun ViewInteraction.assertIsEnabled(isEnabled: Boolean): ViewInteraction {
    return this.check(matches(isEnabled(isEnabled)))!!
}

fun ViewInteraction.assertIsChecked(isChecked: Boolean): ViewInteraction {
    return this.check(matches(isChecked(isChecked)))!!
}

fun ViewInteraction.assertIsSelected(isSelected: Boolean): ViewInteraction {
    return this.check(matches(isSelected(isSelected)))!!
}
