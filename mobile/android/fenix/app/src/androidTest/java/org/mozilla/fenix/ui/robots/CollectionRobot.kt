/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.hasContentDescription
import androidx.compose.ui.test.hasText
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performTouchInput
import androidx.compose.ui.test.swipeLeft
import androidx.compose.ui.test.swipeRight
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.pressImeActionButton
import androidx.test.espresso.matcher.RootMatchers
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.junit.Assert.assertTrue
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertItemContainingTextExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithDescriptionExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithResIdExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.TestHelper.scrollToElementByText
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull

class CollectionRobot {

    fun verifySelectCollectionScreen() {
        assertItemContainingTextExists(
            itemContainingText("Select collection"),
            itemContainingText("Add new collection"),
        )
        assertItemWithResIdExists(itemWithResId("$packageName:id/collections_list"))
    }

    fun clickAddNewCollection() = addNewCollectionButton().click()

    fun verifyCollectionNameTextField() = assertItemWithResIdExists(mainMenuEditCollectionNameField())

    // names a collection saved from tab drawer
    fun typeCollectionNameAndSave(collectionName: String) {
        collectionNameTextField().text = collectionName
        addCollectionButtonPanel.waitForExists(waitingTime)
        addCollectionOkButton.click()
    }

    fun verifyTabsSelectedCounterText(numOfTabs: Int) {
        mDevice.findObject(UiSelector().text("Select tabs to save"))
            .waitUntilGone(waitingTime)

        val tabsCounter = mDevice.findObject(UiSelector().resourceId("$packageName:id/bottom_bar_text"))
        when (numOfTabs) {
            1 -> assertTrue(tabsCounter.text.equals("$numOfTabs tab selected"))
            2 -> assertTrue(tabsCounter.text.equals("$numOfTabs tabs selected"))
        }
    }

    fun saveTabsSelectedForCollection() {
        mDevice.findObject(UiSelector().resourceId("$packageName:id/save_button")).click()
    }

    fun verifyTabSavedInCollection(title: String, visible: Boolean = true) {
        if (visible) {
            scrollToElementByText(title)
            assertItemContainingTextExists(collectionListItem(title))
        } else {
            assertTrue(
                collectionListItem(title).waitUntilGone(waitingTime),
            )
        }
    }

    fun verifyCollectionTabUrl(visible: Boolean, url: String) =
        assertItemContainingTextExists(itemContainingText(url), exists = visible)

    fun verifyShareCollectionButtonIsVisible(visible: Boolean) =
        assertItemWithDescriptionExists(shareCollectionButton(), exists = visible)

    fun verifyCollectionMenuIsVisible(visible: Boolean, rule: ComposeTestRule) {
        if (visible) {
            collectionThreeDotButton(rule)
                .assertExists()
                .assertIsDisplayed()
        } else {
            collectionThreeDotButton(rule)
                .assertDoesNotExist()
        }
    }

    fun clickCollectionThreeDotButton(rule: ComposeTestRule) {
        collectionThreeDotButton(rule)
            .assertIsDisplayed()
            .performClick()
    }

    fun selectOpenTabs(rule: ComposeTestRule) {
        rule.onNode(hasText("Open tabs"))
            .assertIsDisplayed()
            .performClick()
    }

    fun selectRenameCollection(rule: ComposeTestRule) {
        rule.onNode(hasText("Rename collection"))
            .assertIsDisplayed()
            .performClick()
        mainMenuEditCollectionNameField().waitForExists(waitingTime)
    }

    fun selectAddTabToCollection(rule: ComposeTestRule) {
        rule.onNode(hasText("Add tab"))
            .assertIsDisplayed()
            .performClick()

        mDevice.waitNotNull(Until.findObject(By.text("Select Tabs")))
    }

    fun selectDeleteCollection(rule: ComposeTestRule) {
        rule.onNode(hasText("Delete collection"))
            .assertIsDisplayed()
            .performClick()
    }

    fun verifyCollectionItemRemoveButtonIsVisible(title: String, visible: Boolean) =
        assertItemContainingTextExists(removeTabFromCollectionButton(title), exists = visible)

    fun removeTabFromCollection(title: String) = removeTabFromCollectionButton(title).click()

    fun swipeTabLeft(title: String, rule: ComposeTestRule) {
        rule.onNode(hasText(title), useUnmergedTree = true)
            .performTouchInput { swipeLeft() }
        rule.waitForIdle()
    }

    fun swipeTabRight(title: String, rule: ComposeTestRule) {
        rule.onNode(hasText(title), useUnmergedTree = true)
            .performTouchInput { swipeRight() }
        rule.waitForIdle()
    }

    fun verifySnackBarText(expectedText: String) {
        mDevice.findObject(UiSelector().text(expectedText)).waitForExists(waitingTime)
    }

    fun goBackInCollectionFlow() = backButton().click()

    fun swipeToBottom() =
        UiScrollable(
            UiSelector().resourceId("$packageName:id/sessionControlRecyclerView"),
        ).scrollToEnd(3)

    class Transition {
        fun collapseCollection(
            title: String,
            interact: HomeScreenRobot.() -> Unit,
        ): HomeScreenRobot.Transition {
            assertItemContainingTextExists(itemContainingText(title))
            itemContainingText(title).clickAndWaitForNewWindow(waitingTimeShort)
            assertItemWithDescriptionExists(itemWithDescription(getStringResource(R.string.remove_tab_from_collection)), exists = false)

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        // names a collection saved from the 3dot menu
        fun typeCollectionNameAndSave(
            name: String,
            interact: BrowserRobot.() -> Unit,
        ): BrowserRobot.Transition {
            mainMenuEditCollectionNameField().waitForExists(waitingTime)
            mainMenuEditCollectionNameField().text = name
            onView(withId(R.id.name_collection_edittext)).perform(pressImeActionButton())

            // wait for the collection creation wrapper to be dismissed
            mDevice.waitNotNull(Until.gone(By.res("$packageName:id/createCollectionWrapper")))

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun selectExistingCollection(
            title: String,
            interact: BrowserRobot.() -> Unit,
        ): BrowserRobot.Transition {
            collectionTitle(title).waitForExists(waitingTime)
            collectionTitle(title).click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickShareCollectionButton(interact: ShareOverlayRobot.() -> Unit): ShareOverlayRobot.Transition {
            shareCollectionButton().waitForExists(waitingTime)
            shareCollectionButton().click()

            ShareOverlayRobot().interact()
            return ShareOverlayRobot.Transition()
        }
    }
}

fun collectionRobot(interact: CollectionRobot.() -> Unit): CollectionRobot.Transition {
    CollectionRobot().interact()
    return CollectionRobot.Transition()
}

private fun collectionTitle(title: String) =
    mDevice.findObject(
        UiSelector()
            .text(title),
    )

private fun collectionThreeDotButton(rule: ComposeTestRule) =
    rule.onNode(hasContentDescription("Collection menu"))

private fun collectionListItem(title: String) = mDevice.findObject(UiSelector().text(title))

private fun shareCollectionButton() =
    mDevice.findObject(
        UiSelector().description("Share"),
    )

private fun removeTabFromCollectionButton(title: String) =
    mDevice.findObject(
        UiSelector().text(title),
    ).getFromParent(
        UiSelector()
            .description("Remove tab from collection"),
    )

// collection name text field, opened from tab drawer
private fun collectionNameTextField() =
    mDevice.findObject(
        UiSelector().resourceId("$packageName:id/collection_name"),
    )

// collection name text field, when saving from the main menu option
private fun mainMenuEditCollectionNameField() =
    mDevice.findObject(
        UiSelector().resourceId("$packageName:id/name_collection_edittext"),
    )

private fun addNewCollectionButton() =
    mDevice.findObject(UiSelector().text("Add new collection"))

private fun backButton() =
    mDevice.findObject(
        UiSelector().resourceId("$packageName:id/back_button"),
    )
private val addCollectionButtonPanel =
    itemWithResId("$packageName:id/buttonPanel")

private val addCollectionOkButton = onView(withId(android.R.id.button1)).inRoot(RootMatchers.isDialog())
