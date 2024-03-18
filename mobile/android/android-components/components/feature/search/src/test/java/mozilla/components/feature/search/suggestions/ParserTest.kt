/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.suggestions

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ParserTest {

    @Test
    fun `can parse a response from Google`() {
        val json = "[\"firefox\",[\"firefox\",\"firefox for mac\",\"firefox quantum\",\"firefox update\",\"firefox esr\",\"firefox focus\",\"firefox addons\",\"firefox extensions\",\"firefox nightly\",\"firefox clear cache\"]]"

        val results = defaultResponseParser(json)
        val expectedResults = listOf("firefox", "firefox for mac", "firefox quantum", "firefox update", "firefox esr", "firefox focus", "firefox addons", "firefox extensions", "firefox nightly", "firefox clear cache")
        assertEquals(expectedResults, results)
    }

    @Test
    fun `can parse a response from Amazon`() {
        val json = "[\"firefox\",[\"firefox for fire tv\",\"firefox movie\",\"firefox app\",\"firefox books\",\"firefox glider\",\"firefox stick\",\"firefox for firestick\",\"firefox\",\"firefox books series\",\"firefox browser\"],[{\"nodes\":[{\"name\":\"Apps & Games\",\"alias\":\"mobile-apps\"}]},{},{},{},{},{},{},{},{},{}],[],\"344I6KZL0KU9N\"]"

        val results = defaultResponseParser(json)
        val expectedResults = listOf("firefox for fire tv", "firefox movie", "firefox app", "firefox books", "firefox glider", "firefox stick", "firefox for firestick", "firefox", "firefox books series", "firefox browser")
        assertEquals(expectedResults, results)
    }

    @Test
    fun `can parse a response from Azerdict`() {
        val json = "{\"query\":\"code\",\"suggestions\":[\"code\",\"codec\",\"codex\",\"codeine\",\"code of laws\"]}"

        val results = azerdictResponseParser(json)
        val expectedResults = listOf("code", "codec", "codex", "codeine", "code of laws")
        assertEquals(expectedResults, results)
    }

    @Test
    fun `can parse a response from Baidu`() {
        val json = "[\"firefox\",[\"firefox rocket\",\"firefox手机浏览器\",\"firefox是什么意思\",\"firefox focus\",\"firefox风哥\",\"firefox官网\",\"firefox吧\",\"firefox os\",\"firefox国际版\",\"firefox android\"]]"

        val results = defaultResponseParser(json)
        val expectedResults = listOf("firefox rocket", "firefox手机浏览器", "firefox是什么意思", "firefox focus", "firefox风哥", "firefox官网", "firefox吧", "firefox os", "firefox国际版", "firefox android")
        assertEquals(expectedResults, results)
    }

    @Test
    fun `can parse a response from Bing`() {
        val json = "[\"firefox\",[\"firefox\",\"firefox download\",\"firefox for windows 10\",\"firefox browser\",\"firefox quantum\",\"firefox esr\",\"firefox 64-bit\",\"firefox mozilla\",\"firefox nightly\",\"firefox update\",\"firefox install\",\"firefox focus\",\"firefox beta\",\"firefox developer edition\",\"firefox portable\",\"firefox add-ons\"]]"

        val results = defaultResponseParser(json)
        val expectedResults = listOf("firefox", "firefox download", "firefox for windows 10", "firefox browser", "firefox quantum", "firefox esr", "firefox 64-bit", "firefox mozilla", "firefox nightly", "firefox update", "firefox install", "firefox focus", "firefox beta", "firefox developer edition", "firefox portable", "firefox add-ons")
        assertEquals(expectedResults, results)
    }

    @Test
    fun `can parse a response from Coccoc`() {
        val json = "[\"firefox\",[\"firefox\",\"firefox tiếng việt\",\"firefox download\",\"firefox quantum\",\"firefox 51\",\"firefox 42\",\"firefox english\",\"firefox portable\",\"firefox 49\",\"firefox viet nam\"],[\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\"],[\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\"],{\"google:suggesttype\":[\"QUERY\",\"QUERY\",\"QUERY\",\"QUERY\",\"QUERY\",\"QUERY\",\"QUERY\",\"QUERY\",\"QUERY\",\"QUERY\"]}]"

        val results = defaultResponseParser(json)
        val expectedResults = listOf("firefox", "firefox tiếng việt", "firefox download", "firefox quantum", "firefox 51", "firefox 42", "firefox english", "firefox portable", "firefox 49", "firefox viet nam")
        assertEquals(expectedResults, results)
    }

    @Test
    fun `can parse a response from Daum`() {
        val json = "{\"q\":\"firefox\",\"rq\":\"firefox\",\"items\":[\"firefox\",\"firefox download\",\"firefox focus\",\"mozilla firefox\",\"firefox adobe flash\",\"firefox 삭제\",\"firefox 한글판\",\"firefox 즐겨찾기 가져오기\",\"firefox quantum\",\"firefox 57\",\"mozila firefox\",\"mozilla firefox download\",\"mozilla firefox 삭제\",\"Hacking Firefox\",\"Programming Firefox\"],\"r_items\":[]}"

        val results = daumResponseParser(json)
        val expectedResults = listOf("firefox", "firefox download", "firefox focus", "mozilla firefox", "firefox adobe flash", "firefox 삭제", "firefox 한글판", "firefox 즐겨찾기 가져오기", "firefox quantum", "firefox 57", "mozila firefox", "mozilla firefox download", "mozilla firefox 삭제", "Hacking Firefox", "Programming Firefox")
        assertEquals(expectedResults, results)
    }

    @Test
    fun `can parse a response from Duck Duck Go`() {
        val json = "[\"firefox\",[\"firefox\",\"firefox browser\",\"firefox.com\",\"firefox update\",\"firefox for mac\",\"firefox quantum\",\"firefox extensions\",\"firefox esr\",\"firefox clear cache\",\"firefox themes\"]]"

        val results = defaultResponseParser(json)
        val expectedResults = listOf("firefox", "firefox browser", "firefox.com", "firefox update", "firefox for mac", "firefox quantum", "firefox extensions", "firefox esr", "firefox clear cache", "firefox themes")
        assertEquals(expectedResults, results)
    }

    @Test
    fun `can parse a response from Naver`() {
        val json = "[\"firefox\",[\"firefox\",\"Mozilla Firefox\",\"firefox add-on to detect vulnerable websites\",\"firefox ak\",\"firefox as gaeilge\",\"firefox developer edition\",\"firefox down\",\"firefox for dummies\",\"firefox for mac\",\"firefox for mobile\"],[],[\"http://search.naver.com/search.naver?where=nexearch&sm=osp_sug&ie=utf8&query=firefox\",\"http://search.naver.com/search.naver?where=nexearch&sm=osp_sug&ie=utf8&query=Mozilla+Firefox\",\"http://search.naver.com/search.naver?where=nexearch&sm=osp_sug&ie=utf8&query=firefox+add-on+to+detect+vulnerable+websites\",\"http://search.naver.com/search.naver?where=nexearch&sm=osp_sug&ie=utf8&query=firefox+ak\",\"http://search.naver.com/search.naver?where=nexearch&sm=osp_sug&ie=utf8&query=firefox+as+gaeilge\",\"http://search.naver.com/search.naver?where=nexearch&sm=osp_sug&ie=utf8&query=firefox+developer+edition\",\"http://search.naver.com/search.naver?where=nexearch&sm=osp_sug&ie=utf8&query=firefox+down\",\"http://search.naver.com/search.naver?where=nexearch&sm=osp_sug&ie=utf8&query=firefox+for+dummies\",\"http://search.naver.com/search.naver?where=nexearch&sm=osp_sug&ie=utf8&query=firefox+for+mac\",\"http://search.naver.com/search.naver?where=nexearch&sm=osp_sug&ie=utf8&query=firefox+for+mobile\"]]"

        val results = defaultResponseParser(json)
        val expectedResults = listOf("firefox", "Mozilla Firefox", "firefox add-on to detect vulnerable websites", "firefox ak", "firefox as gaeilge", "firefox developer edition", "firefox down", "firefox for dummies", "firefox for mac", "firefox for mobile")
        assertEquals(expectedResults, results)
    }

    @Test
    fun `can parse a response from Prisjakt`() {
        val json = "[\"firefox\",[\"Firefox\",\"Firefox\",\"Mountain Equipment Firefox Pants (Herr)\",\"Mountain Equipment Firefox (Herr)\",\"Firefox (DE)\"],[\"L\\u00e4gsta pris: 149:-\",\"L\\u00e4gsta pris: 205:-\",\"L\\u00e4gsta pris: 1 559:-\",\"L\\u00e4gsta pris: 2 773:-\",\"L\\u00e4gsta pris: 198:-\"],[\"https:\\/\\/www.prisjakt.nu\\/produkt.php?p=886794\",\"https:\\/\\/www.prisjakt.nu\\/produkt.php?p=53272\",\"https:\\/\\/www.prisjakt.nu\\/produkt.php?p=3548472\",\"https:\\/\\/www.prisjakt.nu\\/produkt.php?p=1822581\",\"https:\\/\\/www.prisjakt.nu\\/produkt.php?p=3408551\"]]"

        val results = defaultResponseParser(json)
        val expectedResults = listOf("Firefox", "Firefox", "Mountain Equipment Firefox Pants (Herr)", "Mountain Equipment Firefox (Herr)", "Firefox (DE)")
        assertEquals(expectedResults, results)
    }

    @Test
    fun `can parse a response from Qwant`() {
        val json = "{\"status\":\"success\",\"data\":{\"items\":[{\"value\":\"firefox (video game)\",\"suggestType\":3},{\"value\":\"firefox addons\",\"suggestType\":12},{\"value\":\"firefox\",\"suggestType\":2},{\"value\":\"firefox quantum\",\"suggestType\":12},{\"value\":\"firefox focus\",\"suggestType\":12}],\"special\":[],\"availableQwick\":[]}}"

        val results = qwantResponseParser(json)
        val expectedResults = listOf("firefox (video game)", "firefox addons", "firefox", "firefox quantum", "firefox focus")
        assertEquals(expectedResults, results)
    }

    @Test
    fun `can parse a response from Seznam`() {
        val json = "[\"firefox\", [\"firefox\", \"firefox ak keep it up\", \"firefox ak city to city\", \"firefox ak all those people\", \"firefox ak who can act\", \"firefox ak color the trees\", \"firefox ak habibi\", \"firefox ak zodiac\", \"firefox ak the draft\", \"mozilla firefox\"], [], []]"

        val results = defaultResponseParser(json)
        val expectedResults = listOf("firefox", "firefox ak keep it up", "firefox ak city to city", "firefox ak all those people", "firefox ak who can act", "firefox ak color the trees", "firefox ak habibi", "firefox ak zodiac", "firefox ak the draft", "mozilla firefox")
        assertEquals(expectedResults, results)
    }

    @Test
    fun `can parse a response from Wikipedia`() {
        val json = "[\"code\",[\"Code\",\"Code Geass\",\"Codeine\",\"Codename: Kids Next Door\",\"Code page\",\"Codex Sinaiticus\",\"Code talker\",\"Code Black (TV series)\",\"Codependency\",\"Codex Vaticanus\"],[\"In communications and information processing, code is a system of rules to convert information\\u2014such as a letter, word, sound, image, or gesture\\u2014into another form or representation, sometimes shortened or secret, for communication through a communication channel or storage in a storage medium.\",\"Code Geass: Lelouch of the Rebellion (\\u30b3\\u30fc\\u30c9\\u30ae\\u30a2\\u30b9 \\u53cd\\u9006\\u306e\\u30eb\\u30eb\\u30fc\\u30b7\\u30e5, K\\u014ddo Giasu: Hangyaku no Rur\\u016bshu), often referred to as simply Code Geass, is a Japanese anime series created by Sunrise, directed by Gor\\u014d Taniguchi, and written by Ichir\\u014d \\u014ckouchi, with original character designs by manga authors Clamp.\",\"Codeine is an opiate used to treat pain, as a cough medicine, and for diarrhea. It is typically used to treat mild to moderate degrees of pain.\",\"Codename: Kids Next Door, commonly abbreviated to Kids Next Door or KND, is an American animated television series created by Tom Warburton for Cartoon Network, and the 13th of the network's Cartoon Cartoons.\",\"In computing, a code page is a table of values that describes the character set used for encoding a particular set of characters, usually combined with a number of control characters.\",\"Codex Sinaiticus (Greek: \\u03a3\\u03b9\\u03bd\\u03b1\\u03ca\\u03c4\\u03b9\\u03ba\\u03cc\\u03c2 \\u039a\\u03ce\\u03b4\\u03b9\\u03ba\\u03b1\\u03c2, Hebrew: \\u05e7\\u05d5\\u05d3\\u05e7\\u05e1 \\u05e1\\u05d9\\u05e0\\u05d0\\u05d9\\u05d8\\u05d9\\u05e7\\u05d5\\u05e1\\u200e; Shelfmarks and references: London, Brit.\",\"Code talkers are people in the 20th century who used obscure languages as a means of secret communication during wartime.\",\"Code Black is an American medical drama television series created by Michael Seitzman which premiered on CBS on September 30, 2015. It takes place in an overcrowded and understaffed emergency room in Los Angeles, California, and is based on a documentary by Ryan McGarry.\",\"Codependency is a controversial concept for a dysfunctional helping relationship where one person supports or enables another person's addiction, poor mental health, immaturity, irresponsibility, or under-achievement.\",\"The Codex Vaticanus (The Vatican, Bibl. Vat., Vat. gr. 1209; no. B or 03 Gregory-Aland, \\u03b4 1 von Soden) is regarded as the oldest extant manuscript of the Greek Bible (Old and New Testament), one of the four great uncial codices.\"],[\"https://en.wikipedia.org/wiki/Code\",\"https://en.wikipedia.org/wiki/Code_Geass\",\"https://en.wikipedia.org/wiki/Codeine\",\"https://en.wikipedia.org/wiki/Codename:_Kids_Next_Door\",\"https://en.wikipedia.org/wiki/Code_page\",\"https://en.wikipedia.org/wiki/Codex_Sinaiticus\",\"https://en.wikipedia.org/wiki/Code_talker\",\"https://en.wikipedia.org/wiki/Code_Black_(TV_series)\",\"https://en.wikipedia.org/wiki/Codependency\",\"https://en.wikipedia.org/wiki/Codex_Vaticanus\"]]"

        val results = defaultResponseParser(json)
        val expectedResults = listOf("Code", "Code Geass", "Codeine", "Codename: Kids Next Door", "Code page", "Codex Sinaiticus", "Code talker", "Code Black (TV series)", "Codependency", "Codex Vaticanus")
        assertEquals(expectedResults, results)
    }

    @Test
    fun `can parse a response from Yahoo`() {
        val json = "[\"firefox\",[\"firefox\",\"firefox browser\",\"firefox.com\",\"firefox update\"],[],[]]"

        val results = defaultResponseParser(json)
        val expectedResults = listOf("firefox", "firefox browser", "firefox.com", "firefox update")
        assertEquals(expectedResults, results)
    }

    @Test
    fun `can parse a response from Yandex`() {
        val json = "[\"firefox\",[\"firefox\",\"firefox download\",\"firefox browser\",\"firefox update\",\"firefox.com\"]]"

        val results = defaultResponseParser(json)
        val expectedResults = listOf("firefox", "firefox download", "firefox browser", "firefox update", "firefox.com")
        assertEquals(expectedResults, results)
    }
}
