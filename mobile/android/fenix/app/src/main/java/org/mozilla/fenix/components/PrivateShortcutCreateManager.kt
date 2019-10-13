/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import androidx.core.content.pm.ShortcutInfoCompat
import androidx.core.content.pm.ShortcutManagerCompat
import androidx.core.graphics.drawable.IconCompat
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.home.intent.StartSearchIntentProcessor
import java.util.UUID
import android.content.pm.ShortcutManager
import android.os.Build
import android.os.Build.VERSION.SDK_INT

/**
 * Handles the creation and existence of pinned shortcuts.
 */
object PrivateShortcutCreateManager {

    fun doesPrivateBrowsingPinnedShortcutExist(context: Context): Boolean {
        return if (SDK_INT >= Build.VERSION_CODES.N_MR1) {
            val pinnedShortcuts = context.getSystemService(ShortcutManager::class.java).pinnedShortcuts
            pinnedShortcuts.any {
                it.intent?.extras?.getString(HomeActivity.OPEN_TO_SEARCH) ==
                StartSearchIntentProcessor.PRIVATE_BROWSING_PINNED_SHORTCUT
            }
        } else
            false
    }

    fun createPrivateShortcut(context: Context) {
        if (!ShortcutManagerCompat.isRequestPinShortcutSupported(context)) return

        val icon = IconCompat.createWithResource(context, R.mipmap.ic_launcher_private_round)
        val shortcut = ShortcutInfoCompat.Builder(context, UUID.randomUUID().toString())
            .setShortLabel(context.getString(R.string.app_name_private))
            .setLongLabel(context.getString(R.string.app_name_private))
            .setIcon(icon)
            .setIntent(Intent(context, HomeActivity::class.java).apply {
                action = Intent.ACTION_VIEW
                flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
                putExtra(HomeActivity.PRIVATE_BROWSING_MODE, true)
                putExtra(
                    HomeActivity.OPEN_TO_SEARCH,
                    StartSearchIntentProcessor.PRIVATE_BROWSING_PINNED_SHORTCUT
                )
            })
            .build()
        val homeScreenIntent = Intent(Intent.ACTION_MAIN)
            .addCategory(Intent.CATEGORY_HOME)
            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        val intentSender = PendingIntent
            .getActivity(context, 0, homeScreenIntent, PendingIntent.FLAG_UPDATE_CURRENT)
            .intentSender
        ShortcutManagerCompat.requestPinShortcut(context, shortcut, intentSender)
    }
}
