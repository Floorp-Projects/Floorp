/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.ui.ext

import androidx.annotation.StringRes
import androidx.compose.runtime.Composable
import androidx.compose.runtime.ReadOnlyComposable
import androidx.compose.ui.res.stringResource
import org.mozilla.fenix.R

/**
 * Returns a string resource with the heading a11y suffix.
 *
 * @param id The string resource id.
 */
@Composable
@ReadOnlyComposable
fun headingResource(@StringRes id: Int): String {
    return headingResource(text = stringResource(id = id))
}

/**
 * Returns a string with the heading a11y suffix.
 *
 * @param text The string.
 */
@Composable
@ReadOnlyComposable
fun headingResource(text: String): String {
    return stringResource(id = R.string.a11y_heading, text)
}
