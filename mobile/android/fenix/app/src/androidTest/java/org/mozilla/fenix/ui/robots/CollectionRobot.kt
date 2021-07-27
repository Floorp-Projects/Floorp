package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.NoMatchingViewException
import androidx.test.espresso.action.ViewActions.pressImeActionButton
import androidx.test.espresso.action.ViewActions.replaceText
import androidx.test.espresso.action.ViewActions.swipeLeft
import androidx.test.espresso.action.ViewActions.swipeRight
import androidx.test.espresso.assertion.ViewAssertions.doesNotExist
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.hamcrest.Matchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.TestHelper.scrollToElementByText
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull

class CollectionRobot {

    fun verifySelectCollectionScreen() {
        onView(withText("Select collection"))
            .check(matches(isDisplayed()))

        onView(withId(R.id.collections_list))
            .check(matches(isDisplayed()))

        onView(withText("Add new collection"))
            .check(matches(isDisplayed()))
    }

    fun clickAddNewCollection() = addNewCollectionButton().click()

    fun verifyCollectionNameTextField() {
        mainMenuEditCollectionNameField().check(matches(isDisplayed()))
    }

    // names a collection saved from tab drawer
    fun typeCollectionNameAndSave(collectionName: String) {
        collectionNameTextField().perform(replaceText(collectionName))
        mDevice.findObject(UiSelector().textContains("OK")).click()
    }

    fun verifyTabsSelectedCounterText(numOfTabs: Int) {
        mDevice.findObject(UiSelector().text("Select tabs to save"))
            .waitUntilGone(waitingTime)

        val tabsCounter = onView(withId(R.id.bottom_bar_text))
        when (numOfTabs) {
            1 -> tabsCounter.check(matches(withText("$numOfTabs tab selected")))
            2 -> tabsCounter.check(matches(withText("$numOfTabs tabs selected")))
        }
    }

    fun saveTabsSelectedForCollection() {
        onView(withId(R.id.save_button)).click()
    }

    fun verifyTabSavedInCollection(title: String, visible: Boolean = true) {
        if (visible) {
            scrollToElementByText(title)
            collectionItem(title)
                .check(
                    matches(isDisplayed())
                )
        } else
            collectionItem(title)
                .check(doesNotExist())
    }

    fun verifyCollectionTabUrl() {
        onView(withId(R.id.caption)).check(matches(isDisplayed()))
    }

    fun verifyCollectionTabLogo() {
        onView(withId(R.id.favicon)).check(matches(isDisplayed()))
    }

    fun verifyShareCollectionButtonIsVisible(visible: Boolean) {
        shareCollectionButton()
            .check(
                if (visible) matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE))
                else matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE))
            )
    }

    fun clickShareCollectionButton() = onView(withId(R.id.collection_share_button)).click()

    fun verifyCollectionMenuIsVisible(visible: Boolean) {
        collectionThreeDotButton()
            .check(
                if (visible) matches(
                    withEffectiveVisibility(
                        ViewMatchers.Visibility.VISIBLE
                    )
                )
                else matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE))
            )
    }

    fun clickCollectionThreeDotButton() {
        collectionThreeDotButton().click()
        mDevice.waitNotNull(
            Until.findObject(By.text("Delete collection")),
            waitingTime
        )
    }

    fun selectOpenTabs() {
        onView(withText("Open tabs")).click()
    }

    fun selectRenameCollection() {
        onView(withText("Rename collection")).click()
        mDevice.waitNotNull(Until.findObject(By.text("Rename collection")))
    }

    fun selectAddTabToCollection() {
        onView(withText("Add tab")).click()
        mDevice.waitNotNull(Until.findObject(By.text("Select Tabs")))
    }

    fun selectDeleteCollection() {
        onView(withText("Delete collection")).click()
    }

    fun confirmDeleteCollection() {
        onView(withText("DELETE")).click()
        mDevice.waitNotNull(
            Until.findObject(By.res("$packageName:id/no_collections_header")),
            waitingTime
        )
    }

    fun verifyCollectionItemRemoveButtonIsVisible(title: String, visible: Boolean) {
        removeTabFromCollectionButton(title)
            .check(
                if (visible) matches(
                    withEffectiveVisibility(
                        ViewMatchers.Visibility.VISIBLE
                    )
                )
                else doesNotExist()
            )
    }

    fun removeTabFromCollection(title: String) = removeTabFromCollectionButton(title).click()

    fun swipeCollectionItemRight(title: String) {
        scrollToElementByText(title)
        // Swipping can sometimes fail to remove the tab, so if the tab still exists, we need to repeat it
        var retries = 0 // number of retries before failing, will stop at 2
        while (mDevice.findObject(
                UiSelector()
                    .resourceId("$packageName:id/label")
                    .text(title)
            ).exists() && retries < 2
        ) {
            collectionItem(title).perform(swipeRight())
            retries++
        }
    }

    fun swipeCollectionItemLeft(title: String) {
        scrollToElementByText(title)
        // Swipping can sometimes fail to remove the tab, so if the tab still exists, we need to repeat it
        var retries = 0 // number of retries before failing, will stop at 2
        while (mDevice.findObject(
                UiSelector()
                    .resourceId("$packageName:id/label")
                    .text(title)
            ).exists() && retries < 2
        ) {
            collectionItem(title).perform(swipeLeft())
            retries++
        }
    }

    fun verifySnackBarText(expectedText: String) {
        mDevice.findObject(UiSelector().text(expectedText)).waitForExists(waitingTime)
    }

    fun goBackInCollectionFlow() = backButton().click()

    class Transition {
        fun collapseCollection(
            title: String,
            interact: HomeScreenRobot.() -> Unit
        ): HomeScreenRobot.Transition {
            try {
                mDevice.waitNotNull(Until.findObject(By.text(title)), waitingTime)
                onView(allOf(withId(R.id.chevron), hasSibling(withText(title)))).click()
            } catch (e: NoMatchingViewException) {
                scrollToElementByText(title)
            }

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        // names a collection saved from the 3dot menu
        fun typeCollectionNameAndSave(
            name: String,
            interact: BrowserRobot.() -> Unit
        ): BrowserRobot.Transition {
            mDevice.findObject(UiSelector().resourceId("$packageName:id/name_collection_edittext"))
                .waitForExists(waitingTime)

            mainMenuEditCollectionNameField().perform(
                replaceText(name),
                pressImeActionButton()
            )

            // wait for the collection creation wrapper to be dismissed
            mDevice.waitNotNull(Until.gone(By.res("$packageName:id/createCollectionWrapper")))

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun selectExistingCollection(
            title: String,
            interact: BrowserRobot.() -> Unit
        ): BrowserRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.text(title)), waitingTime)
            onView(withText(title)).click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

fun collectionRobot(interact: CollectionRobot.() -> Unit): CollectionRobot.Transition {
    CollectionRobot().interact()
    return CollectionRobot.Transition()
}

private fun collectionThreeDotButton() =
    onView(withId(R.id.collection_overflow_button))

private fun collectionItem(title: String) =
    onView(allOf(withId(R.id.label), withText(title)))

private fun shareCollectionButton() = onView(withId(R.id.collection_share_button))

private fun removeTabFromCollectionButton(title: String) =
    onView(
        allOf(
            withId(R.id.secondary_button),
            hasSibling(withText(title))
        )
    )

// collection name text field, opened from tab drawer
private fun collectionNameTextField() = onView(withId(R.id.collection_name))

// collection name text field, opened from main menu
private fun mainMenuEditCollectionNameField() =
    onView(withId(R.id.name_collection_edittext))

private fun addNewCollectionButton() = onView(withText("Add new collection"))

private fun backButton() =
    onView(withId(R.id.back_button))
