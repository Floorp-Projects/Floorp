/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.facts

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.processor.CollectionProcessor
import org.junit.Assert.assertEquals
import org.junit.Test

class SyncedTabsFactsTest {

    @Test
    fun `Emits facts for current state`() {
        CollectionProcessor.withFactCollection { facts ->

            emitSyncedTabSuggestionClickedFact()

            assertEquals(1, facts.size)
            facts[0].apply {
                assertEquals(Component.FEATURE_SYNCEDTABS, component)
                assertEquals(Action.INTERACTION, action)
                assertEquals(SyncedTabsFacts.Items.SYNCED_TABS_SUGGESTION_CLICKED, item)
            }
        }
    }
}
