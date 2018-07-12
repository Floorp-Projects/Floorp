/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

import android.content.Context
import com.facebook.stetho.Stetho

object StethoWrapper {
    fun init(context: Context) {
        Stetho.initializeWithDefaults(context)
    }
}
