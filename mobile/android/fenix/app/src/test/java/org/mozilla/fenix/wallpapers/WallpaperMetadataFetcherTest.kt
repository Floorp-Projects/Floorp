/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wallpapers

import androidx.test.ext.junit.runners.AndroidJUnit4
import io.mockk.every
import io.mockk.mockk
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.wallpapers.WallpaperMetadataFetcher.Companion.currentJsonVersion
import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Locale

@RunWith(AndroidJUnit4::class)
class WallpaperMetadataFetcherTest {

    private val expectedRequest = Request(
        url = BuildConfig.WALLPAPER_URL.substringBefore("android") +
            "metadata/v$currentJsonVersion/wallpapers.json",
        method = Request.Method.GET,
        conservative = true,
    )
    private val mockResponse = mockk<Response>()
    private val mockClient = mockk<Client> {
        every { fetch(expectedRequest) } returns mockResponse
    }

    private lateinit var metadataFetcher: WallpaperMetadataFetcher

    @Before
    fun setup() {
        metadataFetcher = WallpaperMetadataFetcher(mockClient)
    }

    @Test
    fun `GIVEN wallpaper metadata WHEN parsed THEN wallpapers have correct ids, text and card colors`() = runTest {
        val json = """
            {
                "last-updated-date": "2022-01-01",
                "collections": [
                    {
                        "id": "classic-firefox",
                        "available-locales": null,
                        "availability-range": null,
                        "wallpapers": [
                            {
                                "id": "beach-vibes",
                                "text-color": "FBFBFE",
                                "card-color-light": "FFFFFF",
                                "card-color-dark": "000000"
                            },
                            {
                                "id": "sunrise",
                                "text-color": "15141A",
                                "card-color-light": "FFFFFF",
                                "card-color-dark": "000000"
                            }
                        ]
                    }
                ]
            }
        """.trimIndent()
        every { mockResponse.body } returns Response.Body(json.byteInputStream())

        val wallpapers = metadataFetcher.downloadWallpaperList()

        with(wallpapers[0]) {
            assertEquals(0xFFFBFBFE, textColor)
            assertEquals(0xFFFFFFFF, cardColorLight)
            assertEquals(0xFF000000, cardColorDark)
        }
        with(wallpapers[1]) {
            assertEquals(0xFF15141A, textColor)
            assertEquals(0xFFFFFFFF, cardColorLight)
            assertEquals(0xFF000000, cardColorDark)
        }
    }

    @Test
    fun `GIVEN wallpaper metadata is missing an id WHEN parsed THEN parsing fails`() = runTest {
        val json = """
            {
                "last-updated-date": "2022-01-01",
                "collections": [
                    {
                        "id": "classic-firefox",
                        "available-locales": null,
                        "availability-range": null,
                        "wallpapers": [
                            {
                                "text-color": "FBFBFE",
                                "card-color-light": "FFFFFF",
                                "card-color-dark": "000000"
                            },
                            {
                                "id": "sunrise",
                                "text-color": "15141A",
                                "card-color-light": "FFFFFF",
                                "card-color-dark": "000000"
                            }
                        ]
                    }
                ]
            }
        """.trimIndent()
        every { mockResponse.body } returns Response.Body(json.byteInputStream())

        val wallpapers = metadataFetcher.downloadWallpaperList()

        assertTrue(wallpapers.isEmpty())
    }

    @Test
    fun `GIVEN wallpaper metadata is missing a text color WHEN parsed THEN parsing fails`() = runTest {
        val json = """
            {
                "last-updated-date": "2022-01-01",
                "collections": [
                    {
                        "id": "classic-firefox",
                        "available-locales": null,
                        "availability-range": null,
                        "wallpapers": [
                            {
                                "id": "beach-vibes",
                                "card-color-light": "FFFFFF",
                                "card-color-dark": "000000"
                            },
                            {
                                "id": "sunrise",
                                "text-color": "15141A",
                                "card-color-light": "FFFFFF",
                                "card-color-dark": "000000"
                            }
                        ]
                    }
                ]
            }
        """.trimIndent()
        every { mockResponse.body } returns Response.Body(json.byteInputStream())

        val wallpapers = metadataFetcher.downloadWallpaperList()

        assertTrue(wallpapers.isEmpty())
    }

    @Test
    fun `GIVEN wallpaper metadata is missing a card color WHEN parsed THEN parsing fails`() = runTest {
        val json = """
            {
                "last-updated-date": "2022-01-01",
                "collections": [
                    {
                        "id": "classic-firefox",
                        "available-locales": null,
                        "availability-range": null,
                        "wallpapers": [
                            {
                                "id": "beach-vibes",
                                "text-color": "FBFBFE",
                            },
                            {
                                "id": "sunrise",
                                "text-color": "15141A",
                                "card-color-light": "FFFFFF",
                                "card-color-dark": "000000"
                            }
                        ]
                    }
                ]
            }
        """.trimIndent()
        every { mockResponse.body } returns Response.Body(json.byteInputStream())

        val wallpapers = metadataFetcher.downloadWallpaperList()

        assertTrue(wallpapers.isEmpty())
    }

    @Test
    fun `GIVEN collection with specified locales WHEN parsed THEN wallpapers includes locales`() = runTest {
        val locales = listOf("en-US", "es-US", "en-CA", "fr-CA")
        val json = """
            {
                "last-updated-date": "2022-01-01",
                "collections": [
                    {
                        "id": "classic-firefox",
                        "available-locales": ["en-US", "es-US", "en-CA", "fr-CA"],
                        "availability-range": null,
                        "wallpapers": [
                            {
                                "id": "beach-vibes",
                                "text-color": "FBFBFE",
                                "card-color-light": "FFFFFF",
                                "card-color-dark": "000000"
                            },
                            {
                                "id": "sunrise",
                                "text-color": "15141A",
                                "card-color-light": "FFFFFF",
                                "card-color-dark": "000000"
                            }
                        ]
                    }
                ]
            }
        """.trimIndent()
        every { mockResponse.body } returns Response.Body(json.byteInputStream())

        val wallpapers = metadataFetcher.downloadWallpaperList()

        assertTrue(wallpapers.isNotEmpty())
        assertTrue(
            wallpapers.all {
                it.collection.availableLocales == locales
            },
        )
    }

    @Test
    fun `GIVEN collection with specified date range WHEN parsed THEN wallpapers includes dates`() = runTest {
        val calendar = Calendar.getInstance()
        val startDate = calendar.run {
            set(2022, Calendar.JUNE, 27)
            time
        }
        val endDate = calendar.run {
            set(2022, Calendar.SEPTEMBER, 30)
            time
        }
        val json = """
            {
                "last-updated-date": "2022-01-01",
                "collections": [
                    {
                        "id": "classic-firefox",
                        "available-locales": null,
                        "availability-range": {
                            "start": "2022-06-27",
                            "end": "2022-09-30"
                        },
                        "wallpapers": [
                            {
                                "id": "beach-vibes",
                                "text-color": "FBFBFE",
                                "card-color-light": "FFFFFF",
                                "card-color-dark": "000000"
                            },
                            {
                                "id": "sunrise",
                                "text-color": "15141A",
                                "card-color-light": "FFFFFF",
                                "card-color-dark": "000000"
                            }
                        ]
                    }
                ]
            }
        """.trimIndent()
        every { mockResponse.body } returns Response.Body(json.byteInputStream())

        val wallpapers = metadataFetcher.downloadWallpaperList()

        assertTrue(wallpapers.isNotEmpty())
        assertTrue(
            wallpapers.all {
                val formatter = SimpleDateFormat("yyyy-MM-dd", Locale.US)
                formatter.format(startDate) == formatter.format(it.collection.startDate!!) &&
                    formatter.format(endDate) == formatter.format(it.collection.endDate!!)
            },
        )
    }

    @Test
    fun `GIVEN collection with specified learn more url WHEN parsed THEN wallpapers includes url`() = runTest {
        val json = """
            {
                "last-updated-date": "2022-01-01",
                "collections": [
                    {
                        "id": "classic-firefox",
                        "available-locales": null,
                        "availability-range": null,
                        "learn-more-url": "https://www.mozilla.org",
                        "wallpapers": [
                            {
                                "id": "beach-vibes",
                                "text-color": "FBFBFE",
                                "card-color-light": "FFFFFF",
                                "card-color-dark": "000000"
                            },
                            {
                                "id": "sunrise",
                                "text-color": "15141A",
                                "card-color-light": "FFFFFF",
                                "card-color-dark": "000000"
                            }
                        ]
                    }
                ]
            }
        """.trimIndent()
        every { mockResponse.body } returns Response.Body(json.byteInputStream())

        val wallpapers = metadataFetcher.downloadWallpaperList()

        assertTrue(wallpapers.isNotEmpty())
        assertTrue(
            wallpapers.all {
                it.collection.learnMoreUrl == "https://www.mozilla.org"
            },
        )
    }

    @Test
    fun `GIVEN collection with specified heading and description WHEN parsed THEN wallpapers include them`() = runTest {
        val heading = "A classic firefox experience"
        val description = "Check out these cool foxes, they're adorable and can be your wallpaper"
        val json = """
            {
                "last-updated-date": "2022-01-01",
                "collections": [
                    {
                        "id": "classic-firefox",
                        "heading": "$heading",
                        "description": "$description",
                        "available-locales": null,
                        "availability-range": null,
                        "learn-more-url": null,
                        "wallpapers": [
                            {
                                "id": "beach-vibes",
                                "text-color": "FBFBFE",
                                "card-color-light": "FFFFFF",
                                "card-color-dark": "000000"
                            },
                            {
                                "id": "sunrise",
                                "text-color": "15141A",
                                "card-color-light": "FFFFFF",
                                "card-color-dark": "000000"
                            }
                        ]
                    }
                ]
            }
        """.trimIndent()
        every { mockResponse.body } returns Response.Body(json.byteInputStream())

        val wallpapers = metadataFetcher.downloadWallpaperList()

        assertTrue(wallpapers.isNotEmpty())
        assertTrue(
            wallpapers.all {
                it.collection.heading == heading && it.collection.description == description
            },
        )
    }

    @Test
    fun `GIVEN string fields with null values WHEN parsed THEN fields are correctly null`() = runTest {
        val json = """
            {
                "last-updated-date": "2022-01-01",
                "collections": [
                    {
                        "id": "classic-firefox",
                        "heading": null,
                        "description": null,
                        "available-locales": null,
                        "availability-range": null,
                        "learn-more-url": null,
                        "wallpapers": [
                            {
                                "id": "beach-vibes",
                                "text-color": "FBFBFE",
                                "card-color-light": "FFFFFF",
                                "card-color-dark": "000000"
                            },
                            {
                                "id": "sunrise",
                                "text-color": "15141A",
                                "card-color-light": "FFFFFF",
                                "card-color-dark": "000000"
                            }
                        ]
                    }
                ]
            }
        """.trimIndent()
        every { mockResponse.body } returns Response.Body(json.byteInputStream())

        val wallpapers = metadataFetcher.downloadWallpaperList()

        assertTrue(wallpapers.isNotEmpty())
        assertTrue(
            wallpapers.all {
                it.collection.heading == null && it.collection.description == null
            },
        )
    }
}
