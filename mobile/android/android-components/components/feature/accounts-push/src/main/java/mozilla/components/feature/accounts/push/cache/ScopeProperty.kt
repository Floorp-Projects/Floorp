/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push.cache

import mozilla.components.feature.push.PushScope

/**
 * A [ScopeProperty] implementation generates and holds the [PushScope].
 */
interface ScopeProperty {

    /**
     * Returns the [PushScope] value.
     */
    suspend fun value(): PushScope
}
