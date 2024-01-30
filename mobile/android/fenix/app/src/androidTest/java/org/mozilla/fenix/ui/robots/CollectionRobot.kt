/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.util.Log
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
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertItemTextEquals
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectIsGone
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.TestHelper.scrollToElementByText
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull

class CollectionRobot {

    fun verifySelectCollectionScreen() =
        assertUIObjectExists(
            itemContainingText("Select collection"),
            itemContainingText("Add new collection"),
            itemWithResId("$packageName:id/collections_list"),
        )

    fun clickAddNewCollection() {
        addNewCollectionButton().click()
        Log.i(TAG, "clickAddNewCollection: Clicked the add new collection button")
    }

    fun verifyCollectionNameTextField() = assertUIObjectExists(mainMenuEditCollectionNameField())

    // names a collection saved from tab drawer
    fun typeCollectionNameAndSave(collectionName: String) {
        collectionNameTextField().text = collectionName
        Log.i(TAG, "typeCollectionNameAndSave: Collection name text field set to: $collectionName")
        addCollectionButtonPanel().waitForExists(waitingTime)
        addCollectionOkButton().click()
        Log.i(TAG, "typeCollectionNameAndSave: Clicked \"OK\" panel button")
    }

    fun verifyTabsSelectedCounterText(numOfTabs: Int) {
        Log.i(TAG, "verifyTabsSelectedCounterText: Waiting for \"Select tabs to save\" prompt to be gone")
        itemWithText("Select tabs to save").waitUntilGone(waitingTime)

        val tabsCounter = mDevice.findObject(UiSelector().resourceId("$packageName:id/bottom_bar_text"))
        when (numOfTabs) {
            1 -> assertItemTextEquals(tabsCounter, expectedText = "$numOfTabs tab selected")
            2 -> assertItemTextEquals(tabsCounter, expectedText = "$numOfTabs tabs selected")
        }
    }

    fun saveTabsSelectedForCollection() {
        itemWithResId("$packageName:id/save_button").click()
        Log.i(TAG, "saveTabsSelectedForCollection: Clicked \"Save\" button")
    }

    fun verifyTabSavedInCollection(title: String, visible: Boolean = true) {
        if (visible) {
            scrollToElementByText(title)
            assertUIObjectExists(collectionListItem(title))
        } else {
            assertUIObjectIsGone(collectionListItem(title))
        }
    }

    fun verifyCollectionTabUrl(visible: Boolean, url: String) =
        assertUIObjectExists(itemContainingText(url), exists = visible)

    fun verifyShareCollectionButtonIsVisible(visible: Boolean) =
        assertUIObjectExists(shareCollectionButton(), exists = visible)

    fun verifyCollectionMenuIsVisible(visible: Boolean, rule: ComposeTestRule) {
        if (visible) {
            collectionThreeDotButton(rule)
                .assertExists()
                .assertIsDisplayed()
            Log.i(TAG, "verifyCollectionMenuIsVisible: Verified collection three dot button exists")
        } else {
            collectionThreeDotButton(rule)
                .assertDoesNotExist()
            Log.i(TAG, "verifyCollectionMenuIsVisible: Verified collection three dot button does not exist")
        }
    }

    fun clickCollectionThreeDotButton(rule: ComposeTestRule) {
        collectionThreeDotButton(rule)
            .assertIsDisplayed()
            .performClick()
        Log.i(TAG, "clickCollectionThreeDotButton: Clicked three dot button")
    }

    fun selectOpenTabs(rule: ComposeTestRule) {
        rule.onNode(hasText("Open tabs"))
            .assertIsDisplayed()
            .performClick()
        Log.i(TAG, "selectOpenTabs: Clicked \"Open tabs\" menu option")
    }

    fun selectRenameCollection(rule: ComposeTestRule) {
        rule.onNode(hasText("Rename collection"))
            .assertIsDisplayed()
            .performClick()
        Log.i(TAG, "selectRenameCollection: Clicked \"Rename collection\" menu option")
        mainMenuEditCollectionNameField().waitForExists(waitingTime)
    }

    fun selectAddTabToCollection(rule: ComposeTestRule) {
        rule.onNode(hasText("Add tab"))
            .assertIsDisplayed()
            .performClick()
        Log.i(TAG, "selectAddTabToCollection: Clicked \"Add tab\" menu option")

        mDevice.waitNotNull(Until.findObject(By.text("Select Tabs")))
    }

    fun selectDeleteCollection(rule: ComposeTestRule) {
        rule.onNode(hasText("Delete collection"))
            .assertIsDisplayed()
            .performClick()
        Log.i(TAG, "selectDeleteCollection: Clicked \"Delete collection\" menu option")
    }

    fun verifyCollectionItemRemoveButtonIsVisible(title: String, visible: Boolean) =
        assertUIObjectExists(removeTabFromCollectionButton(title), exists = visible)

    fun removeTabFromCollection(title: String) {
        removeTabFromCollectionButton(title).click()
        Log.i(TAG, "removeTabFromCollection: Clicked remove button for tab: $title")
    }

    fun swipeTabLeft(title: String, rule: ComposeTestRule) {
        rule.onNode(hasText(title), useUnmergedTree = true)
            .performTouchInput { swipeLeft() }
        Log.i(TAG, "swipeTabLeft: Removed tab: $title using swipe left action")
        rule.waitForIdle()
        Log.i(TAG, "swipeTabLeft: Waited for rule to be idle")
    }

    fun swipeTabRight(title: String, rule: ComposeTestRule) {
        rule.onNode(hasText(title), useUnmergedTree = true)
            .performTouchInput { swipeRight() }
        Log.i(TAG, "swipeTabRight: Removed tab: $title using swipe right action")
        rule.waitForIdle()
        Log.i(TAG, "swipeTabRight: Waited for rule to be idle")
    }

    fun goBackInCollectionFlow() {
        backButton().click()
        Log.i(TAG, "goBackInCollectionFlow: Clicked collection creation flow back button")
    }

    class Transition {
        fun collapseCollection(
            title: String,
            interact: HomeScreenRobot.() -> Unit,
        ): HomeScreenRobot.Transition {
            assertUIObjectExists(itemContainingText(title))
            itemContainingText(title).clickAndWaitForNewWindow(waitingTimeShort)
            Log.i(TAG, "collapseCollection: Clicked collection $title")
            assertUIObjectExists(itemWithDescription(getStringResource(R.string.remove_tab_from_collection)), exists = false)

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
            Log.i(TAG, "typeCollectionNameAndSave: Collection name text field set to: $name")
            onView(withId(R.id.name_collection_edittext)).perform(pressImeActionButton())
            Log.i(TAG, "typeCollectionNameAndSave: Pressed done action button")

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
            Log.i(TAG, "selectExistingCollection: Clicked collection with title: $title")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickShareCollectionButton(interact: ShareOverlayRobot.() -> Unit): ShareOverlayRobot.Transition {
            shareCollectionButton().waitForExists(waitingTime)
            shareCollectionButton().click()
            Log.i(TAG, "clickShareCollectionButton: Clicked share collection button")

            ShareOverlayRobot().interact()
            return ShareOverlayRobot.Transition()
        }
    }
}

fun collectionRobot(interact: CollectionRobot.() -> Unit): CollectionRobot.Transition {
    CollectionRobot().interact()
    return CollectionRobot.Transition()
}

private fun collectionTitle(title: String) = itemWithText(title)

private fun collectionThreeDotButton(rule: ComposeTestRule) =
    rule.onNode(hasContentDescription("Collection menu"))

private fun collectionListItem(title: String) = mDevice.findObject(UiSelector().text(title))

private fun shareCollectionButton() = itemWithDescription("Share")

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
    itemWithResId("$packageName:id/name_collection_edittext")

private fun addNewCollectionButton() =
    mDevice.findObject(UiSelector().text("Add new collection"))

private fun backButton() =
    mDevice.findObject(
        UiSelector().resourceId("$packageName:id/back_button"),
    )
private fun addCollectionButtonPanel() =
    itemWithResId("$packageName:id/buttonPanel")

private fun addCollectionOkButton() = onView(withId(android.R.id.button1)).inRoot(RootMatchers.isDialog())
