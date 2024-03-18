/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.matcher.ViewMatchers.withText
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.HomeActivityComposeTestRule
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the Bookmarks three dot menu.
 */
class ThreeDotMenuBookmarksRobot {

    class Transition {

        fun clickEdit(interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
            Log.i(TAG, "clickEdit: Trying to click the \"Edit\" button")
            editButton().click()
            Log.i(TAG, "clickEdit: Clicked the \"Edit\" button")

            BookmarksRobot().interact()
            return BookmarksRobot.Transition()
        }

        fun clickCopy(interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
            Log.i(TAG, "clickCopy: Trying to click the \"Copy\" button")
            copyButton().click()
            Log.i(TAG, "clickCopy: Clicked the \"Copy\" button")

            BookmarksRobot().interact()
            return BookmarksRobot.Transition()
        }

        fun clickShare(interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
            Log.i(TAG, "clickShare: Trying to click the \"Share\" button")
            shareButton().click()
            Log.i(TAG, "clickShare: Clicked the \"Share\" button")

            BookmarksRobot().interact()
            return BookmarksRobot.Transition()
        }

        fun clickOpenInNewTab(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            Log.i(TAG, "clickOpenInNewTab: Trying to click the \"Open in new tab\" button")
            openInNewTabButton().click()
            Log.i(TAG, "clickOpenInNewTab: Clicked the \"Open in new tab\" button")

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun clickOpenInNewTab(composeTestRule: HomeActivityComposeTestRule, interact: ComposeTabDrawerRobot.() -> Unit): ComposeTabDrawerRobot.Transition {
            Log.i(TAG, "clickOpenInNewTab: Trying to click the \"Open in new tab\" button")
            openInNewTabButton().click()
            Log.i(TAG, "clickOpenInNewTab: Clicked the \"Open in new tab\" button")

            ComposeTabDrawerRobot(composeTestRule).interact()
            return ComposeTabDrawerRobot.Transition(composeTestRule)
        }

        fun clickOpenInPrivateTab(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            Log.i(TAG, "clickOpenInPrivateTab: Trying to click the \"Open in private tab\" button")
            openInPrivateTabButton().click()
            Log.i(TAG, "clickOpenInPrivateTab: Clicked the \"Open in private tab\" button")

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun clickOpenInPrivateTab(composeTestRule: HomeActivityComposeTestRule, interact: ComposeTabDrawerRobot.() -> Unit): ComposeTabDrawerRobot.Transition {
            Log.i(TAG, "clickOpenInPrivateTab: Trying to click the \"Open in private tab\" button")
            openInPrivateTabButton().click()
            Log.i(TAG, "clickOpenInPrivateTab: Clicked the \"Open in private tab\" button")

            ComposeTabDrawerRobot(composeTestRule).interact()
            return ComposeTabDrawerRobot.Transition(composeTestRule)
        }

        fun clickOpenAllInTabs(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            Log.i(TAG, "clickOpenAllInTabs: Trying to click the \"Open all in new tabs\" button")
            openAllInTabsButton().click()
            Log.i(TAG, "clickOpenAllInTabs: Clicked the \"Open all in new tabs\" button")

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun clickOpenAllInTabs(composeTestRule: HomeActivityComposeTestRule, interact: ComposeTabDrawerRobot.() -> Unit): ComposeTabDrawerRobot.Transition {
            Log.i(TAG, "clickOpenAllInTabs: Trying to click the \"Open all in new tabs\" button")
            openAllInTabsButton().click()
            Log.i(TAG, "clickOpenAllInTabs: Clicked the \"Open all in new tabs\" button")

            ComposeTabDrawerRobot(composeTestRule).interact()
            return ComposeTabDrawerRobot.Transition(composeTestRule)
        }

        fun clickOpenAllInPrivateTabs(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            Log.i(TAG, "clickOpenAllInPrivateTabs: Trying to click the \"Open all in private tabs\" button")
            openAllInPrivateTabsButton().click()
            Log.i(TAG, "clickOpenAllInPrivateTabs: Clicked the \"Open all in private tabs\" button")

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun clickOpenAllInPrivateTabs(composeTestRule: HomeActivityComposeTestRule, interact: ComposeTabDrawerRobot.() -> Unit): ComposeTabDrawerRobot.Transition {
            Log.i(TAG, "clickOpenAllInPrivateTabs: Trying to click the \"Open all in private tabs\" button")
            openAllInPrivateTabsButton().click()
            Log.i(TAG, "clickOpenAllInPrivateTabs: Clicked the \"Open all in private tabs\" button")

            ComposeTabDrawerRobot(composeTestRule).interact()
            return ComposeTabDrawerRobot.Transition(composeTestRule)
        }

        fun clickDelete(interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
            Log.i(TAG, "clickDelete: Trying to click the \"Delete\" button")
            deleteButton().click()
            Log.i(TAG, "clickDelete: Clicked the \"Delete\" button")

            BookmarksRobot().interact()
            return BookmarksRobot.Transition()
        }
    }
}

private fun editButton() = onView(withText("Edit"))

private fun copyButton() = onView(withText("Copy"))

private fun shareButton() = onView(withText("Share"))

private fun openInNewTabButton() = onView(withText("Open in new tab"))

private fun openInPrivateTabButton() = onView(withText("Open in private tab"))

private fun openAllInTabsButton() = onView(withText("Open all in new tabs"))

private fun openAllInPrivateTabsButton() = onView(withText("Open all in private tabs"))

private fun deleteButton() = onView(withText("Delete"))
