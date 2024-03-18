/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.mozonline

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.text.style.ClickableSpan
import android.view.View

object Position {
    const val POS1 = 1
    const val POS2 = 2
    const val POS3 = 3
}

object ADDR {
    const val URL1 = "https://www.mozilla.org/en-US/MPL/"
    const val URL2 = "https://www.mozilla.org/en-US/foundation/trademarks/policy/"
    const val URL3 = "https://www.mozilla.org/zh-CN/privacy/firefox/"
}

class PrivacyContentSpan(var pos: Int, var context: Context) :
    ClickableSpan() {
    override fun onClick(widget: View) {
        /**
         *  To avoid users directly using fenix by clicking these urls before
         *  they click positive button of privacy notice alert dialog, start
         *  PrivacyContentDisplayActivity to display them.
         */
        val engineViewIntent = Intent(context, PrivacyContentDisplayActivity::class.java)
        engineViewIntent.flags = Intent.FLAG_ACTIVITY_NEW_TASK
        val addr = Bundle()
        when (pos) {
            Position.POS1 -> addr.putString("url", ADDR.URL1)
            Position.POS2 -> addr.putString("url", ADDR.URL2)
            Position.POS3 -> addr.putString("url", ADDR.URL3)
        }
        engineViewIntent.putExtras(addr)
        context.startActivity(engineViewIntent)
    }
}
