/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import android.graphics.Bitmap
import android.view.View
import org.hamcrest.CoreMatchers.not
import org.hamcrest.Matcher
import org.mozilla.fenix.helpers.matchers.BitmapDrawableMatcher
import androidx.test.espresso.matcher.ViewMatchers.isChecked as espressoIsChecked
import androidx.test.espresso.matcher.ViewMatchers.isEnabled as espressoIsEnabled
import androidx.test.espresso.matcher.ViewMatchers.isSelected as espressoIsSelected

/**
 * The [espressoIsEnabled] function that can also handle disabled state through the boolean argument.
 */
fun isEnabled(isEnabled: Boolean): Matcher<View> = maybeInvertMatcher(espressoIsEnabled(), isEnabled)

/**
 * The [espressoIsChecked] function that can also handle unchecked state through the boolean argument.
 */
fun isChecked(isChecked: Boolean): Matcher<View> = maybeInvertMatcher(espressoIsChecked(), isChecked)

/**
 * The [espressoIsSelected] function that can also handle not selected state through the boolean argument.
 */
fun isSelected(isSelected: Boolean): Matcher<View> = maybeInvertMatcher(espressoIsSelected(), isSelected)

private fun maybeInvertMatcher(matcher: Matcher<View>, useUnmodifiedMatcher: Boolean): Matcher<View> = when {
    useUnmodifiedMatcher -> matcher
    else -> not(matcher)
}

fun withBitmapDrawable(bitmap: Bitmap, name: String): Matcher<View>? = BitmapDrawableMatcher(bitmap, name)
