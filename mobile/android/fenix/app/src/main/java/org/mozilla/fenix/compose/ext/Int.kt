/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.ext

import androidx.compose.ui.text.intl.Locale
import java.text.NumberFormat
import java.util.Locale as JavaLocale

/**
 * Returns a localized string representation of the value.
 */
fun Int.toLocaleString(): String =
    NumberFormat.getNumberInstance(JavaLocale(Locale.current.language)).format(this)
