/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.ext

import android.content.Context
import androidx.appcompat.view.ContextThemeWrapper
import mozilla.components.feature.readerview.R
import mozilla.components.support.test.robolectric.testContext

/**
 * `testContext` wrapped with AppCompat theme.
 *
 * Useful for views that uses theme attributes, for example.
 */
internal val appCompatContext: Context
    get() = ContextThemeWrapper(testContext, R.style.Theme_AppCompat)
