/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.messaging

import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction

class MessagingFeatureTest {
    @OptIn(ExperimentalCoroutinesApi::class)
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `WHEN start is called THEN evaluate messages`() {
        val appStore: AppStore = spyk(AppStore())
        val binding = MessagingFeature(appStore)

        binding.start()

        verify { appStore.dispatch(MessagingAction.Evaluate(FenixMessageSurfaceId.HOMESCREEN)) }
    }
}
