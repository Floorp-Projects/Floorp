/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector

object TestHelper {
    fun scrollToElementByText(text: String): UiScrollable {
        val appView = UiScrollable(UiSelector().scrollable(true))
        appView.scrollTextIntoView(text)
        return appView
    }
}
