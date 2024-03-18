/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import android.view.InputDevice
import android.view.MotionEvent
import androidx.test.espresso.ViewAction
import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.action.GeneralClickAction
import androidx.test.espresso.action.GeneralLocation
import androidx.test.espresso.action.Press
import androidx.test.espresso.action.Tap
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

/**
 * Perform a click (simulate the finger touching the View) at a specific location in the View
 * rather than the default middle of the View.
 *
 * Useful in situations where the View we want clicked contains other Views in it's x,y middle
 * and we need to simulate the touch in some other free space of the View we want clicked.
 */
fun ViewInteraction.clickAtLocationInView(locationInView: GeneralLocation): ViewAction =
    ViewActions.actionWithAssertions(
        GeneralClickAction(
            Tap.SINGLE,
            locationInView,
            Press.FINGER,
            InputDevice.SOURCE_UNKNOWN,
            MotionEvent.BUTTON_PRIMARY,
        ),
    )
