/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa

import android.content.Context
import android.content.Intent
import android.content.pm.ShortcutManager
import android.os.Build.VERSION.SDK_INT
import android.os.Build.VERSION_CODES
import androidx.core.content.getSystemService
import androidx.core.content.pm.ShortcutInfoCompat
import androidx.core.content.pm.ShortcutManagerCompat
import androidx.core.graphics.drawable.IconCompat
import androidx.core.net.toUri
import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.feature.pwa.ext.installableManifest

class WebAppShortcutManager(
    private val storage: ManifestStorage
) {

    /**
     * Request to create a new shortcut on the home screen.
     */
    suspend fun requestPinShortcut(context: Context, session: Session) {
        if (ShortcutManagerCompat.isRequestPinShortcutSupported(context)) {
            val manifest = session.installableManifest()
            val shortcut = if (manifest != null) {
                buildWebAppShortcut(context, manifest)
            } else {
                buildBasicShortcut(context, session)
            }

            if (shortcut != null) {
                ShortcutManagerCompat.requestPinShortcut(context, shortcut, null)
            }
        }
    }

    /**
     * Update existing PWA shortcuts with the latest info from web app manifests.
     *
     * Devices before 7.1 do not allow shortcuts to be dynamically updated,
     * so this method will do nothing.
     */
    suspend fun updateShortcuts(context: Context, manifests: List<WebAppManifest>) {
        if (SDK_INT >= VERSION_CODES.N_MR1) {
            context.getSystemService<ShortcutManager>()?.apply {
                val shortcuts = manifests.mapNotNull { buildWebAppShortcut(context, it)?.toShortcutInfo() }
                updateShortcuts(shortcuts)
            }
        }
    }

    /**
     * Create a new basic pinned website shortcut using info from the session.
     */
    fun buildBasicShortcut(context: Context, session: Session): ShortcutInfoCompat? {
        val shortcutIntent = Intent(context, WebAppLauncherActivity::class.java).apply {
            action = WebAppLauncherActivity.INTENT_ACTION
            data = session.url.toUri()
        }

        val builder = ShortcutInfoCompat.Builder(context, session.url)
            .setShortLabel(session.title)
            .setIntent(shortcutIntent)

        session.icon?.let {
            builder.setIcon(IconCompat.createWithBitmap(it))
        }

        return builder.build()
    }

    /**
     * Create a new Progressive Web App shortcut using a web app manifest.
     */
    suspend fun buildWebAppShortcut(context: Context, manifest: WebAppManifest): ShortcutInfoCompat? {
        val shortcutIntent = Intent(context, WebAppLauncherActivity::class.java).apply {
            action = WebAppLauncherActivity.INTENT_ACTION
            data = manifest.startUrl.toUri()
        }

        storage.saveManifest(manifest)

        return ShortcutInfoCompat.Builder(context, manifest.startUrl)
            .setLongLabel(manifest.name)
            .setShortLabel(manifest.shortName ?: manifest.name)
            .setIntent(shortcutIntent)
            .build()
    }

    /**
     * Finds the shortcut associated with the given startUrl.
     * This method can be used to check if a web app was added to the homescreen.
     */
    fun findShortcut(context: Context, startUrl: String) =
        if (SDK_INT >= VERSION_CODES.N_MR1) {
            context.getSystemService<ShortcutManager>()?.pinnedShortcuts?.find { it.id == startUrl }
        } else {
            null
        }

    /**
     * Uninstalls a set of PWAs from the user's device by disabling their
     * shortcuts and removing the associated manifest data.
     *
     * @param startUrls List of manifest startUrls to remove.
     * @param disabledMessage Message to display when a disable shortcut is tapped.
     */
    suspend fun uninstallShortcuts(context: Context, startUrls: List<String>, disabledMessage: String? = null) {
        if (SDK_INT >= VERSION_CODES.N_MR1) {
            context.getSystemService<ShortcutManager>()?.disableShortcuts(startUrls, disabledMessage)
        }
        storage.removeManifests(startUrls)
    }
}
