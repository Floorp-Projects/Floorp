package org.mozilla.focus.utils

import android.app.Activity
import android.content.Context
import android.view.ContextThemeWrapper

fun Context.asActivity() = (this as? ContextThemeWrapper)?.baseContext as? Activity ?: this as? Activity
