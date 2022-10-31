/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.test.ext

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.appcompat.R
import androidx.appcompat.view.ContextThemeWrapper
import mozilla.components.support.test.robolectric.testContext

/**
 * `testContext` wrapped with AppCompat theme.
 *
 * Useful for views that uses theme attributes, for example.
 */
@VisibleForTesting val appCompatContext: Context
    get() = ContextThemeWrapper(testContext, R.style.Theme_AppCompat)
