package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import org.hamcrest.Matchers.* // ktlint-disable no-wildcard-imports
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
