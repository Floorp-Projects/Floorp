/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import androidx.test.uiautomator.UiObject
import androidx.test.uiautomator.UiSelector
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.mDevice

/**
 * Helper for querying and interacting with items based on their matchers.
 */
object MatcherHelper {

    fun itemWithResId(resourceId: String) =
        mDevice.findObject(UiSelector().resourceId(resourceId))

    fun itemContainingText(itemText: String) =
        mDevice.findObject(UiSelector().textContains(itemText))

    fun itemWithDescription(description: String) =
        mDevice.findObject(UiSelector().descriptionContains(description))

    fun checkedItemWithResId(resourceId: String, isChecked: Boolean) =
        mDevice.findObject(UiSelector().resourceId(resourceId).checked(isChecked))

    fun checkedItemWithResIdAndText(resourceId: String, text: String, isChecked: Boolean) =
        mDevice.findObject(
            UiSelector()
                .resourceId(resourceId)
                .textContains(text)
                .checked(isChecked),
        )

    fun itemWithResIdAndDescription(resourceId: String, description: String) =
        mDevice.findObject(UiSelector().resourceId(resourceId).descriptionContains(description))

    fun itemWithResIdAndText(resourceId: String, text: String) =
        mDevice.findObject(UiSelector().resourceId(resourceId).text(text))

    fun assertItemWithResIdExists(vararg appItems: UiObject, exists: Boolean = true) {
        if (exists) {
            for (appItem in appItems) {
                assertTrue(appItem.waitForExists(waitingTime))
            }
        } else {
            for (appItem in appItems) {
                assertFalse(appItem.waitForExists(waitingTime))
            }
        }
    }

    fun assertItemContainingTextExists(vararg appItems: UiObject) {
        for (appItem in appItems) {
            assertTrue(appItem.waitForExists(waitingTime))
        }
    }

    fun assertItemWithDescriptionExists(vararg appItems: UiObject) {
        for (appItem in appItems) {
            assertTrue(appItem.waitForExists(waitingTime))
        }
    }

    fun assertCheckedItemWithResIdExists(vararg appItems: UiObject) {
        for (appItem in appItems) {
            assertTrue(appItem.waitForExists(waitingTime))
        }
    }

    fun assertCheckedItemWithResIdAndTextExists(vararg appItems: UiObject) {
        for (appItem in appItems) {
            assertTrue(appItem.waitForExists(waitingTime))
        }
    }

    fun assertItemWithResIdAndDescriptionExists(vararg appItems: UiObject) {
        for (appItem in appItems) {
            assertTrue(appItem.waitForExists(waitingTime))
        }
    }

    fun assertItemWithResIdAndTextExists(vararg appItems: UiObject) {
        for (appItem in appItems) {
            assertTrue(appItem.waitForExists(waitingTime))
        }
    }

    fun assertItemIsEnabledAndVisible(vararg appItems: UiObject) {
        for (appItem in appItems) {
            assertTrue(appItem.waitForExists(waitingTime) && appItem.isEnabled)
        }
    }
}
