/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.annotation

import android.content.res.Configuration
import androidx.compose.ui.tooling.preview.Preview

/**
 * A wrapper annotation for the two uiMode that are commonly used
 * in Compose preview functions.
 */
@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES)
@Preview(uiMode = Configuration.UI_MODE_NIGHT_NO)
annotation class LightDarkPreview
