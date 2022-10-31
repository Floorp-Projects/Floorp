/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.view

import android.view.MotionEvent
import android.view.MotionEvent.ACTION_DOWN
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class MotionEventKtTest {

    private lateinit var subject: MotionEvent
    private lateinit var subjectSpy: MotionEvent

    @Before
    fun setUp() {
        subject = MotionEvent.obtain(100, 100, ACTION_DOWN, 0f, 0f, 0)
        subjectSpy = spy(subject)
    }

    @Test
    fun `WHEN use is called without an exception THEN the object is recycled`() {
        subjectSpy.use {}
        verify(subjectSpy, times(1)).recycle()
    }

    @Test
    fun `WHEN use is called with an exception THEN the object is recycled`() {
        try { subjectSpy.use { throw IllegalStateException("Catch me!") } } catch (e: Exception) { /* Do nothing */ }
        verify(subjectSpy, times(1)).recycle()
    }

    @Test
    fun `WHEN use is called and its function returns a value THEN that value is returned`() {
        val expected = 47
        assertEquals(expected, subject.use { expected })
    }

    @Test(expected = IllegalStateException::class)
    fun `WHEN use is called and its function throws an exception THEN that exception is thrown`() {
        subject.use { throw IllegalStateException() }
    }

    @Test
    fun `WHEN use is called THEN the use function's argument is the use receiver`() {
        subject.use { assertEquals(subject, it) }
    }
}
