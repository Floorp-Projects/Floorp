/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system.matcher

import android.net.Uri
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.engine.system.matcher.UrlMatcher.Companion.ADVERTISING
import mozilla.components.browser.engine.system.matcher.UrlMatcher.Companion.ANALYTICS
import mozilla.components.browser.engine.system.matcher.UrlMatcher.Companion.CONTENT
import mozilla.components.browser.engine.system.matcher.UrlMatcher.Companion.SOCIAL
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.io.StringReader
import java.util.HashMap

@RunWith(AndroidJUnit4::class)
class UrlMatcherTest {

    @Test
    fun basicMatching() {
        val matcher = UrlMatcher(arrayOf("bcd.random"))

        assertTrue(matcher.matches("http://bcd.random/something", "http://mozilla.org").first)
        assertTrue(matcher.matches("http://bcd.random", "http://mozilla.org").first)
        assertTrue(matcher.matches("http://www.bcd.random", "http://mozilla.org").first)
        assertTrue(matcher.matches("http://www.bcd.random/something", "http://mozilla.org").first)
        assertTrue(matcher.matches("http://foobar.bcd.random", "http://mozilla.org").first)
        assertTrue(matcher.matches("http://foobar.bcd.random/something", "http://mozilla.org").first)

        assertFalse(matcher.matches("http://other.random", "http://mozilla.org").first)
        assertFalse(matcher.matches("http://other.random/something", "http://mozilla.org").first)
        assertFalse(matcher.matches("http://www.other.random", "http://mozilla.org").first)
        assertFalse(matcher.matches("http://www.other.random/something", "http://mozilla.org").first)
        assertFalse(matcher.matches("http://bcd.specific", "http://mozilla.org").first)
        assertFalse(matcher.matches("http://bcd.specific/something", "http://mozilla.org").first)
        assertFalse(matcher.matches("http://www.bcd.specific", "http://mozilla.org").first)
        assertFalse(matcher.matches("http://www.bcd.specific/something", "http://mozilla.org").first)

        assertFalse(matcher.matches("http://mozilla.org/resource", "data:text/html;stuff here").first)
        assertTrue(matcher.matches("http://bcd.random/resource", "data:text/html;stuff here").first)
    }

    /**
     * Tests that category enabling/disabling works correctly. We test this by creating
     * 4 categories, each with only one domain. We then iterate over all permutations of categories,
     * and test that only the expected domains are actually blocked.
     */
    @Test
    fun enableDisableCategories() {
        val categories = HashMap<String, Trie>()
        val suppportedCategories = mutableSetOf<String>()
        val enabledCategories = mutableSetOf<String>()
        val categoryCount = 4

        for (i in 0 until categoryCount) {
            val trie = Trie.createRootNode()
            trie.put("category$i.com".reverse())

            val categoryName = "category$i"
            categories[categoryName] = trie
            enabledCategories.add(categoryName)
            suppportedCategories.add(categoryName)
        }

        val matcher = UrlMatcher(suppportedCategories, enabledCategories, categories)

        // We can test every permutation by iterating over every value of a 4-bit integer (each bit
        // indicates whether a given category is enabled or disabled).
        // N categories -> N bits == (2^N - 1) == '1111...'
        // 4 categories -> 4 bits == 15 == 2^N-1 = '1111'
        val allEnabledPattern = (1 shl categoryCount) - 1
        for (categoryPattern in 0..allEnabledPattern) {
            // Ensure all the correct categories enabled
            for (currentCategory in 0 until categoryCount) {
                val currentBit = 1 shl currentCategory
                val enabled = currentBit and categoryPattern == currentBit
                matcher.setCategoryEnabled("category$currentCategory", enabled)

                // Make sure our category enabling code actually sets the correct
                // values for a few known combinations (i.e. we're doing a test within the test)
                if (categoryPattern == 0) {
                    assertFalse("All categories should be disabled for categorypattern==0", enabled)
                } else if (categoryPattern == allEnabledPattern) {
                    assertTrue("All categories should be enabled for categorypattern=='111....'", enabled)
                } else if (categoryPattern == Integer.parseInt("1100", 2)) {
                    if (currentCategory < 2) {
                        assertFalse("Categories 0/1 expected to be disabled", enabled)
                    } else {
                        assertTrue("Categories >= 2 expected to be enabled", enabled)
                    }
                }
            }

            for (currentCategory in 0 until categoryCount) {
                val currentBit = 1 shl currentCategory
                val enabled = currentBit and categoryPattern == currentBit
                val url = "http://category$currentCategory.com"
                assertEquals("Incorrect category matched for combo=$categoryPattern url=$url",
                    enabled, matcher.matches(url, "http://www.mozilla.org").first
                )
            }
        }
    }

    val BLOCK_LIST = """{
      "license": "test-license",
      "categories": {
        "Disconnect": [
          {
            "Facebook": {
              "http://www.facebook.com/": [
                "facebook.com"
              ]
            }
          },
          {
            "Disconnect1": {
              "http://www.disconnect1.com/": [
                "disconnect1.com"
              ]
            }
          }
        ],
        "Advertising": [
          {
            "AdTest1": {
              "http://www.adtest1.com/": [
                "adtest1.com",
                "adtest1.de"
              ]
            }
          },
          {
            "AdTest2": {
              "http://www.adtest2.com/": [
                "adtest2.com"
              ]
            }
          }
        ],
        "Analytics": [
          {
            "AnalyticsTest": {
                "http://analyticsTest1.com/": [
                  "analyticsTest1.com",
                  "analyticsTest1.de"
                ]
            }
          }
        ],
        "Content": [
          {
            "ContentTest1": {
              "http://contenttest1.com/": [
                "contenttest1.com"
              ]
            }
          }
        ],
        "Social": [
          {
            "SocialTest1": {
              "http://www.socialtest1.com/": [
                "socialtest1.com",
                "socialtest1.de"
               ]
              }
            }
        ],
        "Legacy Disconnect": [
          {
            "Ignored1": {
              "http://www.ignored1.com/": [
                "ignored.de"
              ]
            }
          }
        ],
        "Legacy Content": [
          {
            "Ignored2": {
              "http://www.ignored2.com/": [
                "ignored.de"
              ]
            }
          }
        ]
      }
    }
    """

    val OVERRIDES = """{
      "categories": {
        "Advertising": [
          {
            "AdTest2": {
              "http://www.adtest2.com/": [
                "adtest2.de",
                "adtest2.at"
              ]
            }
          }
        ]
      }
    }
    """

    val WHITE_LIST = """{
      "SocialTest1": {
        "properties": [
          "www.socialtest1.com"
        ],
        "resources": [
          "socialtest1.de"
        ]
      }
    }"""
    @Test
    fun createMatcher() {
        val matcher = UrlMatcher.createMatcher(
                StringReader(BLOCK_LIST),
                listOf(StringReader(OVERRIDES)),
                StringReader(WHITE_LIST))

        // Check returns correct category
        val (matchesAds, categoryAds) = matcher.matches("http://adtest1.com", "http://www.adtest1.com")

        assertTrue(matchesAds)
        assertEquals(categoryAds, ADVERTISING)

        val (matchesAds2, categoryAd2) = matcher.matches("http://adtest1.de", "http://www.adtest1.com")

        assertTrue(matchesAds2)
        assertEquals(categoryAd2, ADVERTISING)

        val (matchesSocial, categorySocial) = matcher.matches(
            "http://socialtest1.com/",
            "http://www.socialtest1.com/"
        )

        assertTrue(matchesSocial)
        assertEquals(categorySocial, SOCIAL)

        val (matchesContent, categoryContent) = matcher.matches(
            "http://contenttest1.com/",
            "http://www.contenttest1.com/"
        )

        assertTrue(matchesContent)
        assertEquals(categoryContent, CONTENT)

        val (matchesAnalytics, categoryAnalytics) = matcher.matches(
            "http://analyticsTest1.com/",
            "http://www.analyticsTest1.com/"
        )

        assertTrue(matchesAnalytics)
        assertEquals(categoryAnalytics, ANALYTICS)

        // Check that override worked
        assertTrue(matcher.matches("http://adtest2.com", "http://www.adtest2.com").first)
        assertTrue(matcher.matches("http://adtest2.at", "http://www.adtest2.com").first)
        assertTrue(matcher.matches("http://adtest2.de", "http://www.adtest2.com").first)

        // Check that white list worked
        assertTrue(matcher.matches("http://socialtest1.com", "http://www.socialtest1.com").first)
        assertFalse(matcher.matches("http://socialtest1.de", "http://www.socialtest1.com").first)

        // Check ignored categories
        assertFalse(matcher.matches("http://ignored1.de", "http://www.ignored1.com").first)
        assertFalse(matcher.matches("http://ignored2.de", "http://www.ignored2.com").first)

        // Check that we find the social URIs we moved from Disconnect
        assertTrue(matcher.matches("http://facebook.com", "http://www.facebook.com").first)
        assertFalse(matcher.matches("http://disconnect1.com", "http://www.disconnect1.com").first)
    }

    @Test
    fun isWebFont() {
        assertFalse(UrlMatcher.isWebFont(mock()))
        assertFalse(UrlMatcher.isWebFont(Uri.parse("mozilla.org")))
        assertTrue(UrlMatcher.isWebFont(Uri.parse("/fonts/test.woff2")))
        assertTrue(UrlMatcher.isWebFont(Uri.parse("/fonts/test.woff")))
        assertTrue(UrlMatcher.isWebFont(Uri.parse("/fonts/test.eot")))
        assertTrue(UrlMatcher.isWebFont(Uri.parse("/fonts/test.ttf")))
        assertTrue(UrlMatcher.isWebFont(Uri.parse("/fonts/test.otf")))
    }

    @Test
    fun setCategoriesEnabled() {
        val matcher = spy(UrlMatcher.createMatcher(
                StringReader(BLOCK_LIST),
                listOf(StringReader(OVERRIDES)),
                StringReader(WHITE_LIST),
                setOf("Advertising", "Analytics"))
        )

        matcher.setCategoriesEnabled(setOf("Advertising", "Analytics"))
        verify(matcher, never()).setCategoryEnabled(any(), anyBoolean())

        matcher.setCategoriesEnabled(setOf("Advertising", "Analytics", "Content"))
        verify(matcher).setCategoryEnabled("Advertising", true)
        verify(matcher).setCategoryEnabled("Analytics", true)
        verify(matcher).setCategoryEnabled("Content", true)
    }

    @Test
    fun webFontsNotBlockedByDefault() {
        val matcher = UrlMatcher.createMatcher(
                StringReader(BLOCK_LIST),
                listOf(StringReader(OVERRIDES)),
                StringReader(WHITE_LIST),
                setOf(UrlMatcher.ADVERTISING, UrlMatcher.ANALYTICS, UrlMatcher.SOCIAL, UrlMatcher.CONTENT))

        assertFalse(
            matcher.matches(
                "http://mozilla.org/fonts/test.woff2",
                "http://mozilla.org"
            ).first
        )
    }
}
