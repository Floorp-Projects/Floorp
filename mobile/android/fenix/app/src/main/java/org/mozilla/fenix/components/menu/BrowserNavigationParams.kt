/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu

import org.mozilla.fenix.settings.SupportUtils.SumoTopic

/**
 * Browser navigation parameters of the URL or [SumoTopic] to be loaded.
 *
 * @property url The URL to be loaded.
 * @property sumoTopic The [SumoTopic] to be loaded.
 */
data class BrowserNavigationParams(
    val url: String? = null,
    val sumoTopic: SumoTopic? = null,
)
