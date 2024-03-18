/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.ext

/**
 * A helper that will return the default value of -1L for the home fragment navigation if Long is null.
 */
internal fun Long?.orDefault() = this ?: -1L
