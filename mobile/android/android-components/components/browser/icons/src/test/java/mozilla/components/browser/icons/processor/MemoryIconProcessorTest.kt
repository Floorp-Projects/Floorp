/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.processor

import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class MemoryIconProcessorTest {
    @Test
    fun `Generated icons are not saved in cache`() {
        val icon = Icon(mock(), source = Icon.Source.GENERATOR)
        val cache: MemoryIconProcessor.ProcessorMemoryCache = mock()

        val processor = MemoryIconProcessor(cache)
        processor.process(mock(), mock(), mock(), icon, mock())

        verify(cache, never()).put(any(), any(), any())
    }

    @Test
    fun `Icon loaded from memory cache are not saved in cache`() {
        val icon = Icon(mock(), source = Icon.Source.MEMORY)
        val cache: MemoryIconProcessor.ProcessorMemoryCache = mock()

        val processor = MemoryIconProcessor(cache)
        processor.process(mock(), mock(), mock(), icon, mock())

        verify(cache, never()).put(any(), any(), any())
    }

    @Test
    fun `Icon loaded from disk cache are saved in cache`() {
        val icon = Icon(mock(), source = Icon.Source.DISK)
        val cache: MemoryIconProcessor.ProcessorMemoryCache = mock()

        val processor = MemoryIconProcessor(cache)
        processor.process(mock(), mock(), mock(), icon, mock())

        verify(cache).put(any(), any(), any())
    }

    @Test
    fun `Downloaded icon is saved in cache`() {
        val icon = Icon(mock(), source = Icon.Source.DOWNLOAD)
        val cache: MemoryIconProcessor.ProcessorMemoryCache = mock()

        val processor = MemoryIconProcessor(cache)
        val request: IconRequest = mock()
        val resource: IconRequest.Resource = mock()
        processor.process(mock(), request, resource, icon, mock())

        verify(cache).put(request, resource, icon)
    }

    @Test
    fun `Inlined icon is saved in cache`() {
        val icon = Icon(mock(), source = Icon.Source.INLINE)
        val cache: MemoryIconProcessor.ProcessorMemoryCache = mock()

        val processor = MemoryIconProcessor(cache)
        val request: IconRequest = mock()
        val resource: IconRequest.Resource = mock()
        processor.process(mock(), request, resource, icon, mock())

        verify(cache).put(request, resource, icon)
    }

    @Test
    fun `Icon without resource is not saved in cache`() {
        val icon = Icon(mock(), source = Icon.Source.MEMORY)
        val cache: MemoryIconProcessor.ProcessorMemoryCache = mock()

        val processor = MemoryIconProcessor(cache)
        processor.process(context = mock(), request = mock(), resource = null, icon = icon, desiredSize = mock())

        verify(cache, never()).put(any(), any(), any())
    }
}
