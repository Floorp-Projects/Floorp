package org.mozilla.fenix.ui

import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.ui.robots.homeScreen

class LibraryMenuTest {

    @get:Rule
    val activityTestRule = HomeActivityTestRule()

    @Test
    fun libraryMenuItemsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openLibrary {
            verifyLibraryView()
            verifyHistoryButton()
            verifyBookmarksButton()
        }
    }

    @Test
    fun closeMenuButtonTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openLibrary {
        }.closeMenu {
            verifyHomeScreen()
        }
    }

    @Test
    fun backButtonTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openLibrary {
        }.goBack {
            verifyHomeScreen()
        }
    }

    @Test
    fun bookmarksButtonTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openLibrary {
        }.openBookmarks {
            verifyBookmarksMenuView()
        }
    }

    @Test
    fun historyButtonTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openLibrary {
        }.openHistory {
            verifyHistoryMenuView()
        }
    }
}
