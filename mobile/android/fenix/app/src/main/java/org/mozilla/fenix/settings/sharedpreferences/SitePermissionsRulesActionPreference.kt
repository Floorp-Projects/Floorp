/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.sharedpreferences

import mozilla.components.feature.sitepermissions.SitePermissionsRules
import java.security.InvalidParameterException
import kotlin.properties.ReadWriteProperty
import kotlin.reflect.KProperty

private class SitePermissionsRulesActionPreference(
    private val key: String
) : ReadWriteProperty<PreferencesHolder, SitePermissionsRules.Action> {

    override fun getValue(thisRef: PreferencesHolder, property: KProperty<*>): SitePermissionsRules.Action =
        intToAction(thisRef.preferences.getInt(key, ASK_TO_ALLOW_INT))

    override fun setValue(thisRef: PreferencesHolder, property: KProperty<*>, value: SitePermissionsRules.Action) {
        thisRef.preferences.edit().putInt(key, actionToInt(value)).apply()
    }

    companion object {
        private const val BLOCKED_INT = 0
        private const val ASK_TO_ALLOW_INT = 1

        private fun actionToInt(action: SitePermissionsRules.Action) = when (action) {
            SitePermissionsRules.Action.BLOCKED -> BLOCKED_INT
            SitePermissionsRules.Action.ASK_TO_ALLOW -> ASK_TO_ALLOW_INT
        }

        private fun intToAction(action: Int) = when (action) {
            BLOCKED_INT -> SitePermissionsRules.Action.BLOCKED
            ASK_TO_ALLOW_INT -> SitePermissionsRules.Action.ASK_TO_ALLOW
            else -> throw InvalidParameterException("$action is not a valid SitePermissionsRules.Action")
        }
    }
}

/**
 * Property delegate for getting and setting a [SitePermissionsRules.Action] preference.
 */
fun sitePermissionsRulesActionPreference(
    key: String
): ReadWriteProperty<PreferencesHolder, SitePermissionsRules.Action> = SitePermissionsRulesActionPreference(key)
