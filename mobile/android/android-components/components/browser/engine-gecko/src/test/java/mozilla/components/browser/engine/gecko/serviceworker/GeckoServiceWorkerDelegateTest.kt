/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.serviceworker

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.serviceworker.ServiceWorkerDelegate
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn

@RunWith(AndroidJUnit4::class)
class GeckoServiceWorkerDelegateTest() {
    @Test
    fun `GIVEN a delegate to add tabs WHEN it added a new tab for the request to open a new window THEN return a the new closed session`() {
        val delegate = mock<ServiceWorkerDelegate>()
        doReturn(true).`when`(delegate).addNewTab(any())
        val geckoDelegate = GeckoServiceWorkerDelegate(delegate, mock(), mock())

        val result = geckoDelegate.onOpenWindow("").poll(1)

        assertFalse(result!!.isOpen)
    }

    @Test
    fun `GIVEN a delegate to add tabs WHEN it disn't add a new tab for the request to open a new window THEN return null`() {
        val delegate = mock<ServiceWorkerDelegate>()
        doReturn(false).`when`(delegate).addNewTab(any())
        val geckoDelegate = GeckoServiceWorkerDelegate(delegate, mock(), mock())

        val result = geckoDelegate.onOpenWindow("").poll(1)

        assertNull(result)
    }
}
