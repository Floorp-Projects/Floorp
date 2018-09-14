/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

import android.app.Activity
import android.content.Context
import android.support.v4.app.FragmentActivity
import android.view.ContextThemeWrapper

fun Context.asActivity() = (this as? ContextThemeWrapper)?.baseContext as? Activity ?: this as? Activity
fun Context.asFragmentActivity() = (this as? ContextThemeWrapper)?.baseContext as? FragmentActivity
        ?: this as? FragmentActivity
