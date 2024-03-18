/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.service

import mozilla.components.support.test.mock
import org.junit.Assert.assertSame
import org.junit.Test

class MediaServiceBinderTest {
    @Test
    fun `GIVEN a constructed instance THEN the internal service is kept as a weak reference`() {
        val delegate: MediaSessionDelegate = mock()

        val binder = MediaServiceBinder(delegate)

        assertSame(delegate, binder.service.get())
    }

    @Test
    fun `GIVEN a constructed instance WHEN the media service is asked for THEN return the initially provided instance`() {
        val delegate: MediaSessionDelegate = mock()
        val binder = MediaServiceBinder(delegate)

        val result = binder.getMediaService()

        assertSame(delegate, result)
    }
}
