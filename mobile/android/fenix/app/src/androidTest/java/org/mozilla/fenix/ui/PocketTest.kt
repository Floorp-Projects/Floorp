package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.Constants
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.RetryTestRule
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.ui.robots.homeScreen

/**
 *  Tests for verifying the presence of the Pocket section and its elements
 */

class PocketTest : TestSetup() {
    private lateinit var firstPocketStoryPublisher: String

    @get:Rule(order = 0)
    val activityTestRule =
        AndroidComposeTestRule(HomeActivityTestRule.withDefaultSettingsOverrides()) { it.activity }

    @Rule(order = 1)
    @JvmField
    val retryTestRule = RetryTestRule(3)

    @Before
    override fun setUp() {
        super.setUp()
        // Workaround to make sure the Pocket articles are populated before starting the tests.
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.goBack {}
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2252509
    @Test
    fun verifyPocketSectionTest() {
        activityTestRule.activityRule.applySettingsExceptions {
            it.isRecentTabsFeatureEnabled = false
            it.isRecentlyVisitedFeatureEnabled = false
        }

        homeScreen {
            verifyThoughtProvokingStories(true)
            scrollToPocketProvokingStories()
            verifyPocketRecommendedStoriesItems()
            // Sponsored Pocket stories are only advertised for a limited time.
            // See also known issue https://bugzilla.mozilla.org/show_bug.cgi?id=1828629
            // verifyPocketSponsoredStoriesItems(2, 8)
            verifyDiscoverMoreStoriesButton()
            verifyStoriesByTopic(true)
            verifyPoweredByPocket()
        }.openThreeDotMenu {
        }.openCustomizeHome {
            clickPocketButton()
        }.goBackToHomeScreen {
            verifyThoughtProvokingStories(false)
            verifyStoriesByTopic(false)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2252513
    @Test
    fun openPocketStoryItemTest() {
        activityTestRule.activityRule.applySettingsExceptions {
            it.isRecentTabsFeatureEnabled = false
            it.isRecentlyVisitedFeatureEnabled = false
        }

        homeScreen {
            verifyThoughtProvokingStories(true)
            scrollToPocketProvokingStories()
            firstPocketStoryPublisher = getProvokingStoryPublisher(1)
        }.clickPocketStoryItem(firstPocketStoryPublisher, 1) {
            verifyUrl(Constants.POCKET_RECOMMENDED_STORIES_UTM_PARAM)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2252514
    @Test
    fun pocketDiscoverMoreButtonTest() {
        activityTestRule.activityRule.applySettingsExceptions {
            it.isRecentTabsFeatureEnabled = false
            it.isRecentlyVisitedFeatureEnabled = false
        }

        homeScreen {
            scrollToPocketProvokingStories()
            verifyDiscoverMoreStoriesButton()
        }.clickPocketDiscoverMoreButton {
            verifyUrl("getpocket.com/explore")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2252515
    @Test
    fun selectPocketStoriesByTopicTest() {
        activityTestRule.activityRule.applySettingsExceptions {
            it.isRecentTabsFeatureEnabled = false
            it.isRecentlyVisitedFeatureEnabled = false
        }

        homeScreen {
            verifyStoriesByTopicItemState(activityTestRule, false, 1)
            clickStoriesByTopicItem(activityTestRule, 1)
            verifyStoriesByTopicItemState(activityTestRule, true, 1)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2252516
    @Test
    fun pocketLearnMoreButtonTest() {
        activityTestRule.activityRule.applySettingsExceptions {
            it.isRecentTabsFeatureEnabled = false
            it.isRecentlyVisitedFeatureEnabled = false
        }

        homeScreen {
            verifyPoweredByPocket()
        }.clickPocketLearnMoreLink(activityTestRule) {
            verifyUrl("mozilla.org/en-US/firefox/pocket")
        }
    }
}
