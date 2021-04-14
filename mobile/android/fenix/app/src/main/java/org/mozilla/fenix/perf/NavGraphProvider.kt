/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import androidx.lifecycle.LifecycleCoroutineScope
import androidx.navigation.NavController
import java.util.WeakHashMap
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import org.mozilla.fenix.R

/**
 * This class asynchronously loads the navigation graph XML. This is a performance optimization:
 * large nav graphs take ~29ms to inflate on the Moto G5 (#16900). This also seemingly prevents the
 * HomeFragment layout XML from being unnecessarily inflated when it isn't used, improving perf by
 * ~148ms on the Moto G5 for VIEW start up (#18245) though it was unintentional and we may wish to
 * implement more intentional for that.
 *
 * This class is defined as an Object, rather than as a class instance in our Components, because
 * it needs to be called by the [NavController] extension function which can't easily access Components.
 *
 * To use this class properly, [inflateNavGraphAsync] must be called first before
 * [blockForNavGraphInflation] using the same [NavController] instance.
 */
object NavGraphProvider {

    // We want to store member state on the NavController. However, there is no way to do this.
    // Instead, we store state as part of a map: NavController instance -> State. In order to
    // garbage collect our data when the NavController is no longer relevant, we use a WeakHashMap.
    private val map = WeakHashMap<NavController, Job>()

    fun inflateNavGraphAsync(navController: NavController, lifecycleScope: LifecycleCoroutineScope) {
        val inflationJob = lifecycleScope.launch(Dispatchers.IO) {
            val inflater = navController.navInflater
            navController.graph = inflater.inflate(R.navigation.nav_graph)
        }

        map[navController] = inflationJob
    }

    /**
     * The job should block the main thread if it isn't completed so that the NavGraph can be loaded
     * before any navigation is done.
     *
     * [inflateNavGraphAsync] must be called before this method.
     *
     * @throws IllegalStateException if [inflateNavGraphAsync] wasn't called first for this [NavController]
     */
    fun blockForNavGraphInflation(navController: NavController) {
        val inflationJob = map[navController] ?: throw IllegalStateException("Expected " +
            "`NavGraphProvider.inflateNavGraphAsync` to be called before this method with the same " +
            "`NavController` instance. If this occurred in a test, you probably need to add the " +
            "DisableNavGraphProviderAssertionRule.")
        runBlockingIncrement { inflationJob.join() }
    }
}
