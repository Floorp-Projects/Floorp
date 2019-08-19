/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.toolbar

import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.verify

class ToolbarFeatureTest {

    @Test
    fun `when app is backgrounded, toolbar onStop method is called`() {
        val toolbarFeature = ToolbarFeature(
                toolbar = mock(), sessionManager = mock(), loadUrlUseCase = mock())

        toolbarFeature.stop()

        verify(toolbarFeature.toolbar).onStop()
    }
}