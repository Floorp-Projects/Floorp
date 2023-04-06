/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.view.View
import android.widget.ImageButton
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.verify
import org.junit.Before
import org.junit.Test

class ImageButtonTest {
    private val imageButton: ImageButton = mockk()

    @Before
    fun setup() {
        every { imageButton.visibility = any() } just Runs
        every { imageButton.isEnabled = any() } just Runs
    }

    @Test
    fun `Hide and disable`() {
        imageButton.hideAndDisable()

        verify { imageButton.isEnabled = false }
        verify { imageButton.visibility = View.INVISIBLE }
    }

    @Test
    fun `Show and enable`() {
        imageButton.showAndEnable()

        verify { imageButton.isEnabled = true }
        verify { imageButton.visibility = View.VISIBLE }
    }

    @Test
    fun `Remove and disable`() {
        imageButton.removeAndDisable()

        verify { imageButton.isEnabled = false }
        verify { imageButton.visibility = View.GONE }
    }
}
