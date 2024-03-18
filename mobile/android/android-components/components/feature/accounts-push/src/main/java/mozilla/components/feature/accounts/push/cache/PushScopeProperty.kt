/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push.cache

import android.content.Context
import android.content.SharedPreferences
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.withContext
import mozilla.components.feature.accounts.push.FxaPushSupportFeature
import mozilla.components.feature.accounts.push.PREF_FXA_SCOPE
import mozilla.components.feature.accounts.push.preference
import mozilla.components.feature.push.PushScope
import java.util.UUID

/**
 * An implementation of a [ScopeProperty] that generates and stores a scope in [SharedPreferences].
 */
internal class PushScopeProperty(
    private val context: Context,
    private val coroutineScope: CoroutineScope,
) : ScopeProperty {

    override suspend fun value(): PushScope = withContext(coroutineScope.coroutineContext) {
        val prefs = preference(context)

        // Generate a unique scope if one doesn't exist.
        val randomUuid = UUID.randomUUID().toString().replace("-", "")

        // Return a scope in the format example: "fxa_push_scope_a62d5f27c9d74af4996d057f0e0e9c38"
        val scope = FxaPushSupportFeature.PUSH_SCOPE_PREFIX + randomUuid

        if (!prefs.contains(PREF_FXA_SCOPE)) {
            prefs.edit().putString(PREF_FXA_SCOPE, scope).apply()

            return@withContext scope
        }

        // The default string is non-null, so we can safely cast.
        prefs.getString(PREF_FXA_SCOPE, scope) as String
    }
}
