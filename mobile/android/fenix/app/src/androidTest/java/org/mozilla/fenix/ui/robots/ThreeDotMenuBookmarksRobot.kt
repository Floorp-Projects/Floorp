/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.matcher.ViewMatchers.withText
import org.mozilla.fenix.helpers.HomeActivityComposeTestRule
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the Bookmarks three dot menu.
 */
class ThreeDotMenuBookmarksRobot {

    class Transition {

        fun clickEdit(interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
            editButton().click()

            BookmarksRobot().interact()
            return BookmarksRobot.Transition()
        }

        fun clickCopy(interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
            copyButton().click()

            BookmarksRobot().interact()
            return BookmarksRobot.Transition()
        }

        fun clickShare(interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
            shareButton().click()

            BookmarksRobot().interact()
            return BookmarksRobot.Transition()
        }

        fun clickOpenInNewTab(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            openInNewTabButton().click()

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun clickOpenInNewTab(composeTestRule: HomeActivityComposeTestRule, interact: ComposeTabDrawerRobot.() -> Unit): ComposeTabDrawerRobot.Transition {
            openInNewTabButton().click()

            ComposeTabDrawerRobot(composeTestRule).interact()
            return ComposeTabDrawerRobot.Transition(composeTestRule)
        }

        fun clickOpenInPrivateTab(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            openInPrivateTabButton().click()

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun clickOpenInPrivateTab(composeTestRule: HomeActivityComposeTestRule, interact: ComposeTabDrawerRobot.() -> Unit): ComposeTabDrawerRobot.Transition {
            openInPrivateTabButton().click()

            ComposeTabDrawerRobot(composeTestRule).interact()
            return ComposeTabDrawerRobot.Transition(composeTestRule)
        }

        fun clickOpenAllInTabs(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            openAllInTabsButton().click()

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun clickOpenAllInTabs(composeTestRule: HomeActivityComposeTestRule, interact: ComposeTabDrawerRobot.() -> Unit): ComposeTabDrawerRobot.Transition {
            openAllInTabsButton().click()

            ComposeTabDrawerRobot(composeTestRule).interact()
            return ComposeTabDrawerRobot.Transition(composeTestRule)
        }

        fun clickOpenAllInPrivateTabs(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            openAllInPrivateTabsButton().click()

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun clickOpenAllInPrivateTabs(composeTestRule: HomeActivityComposeTestRule, interact: ComposeTabDrawerRobot.() -> Unit): ComposeTabDrawerRobot.Transition {
            openAllInPrivateTabsButton().click()

            ComposeTabDrawerRobot(composeTestRule).interact()
            return ComposeTabDrawerRobot.Transition(composeTestRule)
        }

        fun clickDelete(interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
            deleteButton().click()

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
