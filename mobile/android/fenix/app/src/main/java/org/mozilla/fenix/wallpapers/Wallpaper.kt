/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wallpapers

import org.mozilla.fenix.R

/**
 * A enum that represents the available wallpapers and their states.
 */
enum class Wallpaper(val drawable: Int, val isDark: Boolean) {
    NONE(drawable = R.attr.homeBackground, isDark = false),
    FIRST(drawable = R.drawable.wallpaper_1, isDark = true),
    SECOND(drawable = R.drawable.wallpaper_2, isDark = false);
}
