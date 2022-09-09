/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.wallpaper

import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.wallpapers.Wallpaper

class ExtensionsTest {
    private val classicCollection = getSeasonalCollection("classic-firefox")

    @Test
    fun `GIVEN wallpapers that include the default WHEN grouped by collection THEN default will be added to classic firefox`() {
        val seasonalCollection = getSeasonalCollection("finally fall")
        val classicFirefoxWallpapers = (0..5).map { generateClassicFirefoxWallpaper("firefox$it") }
        val seasonalWallpapers = (0..5).map { generateSeasonalWallpaperCollection("${seasonalCollection.name}$it", seasonalCollection.name) }
        val allWallpapers = listOf(Wallpaper.Default) + classicFirefoxWallpapers + seasonalWallpapers

        val result = allWallpapers.groupByDisplayableCollection()

        assertEquals(2, result.size)
        assertEquals(listOf(Wallpaper.Default) + classicFirefoxWallpapers, result[classicCollection])
        assertEquals(seasonalWallpapers, result[seasonalCollection])
    }

    @Test
    fun `GIVEN no wallpapers but the default WHEN grouped by collection THEN the default will still be present`() {
        val result = listOf(Wallpaper.Default).groupByDisplayableCollection()

        assertEquals(1, result.size)
        assertEquals(listOf(Wallpaper.Default), result[Wallpaper.DefaultCollection])
    }

    @Test
    fun `GIVEN wallpapers with thumbnails that have not downloaded WHEN grouped by collection THEN wallpapers without thumbnails will not be included`() {
        val seasonalCollection = getSeasonalCollection("finally fall")
        val classicFirefoxWallpapers = (0..5).map { generateClassicFirefoxWallpaper("firefox$it") }
        val downloadedSeasonalWallpapers = (0..5).map { generateSeasonalWallpaperCollection("${seasonalCollection.name}$it", seasonalCollection.name) }
        val nonDownloadedSeasonalWallpapers = (0..5).map {
            generateSeasonalWallpaperCollection(
                "${seasonalCollection.name}$it",
                seasonalCollection.name,
                Wallpaper.ImageFileState.Error,
            )
        }
        val allWallpapers = listOf(Wallpaper.Default) + classicFirefoxWallpapers + downloadedSeasonalWallpapers + nonDownloadedSeasonalWallpapers

        val result = allWallpapers.groupByDisplayableCollection()

        assertEquals(2, result.size)
        assertEquals(listOf(Wallpaper.Default) + classicFirefoxWallpapers, result[classicCollection])
        assertEquals(downloadedSeasonalWallpapers, result[seasonalCollection])
    }

    private fun generateClassicFirefoxWallpaper(name: String) = Wallpaper(
        name = name,
        textColor = 0L,
        cardColor = 0L,
        thumbnailFileState = Wallpaper.ImageFileState.Downloaded,
        assetsFileState = Wallpaper.ImageFileState.Downloaded,
        collection = classicCollection,
    )

    private fun getSeasonalCollection(name: String) = Wallpaper.Collection(
        name = name,
        heading = null,
        description = null,
        learnMoreUrl = null,
        availableLocales = null,
        startDate = null,
        endDate = null,
    )

    private fun generateSeasonalWallpaperCollection(
        wallpaperName: String,
        collectionName: String,
        thumbnailState: Wallpaper.ImageFileState = Wallpaper.ImageFileState.Downloaded,
    ) = Wallpaper(
        name = wallpaperName,
        textColor = 0L,
        cardColor = 0L,
        thumbnailFileState = thumbnailState,
        assetsFileState = Wallpaper.ImageFileState.Downloaded,
        collection = getSeasonalCollection(collectionName),
    )
}
