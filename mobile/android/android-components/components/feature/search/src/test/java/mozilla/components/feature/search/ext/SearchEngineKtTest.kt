/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.ext

import android.graphics.Bitmap
import android.net.Uri
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.SearchState
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import java.util.UUID

@RunWith(AndroidJUnit4::class)
class SearchEngineKtTest {

    @Test
    fun `WHEN search engine is created THEN the correct properties are set`() {
        val name = "name"
        val url = "https://www.example.com/search?q={searchTerms}"
        val icon: Bitmap = mock()
        val suggestUrl = "https://www.example.com/search"
        val isGeneral = true
        val searchEngine = createSearchEngine(
            name = name,
            url = url,
            icon = icon,
            suggestUrl = suggestUrl,
            isGeneral = isGeneral,
        )

        assertNotNull(searchEngine.id)
        assertEquals(name, searchEngine.name)
        assertEquals(icon, searchEngine.icon)
        assertEquals(SearchEngine.Type.CUSTOM, searchEngine.type)
        assertEquals(listOf(url), searchEngine.resultUrls)
        assertEquals(suggestUrl, searchEngine.suggestUrl)
        assertEquals(isGeneral, searchEngine.isGeneral)
    }

    @Test
    fun `Create search URL for startpage`() {
        val searchEngine = SearchEngine(
            id = UUID.randomUUID().toString(),
            name = "Escosia",
            icon = mock(),
            type = SearchEngine.Type.CUSTOM,
            resultUrls = listOf(
                "https://www.startpage.com/sp/search?q={searchTerms}",
            ),
        )

        assertEquals(
            "https://www.startpage.com/sp/search?q=Hello%20World",
            searchEngine.buildSearchUrl("Hello World"),
        )
    }

    @Test
    fun `Create search URL for ecosia`() {
        val searchEngine = createSearchEngine(
            name = "Ecosia",
            icon = mock(),
            url = "https://www.ecosia.org/search?q={searchTerms}",
        )

        assertEquals(
            "https://www.ecosia.org/search?q=Hello%20World",
            searchEngine.buildSearchUrl("Hello World"),
        )
    }

    @Test
    fun `GIVEN ecosia search engine and a set of urls THEN search terms are determined when present`() {
        val searchEngine = createSearchEngine(
            name = "Ecosia",
            icon = mock(),
            url = "https://www.ecosia.org/search?q={searchTerms}",
        )

        assertNull(searchEngine.parseSearchTerms(Uri.parse("https://www.ecosia.org/search?q=")))
        assertNull(searchEngine.parseSearchTerms(Uri.parse("https://www.ecosia.org/search?attr=moz-test")))

        assertEquals(
            "second test search",
            searchEngine.parseSearchTerms(Uri.parse("https://www.ecosia.org/search?q=second%20test%20search")),
        )

        assertEquals(
            "Another test",
            searchEngine.parseSearchTerms(Uri.parse("https://www.ecosia.org/search?r=134s7&attr=moz-test&q=Another%20test&d=136697676793")),
        )
    }

    @Test
    fun `GIVEN empty search state THEN search terms are never determined`() {
        val searchState = SearchState()
        assertNull(searchState.parseSearchTerms("https://google.com/search/?q=the%20sandbaggers"))
    }

    @Test
    fun `GIVEN a search state and a set of urls THEN search terms are determined when present`() {
        val google = createSearchEngine(
            name = "Google",
            icon = mock(),
            url = "https://google.com/search/?q={searchTerms}",
        )
        val ecosia = createSearchEngine(
            name = "Ecosia",
            icon = mock(),
            url = "https://www.ecosia.org/search?q={searchTerms}",
        )
        val baidu = createSearchEngine(
            name = "Baidu",
            icon = mock(),
            url = "https://www.baidu.com/s?wd={searchTerms}",
        )
        val searchState = SearchState(
            regionSearchEngines = listOf(google, baidu),
            additionalSearchEngines = listOf(ecosia),
            customSearchEngines = listOf(baidu, ecosia),
        )

        assertNull(searchState.parseSearchTerms("https://www.ecosia.org/search?q="))
        assertNull(searchState.parseSearchTerms("http://help.baidu.com/"))
        assertEquals(
            "神舟十二号载人飞行任务标识发布",
            searchState.parseSearchTerms("https://www.baidu.com/s?cl=3&tn=baidutop10&fr=top1000&wd=%E7%A5%9E%E8%88%9F%E5%8D%81%E4%BA%8C%E5%8F%B7%E8%BD%BD%E4%BA%BA%E9%A3%9E%E8%A1%8C%E4%BB%BB%E5%8A%A1%E6%A0%87%E8%AF%86%E5%8F%91%E5%B8%83&rsv_idx=2&rsv_dl=fyb_n_homepage&hisfilter=1"),
        )
        assertEquals(
            "the sandbaggers",
            searchState.parseSearchTerms("https://google.com/search/?q=the%20sandbaggers"),
        )
        assertEquals(
            "фаерфокс",
            searchState.parseSearchTerms("https://google.com/search/?q=%D1%84%D0%B0%D0%B5%D1%80%D1%84%D0%BE%D0%BA%D1%81"),
        )
        assertEquals(
            "Another test",
            searchState.parseSearchTerms("https://www.ecosia.org/search?r=134s7&attr=moz-test&q=Another%20test&d=136697676793"),
        )
    }

    @Test
    fun `GIVEN search engine parameter can not be found THEN search terms are never determined`() {
        val invalidEngine = SearchEngine(
            id = UUID.randomUUID().toString(),
            name = "invalid",
            icon = mock(),
            type = SearchEngine.Type.CUSTOM,
            resultUrls = listOf("https://mozilla.org/search/?q={invalid}"),
        )

        val searchState = SearchState(
            regionSearchEngines = listOf(invalidEngine),
        )

        assertNull(searchState.parseSearchTerms("https://mozilla.org/search/?q=test"))
    }

    @Test
    fun `GIVEN a search state and a set of input encoding THEN search terms are encoded by input encoding parameter`() {
        val searchEngine = createSearchEngine(
            name = "Yahoo! Auctions",
            icon = mock(),
            url = "https://auctions.yahoo.co.jp/search/search&p={searchTerms}",
            inputEncoding = "EUC-JP",
        )

        assertEquals(
            "https://auctions.yahoo.co.jp/search/search&p=%A5%D5%A5%A1%A5%A4%A5%E4%A1%BC%A5%D5%A5%A9%A5%C3%A5%AF%A5%B9",
            searchEngine.buildSearchUrl("ファイヤーフォックス"),
        )
    }

    @Test
    fun `GIVEN invalid input encoding THEN encoding of search terms are determined as UTF-8`() {
        val searchEngine = createSearchEngine(
            name = "name",
            icon = mock(),
            url = "https://www.example.com/search?q={searchTerms}",
            inputEncoding = "INVALID-ENOCODING",
        )

        assertEquals(
            "https://www.example.com/search?q=%E7%81%AB%E7%8B%90",
            searchEngine.buildSearchUrl("火狐"),
        )
    }
}
