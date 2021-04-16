/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.ext

import org.mozilla.fenix.tabstray.TabsTrayState.Mode

/**
 * A helper to check if we're in [Mode.Select] mode.
 */
fun Mode.isSelect() = this is Mode.Select
