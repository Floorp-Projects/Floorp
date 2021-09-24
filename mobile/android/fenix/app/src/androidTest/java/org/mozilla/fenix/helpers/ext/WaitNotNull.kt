/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers.ext

import androidx.test.uiautomator.SearchCondition
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiObject
import org.junit.Assert.assertNotNull
import org.mozilla.fenix.helpers.TestAssetHelper

/**
 * Blocks the test for [waitTime] miliseconds before continuing.
 *
 * Will cause the test to fail is the condition is not met before the timeout.
 */
fun UiDevice.waitNotNull(
    searchCondition: SearchCondition<*>,
    waitTime: Long = TestAssetHelper.waitingTime
) = assertNotNull(wait(searchCondition, waitTime))

fun UiDevice.waitForAwesomeBarContent(obj: UiObject, waitingTime: Long = TestAssetHelper.waitingTime) {
    this.waitForIdle()
    assertNotNull(obj.waitForExists(waitingTime))
}
