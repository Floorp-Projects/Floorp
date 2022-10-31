/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.view

import android.graphics.drawable.Drawable
import android.widget.EditText
import android.widget.TextView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertArrayEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class TextViewTest {

    @Test
    fun `putCompoundDrawablesRelative defaults to all null`() {
        val view = TextView(testContext)

        view.putCompoundDrawablesRelative()

        assertArrayEquals(
            arrayOf<Drawable?>(null, null, null, null),
            view.compoundDrawablesRelative,
        )
    }

    @Test
    fun `putCompoundDrawablesRelativeWithIntrinsicBounds defaults to all null`() {
        val view = TextView(testContext)

        view.putCompoundDrawablesRelativeWithIntrinsicBounds()

        assertArrayEquals(
            arrayOf<Drawable?>(null, null, null, null),
            view.compoundDrawablesRelative,
        )
    }

    @Test
    fun `putCompoundDrawablesRelative should set drawableStart and drawableEnd`() {
        val view = EditText(testContext)
        val drawable: Drawable = mock()

        view.putCompoundDrawablesRelative(start = drawable)

        assertArrayEquals(
            arrayOf(drawable, null, null, null),
            view.compoundDrawablesRelative,
        )

        view.putCompoundDrawablesRelative(end = drawable)

        assertArrayEquals(
            arrayOf(null, null, drawable, null),
            view.compoundDrawablesRelative,
        )
    }

    @Test
    fun `putCompoundDrawablesRelativeWithIntrinsicBounds should set drawableStart and drawableEnd`() {
        val view = EditText(testContext)
        val drawable: Drawable = mock()

        view.putCompoundDrawablesRelativeWithIntrinsicBounds(start = drawable)

        assertArrayEquals(
            arrayOf(drawable, null, null, null),
            view.compoundDrawablesRelative,
        )

        view.putCompoundDrawablesRelativeWithIntrinsicBounds(end = drawable)

        assertArrayEquals(
            arrayOf(null, null, drawable, null),
            view.compoundDrawablesRelative,
        )
    }

    @Test
    fun `putCompoundDrawablesRelative should call setCompoundDrawablesRelative`() {
        val view: TextView = mock()
        val drawable: Drawable = mock()

        view.putCompoundDrawablesRelative(top = drawable)

        verify(view).setCompoundDrawablesRelative(null, drawable, null, null)

        view.putCompoundDrawablesRelativeWithIntrinsicBounds(bottom = drawable)

        verify(view).setCompoundDrawablesRelativeWithIntrinsicBounds(null, null, null, drawable)
    }
}
