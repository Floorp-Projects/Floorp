package org.mozilla.geckoview.test

import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
@MediumTest
class DisplayTest : BaseSessionTest() {

    @Test(expected = IllegalStateException::class)
    fun doubleAcquire() {
        val display = mainSession.acquireDisplay()
        assertThat("Display should not be null", display, notNullValue())
        try {
            mainSession.acquireDisplay()
        } finally {
            mainSession.releaseDisplay(display)
        }
    }
}