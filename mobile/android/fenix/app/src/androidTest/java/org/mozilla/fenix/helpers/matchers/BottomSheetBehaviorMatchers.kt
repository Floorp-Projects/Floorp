/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers.matchers

import android.view.View
import androidx.test.espresso.matcher.BoundedMatcher
import com.google.android.material.bottomsheet.BottomSheetBehavior
import org.hamcrest.Description

class BottomSheetBehaviorStateMatcher(private val expectedState: Int) :
    BoundedMatcher<View, View>(View::class.java) {

    override fun describeTo(description: Description?) {
        description?.appendText("BottomSheetBehavior in state: \"$expectedState\"")
    }

    override fun matchesSafely(item: View): Boolean {
        val behavior = BottomSheetBehavior.from(item)
        return behavior.state == expectedState
    }
}

class BottomSheetBehaviorHalfExpandedMaxRatioMatcher(private val maxHalfExpandedRatio: Float) :
    BoundedMatcher<View, View>(View::class.java) {

    override fun describeTo(description: Description?) {
        description?.appendText(
            "BottomSheetBehavior with an at max halfExpandedRation: " +
                "$maxHalfExpandedRatio",
        )
    }

    override fun matchesSafely(item: View): Boolean {
        val behavior = BottomSheetBehavior.from(item)
        return behavior.halfExpandedRatio <= maxHalfExpandedRatio
    }
}
