/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.packageName
import org.mozilla.focus.helpers.TestHelper.waitingTime

class AddToHomeScreenRobot {

    fun handleAddAutomaticallyDialog() {
        if (addAutomaticallyBtn.waitForExists(waitingTime)) {
            addAutomaticallyBtn.click()
            addAutomaticallyBtn.waitUntilGone(waitingTime)
        }
    }

    fun addShortcutWithTitle(title: String) {
        shortcutTitle.waitForExists(waitingTime)
        shortcutTitle.clearTextField()
        shortcutTitle.text = title
        addToHSOKBtn.click()
    }

    fun addShortcutNoTitle() {
        shortcutTitle.waitForExists(waitingTime)
        shortcutTitle.clearTextField()
        addToHSOKBtn.click()
    }

    class Transition {
        // Searches a page shortcut on the device homescreen
        fun searchAndOpenHomeScreenShortcut(title: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitForIdle(waitingTime)
            mDevice.pressHome()

            fun deviceHomeScreen() = UiScrollable(UiSelector().scrollable(true))
            deviceHomeScreen().setAsHorizontalList()

            fun shortcut() =
                deviceHomeScreen()
                    .getChildByText(UiSelector().text(title), title, true)
            shortcut().waitForExists(waitingTime)
            shortcut().clickAndWaitForNewWindow()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

private val addToHSOKBtn = mDevice.findObject(
    UiSelector()
        .resourceId("$packageName:id/addtohomescreen_dialog_add")
        .enabled(true)
)

private val addAutomaticallyBtn = mDevice.findObject(
    UiSelector()
        .className("android.widget.Button")
        .textContains("Add automatically")
)

private val shortcutTitle = mDevice.findObject(
    UiSelector()
        .resourceId("$packageName:id/edit_title")
)
