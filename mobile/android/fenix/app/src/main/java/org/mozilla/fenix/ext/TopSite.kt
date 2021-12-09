/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import mozilla.components.feature.top.sites.TopSite

/**
 * Returns the type name of the [TopSite].
 */
fun TopSite.name(): String = when (this) {
    is TopSite.Default -> "DEFAULT"
    is TopSite.Frecent -> "FRECENT"
    is TopSite.Pinned -> "PINNED"
    is TopSite.Provided -> "PROVIDED"
}
