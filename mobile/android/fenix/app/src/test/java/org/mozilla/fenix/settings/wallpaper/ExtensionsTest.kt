/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.wallpaper

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
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
        assertEquals(listOf(Wallpaper.Default), result[Wallpaper.ClassicFirefoxCollection])
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

    @Test
    fun `GIVEN that classic firefox thumbnails fail to download WHEN grouped by collection THEN default is still available`() {
        val seasonalCollection = getSeasonalCollection("finally fall")
        val downloadedSeasonalWallpapers = (0..5).map {
            generateSeasonalWallpaperCollection(
                "${seasonalCollection.name}$it",
                seasonalCollection.name,
            )
        }
        val allWallpapers = listOf(Wallpaper.Default) + downloadedSeasonalWallpapers

        val result = allWallpapers.groupByDisplayableCollection()

        assertEquals(2, result.size)
        assertEquals(listOf(Wallpaper.Default), result[classicCollection])
        assertEquals(downloadedSeasonalWallpapers, result[seasonalCollection])
    }

    @Test
    fun `GIVEN two collections of appropriate size WHEN fetched for onboarding THEN result contains 3 seasonal and 2 classic`() {
        val seasonalCollection = getSeasonalCollection("finally fall")
        val seasonalWallpapers = (0..5).map { generateSeasonalWallpaperCollection("${seasonalCollection.name}$it", seasonalCollection.name) }
        val classicFirefoxWallpapers = (0..5).map { generateClassicFirefoxWallpaper("firefox$it") }
        val allWallpapers = listOf(Wallpaper.Default) + classicFirefoxWallpapers + seasonalWallpapers

        val result = allWallpapers.getWallpapersForOnboarding()

        assertEquals(3, result.count { it.collection.name == "finally fall" })
        assertEquals(2, result.count { it.collection.name == classicCollection.name })
        assertTrue(result.contains(Wallpaper.Default))
    }

    @Test
    fun `GIVEN five collections of insufficient size WHEN fetched for onboarding THEN result contains 3 seasonal and 2 classic`() {
        val seasonalCollectionA = getSeasonalCollection("finally winter")
        val seasonalWallpapers = generateSeasonalWallpaperCollection("${seasonalCollectionA.name}$0", seasonalCollectionA.name)
        val seasonalCollectionB = getSeasonalCollection("finally spring")
        val seasonalWallpaperB = generateSeasonalWallpaperCollection("${seasonalCollectionB.name}$0", seasonalCollectionB.name)
        val seasonalCollectionC = getSeasonalCollection("finally summer")
        val seasonalWallpapersC = generateSeasonalWallpaperCollection("${seasonalCollectionC.name}$0", seasonalCollectionC.name)
        val seasonalCollectionD = getSeasonalCollection("finally autumn")
        val seasonalWallpaperD = generateSeasonalWallpaperCollection("${seasonalCollectionD.name}$0", seasonalCollectionD.name)
        val seasonalCollectionE = getSeasonalCollection("finally vacation")
        val seasonalWallpapersE = generateSeasonalWallpaperCollection("${seasonalCollectionE.name}$0", seasonalCollectionE.name)

        val classicFirefoxWallpapers = (0..5).map { generateClassicFirefoxWallpaper("firefox$it") }
        val allWallpapers = listOf(Wallpaper.Default) + classicFirefoxWallpapers + seasonalWallpapers +
            seasonalWallpaperB + seasonalWallpapersC + seasonalWallpaperD + seasonalWallpapersE

        val result = allWallpapers.getWallpapersForOnboarding()

        assertEquals(3, result.count { it.collection.name != classicCollection.name && it != Wallpaper.Default })
        assertEquals(2, result.count { it.collection.name == classicCollection.name })
        assertTrue(result.contains(Wallpaper.Default))
    }

    @Test
    fun `GIVEN seasonal collection of insufficient size WHEN grouped for onboarding THEN result contains all seasonal and the rest is classic`() {
        val seasonalCollection = getSeasonalCollection("finally fall")
        val seasonalWallpapers = generateSeasonalWallpaperCollection("${seasonalCollection.name}$0", seasonalCollection.name)
        val classicFirefoxWallpapers = (0..5).map { generateClassicFirefoxWallpaper("firefox$it") }
        val allWallpapers = listOf(Wallpaper.Default) + classicFirefoxWallpapers + seasonalWallpapers

        val result = allWallpapers.getWallpapersForOnboarding()

        assertEquals(1, result.count { it.collection.name == "finally fall" })
        assertEquals(4, result.count { it.collection.name == classicCollection.name })
        assertTrue(result.contains(Wallpaper.Default))
    }

    @Test
    fun `GIVEN no seasonal collection WHEN grouped for onboarding THEN result contains all classic`() {
        val classicFirefoxWallpapers = (0..5).map { generateClassicFirefoxWallpaper("firefox$it") }
        val allWallpapers = listOf(Wallpaper.Default) + classicFirefoxWallpapers

        val result = allWallpapers.getWallpapersForOnboarding()

        assertEquals(5, result.count { it.collection.name == classicCollection.name })
        assertTrue(result.contains(Wallpaper.Default))
    }

    @Test
    fun `GIVEN insufficient items in classic collection WHEN grouped for onboarding THEN result contains all classic`() {
        val classicFirefoxWallpapers = (0..2).map { generateClassicFirefoxWallpaper("firefox$it") }
        val allWallpapers = listOf(Wallpaper.Default) + classicFirefoxWallpapers

        val result = allWallpapers.getWallpapersForOnboarding()

        assertEquals(3, result.count { it.collection.name == classicCollection.name })
        assertTrue(result.contains(Wallpaper.Default))
    }

    @Test
    fun `GIVEN no items in classic collection and some seasonal WHEN grouped for onboarding THEN result contains all seasonal`() {
        val seasonalCollection = getSeasonalCollection("finally fall")
        val seasonalWallpapers = (0..5).map { generateSeasonalWallpaperCollection("${seasonalCollection.name}$it", seasonalCollection.name) }
        val allWallpapers = listOf(Wallpaper.Default) + seasonalWallpapers

        val result = allWallpapers.getWallpapersForOnboarding()

        assertEquals(5, result.count { it.collection.name == "finally fall" })
        assertTrue(result.contains(Wallpaper.Default))
    }

    @Test
    fun `GIVEN no items WHEN grouped for onboarding THEN result contains the default option`() {
        val allWallpapers = listOf(Wallpaper.Default)

        val result = allWallpapers.getWallpapersForOnboarding()

        assertEquals(1, result.size)
        assertTrue(result.contains(Wallpaper.Default))
    }

    private fun generateClassicFirefoxWallpaper(name: String) = Wallpaper(
        name = name,
        textColor = 0L,
        cardColorLight = 0L,
        cardColorDark = 0L,
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
        cardColorLight = 0L,
        cardColorDark = 0L,
        thumbnailFileState = thumbnailState,
        assetsFileState = Wallpaper.ImageFileState.Downloaded,
        collection = getSeasonalCollection(collectionName),
    )
}
