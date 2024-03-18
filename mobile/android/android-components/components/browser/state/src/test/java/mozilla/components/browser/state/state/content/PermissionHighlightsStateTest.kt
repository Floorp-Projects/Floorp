/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state.content

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class PermissionHighlightsStateTest {

    @Test
    fun `WHEN a site has blocked both autoplay audible and inaudible THEN isAutoplayBlocking is true`() {
        val highlightsState = PermissionHighlightsState(
            autoPlayAudibleBlocking = true,
            autoPlayInaudibleBlocking = true,
        )

        assertTrue(highlightsState.isAutoPlayBlocking)
    }

    @Test
    fun `WHEN a site has blocked either autoplay audible or inaudible autoplay THEN isAutoplayBlocking is true`() {
        var highlightsState = PermissionHighlightsState(
            autoPlayAudibleBlocking = true,
            autoPlayInaudibleBlocking = false,
        )

        assertTrue(highlightsState.isAutoPlayBlocking)

        highlightsState = highlightsState.copy(
            autoPlayAudibleBlocking = false,
            autoPlayInaudibleBlocking = true,
        )

        assertTrue(highlightsState.isAutoPlayBlocking)

        highlightsState = highlightsState.copy(
            autoPlayAudibleBlocking = false,
            autoPlayInaudibleBlocking = false,
        )

        assertFalse(highlightsState.isAutoPlayBlocking)
    }

    @Test
    fun `WHEN all permissions has not changed from default value THEN permissionsChanged is false`() {
        val highlightsState = PermissionHighlightsState(
            notificationChanged = false,
            cameraChanged = false,
            locationChanged = false,
            microphoneChanged = false,
            persistentStorageChanged = false,
            mediaKeySystemAccessChanged = false,
            autoPlayAudibleChanged = false,
            autoPlayInaudibleChanged = false,
        )

        assertFalse(highlightsState.permissionsChanged)
    }

    @Test
    fun `WHEN some permissions has changed from default value THEN permissionsChanged is true`() {
        val highlightsState = PermissionHighlightsState(
            notificationChanged = false,
            cameraChanged = true,
            locationChanged = true,
            microphoneChanged = true,
            persistentStorageChanged = true,
            mediaKeySystemAccessChanged = true,
            autoPlayAudibleChanged = true,
            autoPlayInaudibleChanged = true,
        )

        assertTrue(highlightsState.permissionsChanged)
    }
}
