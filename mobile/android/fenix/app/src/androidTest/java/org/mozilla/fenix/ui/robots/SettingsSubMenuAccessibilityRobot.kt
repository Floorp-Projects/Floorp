/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.util.Log
import android.view.KeyEvent
import android.view.KeyEvent.ACTION_DOWN
import android.view.KeyEvent.KEYCODE_DPAD_LEFT
import android.view.KeyEvent.KEYCODE_DPAD_RIGHT
import android.view.View
import android.widget.SeekBar
import android.widget.TextView
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.UiController
import androidx.test.espresso.ViewAction
import androidx.test.espresso.ViewAssertion
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.Matcher
import org.junit.Assert.assertTrue
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.assertIsEnabled
import org.mozilla.fenix.helpers.isEnabled
import org.mozilla.fenix.ui.robots.SettingsSubMenuAccessibilityRobot.Companion.DECIMAL_CONVERSION
import org.mozilla.fenix.ui.robots.SettingsSubMenuAccessibilityRobot.Companion.MIN_VALUE
import org.mozilla.fenix.ui.robots.SettingsSubMenuAccessibilityRobot.Companion.STEP_SIZE
import org.mozilla.fenix.ui.robots.SettingsSubMenuAccessibilityRobot.Companion.TEXT_SIZE
import kotlin.math.roundToInt

/**
 * Implementation of Robot Pattern for the settings Accessibility sub menu.
 */
class SettingsSubMenuAccessibilityRobot {

    companion object {
        const val STEP_SIZE = 5
        const val MIN_VALUE = 50
        const val DECIMAL_CONVERSION = 100f
        const val TEXT_SIZE = 16f
    }

    fun clickFontSizingSwitch() = toggleFontSizingSwitch()

    fun verifyEnabledMenuItems() {
        Log.i(TAG, "verifyEnabledMenuItems: Trying to verify that the \"Font Size\" title is visible")
        onView(withText("Font Size")).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyEnabledMenuItems: Verified that the \"Font Size\" title is visible")
        Log.i(TAG, "verifyEnabledMenuItems: Trying to verify that the \"Font Size\" title is enabled")
        onView(withText("Font Size")).check(matches(isEnabled(true)))
        Log.i(TAG, "verifyEnabledMenuItems: Verified that the \"Font Size\" title is enabled")
        Log.i(TAG, "verifyEnabledMenuItems: Trying to verify that the \"Make text on websites larger or smaller\" summary is visible")
        onView(withText("Make text on websites larger or smaller")).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyEnabledMenuItems: Verified that the \"Make text on websites larger or smaller\" summary is visible")
        Log.i(TAG, "verifyEnabledMenuItems: Trying to verify that the \"Make text on websites larger or smaller\" summary is enabled")
        onView(withText("Make text on websites larger or smaller")).check(matches(isEnabled(true)))
        Log.i(TAG, "verifyEnabledMenuItems: Verified that the \"Make text on websites larger or smaller\" summary is enabled")
        Log.i(TAG, "verifyEnabledMenuItems: Trying to verify the \"This is sample text. It is here to show how text will appear\" sample text")
        onView(withId(org.mozilla.fenix.R.id.sampleText))
            .check(matches(withText("This is sample text. It is here to show how text will appear when you increase or decrease the size with this setting.")))
        Log.i(TAG, "verifyEnabledMenuItems: Verified the \"This is sample text. It is here to show how text will appear\" sample text")
        Log.i(TAG, "verifyEnabledMenuItems: Trying to verify that the seek bar value is set to 100%")
        onView(withId(org.mozilla.fenix.R.id.seekbar_value)).check(matches(withText("100%")))
        Log.i(TAG, "verifyEnabledMenuItems: Verified that the seek bar value is set to 100%")
        Log.i(TAG, "verifyEnabledMenuItems: Trying to verify that the seek bar is visible")
        onView(withId(org.mozilla.fenix.R.id.seekbar)).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyEnabledMenuItems: Verified that the seek bar is visible")
    }

    fun verifyMenuItemsAreDisabled() {
        Log.i(TAG, "verifyMenuItemsAreDisabled: Trying to verify that the \"Font Size\" title is not enabled")
        onView(withText("Font Size")).assertIsEnabled(false)
        Log.i(TAG, "verifyMenuItemsAreDisabled: Verified that the \"Font Size\" title is not enabled")
        Log.i(TAG, "verifyMenuItemsAreDisabled: Trying to verify that the \"Make text on websites larger or smaller\" summary is not enabled")
        onView(withText("Make text on websites larger or smaller")).assertIsEnabled(false)
        Log.i(TAG, "verifyMenuItemsAreDisabled: Verified that the \"Make text on websites larger or smaller\" summary is not enabled")
        Log.i(TAG, "verifyMenuItemsAreDisabled: Trying to verify that the \"This is sample text. It is here to show how text will appear\" sample text is not enabled")
        onView(withId(org.mozilla.fenix.R.id.sampleText)).assertIsEnabled(false)
        Log.i(TAG, "verifyMenuItemsAreDisabled: Verified that the \"This is sample text. It is here to show how text will appear\" sample text is not enabled")
        Log.i(TAG, "verifyMenuItemsAreDisabled: Trying to verify that the seek bar value is not enabled")
        onView(withId(org.mozilla.fenix.R.id.seekbar_value)).assertIsEnabled(false)
        Log.i(TAG, "verifyMenuItemsAreDisabled: Verified that the seek bar value is not enabled")
        Log.i(TAG, "verifyMenuItemsAreDisabled: Trying to verify that the seek bar is not enabled")
        onView(withId(org.mozilla.fenix.R.id.seekbar)).assertIsEnabled(false)
        Log.i(TAG, "verifyMenuItemsAreDisabled: Verified that the seek bar is not enabled")
    }

    fun changeTextSizeSlider(seekBarPercentage: Int) = adjustTextSizeSlider(seekBarPercentage)

    fun verifyTextSizePercentage(textSize: Int) {
        Log.i(TAG, "verifyTextSizePercentage: Trying to verify that the text size percentage is set to: $textSize")
        onView(withId(org.mozilla.fenix.R.id.sampleText))
            .check(textSizePercentageEquals(textSize))
        Log.i(TAG, "verifyTextSizePercentage: Verified that the text size percentage is set to: $textSize")
    }

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            Log.i(TAG, "goBack: Waiting for device to be idle")
            mDevice.waitForIdle()
            Log.i(TAG, "goBack: Waited for device to be idle")
            Log.i(TAG, "goBack: Trying to click the navigate up toolbar button")
            goBackButton().perform(click())
            Log.i(TAG, "goBack: Clicked the navigate up toolbar button")

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private fun toggleFontSizingSwitch() {
    Log.i(TAG, "toggleFontSizingSwitch: Trying to click the \"Automatic font sizing\" toggle")
    // Toggle font size to off
    onView(withText("Automatic font sizing"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        .perform(click())
    Log.i(TAG, "toggleFontSizingSwitch: Clicked the \"Automatic font sizing\" toggle")
}

private fun adjustTextSizeSlider(seekBarPercentage: Int) {
    Log.i(TAG, "adjustTextSizeSlider: Trying to set the seek bar value to: $seekBarPercentage")
    onView(withId(org.mozilla.fenix.R.id.seekbar))
        .perform(SeekBarChangeProgressViewAction(seekBarPercentage))
    Log.i(TAG, "adjustTextSizeSlider: Seek bar value was set to: $seekBarPercentage")
}

private fun goBackButton() =
    onView(allOf(withContentDescription("Navigate up")))

class SeekBarChangeProgressViewAction(val seekBarPercentage: Int) : ViewAction {
    override fun getConstraints(): Matcher<View> {
        return isAssignableFrom(SeekBar::class.java)
    }

    override fun perform(uiController: UiController?, view: View?) {
        val targetStepSize = calculateStepSizeFromPercentage(seekBarPercentage)
        val seekbar = view as SeekBar
        var progress = seekbar.progress

        if (targetStepSize > progress) {
            for (i in progress until targetStepSize) {
                seekbar.onKeyDown(KEYCODE_DPAD_RIGHT, KeyEvent(ACTION_DOWN, KEYCODE_DPAD_RIGHT))
            }
        } else if (progress > targetStepSize) {
            for (i in progress downTo targetStepSize) {
                seekbar.onKeyDown(KEYCODE_DPAD_LEFT, KeyEvent(ACTION_DOWN, KEYCODE_DPAD_LEFT))
            }
        }
    }

    override fun getDescription(): String {
        return "Changes the progress on a SeekBar, based on the percentage value."
    }
}

fun textSizePercentageEquals(textSizePercentage: Int): ViewAssertion {
    return ViewAssertion { view, noViewFoundException ->
        if (noViewFoundException != null) throw noViewFoundException

        val textView = view as TextView
        val scaledPixels =
            textView.textSize / InstrumentationRegistry.getInstrumentation().context.resources.displayMetrics.density
        val currentTextSizePercentage = calculateTextPercentageFromTextSize(scaledPixels)

        if (currentTextSizePercentage != textSizePercentage) throw AssertionError("The textview has a text size percentage of $currentTextSizePercentage, and does not match $textSizePercentage")
    }
}

fun calculateTextPercentageFromTextSize(textSize: Float): Int {
    val decimal = textSize / TEXT_SIZE
    return (decimal * DECIMAL_CONVERSION).roundToInt()
}

fun calculateStepSizeFromPercentage(textSizePercentage: Int): Int {
    return ((textSizePercentage - MIN_VALUE) / STEP_SIZE)
}

fun checkTextSizeOnWebsite(textSizePercentage: Int, components: Components) {
    Log.i(TAG, "checkTextSizeOnWebsite: Trying to verify that text size on website is: $textSizePercentage")
    // Checks the Gecko engine settings for the font size
    val textSize = calculateStepSizeFromPercentage(textSizePercentage)
    val newTextScale = ((textSize * STEP_SIZE) + MIN_VALUE).toFloat() / DECIMAL_CONVERSION
    assertTrue("$TAG: text size on website was not set to: $textSizePercentage", components.core.engine.settings.fontSizeFactor == newTextScale)
    Log.i(TAG, "checkTextSizeOnWebsite: Verified that text size on website is: $textSizePercentage")
}
