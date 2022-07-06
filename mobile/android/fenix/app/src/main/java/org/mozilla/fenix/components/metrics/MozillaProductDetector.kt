/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Context
import android.content.pm.PackageManager
import mozilla.components.support.utils.ext.getPackageInfoCompat
import org.mozilla.fenix.utils.BrowsersCache

object MozillaProductDetector {
    enum class MozillaProducts(val productName: String) {
        // Browsers
        FIREFOX("org.mozilla.firefox"),
        FIREFOX_NIGHTLY("org.mozilla.fennec_aurora"),
        FIREFOX_BETA("org.mozilla.firefox_beta"),
        FIREFOX_FDROID("org.mozilla.fennec_fdroid"),
        FIREFOX_LITE("org.mozilla.rocket"),
        REFERENCE_BROWSER("org.mozilla.reference.browser"),
        REFERENCE_BROWSER_DEBUG("org.mozilla.reference.browser.debug"),
        FENIX("org.mozilla.fenix"),
        FENIX_NIGHTLY("org.mozilla.fenix.nightly"),
        FOCUS("org.mozilla.focus"),
        KLAR("org.mozilla.klar"),

        // Other products
        LOCKWISE("mozilla.lockbox"),
    }

    fun getInstalledMozillaProducts(context: Context): List<String> {
        val mozillaProducts = mutableListOf<String>()

        for (product in MozillaProducts.values()) {
            if (packageIsInstalled(context, product.productName)) { mozillaProducts.add(product.productName) }
        }

        getMozillaBrowserDefault(context)?.let {
            if (!mozillaProducts.contains(it)) {
                mozillaProducts.add(it)
            }
        }

        return mozillaProducts
    }

    fun packageIsInstalled(context: Context, packageName: String): Boolean {
        try {
            context.packageManager.getPackageInfoCompat(packageName, 0)
        } catch (e: PackageManager.NameNotFoundException) {
            return false
        }

        return true
    }

    /**
     * Returns the default browser if and only if it is a Mozilla product.
     */
    fun getMozillaBrowserDefault(context: Context): String? {
        val browserPackageName = BrowsersCache.all(context).defaultBrowser?.packageName
        return if (isMozillaProduct(browserPackageName)) { browserPackageName } else { null }
    }

    // Note: we intentionally do not use a-c `firefoxBrandedBrowser` as this only gives us the first from that list
    private fun isMozillaProduct(packageName: String?): Boolean {
        packageName ?: return false
        return MozillaProducts.values().any { product -> product.productName == packageName }
    }
}
