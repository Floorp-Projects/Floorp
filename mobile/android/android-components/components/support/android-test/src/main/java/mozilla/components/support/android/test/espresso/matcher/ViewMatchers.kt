/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.android.test.espresso.matcher

import android.view.View
import mozilla.components.support.android.test.Matchers.maybeInvertMatcher
import org.hamcrest.CoreMatchers.not
import org.hamcrest.Matcher
import androidx.test.espresso.matcher.ViewMatchers.hasFocus as espressoHasFocus
import androidx.test.espresso.matcher.ViewMatchers.isChecked as espressoIsChecked
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed as espressoIsDisplayed
import androidx.test.espresso.matcher.ViewMatchers.isEnabled as espressoIsEnabled
import androidx.test.espresso.matcher.ViewMatchers.isSelected as espressoIsSelected

// These functions are defined at the top-level so they appear in autocomplete, like the static methods on
// Android's [ViewMatchers] class.

/**
 * The existing [espressoHasFocus] function but uses a boolean to invert rather than requiring the [not] matcher.
 */
fun hasFocus(hasFocus: Boolean): Matcher<View> = maybeInvertMatcher(espressoHasFocus(), hasFocus)

/**
 * The existing [espressoIsChecked] function but uses a boolean to invert rather than requiring the [not] matcher.
 */
fun isChecked(isChecked: Boolean): Matcher<View> = maybeInvertMatcher(espressoIsChecked(), isChecked)

/**
 * The existing [espressoIsDisplayed] function but uses a boolean to invert rather than requiring the [not] matcher.
 */
fun isDisplayed(isDisplayed: Boolean): Matcher<View> = maybeInvertMatcher(espressoIsDisplayed(), isDisplayed)

/**
 * The existing [espressoIsEnabled] function but uses a boolean to invert rather than requiring the [not] matcher.
 */
fun isEnabled(isEnabled: Boolean): Matcher<View> = maybeInvertMatcher(espressoIsEnabled(), isEnabled)

/**
 * The existing [espressoIsSelected] function but uses a boolean to invert rather than requiring the [not] matcher.
 */
fun isSelected(isSelected: Boolean): Matcher<View> = maybeInvertMatcher(espressoIsSelected(), isSelected)
