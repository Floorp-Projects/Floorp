/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wallpapers

import io.mockk.every
import io.mockk.mockk
import kotlinx.coroutines.test.UnconfinedTestDispatcher
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.mozilla.fenix.BuildConfig
import java.io.File
import java.lang.IllegalStateException

class WallpaperDownloaderTest {
    @Rule
    @JvmField
    val tempFolder = TemporaryFolder()

    private val remoteHost = BuildConfig.WALLPAPER_URL

    private val wallpaperBytes = "file contents"
    private val portraitResponseBodySuccess = Response.Body(wallpaperBytes.byteInputStream())
    private val landscapeResponseBodySuccess = Response.Body(wallpaperBytes.byteInputStream())
    private val mockPortraitResponse = mockk<Response>()
    private val mockLandscapeResponse = mockk<Response>()
    private val mockClient = mockk<Client>()

    private val dispatcher = UnconfinedTestDispatcher()

    private val wallpaperCollection = Wallpaper.Collection(
        name = "collection",
        heading = null,
        description = null,
        learnMoreUrl = null,
        availableLocales = null,
        startDate = null,
        endDate = null,
    )

    private lateinit var downloader: WallpaperDownloader

    @Before
    fun setup() {
        downloader = WallpaperDownloader(tempFolder.root, mockClient, dispatcher)
    }

    @Test
    fun `GIVEN that asset request is successful WHEN downloading assets THEN both files are created in expected location`() = runTest {
        val wallpaper = generateWallpaper()
        val portraitRequest = wallpaper.generateRequest("portrait")
        val landscapeRequest = wallpaper.generateRequest("landscape")
        every { mockPortraitResponse.status } returns 200
        every { mockLandscapeResponse.status } returns 200
        every { mockPortraitResponse.body } returns portraitResponseBodySuccess
        every { mockLandscapeResponse.body } returns landscapeResponseBodySuccess
        every { mockClient.fetch(portraitRequest) } returns mockPortraitResponse
        every { mockClient.fetch(landscapeRequest) } returns mockLandscapeResponse

        downloader.downloadWallpaper(wallpaper)

        val expectedPortraitFile = File(tempFolder.root, "wallpapers/${wallpaper.name}/portrait.png")
        val expectedLandscapeFile = File(tempFolder.root, "wallpapers/${wallpaper.name}/landscape.png")
        assertTrue(expectedPortraitFile.exists() && expectedPortraitFile.readText() == wallpaperBytes)
        assertTrue(expectedLandscapeFile.exists() && expectedLandscapeFile.readText() == wallpaperBytes)
    }

    @Test
    fun `GIVEN that thumbnail request is successful WHEN downloading THEN file is created in expected location`() = runTest {
        val wallpaper = generateWallpaper()
        val thumbnailRequest = wallpaper.generateRequest("thumbnail")
        val mockThumbnailResponse = mockk<Response>()
        every { mockThumbnailResponse.status } returns 200
        every { mockThumbnailResponse.body } returns Response.Body(wallpaperBytes.byteInputStream())
        every { mockClient.fetch(thumbnailRequest) } returns mockThumbnailResponse

        val result = downloader.downloadThumbnail(wallpaper)

        val expectedThumbnailFile = File(tempFolder.root, "wallpapers/${wallpaper.name}/thumbnail.png")
        assertTrue(expectedThumbnailFile.exists() && expectedThumbnailFile.readText() == wallpaperBytes)
        assertEquals(Wallpaper.ImageFileState.Downloaded, result)
    }

    @Test
    fun `GIVEN that request fails WHEN downloading THEN file is not created`() = runTest {
        val wallpaper = generateWallpaper()
        val portraitRequest = wallpaper.generateRequest("portrait")
        val landscapeRequest = wallpaper.generateRequest("landscape")
        every { mockPortraitResponse.status } returns 400
        every { mockLandscapeResponse.status } returns 400
        every { mockClient.fetch(portraitRequest) } returns mockPortraitResponse
        every { mockClient.fetch(landscapeRequest) } returns mockLandscapeResponse

        downloader.downloadWallpaper(wallpaper)

        val expectedPortraitFile = File(tempFolder.root, "wallpapers/${wallpaper.name}/portrait.png")
        val expectedLandscapeFile = File(tempFolder.root, "wallpapers/${wallpaper.name}/landscape.png")
        assertFalse(expectedPortraitFile.exists())
        assertFalse(expectedLandscapeFile.exists())
    }

    @Test
    fun `GIVEN that copying the file fails WHEN downloading THEN file is not created`() = runTest {
        val wallpaper = generateWallpaper()
        val portraitRequest = wallpaper.generateRequest("portrait")
        val landscapeRequest = wallpaper.generateRequest("landscape")
        every { mockPortraitResponse.status } returns 200
        every { mockLandscapeResponse.status } returns 200
        every { mockPortraitResponse.body } throws IllegalStateException()
        every { mockClient.fetch(portraitRequest) } throws IllegalStateException()
        every { mockClient.fetch(landscapeRequest) } returns mockLandscapeResponse

        downloader.downloadWallpaper(wallpaper)

        val expectedPortraitFile = File(tempFolder.root, "wallpapers/${wallpaper.name}/portrait.png")
        val expectedLandscapeFile = File(tempFolder.root, "wallpapers/${wallpaper.name}/landscape.png")
        assertFalse(expectedPortraitFile.exists())
        assertFalse(expectedLandscapeFile.exists())
    }

    private fun generateWallpaper(name: String = "name") = Wallpaper(
        name = name,
        collection = wallpaperCollection,
        textColor = null,
        cardColorLight = null,
        cardColorDark = null,
        thumbnailFileState = Wallpaper.ImageFileState.Unavailable,
        assetsFileState = Wallpaper.ImageFileState.Unavailable,
    )

    private fun Wallpaper.generateRequest(type: String) = Request(
        url = "$remoteHost/${collection.name}/$name/$type.png",
        method = Request.Method.GET,
        conservative = true,
    )
}
