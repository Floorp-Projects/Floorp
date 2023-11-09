/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import android.util.Log
import androidx.test.uiautomator.UiObject
import androidx.test.uiautomator.UiSelector
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper.mDevice

/**
 * Helper for querying and interacting with items based on their matchers.
 */
object MatcherHelper {

    fun itemWithResId(resourceId: String): UiObject {
        Log.i(TAG, "Looking for item with resource id: $resourceId")
        return mDevice.findObject(UiSelector().resourceId(resourceId))
    }

    fun itemContainingText(itemText: String): UiObject {
        Log.i(TAG, "Looking for item with text: $itemText")
        return mDevice.findObject(UiSelector().textContains(itemText))
    }

    fun itemWithText(itemText: String): UiObject {
        Log.i(TAG, "Looking for item with text: $itemText")
        return mDevice.findObject(UiSelector().text(itemText))
    }

    fun itemWithDescription(description: String): UiObject {
        Log.i(TAG, "Looking for item with description: $description")
        return mDevice.findObject(UiSelector().descriptionContains(description))
    }

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

    fun itemWithResIdContainingText(resourceId: String, text: String) =
        mDevice.findObject(UiSelector().resourceId(resourceId).textContains(text))

    fun assertItemWithResIdExists(vararg appItems: UiObject, exists: Boolean = true) {
        if (exists) {
            for (appItem in appItems) {
                assertTrue("${appItem.selector} does not exist", appItem.waitForExists(waitingTime))
                Log.i(TAG, "assertItemWithResIdExists: Verified ${appItem.selector} exists")
            }
        } else {
            for (appItem in appItems) {
                assertFalse("${appItem.selector} exists", appItem.waitForExists(waitingTimeShort))
                Log.i(TAG, "assertItemWithResIdExists: Verified ${appItem.selector} does not exist")
            }
        }
    }

    fun assertItemContainingTextExists(vararg appItems: UiObject, exists: Boolean = true) {
        for (appItem in appItems) {
            if (exists) {
                assertTrue("${appItem.selector} does not exist", appItem.waitForExists(waitingTime))
                Log.i(TAG, "assertItemContainingTextExists: Verified ${appItem.selector} exists")
            } else {
                assertFalse("${appItem.selector} exists", appItem.waitForExists(waitingTimeShort))
                Log.i(TAG, "assertItemContainingTextExists: Verified ${appItem.selector} does not exist")
            }
        }
    }

    fun assertItemWithDescriptionExists(vararg appItems: UiObject, exists: Boolean = true) {
        for (appItem in appItems) {
            if (exists) {
                assertTrue("${appItem.selector} does not exist", appItem.waitForExists(waitingTime))
                Log.i(TAG, "assertItemContainingTextExists: Verified ${appItem.selector} exists")
            } else {
                assertFalse("${appItem.selector} exists", appItem.waitForExists(waitingTimeShort))
                Log.i(TAG, "assertItemContainingTextExists: Verified ${appItem.selector} does not exist")
            }
        }
    }

    fun assertCheckedItemWithResIdExists(vararg appItems: UiObject, exists: Boolean = true) {
        for (appItem in appItems) {
            if (exists) {
                assertTrue("${appItem.selector} does not exist", appItem.waitForExists(waitingTime))
                Log.i(TAG, "assertCheckedItemWithResIdExists: Verified ${appItem.selector} exists")
            } else {
                assertFalse("${appItem.selector} exists", appItem.waitForExists(waitingTimeShort))
                Log.i(TAG, "assertItemContainingTextExists: Verified ${appItem.selector} does not exist")
            }
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

    fun assertItemWithResIdAndTextExists(vararg appItems: UiObject, exists: Boolean = true) {
        for (appItem in appItems) {
            if (exists) {
                assertTrue("${appItem.selector} does not exist", appItem.waitForExists(waitingTime))
                Log.i(TAG, "assertItemWithResIdExists: Verified ${appItem.selector} exists")
            } else {
                assertFalse("${appItem.selector} exists", appItem.waitForExists(waitingTimeShort))
                Log.i(TAG, "assertItemWithResIdExists: Verified ${appItem.selector} does not exist")
            }
        }
    }

    fun assertItemIsEnabledAndVisible(vararg appItems: UiObject) {
        for (appItem in appItems) {
            assertTrue(appItem.waitForExists(waitingTime) && appItem.isEnabled)
        }
    }
}
