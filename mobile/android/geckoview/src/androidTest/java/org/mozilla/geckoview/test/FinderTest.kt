/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import org.hamcrest.Matchers.* // ktlint-disable no-wildcard-imports
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoSession

@RunWith(AndroidJUnit4::class)
@MediumTest
class FinderTest : BaseSessionTest() {

    @Test fun find() {
        mainSession.loadTestPath(LOREM_IPSUM_HTML_PATH)
        mainSession.waitForPageStop()

        // Initial search.
        var result = sessionRule.waitForResult(mainSession.finder.find("dolore", 0))

        assertThat("Should be found", result.found, equalTo(true))
        assertThat("Should not have wrapped", result.wrapped, equalTo(false))
        assertThat("Current count should be correct", result.current, equalTo(1))
        assertThat("Total count should be correct", result.total, equalTo(2))
        assertThat(
            "Search string should be correct",
            result.searchString,
            equalTo("dolore"),
        )
        assertThat("Flags should be correct", result.flags, equalTo(0))

        // Search again using new flags.
        result = sessionRule.waitForResult(
            mainSession.finder.find(
                null,
                GeckoSession.FINDER_FIND_BACKWARDS
                    or GeckoSession.FINDER_FIND_MATCH_CASE
                    or GeckoSession.FINDER_FIND_WHOLE_WORD,
            ),
        )

        assertThat("Should be found", result.found, equalTo(true))
        assertThat("Should have wrapped", result.wrapped, equalTo(true))
        assertThat("Current count should be correct", result.current, equalTo(2))
        assertThat("Total count should be correct", result.total, equalTo(2))
        assertThat(
            "Search string should be correct",
            result.searchString,
            equalTo("dolore"),
        )
        assertThat(
            "Flags should be correct",
            result.flags,
            equalTo(
                GeckoSession.FINDER_FIND_BACKWARDS
                    or GeckoSession.FINDER_FIND_MATCH_CASE
                    or GeckoSession.FINDER_FIND_WHOLE_WORD,
            ),
        )

        // And again using same flags.
        result = sessionRule.waitForResult(
            mainSession.finder.find(
                null,
                GeckoSession.FINDER_FIND_BACKWARDS
                    or GeckoSession.FINDER_FIND_MATCH_CASE
                    or GeckoSession.FINDER_FIND_WHOLE_WORD,
            ),
        )

        assertThat("Should be found", result.found, equalTo(true))
        assertThat("Should not have wrapped", result.wrapped, equalTo(false))
        assertThat("Current count should be correct", result.current, equalTo(1))
        assertThat("Total count should be correct", result.total, equalTo(2))
        assertThat(
            "Search string should be correct",
            result.searchString,
            equalTo("dolore"),
        )
        assertThat(
            "Flags should be correct",
            result.flags,
            equalTo(
                GeckoSession.FINDER_FIND_BACKWARDS
                    or GeckoSession.FINDER_FIND_MATCH_CASE
                    or GeckoSession.FINDER_FIND_WHOLE_WORD,
            ),
        )

        // And again but go forward.
        result = sessionRule.waitForResult(
            mainSession.finder.find(
                null,
                GeckoSession.FINDER_FIND_MATCH_CASE
                    or GeckoSession.FINDER_FIND_WHOLE_WORD,
            ),
        )

        assertThat("Should be found", result.found, equalTo(true))
        assertThat("Should not have wrapped", result.wrapped, equalTo(false))
        assertThat("Current count should be correct", result.current, equalTo(2))
        assertThat("Total count should be correct", result.total, equalTo(2))
        assertThat(
            "Search string should be correct",
            result.searchString,
            equalTo("dolore"),
        )
        assertThat(
            "Flags should be correct",
            result.flags,
            equalTo(
                GeckoSession.FINDER_FIND_MATCH_CASE
                    or GeckoSession.FINDER_FIND_WHOLE_WORD,
            ),
        )
    }

    @Test fun find_notFound() {
        mainSession.loadTestPath(LOREM_IPSUM_HTML_PATH)
        mainSession.waitForPageStop()

        var result = sessionRule.waitForResult(mainSession.finder.find("foo", 0))

        assertThat("Should not be found", result.found, equalTo(false))
        assertThat("Should have wrapped", result.wrapped, equalTo(true))
        assertThat("Current count should be correct", result.current, equalTo(0))
        assertThat("Total count should be correct", result.total, equalTo(0))
        assertThat(
            "Search string should be correct",
            result.searchString,
            equalTo("foo"),
        )
        assertThat("Flags should be correct", result.flags, equalTo(0))

        result = sessionRule.waitForResult(mainSession.finder.find("lore", 0))

        assertThat("Should be found", result.found, equalTo(true))
    }

    @Test fun find_matchCase() {
        mainSession.loadTestPath(LOREM_IPSUM_HTML_PATH)
        mainSession.waitForPageStop()

        var result = sessionRule.waitForResult(mainSession.finder.find("lore", 0))

        assertThat("Total count should be correct", result.total, equalTo(3))

        result = sessionRule.waitForResult(
            mainSession.finder.find(
                null,
                GeckoSession.FINDER_FIND_MATCH_CASE,
            ),
        )

        assertThat("Total count should be correct", result.total, equalTo(2))
        assertThat(
            "Flags should be correct",
            result.flags,
            equalTo(GeckoSession.FINDER_FIND_MATCH_CASE),
        )
    }

    @Test fun find_wholeWord() {
        mainSession.loadTestPath(LOREM_IPSUM_HTML_PATH)
        mainSession.waitForPageStop()

        var result = sessionRule.waitForResult(mainSession.finder.find("dolor", 0))

        assertThat("Total count should be correct", result.total, equalTo(4))

        result = sessionRule.waitForResult(
            mainSession.finder.find(
                null,
                GeckoSession.FINDER_FIND_WHOLE_WORD,
            ),
        )

        assertThat("Total count should be correct", result.total, equalTo(2))
        assertThat(
            "Flags should be correct",
            result.flags,
            equalTo(GeckoSession.FINDER_FIND_WHOLE_WORD),
        )
    }

    @Test fun find_linksOnly() {
        mainSession.loadTestPath(LOREM_IPSUM_HTML_PATH)
        mainSession.waitForPageStop()

        val result = sessionRule.waitForResult(
            mainSession.finder.find(
                "nim",
                GeckoSession.FINDER_FIND_LINKS_ONLY,
            ),
        )

        assertThat("Total count should be correct", result.total, equalTo(1))
        assertThat(
            "Flags should be correct",
            result.flags,
            equalTo(GeckoSession.FINDER_FIND_LINKS_ONLY),
        )
    }

    @Test fun clear() {
        mainSession.loadTestPath(LOREM_IPSUM_HTML_PATH)
        mainSession.waitForPageStop()

        val result = sessionRule.waitForResult(mainSession.finder.find("lore", 0))

        assertThat("Match should be found", result.found, equalTo(true))

        assertThat(
            "Match should be selected",
            mainSession.evaluateJS("window.getSelection().toString()") as String,
            equalTo("Lore"),
        )

        mainSession.finder.clear()

        assertThat(
            "Match should be cleared",
            mainSession.evaluateJS("window.getSelection().isCollapsed") as Boolean,
            equalTo(true),
        )
    }

    @Test fun find_in_pdf() {
        mainSession.loadTestPath(TRACEMONKEY_PDF_PATH)
        mainSession.waitForPageStop()

        // Initial search.
        var result = sessionRule.waitForResult(mainSession.finder.find("trace", 0))

        assertThat("Should be found", result.found, equalTo(true))
        assertThat("Should not have wrapped", result.wrapped, equalTo(false))
        assertThat("Current count should be correct", result.current, equalTo(1))
        assertThat("Total count should be correct", result.total, equalTo(141))
        assertThat(
            "Search string should be correct",
            result.searchString,
            equalTo("trace"),
        )
        assertThat("Flags should be correct", result.flags, equalTo(0))

        // Search again using new flags.
        result = sessionRule.waitForResult(
            mainSession.finder.find(
                null,
                GeckoSession.FINDER_FIND_BACKWARDS
                    or GeckoSession.FINDER_FIND_MATCH_CASE
                    or GeckoSession.FINDER_FIND_WHOLE_WORD,
            ),
        )

        assertThat("Should be found", result.found, equalTo(true))
        assertThat("Should not have wrapped", result.wrapped, equalTo(false))
        assertThat("Current count should be correct", result.current, equalTo(6))
        assertThat("Total count should be correct", result.total, equalTo(85))
        assertThat(
            "Search string should be correct",
            result.searchString,
            equalTo("trace"),
        )
        assertThat(
            "Flags should be correct",
            result.flags,
            equalTo(
                GeckoSession.FINDER_FIND_BACKWARDS
                    or GeckoSession.FINDER_FIND_MATCH_CASE
                    or GeckoSession.FINDER_FIND_WHOLE_WORD,
            ),
        )

        // And again using same flags.
        result = sessionRule.waitForResult(
            mainSession.finder.find(
                null,
                GeckoSession.FINDER_FIND_BACKWARDS
                    or GeckoSession.FINDER_FIND_MATCH_CASE
                    or GeckoSession.FINDER_FIND_WHOLE_WORD,
            ),
        )

        assertThat("Should be found", result.found, equalTo(true))
        assertThat("Should not have wrapped", result.wrapped, equalTo(false))
        assertThat("Current count should be correct", result.current, equalTo(5))
        assertThat("Total count should be correct", result.total, equalTo(85))
        assertThat(
            "Search string should be correct",
            result.searchString,
            equalTo("trace"),
        )
        assertThat(
            "Flags should be correct",
            result.flags,
            equalTo(
                GeckoSession.FINDER_FIND_BACKWARDS
                    or GeckoSession.FINDER_FIND_MATCH_CASE
                    or GeckoSession.FINDER_FIND_WHOLE_WORD,
            ),
        )

        // And again but go forward.
        result = sessionRule.waitForResult(
            mainSession.finder.find(
                null,
                GeckoSession.FINDER_FIND_MATCH_CASE
                    or GeckoSession.FINDER_FIND_WHOLE_WORD,
            ),
        )

        assertThat("Should be found", result.found, equalTo(true))
        assertThat("Should not have wrapped", result.wrapped, equalTo(false))
        assertThat("Current count should be correct", result.current, equalTo(6))
        assertThat("Total count should be correct", result.total, equalTo(85))
        assertThat(
            "Search string should be correct",
            result.searchString,
            equalTo("trace"),
        )
        assertThat(
            "Flags should be correct",
            result.flags,
            equalTo(
                GeckoSession.FINDER_FIND_MATCH_CASE
                    or GeckoSession.FINDER_FIND_WHOLE_WORD,
            ),
        )
    }

    @Test fun find_in_pdf_with_wrapped_result() {
        mainSession.loadTestPath(TRACEMONKEY_PDF_PATH)
        mainSession.waitForPageStop()

        // Initial search.
        var result = sessionRule.waitForResult(
            mainSession.finder.find(
                "SpiderMonkey",
                GeckoSession.FINDER_FIND_MATCH_CASE
                    or GeckoSession.FINDER_FIND_WHOLE_WORD,
            ),
        )

        for (count in 1..4) {
            assertThat("Should be found", result.found, equalTo(true))
            assertThat("Should (not) have wrapped", result.wrapped, equalTo(count == 4))
            assertThat("Current count should be correct", result.current, equalTo(if (count == 4) 1 else count))
            assertThat("Total count should be correct", result.total, equalTo(3))
            assertThat(
                "Search string should be correct",
                result.searchString,
                equalTo("SpiderMonkey"),
            )

            // And again.
            result = sessionRule.waitForResult(
                mainSession.finder.find(
                    null,
                    GeckoSession.FINDER_FIND_MATCH_CASE
                        or GeckoSession.FINDER_FIND_WHOLE_WORD,
                ),
            )
        }
    }

    @Test fun find_in_pdf_notFound() {
        mainSession.loadTestPath(TRACEMONKEY_PDF_PATH)
        mainSession.waitForPageStop()

        var result = sessionRule.waitForResult(mainSession.finder.find("foo", 0))

        assertThat("Should not be found", result.found, equalTo(false))
        assertThat("Should have wrapped", result.wrapped, equalTo(true))
        assertThat("Current count should be correct", result.current, equalTo(0))
        assertThat("Total count should be correct", result.total, equalTo(0))
        assertThat(
            "Search string should be correct",
            result.searchString,
            equalTo("foo"),
        )
        assertThat("Flags should be correct", result.flags, equalTo(0))

        result = sessionRule.waitForResult(mainSession.finder.find("Spi", 0))

        assertThat("Should be found", result.found, equalTo(true))
    }

    @Test fun find_in_pdf_matchCase() {
        mainSession.loadTestPath(TRACEMONKEY_PDF_PATH)
        mainSession.waitForPageStop()

        var result = sessionRule.waitForResult(mainSession.finder.find("language", 0))

        assertThat("Total count should be correct", result.total, equalTo(15))

        result = sessionRule.waitForResult(
            mainSession.finder.find(
                null,
                GeckoSession.FINDER_FIND_MATCH_CASE,
            ),
        )

        assertThat("Total count should be correct", result.total, equalTo(13))
        assertThat(
            "Flags should be correct",
            result.flags,
            equalTo(GeckoSession.FINDER_FIND_MATCH_CASE),
        )
    }

    @Test fun find_in_pdf_wholeWord() {
        mainSession.loadTestPath(TRACEMONKEY_PDF_PATH)
        mainSession.waitForPageStop()

        var result = sessionRule.waitForResult(mainSession.finder.find("speed", 0))

        assertThat("Total count should be correct", result.total, equalTo(5))

        result = sessionRule.waitForResult(
            mainSession.finder.find(
                null,
                GeckoSession.FINDER_FIND_WHOLE_WORD,
            ),
        )

        assertThat("Total count should be correct", result.total, equalTo(1))
        assertThat(
            "Flags should be correct",
            result.flags,
            equalTo(GeckoSession.FINDER_FIND_WHOLE_WORD),
        )
    }

    @Test fun find_in_pdf_and_html() {
        for (i in 1..2) {
            mainSession.loadTestPath(TRACEMONKEY_PDF_PATH)
            mainSession.waitForPageStop()

            var result = sessionRule.waitForResult(mainSession.finder.find("trace", 0))

            assertThat("Total count should be correct", result.total, equalTo(141))

            mainSession.loadTestPath(LOREM_IPSUM_HTML_PATH)
            mainSession.waitForPageStop()

            result = sessionRule.waitForResult(mainSession.finder.find("dolore", 0))

            assertThat("Total count should be correct", result.total, equalTo(2))
        }
    }
}
