/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package mozilla.components.ui.progress

import android.view.View
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class AnimatedProgressBarTest {

    @Test
    fun setProgress() {
        val progressBar = AnimatedProgressBar(testContext)

        progressBar.progress = -1
        assertEquals(0, progressBar.progress)

        progressBar.progress = 50
        assertEquals(50, progressBar.progress)

        progressBar.progress = 100
        assertEquals(100, progressBar.progress)

        progressBar.progress = 101
        assertEquals(100, progressBar.progress)
    }

    @Test
    fun setVisibility() {
        val progressBar = spy(AnimatedProgressBar(testContext))

        progressBar.visibility = View.GONE
        verify(progressBar, never()).animateClosing()
        assertEquals(View.GONE, progressBar.visibility)

        progressBar.progress = progressBar.max
        progressBar.visibility = View.GONE
        verify(progressBar).animateClosing()
        assertEquals(View.GONE, progressBar.visibility)

        progressBar.visibility = View.VISIBLE
        assertEquals(View.VISIBLE, progressBar.visibility)
    }
}
