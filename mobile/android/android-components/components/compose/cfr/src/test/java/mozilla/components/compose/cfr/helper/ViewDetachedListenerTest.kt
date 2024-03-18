/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.cfr.helper

import mozilla.components.support.test.mock
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ViewDetachedListenerTest {
    @Test
    fun `WHEN the View is attached THEN don't inform the client`() {
        var wasCallbackCalled = false
        val listener = ViewDetachedListener { wasCallbackCalled = true }

        listener.onViewAttachedToWindow(mock())

        assertFalse(wasCallbackCalled)
    }

    @Test
    fun `WHEN the View is detached THEN don't inform the client`() {
        var wasCallbackCalled = false
        val listener = ViewDetachedListener { wasCallbackCalled = true }

        listener.onViewDetachedFromWindow(mock())

        assertTrue(wasCallbackCalled)
    }
}
