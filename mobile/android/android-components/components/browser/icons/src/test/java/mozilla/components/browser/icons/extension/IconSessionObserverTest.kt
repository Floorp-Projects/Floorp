/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.extension

import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class IconSessionObserverTest {
    @Test
    fun `Starting a load will register a message handler`() {
        val extension: WebExtension = mock()
        `when`(extension.hasContentMessageHandler(any(), any())).thenReturn(false)

        val observer = IconSessionObserver(icons = mock(), sessionManager = mock(), extension = extension)
        observer.onLoadingStateChanged(session = mock(), loading = true)

        verify(extension).registerContentMessageHandler(any(), eq("MozacBrowserIcons"), any())
    }

    @Test
    fun `Stoping a load will not register a message handler`() {
        val extension: WebExtension = mock()
        `when`(extension.hasContentMessageHandler(any(), any())).thenReturn(false)

        val observer = IconSessionObserver(icons = mock(), sessionManager = mock(), extension = extension)
        observer.onLoadingStateChanged(session = mock(), loading = false)

        verify(extension, never()).registerContentMessageHandler(any(), any(), any())
    }

    @Test
    fun `Message handler will not be registered twice`() {
        val extension: WebExtension = mock()
        `when`(extension.hasContentMessageHandler(any(), any())).thenReturn(true)

        val observer = IconSessionObserver(icons = mock(), sessionManager = mock(), extension = extension)
        observer.onLoadingStateChanged(session = mock(), loading = true)

        verify(extension, never()).registerContentMessageHandler(any(), eq("MozacBrowserIcons"), any())
    }
}
