/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.ext

import android.content.Context
import androidx.appcompat.R
import androidx.appcompat.view.ContextThemeWrapper
import mozilla.components.support.test.robolectric.testContext

internal val appCompatContext: Context
    get() = ContextThemeWrapper(testContext, R.style.Theme_AppCompat)