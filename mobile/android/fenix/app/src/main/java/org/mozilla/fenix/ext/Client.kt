/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import java.io.IOException

/**
 * Given an image [url], fetches and returns a [Bitmap] if possible, otherwise null.
 *
 * @param url The image URL to fetch from.
 */
suspend fun Client.bitmapForUrl(url: String): Bitmap? = withContext(Dispatchers.IO) {
    // Code below will cache it in Gecko's cache, which ensures that as long as we've fetched it once,
    // we will be able to display this avatar as long as the cache isn't purged (e.g. via 'clear user data').
    val body = try {
        fetch(Request(url, useCaches = true, conservative = true)).body
    } catch (e: IOException) {
        return@withContext null
    }
    body.useStream { BitmapFactory.decodeStream(it) }
}
