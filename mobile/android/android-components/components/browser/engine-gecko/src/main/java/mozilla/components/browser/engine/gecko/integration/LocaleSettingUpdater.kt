/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.integration

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import org.mozilla.geckoview.GeckoRuntime
import androidx.core.os.LocaleListCompat as LocaleList

/**
 * Class to set the locales setting for geckoview, updating from the locale of the device.
 */
class LocaleSettingUpdater(
    private val context: Context,
    private val runtime: GeckoRuntime,
) : SettingUpdater<Array<String>>() {

    override var value: Array<String> = findValue()
        set(value) {
            runtime.settings.locales = value
            field = value
        }

    private val localeChangedReceiver by lazy {
        object : BroadcastReceiver() {
            override fun onReceive(context: Context, intent: Intent?) {
                updateValue()
            }
        }
    }

    override fun registerForUpdates() {
        context.registerReceiver(localeChangedReceiver, IntentFilter(Intent.ACTION_LOCALE_CHANGED))
    }

    override fun unregisterForUpdates() {
        context.unregisterReceiver(localeChangedReceiver)
    }

    override fun findValue(): Array<String> {
        val localeList = LocaleList.getAdjustedDefault()
        return arrayOfNulls<Unit>(localeList.size())
            .mapIndexedNotNull { i, _ -> localeList.get(i)?.toLanguageTag() }
            .toTypedArray()
    }
}
