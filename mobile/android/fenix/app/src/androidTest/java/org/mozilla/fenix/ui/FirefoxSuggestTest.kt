/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.AppAndSystemHelper
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying the Firefox suggest search fragment
 *
 */

class FirefoxSuggestTest {
    @get:Rule
    val activityTestRule = AndroidComposeTestRule(
        HomeActivityTestRule(
            skipOnboarding = true,
            isPocketEnabled = false,
            isJumpBackInCFREnabled = false,
            isRecentTabsFeatureEnabled = false,
            isTCPCFREnabled = false,
            isWallpaperOnboardingEnabled = false,
            tabsTrayRewriteEnabled = false,
        ),
    ) { it.activity }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2348361
    @SmokeTest
    @Test
    fun verifyFirefoxSuggestSponsoredSearchResultsTest() {
        AppAndSystemHelper.runWithCondition(TestHelper.appContext.settings().enableFxSuggest) {
            navigationToolbar {
            }.clickUrlbar {
                typeSearch(searchTerm = "Amazon")
                verifySearchEngineSuggestionResults(
                    rule = activityTestRule,
                    searchSuggestions = arrayOf(
                        "Firefox Suggest",
                        "Amazon.com - Official Site",
                        "Sponsored",
                    ),
                    searchTerm = "Amazon",
                )
            }
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2348362
    @Test
    fun verifyFirefoxSuggestSponsoredSearchResultsWithPartialKeywordTest() {
        AppAndSystemHelper.runWithCondition(TestHelper.appContext.settings().enableFxSuggest) {
            navigationToolbar {
            }.clickUrlbar {
                typeSearch(searchTerm = "Amaz")
                verifySearchEngineSuggestionResults(
                    rule = activityTestRule,
                    searchSuggestions = arrayOf(
                        "Firefox Suggest",
                        "Amazon.com - Official Site",
                        "Sponsored",
                    ),
                    searchTerm = "Amaz",
                )
            }
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2348363
    @Test
    fun openFirefoxSuggestSponsoredSearchResultsTest() {
        AppAndSystemHelper.runWithCondition(TestHelper.appContext.settings().enableFxSuggest) {
            navigationToolbar {
            }.clickUrlbar {
                typeSearch(searchTerm = "Amazon")
                verifySearchEngineSuggestionResults(
                    rule = activityTestRule,
                    searchSuggestions = arrayOf(
                        "Firefox Suggest",
                        "Amazon.com - Official Site",
                        "Sponsored",
                    ),
                    searchTerm = "Amazon",
                )
            }.clickSearchSuggestion("Amazon.com - Official Site") {
                waitForPageToLoad()
                verifyUrl(
                    "amazon.com/?tag=admarketus-20&ref=pd_sl_924ab4435c5a5c23aa2804307ee0669ab36f88caee841ce51d1f2ecb&mfadid=adm",
                )
                verifyTabCounter("1")
            }
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2348369
    @Test
    fun verifyFirefoxSuggestSponsoredSearchResultsWithEditedKeywordTest() {
        AppAndSystemHelper.runWithCondition(TestHelper.appContext.settings().enableFxSuggest) {
            navigationToolbar {
            }.clickUrlbar {
                typeSearch(searchTerm = "Amazon")
                deleteSearchKeywordCharacters(numberOfDeletionSteps = 3)
                verifySearchEngineSuggestionResults(
                    rule = activityTestRule,
                    searchSuggestions = arrayOf(
                        "Firefox Suggest",
                        "Amazon.com - Official Site",
                        "Sponsored",
                    ),
                    searchTerm = "Amazon",
                    shouldEditKeyword = true,
                    numberOfDeletionSteps = 3,
                )
            }
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2348374
    @SmokeTest
    @Test
    fun verifyFirefoxSuggestNonSponsoredSearchResultsTest() {
        AppAndSystemHelper.runWithCondition(TestHelper.appContext.settings().enableFxSuggest) {
            navigationToolbar {
            }.clickUrlbar {
                typeSearch(searchTerm = "Marvel")
                verifySearchEngineSuggestionResults(
                    rule = activityTestRule,
                    searchSuggestions = arrayOf(
                        "Firefox Suggest",
                        "Wikipedia - Marvel Cinematic Universe",
                    ),
                    searchTerm = "Marvel",
                )
                verifySuggestionsAreNotDisplayed(
                    rule = activityTestRule,
                    searchSuggestions = arrayOf(
                        "Sponsored",
                    ),
                )
            }
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2348375
    @Test
    fun verifyFirefoxSuggestNonSponsoredSearchResultsWithPartialKeywordTest() {
        AppAndSystemHelper.runWithCondition(TestHelper.appContext.settings().enableFxSuggest) {
            navigationToolbar {
            }.clickUrlbar {
                typeSearch(searchTerm = "Marv")
                verifySearchEngineSuggestionResults(
                    rule = activityTestRule,
                    searchSuggestions = arrayOf(
                        "Firefox Suggest",
                        "Wikipedia - Marvel Cinematic Universe",
                    ),
                    searchTerm = "Marv",
                )
            }
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2348376
    @Test
    fun openFirefoxSuggestNonSponsoredSearchResultsTest() {
        AppAndSystemHelper.runWithCondition(TestHelper.appContext.settings().enableFxSuggest) {
            navigationToolbar {
            }.clickUrlbar {
                typeSearch(searchTerm = "Marvel")
                verifySearchEngineSuggestionResults(
                    rule = activityTestRule,
                    searchSuggestions = arrayOf(
                        "Firefox Suggest",
                        "Wikipedia - Marvel Cinematic Universe",
                    ),
                    searchTerm = "Marvel",
                )
            }.clickSearchSuggestion("Wikipedia - Marvel Cinematic Universe") {
                waitForPageToLoad()
                verifyUrl(
                    "wikipedia.org/wiki/Marvel_Cinematic_Universe",
                )
            }
        }
    }
}
