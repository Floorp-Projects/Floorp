/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.search.provider.AssetsSearchEngineProvider
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.mozilla.fenix.components.metrics.Event.PerformedSearch
import org.mozilla.fenix.components.metrics.Event.PerformedSearch.EngineSource
import org.mozilla.fenix.components.metrics.Event.PerformedSearch.EventSource
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.TestApplication
import org.mozilla.fenix.components.FenixLocaleSearchLocalizationProvider
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@ObsoleteCoroutinesApi
@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
class PerformedSearchTest {
    private lateinit var searchEngines: List<SearchEngine>

    // Match against the Regex defined at
    // https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/private/LabeledMetricType.kt#L43
    // We're temporarily using it until better Glean testing APIs are available.
    private val countLabelRegex = Regex("^[a-z_][a-z0-9_-]{0,29}(\\.[a-z0-9_-]{0,29})*$")

    @Before
    fun setUp() {
        searchEngines = SearchEngineManager(listOf(provider)).getSearchEngines(testContext)
    }

    @Test
    fun testThatCountLabelIsValid() {
        val labels = searchEngines.map {
            PerformedSearch(EventSource.Action(EngineSource.Shortcut(it))).eventSource.countLabel
        }

        labels.forEach {
            assertTrue("$it does not match!", it.matches(countLabelRegex))
        }
    }

    private val provider = AssetsSearchEngineProvider(
        localizationProvider = FenixLocaleSearchLocalizationProvider(),
        additionalIdentifiers = listOf(
            "amazon-au",
            "amazon-br",
            "amazon-ca",
            "amazon-co-uk",
            "amazon-de",
            "amazon-fr",
            "amazon-in",
            "amazon-it",
            "amazon-jp",
            "amazon-mx",
            "amazon-nl",
            "amazondotcom",
            "azerdict",
            "azet-sk",
            "baidu",
            "bing",
            "bolcom-fy-NL",
            "bolcom-nl",
            "ceneje",
            "coccoc",
            "danawa-kr",
            "daum-kr",
            "ddg",
            "diec2",
            "drae",
            "duckduckgo",
            "elebila",
            "faclair-beag",
            "google-2018",
            "google-b-1-m",
            "google-b-m",
            "google",
            "gulesider-mobile-NO",
            "heureka-cz",
            "hotline-ua",
            "leit-is",
            "leo_ende_de",
            "list-am",
            "mapy-cz",
            "mercadolibre-ar",
            "mercadolibre-cl",
            "mercadolibre-mx",
            "naver-kr",
            "odpiralni",
            "pazaruvaj",
            "pledarigrond",
            "prisjakt-sv-SE",
            "qwant",
            "rediff",
            "reta-vortaro",
            "salidzinilv",
            "seznam-cz",
            "skroutz",
            "slovnik-sk",
            "sslv",
            "sztaki-en-hu",
            "taobao",
            "tearma",
            "twitter-ja",
            "twitter",
            "vatera",
            "wikipedia-NN",
            "wikipedia-NO",
            "wikipedia-an",
            "wikipedia-ar",
            "wikipedia-as",
            "wikipedia-ast",
            "wikipedia-az",
            "wikipedia-be",
            "wikipedia-bg",
            "wikipedia-bn",
            "wikipedia-br",
            "wikipedia-bs",
            "wikipedia-ca",
            "wikipedia-cy",
            "wikipedia-cz",
            "wikipedia-da",
            "wikipedia-de",
            "wikipedia-dsb",
            "wikipedia-el",
            "wikipedia-eo",
            "wikipedia-es",
            "wikipedia-et",
            "wikipedia-eu",
            "wikipedia-fa",
            "wikipedia-fi",
            "wikipedia-fr",
            "wikipedia-fy-NL",
            "wikipedia-ga-IE",
            "wikipedia-gd",
            "wikipedia-gl",
            "wikipedia-gn",
            "wikipedia-gu",
            "wikipedia-he",
            "wikipedia-hi",
            "wikipedia-hr",
            "wikipedia-hsb",
            "wikipedia-hu",
            "wikipedia-hy-AM",
            "wikipedia-ia",
            "wikipedia-id",
            "wikipedia-is",
            "wikipedia-it",
            "wikipedia-ja",
            "wikipedia-ka",
            "wikipedia-kab",
            "wikipedia-kk",
            "wikipedia-km",
            "wikipedia-kn",
            "wikipedia-lij",
            "wikipedia-lo",
            "wikipedia-lt",
            "wikipedia-ltg",
            "wikipedia-lv",
            "wikipedia-ml",
            "wikipedia-mr",
            "wikipedia-ms",
            "wikipedia-my",
            "wikipedia-ne",
            "wikipedia-nl",
            "wikipedia-oc",
            "wikipedia-or",
            "wikipedia-pa",
            "wikipedia-pl",
            "wikipedia-pt",
            "wikipedia-rm",
            "wikipedia-ro",
            "wikipedia-ru",
            "wikipedia-sk",
            "wikipedia-sl",
            "wikipedia-sq",
            "wikipedia-sr",
            "wikipedia-sv-SE",
            "wikipedia-ta",
            "wikipedia-te",
            "wikipedia-th",
            "wikipedia-tr",
            "wikipedia-uk",
            "wikipedia-ur",
            "wikipedia-uz",
            "wikipedia-vi",
            "wikipedia-wo",
            "wikipedia-zh-CN",
            "wikipedia-zh-TW",
            "wikipedia",
            "wiktionary-kn",
            "wiktionary-oc",
            "wiktionary-or",
            "wiktionary-ta",
            "wiktionary-te",
            "yahoo-jp",
            "yandex-en",
            "yandex-ru",
            "yandex-tr",
            "yandex.by",
            "yandex"
        )
    )
}