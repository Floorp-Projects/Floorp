/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers.matchers

import android.graphics.Bitmap
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.Drawable
import android.graphics.drawable.StateListDrawable
import android.view.View
import android.widget.ImageView
import androidx.test.espresso.matcher.BoundedMatcher
import org.hamcrest.Description

class BitmapDrawableMatcher(private val bitmap: Bitmap, private val name: String) :
    BoundedMatcher<View, ImageView>(ImageView::class.java) {

    override fun describeTo(description: Description?) {
        description?.appendText("has image drawable resource $name")
    }

    override fun matchesSafely(item: ImageView): Boolean {
        return sameBitmap(item.drawable, bitmap)
    }

    private fun sameBitmap(drawable: Drawable?, otherBitmap: Bitmap): Boolean {
        var currentDrawable = drawable ?: return false

        if (currentDrawable is StateListDrawable) {
            currentDrawable = currentDrawable.current
        }
        if (currentDrawable is BitmapDrawable) {
            val bitmap = currentDrawable.bitmap
            return bitmap.sameAs(otherBitmap)
        }
        return false
    }
}
