/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.graphics.PointF
import mozilla.components.browser.state.state.TabSessionState

data class TabDragData(val tab: TabSessionState, val dragOffset: PointF)
