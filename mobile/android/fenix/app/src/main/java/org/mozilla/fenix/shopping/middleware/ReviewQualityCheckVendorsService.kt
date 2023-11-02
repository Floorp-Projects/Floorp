/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.ProductVendor
import java.net.URI
import java.net.URISyntaxException

private const val AMAZON_COM = "amazon.com"
private const val BEST_BUY_COM = "bestbuy.com"
private const val WALMART_COM = "walmart.com"
private val defaultVendorsList = enumValues<ProductVendor>().toList()

/**
 * Service for getting the list of product vendors.
 */
interface ReviewQualityCheckVendorsService {

    /**
     * Returns the list of product vendors in order.
     */
    fun productVendors(): List<ProductVendor>
}

/**
 * Default implementation of [ReviewQualityCheckVendorsService] that uses the [BrowserStore] to
 * identify the selected tab.
 *
 * @param browserStore The [BrowserStore] instance to use.
 */
class DefaultReviewQualityCheckVendorsService(
    private val browserStore: BrowserStore,
) : ReviewQualityCheckVendorsService {

    override fun productVendors(): List<ProductVendor> {
        val selectedTabUrl = browserStore.state.selectedTab?.content?.url

        return if (selectedTabUrl == null) {
            defaultVendorsList
        } else {
            val host = selectedTabUrl.toJavaUri()?.host
            when {
                host == null -> defaultVendorsList
                host.contains(AMAZON_COM) -> createProductVendorsList(ProductVendor.AMAZON)
                host.contains(BEST_BUY_COM) -> createProductVendorsList(ProductVendor.BEST_BUY)
                host.contains(WALMART_COM) -> createProductVendorsList(ProductVendor.WALMART)
                else -> defaultVendorsList
            }
        }
    }

    /**
     * Creates list of product vendors using the firstVendor param as the first item in the list.
     */
    private fun createProductVendorsList(firstVendor: ProductVendor): List<ProductVendor> =
        listOf(firstVendor) + defaultVendorsList.filterNot { it == firstVendor }

    /**
     * Convenience function to converts a given string to a [URI] instance. Returns null if the
     * string is not a valid URI.
     */
    private fun String.toJavaUri(): URI? {
        return try {
            URI.create(this)
        } catch (e: URISyntaxException) {
            Logger.error("Unable to create URI with the given string $this", e)
            null
        } catch (e: IllegalArgumentException) {
            Logger.error("Unable to create URI with the given string $this", e)
            null
        }
    }
}

/**
 * Returns the first matching product vendor for the selected tab.
 */
fun ReviewQualityCheckVendorsService.productVendor(): ProductVendor =
    productVendors().first()
