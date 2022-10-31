package mozilla.components.feature.top.sites.fact

import mozilla.components.feature.top.sites.facts.TopSitesFacts
import mozilla.components.feature.top.sites.facts.emitTopSitesCountFact
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.processor.CollectionProcessor
import org.junit.Assert.assertEquals
import org.junit.Test

class TopSitesFactsTest {

    @Test
    fun `Emits facts for current state`() {
        CollectionProcessor.withFactCollection { facts ->

            assertEquals(0, facts.size)

            emitTopSitesCountFact(5)

            assertEquals(1, facts.size)
            facts[0].apply {
                assertEquals(Component.FEATURE_TOP_SITES, component)
                assertEquals(Action.INTERACTION, action)
                assertEquals(TopSitesFacts.Items.COUNT, item)
                assertEquals(5, value?.toInt())
            }

            emitTopSitesCountFact(1)

            assertEquals(2, facts.size)
            facts[1].apply {
                assertEquals(Component.FEATURE_TOP_SITES, component)
                assertEquals(Action.INTERACTION, action)
                assertEquals(TopSitesFacts.Items.COUNT, item)
                assertEquals(1, value?.toInt())
            }
        }
    }
}
