/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.ReuseSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDevToolsAPI

import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
@MediumTest
class FinderTest : BaseSessionTest() {

    @ReuseSession(false) // "wrapped" in the first result depends on a fresh session.
    @Test fun find() {
        mainSession.loadTestPath(LOREM_IPSUM_HTML_PATH)
        mainSession.waitForPageStop()

        // Initial search.
        var result = sessionRule.waitForResult(mainSession.finder.find("dolore", 0))

        assertThat("Should be found", result.found, equalTo(true))
        // "wrapped" is true for the first found result of a new session.
        assertThat("Should have wrapped", result.wrapped, equalTo(true))
        assertThat("Current count should be correct", result.current, equalTo(1))
        assertThat("Total count should be correct", result.total, equalTo(2))
        assertThat("Search string should be correct",
                   result.searchString, equalTo("dolore"))
        assertThat("Flags should be correct", result.flags, equalTo(0))

        // Search again using new flags.
        result = sessionRule.waitForResult(mainSession.finder.find(
                null, GeckoSession.FINDER_FIND_BACKWARDS
                                        or GeckoSession.FINDER_FIND_MATCH_CASE
                                        or GeckoSession.FINDER_FIND_WHOLE_WORD))

        assertThat("Should be found", result.found, equalTo(true))
        assertThat("Should have wrapped", result.wrapped, equalTo(true))
        assertThat("Current count should be correct", result.current, equalTo(2))
        assertThat("Total count should be correct", result.total, equalTo(2))
        assertThat("Search string should be correct",
                   result.searchString, equalTo("dolore"))
        assertThat("Flags should be correct", result.flags,
                   equalTo(GeckoSession.FINDER_FIND_BACKWARDS
                                            or GeckoSession.FINDER_FIND_MATCH_CASE
                                            or GeckoSession.FINDER_FIND_WHOLE_WORD))

        // And again using same flags.
        result = sessionRule.waitForResult(mainSession.finder.find(
                null, GeckoSession.FINDER_FIND_BACKWARDS
                                        or GeckoSession.FINDER_FIND_MATCH_CASE
                                        or GeckoSession.FINDER_FIND_WHOLE_WORD))

        assertThat("Should be found", result.found, equalTo(true))
        assertThat("Should not have wrapped", result.wrapped, equalTo(false))
        assertThat("Current count should be correct", result.current, equalTo(1))
        assertThat("Total count should be correct", result.total, equalTo(2))
        assertThat("Search string should be correct",
                   result.searchString, equalTo("dolore"))
        assertThat("Flags should be correct", result.flags,
                   equalTo(GeckoSession.FINDER_FIND_BACKWARDS
                                            or GeckoSession.FINDER_FIND_MATCH_CASE
                                            or GeckoSession.FINDER_FIND_WHOLE_WORD))

        // And again but go forward.
        result = sessionRule.waitForResult(mainSession.finder.find(
                null, GeckoSession.FINDER_FIND_MATCH_CASE
                                        or GeckoSession.FINDER_FIND_WHOLE_WORD))

        assertThat("Should be found", result.found, equalTo(true))
        assertThat("Should not have wrapped", result.wrapped, equalTo(false))
        assertThat("Current count should be correct", result.current, equalTo(2))
        assertThat("Total count should be correct", result.total, equalTo(2))
        assertThat("Search string should be correct",
                   result.searchString, equalTo("dolore"))
        assertThat("Flags should be correct", result.flags,
                   equalTo(GeckoSession.FINDER_FIND_MATCH_CASE
                                            or GeckoSession.FINDER_FIND_WHOLE_WORD))
    }

    @Test fun find_notFound() {
        mainSession.loadTestPath(LOREM_IPSUM_HTML_PATH)
        mainSession.waitForPageStop()

        var result = sessionRule.waitForResult(mainSession.finder.find("foo", 0))

        assertThat("Should not be found", result.found, equalTo(false))
        assertThat("Should have wrapped", result.wrapped, equalTo(true))
        assertThat("Current count should be correct", result.current, equalTo(0))
        assertThat("Total count should be correct", result.total, equalTo(0))
        assertThat("Search string should be correct",
                   result.searchString, equalTo("foo"))
        assertThat("Flags should be correct", result.flags, equalTo(0))

        result = sessionRule.waitForResult(mainSession.finder.find("lore", 0))

        assertThat("Should be found", result.found, equalTo(true))
    }

    @Test fun find_matchCase() {
        mainSession.loadTestPath(LOREM_IPSUM_HTML_PATH)
        mainSession.waitForPageStop()

        var result = sessionRule.waitForResult(mainSession.finder.find("lore", 0))

        assertThat("Total count should be correct", result.total, equalTo(3))

        result = sessionRule.waitForResult(mainSession.finder.find(
                null, GeckoSession.FINDER_FIND_MATCH_CASE))

        assertThat("Total count should be correct", result.total, equalTo(2))
        assertThat("Flags should be correct",
                   result.flags, equalTo(GeckoSession.FINDER_FIND_MATCH_CASE))
    }

    @Test fun find_wholeWord() {
        mainSession.loadTestPath(LOREM_IPSUM_HTML_PATH)
        mainSession.waitForPageStop()

        var result = sessionRule.waitForResult(mainSession.finder.find("dolor", 0))

        assertThat("Total count should be correct", result.total, equalTo(4))

        result = sessionRule.waitForResult(mainSession.finder.find(
                null, GeckoSession.FINDER_FIND_WHOLE_WORD))

        assertThat("Total count should be correct", result.total, equalTo(2))
        assertThat("Flags should be correct",
                   result.flags, equalTo(GeckoSession.FINDER_FIND_WHOLE_WORD))
    }

    @Test fun find_linksOnly() {
        mainSession.loadTestPath(LOREM_IPSUM_HTML_PATH)
        mainSession.waitForPageStop()

        val result = sessionRule.waitForResult(mainSession.finder.find(
                "lore", GeckoSession.FINDER_FIND_LINKS_ONLY))

        assertThat("Total count should be correct", result.total, equalTo(1))
        assertThat("Flags should be correct",
                   result.flags, equalTo(GeckoSession.FINDER_FIND_LINKS_ONLY))
    }

    @WithDevToolsAPI
    @Test fun clear() {
        mainSession.loadTestPath(LOREM_IPSUM_HTML_PATH)
        mainSession.waitForPageStop()

        val result = sessionRule.waitForResult(mainSession.finder.find("lore", 0))

        assertThat("Match should be found", result.found, equalTo(true))

        assertThat("Match should be selected",
                   mainSession.evaluateJS("window.getSelection().toString()") as String,
                   equalTo("Lore"))

        mainSession.finder.clear()

        assertThat("Match should be cleared",
                   mainSession.evaluateJS("window.getSelection().isCollapsed") as Boolean,
                   equalTo(true))
    }
}