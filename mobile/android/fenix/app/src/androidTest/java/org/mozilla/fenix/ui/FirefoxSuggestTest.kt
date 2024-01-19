/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.AppAndSystemHelper.runWithCondition
import org.mozilla.fenix.helpers.DataGenerationHelper.getSponsoredFxSuggestPlaceHolder
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

    private val sponsoredKeyWords: Map<String, List<String>> =
        mapOf(
            "Amazon" to
                listOf(
                    "Amazon.com - Official Site",
                    "amazon.com/?tag=admarketus-20&ref=pd_sl_924ab4435c5a5c23aa2804307ee0669ab36f88caee841ce51d1f2ecb&mfadid=adm",
                ),
            "Nike" to
                listOf(
                    "Nike.com - Official Site",
                    "nike.com/?cp=16423867261_search_318370984us128${getSponsoredFxSuggestPlaceHolder()}&mfadid=adm",
                ),
            "Macy" to listOf(
                "macys.com - Official Site",
                "macys.com/?cm_mmc=Google_AdMarketPlace-_-Privacy_Instant%20Suggest-_-319101130_Broad-_-kclickid__kenshoo_clickid_&m_sc=sem&m_sb=Admarketplace&m_tp=Search&m_ac=Admarketplace&m_ag=Instant%20Suggest&m_cn=Privacy&m_pi=kclickid__kenshoo_clickid__319101130us1201${getSponsoredFxSuggestPlaceHolder()}&mfadid=adm",
            ),
            "Spanx" to listOf(
                "SPANX® -  Official Site",
                "spanx.com/?utm_source=admarketplace&utm_medium=cpc&utm_campaign=privacy&utm_content=319093361us1202${getSponsoredFxSuggestPlaceHolder()}&mfadid=adm",
            ),
            "Bloom" to listOf(
                "Bloomingdales.com - Official Site",
                "bloomingdales.com/?cm_mmc=Admarketplace-_-Privacy-_-Privacy-_-privacy%20instant%20suggest-_-319093353us1228${getSponsoredFxSuggestPlaceHolder()}-_-kclickid__kenshoo_clickid_&mfadid=adm",
            ),
            "Groupon" to listOf(
                "groupon.com - Discover & Save!",
                "groupon.com/?utm_source=google&utm_medium=cpc&utm_campaign=us_dt_sea_ggl_txt_smp_sr_cbp_ch1_nbr_k*{keyword}_m*{match-type}_d*ADMRKT_319093357us1279${getSponsoredFxSuggestPlaceHolder()}&mfadid=adm",
            ),
        )

    private val sponsoredKeyWord = sponsoredKeyWords.keys.random()

    private val nonSponsoredKeyWords: Map<String, List<String>> =
        mapOf(
            "Marvel" to
                listOf(
                    "Wikipedia - Marvel Cinematic Universe",
                    "wikipedia.org/wiki/Marvel_Cinematic_Universe",
                ),
            "Apple" to
                listOf(
                    "Wikipedia - Apple Inc.",
                    "wikipedia.org/wiki/Apple_Inc",
                ),
            "Africa" to listOf(
                "Wikipedia - African Union",
                "wikipedia.org/wiki/African_Union",
            ),
            "Ultimate" to listOf(
                "Wikipedia - Ultimate Fighting Championship",
                "wikipedia.org/wiki/Ultimate_Fighting_Championship",
            ),
            "Youtube" to listOf(
                "Wikipedia - YouTube",
                "wikipedia.org/wiki/YouTube",
            ),
            "Fifa" to listOf(
                "Wikipedia - FIFA World Cup",
                "en.m.wikipedia.org/wiki/FIFA_World_Cup",
            ),
        )

    private val nonSponsoredKeyWord = nonSponsoredKeyWords.keys.random()

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2348361
    @Ignore("Failing, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1874831")
    @SmokeTest
    @Test
    fun verifyFirefoxSuggestSponsoredSearchResultsTest() {
        runWithCondition(TestHelper.appContext.settings().enableFxSuggest) {
            navigationToolbar {
            }.clickUrlbar {
                typeSearch(searchTerm = sponsoredKeyWord)
                verifySearchEngineSuggestionResults(
                    rule = activityTestRule,
                    searchSuggestions = arrayOf(
                        "Firefox Suggest",
                        sponsoredKeyWords.getValue(sponsoredKeyWord)[0],
                        "Sponsored",
                    ),
                    searchTerm = sponsoredKeyWord,
                )
            }
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2348362
    @Ignore("Failing, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1874831")
    @Test
    fun verifyFirefoxSuggestSponsoredSearchResultsWithPartialKeywordTest() {
        runWithCondition(TestHelper.appContext.settings().enableFxSuggest) {
            navigationToolbar {
            }.clickUrlbar {
                typeSearch(searchTerm = sponsoredKeyWord.dropLast(1))
                verifySearchEngineSuggestionResults(
                    rule = activityTestRule,
                    searchSuggestions = arrayOf(
                        "Firefox Suggest",
                        sponsoredKeyWords.getValue(sponsoredKeyWord)[0],
                        "Sponsored",
                    ),
                    searchTerm = sponsoredKeyWord.dropLast(1),
                )
            }
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2348363
    @Ignore("Failing, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1874831")
    @Test
    fun openFirefoxSuggestSponsoredSearchResultsTest() {
        runWithCondition(TestHelper.appContext.settings().enableFxSuggest) {
            navigationToolbar {
            }.clickUrlbar {
                typeSearch(searchTerm = sponsoredKeyWord)
                verifySearchEngineSuggestionResults(
                    rule = activityTestRule,
                    searchSuggestions = arrayOf(
                        "Firefox Suggest",
                        sponsoredKeyWords.getValue(sponsoredKeyWord)[0],
                        "Sponsored",
                    ),
                    searchTerm = sponsoredKeyWord,
                )
            }.clickSearchSuggestion(sponsoredKeyWords.getValue(sponsoredKeyWord)[0]) {
                verifyUrl(sponsoredKeyWords.getValue(sponsoredKeyWord)[1])
                verifyTabCounter("1")
            }
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2348369
    @Ignore("Failing, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1874831")
    @Test
    fun verifyFirefoxSuggestSponsoredSearchResultsWithEditedKeywordTest() {
        runWithCondition(TestHelper.appContext.settings().enableFxSuggest) {
            navigationToolbar {
            }.clickUrlbar {
                typeSearch(searchTerm = sponsoredKeyWord)
                deleteSearchKeywordCharacters(numberOfDeletionSteps = 1)
                verifySearchEngineSuggestionResults(
                    rule = activityTestRule,
                    searchSuggestions = arrayOf(
                        "Firefox Suggest",
                        sponsoredKeyWords.getValue(sponsoredKeyWord)[0],
                        "Sponsored",
                    ),
                    searchTerm = sponsoredKeyWord,
                    shouldEditKeyword = true,
                    numberOfDeletionSteps = 1,
                )
            }
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2348374
    @SmokeTest
    @Test
    fun verifyFirefoxSuggestNonSponsoredSearchResultsTest() {
        runWithCondition(TestHelper.appContext.settings().enableFxSuggest) {
            navigationToolbar {
            }.clickUrlbar {
                typeSearch(searchTerm = nonSponsoredKeyWord)
                verifySearchEngineSuggestionResults(
                    rule = activityTestRule,
                    searchSuggestions = arrayOf(
                        "Firefox Suggest",
                        nonSponsoredKeyWords.getValue(nonSponsoredKeyWord)[0],
                    ),
                    searchTerm = nonSponsoredKeyWord,
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
        runWithCondition(TestHelper.appContext.settings().enableFxSuggest) {
            navigationToolbar {
            }.clickUrlbar {
                typeSearch(searchTerm = nonSponsoredKeyWord.dropLast(1))
                verifySearchEngineSuggestionResults(
                    rule = activityTestRule,
                    searchSuggestions = arrayOf(
                        "Firefox Suggest",
                        nonSponsoredKeyWords.getValue(nonSponsoredKeyWord)[0],
                    ),
                    searchTerm = nonSponsoredKeyWord.dropLast(1),
                )
            }
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2348376
    @Test
    fun openFirefoxSuggestNonSponsoredSearchResultsTest() {
        runWithCondition(TestHelper.appContext.settings().enableFxSuggest) {
            navigationToolbar {
            }.clickUrlbar {
                typeSearch(searchTerm = nonSponsoredKeyWord)
                verifySearchEngineSuggestionResults(
                    rule = activityTestRule,
                    searchSuggestions = arrayOf(
                        "Firefox Suggest",
                        nonSponsoredKeyWords.getValue(nonSponsoredKeyWord)[0],
                    ),
                    searchTerm = nonSponsoredKeyWord,
                )
            }.clickSearchSuggestion(nonSponsoredKeyWords.getValue(nonSponsoredKeyWord)[0]) {
                waitForPageToLoad()
                verifyUrl(nonSponsoredKeyWords.getValue(nonSponsoredKeyWord)[1])
            }
        }
    }
}
